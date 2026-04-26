#include <SDL.h>
#include <iostream>
#include "mmu.h"
#include "Timers.h"
#include "APU.h"

extern Timer* globalTimer;
extern void SPU_SetMMU(MMU* mmu);

using namespace std;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool isBitSetMMU(uint8_t n, uint8_t k) {
    return (n & (1 << k)) != 0;
}

// ---------------------------------------------------------------------------
// loadROM — almacena el dump completo y resetea el estado MBC
// ---------------------------------------------------------------------------
void MMU::loadROM(const std::vector<uint8_t>& data) {
    romData = data;

    // Detectar tipo MBC desde el header (byte 0x147)
    typeMBC = (data.size() > 0x147) ? data[0x147] : MBC_ROM_ONLY;

    // Calcular tamano de RAM externa segun header (byte 0x149)
    uint8_t ramSizeByte = (data.size() > 0x149) ? data[0x149] : 0;
    size_t ramBytes = 0;
    switch (ramSizeByte) {
        case 0x00: ramBytes = 0;        break; // Sin RAM (o MBC2 usa su propia)
        case 0x01: ramBytes = 0x800;    break; // 2 KB
        case 0x02: ramBytes = 0x2000;   break; // 8 KB
        case 0x03: ramBytes = 0x8000;   break; // 32 KB (4 bancos)
        case 0x04: ramBytes = 0x20000;  break; // 128 KB (16 bancos)
        case 0x05: ramBytes = 0x10000;  break; // 64 KB (8 bancos)
        default:   ramBytes = 0;        break;
    }

    // MBC2 tiene 512x4 bits de RAM integrada
    if (typeMBC == MBC_MBC2 || typeMBC == MBC_MBC2_BAT) {
        ramBytes = 0x200;
    }

    ramData.assign(ramBytes, 0xFF);

    // Reset estado MBC
    romBank      = 1;
    ramBank      = 0;
    ramEnabled   = false;
    mbc1Mode     = false;
    romBankHi    = 0;
    rtcReg       = 0xFF; // sin RTC mapeado
    rtcLatchReady = false;

    cout << "[MBC] Tipo: 0x" << hex << (unsigned)typeMBC
         << "  ROM: " << dec << romData.size() / 1024 << " KB"
         << "  RAM: " << ramData.size() / 1024 << " KB" << endl;
}

// ---------------------------------------------------------------------------
// readROM — traduce la direccion fisica segun banco activo
// ---------------------------------------------------------------------------
uint8_t MMU::readROM(uint16_t addr) {
    if (romData.empty()) return 0xFF;

    size_t physAddr = 0;

    if (addr < 0x4000) {
        // Banco 0 fijo, salvo MBC1 modo avanzado
        if ((typeMBC == MBC_MBC1 || typeMBC == MBC_MBC1_RAM || typeMBC == MBC_MBC1_RAM_BAT) && mbc1Mode) {
            // En modo RAM banking el banco base puede desplazarse
            uint8_t upperBits = ramBank & 0x03;
            physAddr = ((size_t)upperBits << 5) * 0x4000 + addr;
        } else {
            physAddr = addr;
        }
    } else {
        // Banco intercambiable 0x4000-0x7FFF
        uint32_t bank = 0;

        switch (typeMBC) {
        case MBC_ROM_ONLY:
            bank = 1;
            break;

        case MBC_MBC1:
        case MBC_MBC1_RAM:
        case MBC_MBC1_RAM_BAT: {
            uint8_t lower = romBank & 0x1F;
            uint8_t upper = mbc1Mode ? 0 : (ramBank & 0x03);
            bank = ((uint32_t)upper << 5) | lower;
            if (bank == 0x00 || bank == 0x20 || bank == 0x40 || bank == 0x60)
                bank++;
            break;
        }

        case MBC_MBC2:
        case MBC_MBC2_BAT:
            bank = romBank & 0x0F;
            if (bank == 0) bank = 1;
            break;

        case MBC_MBC3_TIMER_BAT:
        case MBC_MBC3_TIMER_RAM_BAT:
        case MBC_MBC3:
        case MBC_MBC3_RAM:
        case MBC_MBC3_RAM_BAT:
            bank = romBank & 0x7F;
            if (bank == 0) bank = 1;
            break;

        case MBC_MBC5:
        case MBC_MBC5_RAM:
        case MBC_MBC5_RAM_BAT:
        case MBC_MBC5_RUMBLE:
        case MBC_MBC5_RUMBLE_RAM:
        case MBC_MBC5_RUMBLE_RAM_BAT:
            bank = ((uint32_t)(romBankHi & 0x01) << 8) | romBank;
            break;

        default:
            bank = 1;
            break;
        }

        physAddr = (size_t)bank * 0x4000 + (addr - 0x4000);
    }

    if (physAddr >= romData.size()) return 0xFF;
    return romData[physAddr];
}

// ---------------------------------------------------------------------------
// readRAM / writeRAM
// ---------------------------------------------------------------------------
uint8_t MMU::readRAM(uint16_t addr) {
    uint16_t offset = addr - 0xA000;

    // MBC3 RTC mapeado
    if (rtcReg >= 0x08 && rtcReg <= 0x0C) {
        switch (rtcReg) {
        case 0x08: return rtcS;
        case 0x09: return rtcM;
        case 0x0A: return rtcH;
        case 0x0B: return rtcDL;
        case 0x0C: return rtcDH;
        }
    }

    if (!ramEnabled || ramData.empty()) return 0xFF;

    // MBC2: solo 512 nibbles (bits 3-0 validos)
    if (typeMBC == MBC_MBC2 || typeMBC == MBC_MBC2_BAT) {
        uint16_t mbcOffset = offset & 0x1FF;
        return ramData[mbcOffset] | 0xF0; // bits altos siempre 1
    }

    size_t physAddr = (size_t)ramBank * 0x2000 + offset;
    if (physAddr >= ramData.size()) return 0xFF;
    return ramData[physAddr];
}

void MMU::writeRAM(uint16_t addr, uint8_t value) {
    uint16_t offset = addr - 0xA000;

    // MBC3 RTC mapeado
    if (rtcReg >= 0x08 && rtcReg <= 0x0C) {
        switch (rtcReg) {
        case 0x08: rtcS  = value & 0x3F; return;
        case 0x09: rtcM  = value & 0x3F; return;
        case 0x0A: rtcH  = value & 0x1F; return;
        case 0x0B: rtcDL = value;        return;
        case 0x0C: rtcDH = value & 0xC1; return;
        }
    }

    if (!ramEnabled || ramData.empty()) return;

    if (typeMBC == MBC_MBC2 || typeMBC == MBC_MBC2_BAT) {
        uint16_t mbcOffset = offset & 0x1FF;
        ramData[mbcOffset] = value & 0x0F; // solo nibble bajo
        return;
    }

    size_t physAddr = (size_t)ramBank * 0x2000 + offset;
    if (physAddr >= ramData.size()) return;
    ramData[physAddr] = value;
}

// ---------------------------------------------------------------------------
// handleMBCWrite — interpreta escrituras a 0x0000-0x7FFF como registros MBC
// ---------------------------------------------------------------------------
void MMU::handleMBCWrite(uint16_t addr, uint8_t value) {
    switch (typeMBC) {

    // ----- ROM ONLY -----
    case MBC_ROM_ONLY:
        break;

    // ----- MBC1 -----
    case MBC_MBC1:
    case MBC_MBC1_RAM:
    case MBC_MBC1_RAM_BAT:
        if (addr < 0x2000) {
            // Habilitar/deshabilitar RAM
            ramEnabled = ((value & 0x0F) == 0x0A);
        } else if (addr < 0x4000) {
            // Bits 0-4 del numero de banco ROM
            romBank = (value & 0x1F);
            if (romBank == 0) romBank = 1;
        } else if (addr < 0x6000) {
            // Registro secundario: bits 5-6 del banco ROM o banco RAM
            ramBank = value & 0x03;
        } else {
            // Modo banking
            mbc1Mode = (value & 0x01) != 0;
            if (!mbc1Mode) ramBank = 0; // modo ROM: RAM bank fijado a 0
        }
        break;

    // ----- MBC2 -----
    case MBC_MBC2:
    case MBC_MBC2_BAT:
        if (addr < 0x4000) {
            if (addr & 0x0100) {
                // Bit 8 de la direccion = 1 -> seleccion de banco ROM
                romBank = value & 0x0F;
                if (romBank == 0) romBank = 1;
            } else {
                // Bit 8 = 0 -> habilitar/deshabilitar RAM
                ramEnabled = ((value & 0x0F) == 0x0A);
            }
        }
        break;

    // ----- MBC3 -----
    case MBC_MBC3_TIMER_BAT:
    case MBC_MBC3_TIMER_RAM_BAT:
    case MBC_MBC3:
    case MBC_MBC3_RAM:
    case MBC_MBC3_RAM_BAT:
        if (addr < 0x2000) {
            ramEnabled = ((value & 0x0F) == 0x0A);
        } else if (addr < 0x4000) {
            romBank = value & 0x7F;
            if (romBank == 0) romBank = 1;
        } else if (addr < 0x6000) {
            if (value <= 0x07) {
                // Banco RAM
                ramBank = value;
                rtcReg  = 0xFF; // desactivar RTC
            } else if (value >= 0x08 && value <= 0x0C) {
                // Registro RTC
                rtcReg = value;
            }
        } else {
            // Latch RTC: secuencia 0x00 -> 0x01
            if (value == 0x00) {
                rtcLatchReady = true;
            } else if (value == 0x01 && rtcLatchReady) {
                // En una implementacion completa aqui se congelaria el RTC
                rtcLatchReady = false;
            }
        }
        break;

    // ----- MBC5 -----
    case MBC_MBC5:
    case MBC_MBC5_RAM:
    case MBC_MBC5_RAM_BAT:
    case MBC_MBC5_RUMBLE:
    case MBC_MBC5_RUMBLE_RAM:
    case MBC_MBC5_RUMBLE_RAM_BAT:
        if (addr < 0x2000) {
            ramEnabled = ((value & 0x0F) == 0x0A);
        } else if (addr < 0x3000) {
            // Bits 0-7 del banco ROM
            romBank = value;
        } else if (addr < 0x4000) {
            // Bit 8 del banco ROM
            romBankHi = value & 0x01;
        } else if (addr < 0x6000) {
            // Banco RAM (bits 0-3); en cartuchos rumble bit 3 = motor
            ramBank = value & 0x0F;
        }
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// read / read8
// ---------------------------------------------------------------------------
uint16_t MMU::read(uint16_t addr) {
    return (read8(addr + 1) << 8) | read8(addr);
}

uint8_t MMU::read8(uint16_t addr) {
    switch (addr & 0xF000)
    {
    // ROM banco 0 fijo + banco intercambiable
    case 0x0000: case 0x1000: case 0x2000: case 0x3000:
    case 0x4000: case 0x5000: case 0x6000: case 0x7000:
        return readROM(addr);

    case 0x8000:
    case 0x9000: // VRAM
        return vram[addr - 0x8000];

    case 0xA000:
    case 0xB000: // RAM externa / RTC
        return readRAM(addr);

    case 0xC000:
    case 0xD000: // Working RAM
        return wram[addr - 0xC000];

    case 0xE000: // Shadow RAM
        return wram[addr - 0xE000];

    case 0xF000:
        if (addr >= 0xFE00 && addr <= 0xFE9F) return sprite_attrib[addr - 0xFE00];
        if (addr >= 0xFEA0 && addr <  0xFF00) return 0;

        if (addr >= 0xFF00 && addr < 0xFF80) {
            if (addr == 0xFF00) {
                uint8_t currentReg = io[0x00];
                uint8_t result = 0x0F;
                result |= (currentReg & 0x30);
                if (!(currentReg & 0x10)) result &= directionsButton;
                if (!(currentReg & 0x20)) result &= actionButton;
                result |= 0xC0;
                return result;
            }
            if (addr == 0xFF0F) return io[0x0F] | 0xE0;
            return io[addr - 0xFF00];
        }

        if (addr >= 0xFF80 && addr < 0xFFFF) return internal_ram[addr - 0xFF80];
        if (addr == 0xFFFF)                  return IE;
        return 0;

    default:
        return 0xFF;
    }
}

// ---------------------------------------------------------------------------
// write / write8
// ---------------------------------------------------------------------------
void MMU::write(uint16_t addr, uint16_t value) {
    write8(addr,     (uint8_t)(value & 0xFF));
    write8(addr + 1, (uint8_t)(value >> 8));
}

void MMU::write8(uint16_t addr, uint8_t value) {
    switch (addr & 0xF000)
    {
    // Escrituras a ROM -> registros MBC
    case 0x0000: case 0x1000: case 0x2000: case 0x3000:
    case 0x4000: case 0x5000: case 0x6000: case 0x7000:
        handleMBCWrite(addr, value);
        break;

    case 0x8000:
    case 0x9000: // VRAM
        vram[addr - 0x8000] = value;
        break;

    case 0xA000:
    case 0xB000: // RAM externa / RTC
        writeRAM(addr, value);
        break;

    case 0xC000:
    case 0xD000: // Working RAM
        wram[addr - 0xC000] = value;
        break;

    case 0xE000: // Shadow RAM
        wram[addr - 0xE000] = value;
        break;

    case 0xF000:
        if (addr >= 0xFE00 && addr <= 0xFE9F) {
            sprite_attrib[addr - 0xFE00] = value;
            return;
        }

        if (addr >= 0xFF00 && addr < 0xFF80) {
            if (addr >= 0xFF10 && addr <= 0xFF3F) {
                io[addr - 0xFF00] = value;
                if      (addr == 0xFF14 && (value & 0x80)) resetSC1length(io[0x11] & 0x3F);
                else if (addr == 0xFF19 && (value & 0x80)) resetSC2length(io[0x16] & 0x3F);
                else if (addr == 0xFF1E && (value & 0x80)) resetSC3length(io[0x1B]);
                else if (addr == 0xFF23 && (value & 0x80)) resetSC4length(io[0x20] & 0x3F);
                return;
            }
            if (addr == 0xFF00) { io[0x00] = (value & 0x30) | 0xCF; return; }
            if (addr == 0xFF04) {
                io[0x04] = 0;
                if (globalTimer) globalTimer->divCounter = 0;
                return;
            }
            if (addr == 0xFF07) {
                uint8_t oldTAC = io[0x07];
                uint8_t newTAC = value | 0xF8;
                io[0x07] = newTAC;
                if ((oldTAC & 0x04) && !(newTAC & 0x04) && globalTimer)
                    globalTimer->timaCounter = 0;
                return;
            }
            if (addr == 0xFF41) {
                io[0x41] = (value & 0xF8) | (io[0x41] & 0x07);
                return;
            }
            if (addr == 0xFF46) { DMATransfer(value); cyclesToAdd += 160; return; }
            if (addr == 0xFF44) { io[addr - 0xFF00] = 0; return; }
            io[addr - 0xFF00] = value;
            return;
        }

        if (addr >= 0xFF80 && addr < 0xFFFF) { internal_ram[addr - 0xFF80] = value; return; }
        if (addr == 0xFFFF) { IE = value; return; }
        break;
    }
}

// ---------------------------------------------------------------------------
// DMA Transfer
// ---------------------------------------------------------------------------
void MMU::DMATransfer(uint8_t value) {
    uint16_t address = (uint16_t)value << 8;
    for (int i = 0; i < 0xA0; i++)
        write8(0xFE00 + i, read8(address + i));
}

// ---------------------------------------------------------------------------
// Stack
// ---------------------------------------------------------------------------
void MMU::push(uint16_t value) {
    sp -= 1; write8(sp, (value >> 8) & 0xFF);
    sp -= 1; write8(sp, value & 0xFF);
}

void MMU::pop(uint16_t* value) {
    uint8_t low  = read8(sp); sp++;
    uint8_t high = read8(sp); sp++;
    *value = (high << 8) | low;
}

// ---------------------------------------------------------------------------
// Register helpers
// ---------------------------------------------------------------------------
void MMU::setRegisters16Bit(GameboyRegisters* reg, const char* regName, uint16_t valueToSet, GameboyFlags* flags) {
    string name = regName;
    if (name == "AF") {
        reg->A  = (valueToSet >> 8);
        reg->F  = valueToSet & 0xF0;
        reg->AF = (reg->A << 8) | reg->F;
    } else if (name == "BC") {
        reg->B  = (valueToSet >> 8);
        reg->C  = (uint8_t)valueToSet;
        reg->BC = valueToSet;
    } else if (name == "DE") {
        reg->D  = (valueToSet >> 8);
        reg->E  = (uint8_t)valueToSet;
        reg->DE = valueToSet;
    } else if (name == "HL") {
        reg->H  = (valueToSet >> 8);
        reg->L  = (uint8_t)valueToSet;
        reg->HL = valueToSet;
    }
}

void MMU::setRegisters8Bit(GameboyRegisters* reg, const char* regName, uint8_t valueToSet, GameboyFlags* flags) {
    string name = regName;
    if      (name == "A") { reg->A = valueToSet; reg->AF = (reg->A << 8) | reg->F; }
    else if (name == "F") { reg->F = valueToSet & 0xF0; reg->AF = (reg->A << 8) | reg->F; }
    else if (name == "B") { reg->B = valueToSet; reg->BC = (reg->B << 8) | reg->C; }
    else if (name == "C") { reg->C = valueToSet; reg->BC = (reg->B << 8) | reg->C; }
    else if (name == "D") { reg->D = valueToSet; reg->DE = (reg->D << 8) | reg->E; }
    else if (name == "E") { reg->E = valueToSet; reg->DE = (reg->D << 8) | reg->E; }
    else if (name == "H") { reg->H = valueToSet; reg->HL = (reg->H << 8) | reg->L; }
    else if (name == "L") { reg->L = valueToSet; reg->HL = (reg->H << 8) | reg->L; }
}
