#include <SDL.h>
#include <iostream>
#include "mmu.h"
#include "Timers.h"
#include "APU.h"

extern Timer* globalTimer;
extern void SPU_SetMMU(MMU* mmu);

using namespace std;

// Helper para consistencia (usaremos este en lugar de isBitSet)
bool isBitSetMMU(uint8_t n, uint8_t k) {
    return (n & (1 << k));
}

uint16_t MMU::read(uint16_t addr) {
    // Little Endian: Byte bajo en addr, Byte alto en addr+1
    return (read8(addr + 1) << 8) | read8(addr);
}

void MMU::write(uint16_t addr, uint16_t value) {
    // CORRECCI�N IMPORTANTE: GameBoy es Little Endian.
    // Primero escribimos el byte bajo, luego el alto.
    write8(addr, (uint8_t)(value & 0xFF));
    write8(addr + 1, (uint8_t)(value >> 8));
}

uint8_t MMU::read8(uint16_t addr) {
    switch (addr & 0xF000)
    {
    case 0x0000:
    case 0x1000:
    case 0x2000:
    case 0x3000:
    case 0x4000:
    case 0x5000:
    case 0x6000:
    case 0x7000:
        return rom[addr];

    case 0x8000:
    case 0x9000: // VRAM
        return vram[addr - 0x8000];

    case 0xA000:
    case 0xB000: // External RAM (Cartucho)
        return ram[addr - 0xA000];

    case 0xC000:
    case 0xD000: // Working RAM
        return wram[addr - 0xC000];

    case 0xE000: // Working RAM Shadow
        return wram[addr - 0xE000];

    case 0xF000:
        // OAM (Sprite Attribute Table) 0xFE00 - 0xFE9F
        if (addr >= 0xFE00 && addr <= 0xFE9F) {
            return sprite_attrib[addr - 0xFE00];
        }

        // Unusable Memory
        if (addr >= 0xFEA0 && addr < 0xFF00) {
            return 0;
        }

        // IO Registers (0xFF00 - 0xFF7F)
        if (addr >= 0xFF00 && addr < 0xFF80) {

            if (addr == 0xFF00) {
                uint8_t currentReg = io[0x00];

                // Empezamos con 0x0F (bits 0-3 en 1 = nada presionado)
                uint8_t result = 0x0F;

                // Copiamos los bits de selecci�n (4 y 5) que escribi� el juego
                result |= (currentReg & 0x30);

                // Si P14=0 (bit 4), leemos direcciones
                if (!(currentReg & 0x10)) {
                    result &= directionsButton;
                }

                // Si P15=0 (bit 5), leemos botones de acci�n
                if (!(currentReg & 0x20)) {
                    result &= actionButton;
                }

                // Bits 6-7 siempre en 1
                result |= 0xC0;
                return result;
            }

            if (addr == 0xFF0F) {
                return io[0x0F] | 0xE0;
            }

            return io[addr - 0xFF00];
        }

        // High RAM (HRAM) (0xFF80 - 0xFFFE)
        if (addr >= 0xFF80 && addr < 0xFFFF) {
            return internal_ram[addr - 0xFF80];
        }

        // Interrupt Enable Register (0xFFFF)
        if (addr == 0xFFFF) {
            return IE;
        }

        return 0;

    default:
        // cout << "Read Warning: Unmapped memory " << hex << addr << endl;
        return 0xFF;
    }
}

void MMU::write8(uint16_t addr, uint8_t value) {
    switch (addr & 0xF000)
    {
    case 0x0000:
    case 0x1000:
    case 0x2000:
    case 0x3000:
    case 0x4000:
    case 0x5000:
    case 0x6000:
    case 0x7000:
        // CORRECCI�N CR�TICA: NO ESCRIBIR EN ROM
        // Escribir aqu� se usa para configurar el MBC (Memory Bank Controller).
        // Por ahora no lo implementamos, pero JAM�S debemos sobrescribir rom[].
        // handleMBC(addr, value); 
        break;

    case 0x8000:
    case 0x9000: // VRAM
        vram[addr - 0x8000] = value;
        break;

    case 0xA000:
    case 0xB000: // External RAM
        ram[addr - 0xA000] = value;
        break;

    case 0xC000:
    case 0xD000: // Working RAM
        wram[addr - 0xC000] = value;
        break;

    case 0xE000: // Shadow RAM
        wram[addr - 0xE000] = value;
        break;

    case 0xF000:
        // OAM
        if (addr >= 0xFE00 && addr <= 0xFE9F) {
            sprite_attrib[addr - 0xFE00] = value;
            return;
        }

        // IO Registers
        if (addr >= 0xFF00 && addr < 0xFF80) {

            // Audio registers - write to io[] and trigger channel resets on NRx4
            if (addr >= 0xFF10 && addr <= 0xFF3F) {
                io[addr - 0xFF00] = value;
                // NR14: SC1 trigger
                if (addr == 0xFF14 && (value & 0x80))
                    resetSC1length(io[0x11] & 0x3F);
                // NR24: SC2 trigger
                else if (addr == 0xFF19 && (value & 0x80))
                    resetSC2length(io[0x16] & 0x3F);
                // NR34: SC3 trigger
                else if (addr == 0xFF1E && (value & 0x80))
                    resetSC3length(io[0x1B]);
                // NR44: SC4 trigger
                else if (addr == 0xFF23 && (value & 0x80))
                    resetSC4length(io[0x20] & 0x3F);
                return;
            }

            if (addr == 0xFF00) {
                io[0x00] = (value & 0x30) | 0xCF;
                //std::cout << "Value to set in 0xFF00: " << static_cast<unsigned>(result) << std::hex << std::endl;
                return;
            }

            if (addr == 0xFF04) {
                io[0x04] = 0;
                if (globalTimer != nullptr) {
                    globalTimer->divCounter = 0;
                }
                return;
            }

            if (addr == 0xFF07) {
                uint8_t oldTAC = io[0x07];
                uint8_t newTAC = value | 0xF8;
                io[0x07] = newTAC;

                // Si el timer se desactiva, resetear timaCounter
                if ((oldTAC & 0x04) && !(newTAC & 0x04)) {
                    if (globalTimer != nullptr) {
                        globalTimer->timaCounter = 0;  // Resetear cuando se desactiva
                    }
                }
                return;
            }

            if (addr == 0xFF41) {
                uint8_t currentStat = io[0x41];

                // M�scara: Conservamos los bits 0,1,2 actuales, tomamos el resto del nuevo valor
                // Bit 7 siempre es 1 en Game Boy original, pero a veces se ignora.
                io[0x41] = (value & 0xF8) | (currentStat & 0x07);
                return;
            }

            // DMA Transfer Trigger
            if (addr == 0xFF46) {
                DMATransfer(value);
                cyclesToAdd += 160;
                return;
            }

            // Reset LY (Scanline) si se escribe en 0xFF44
            if (addr == 0xFF44) {
                io[addr - 0xFF00] = 0;
                return;
            }

            // Serial Output (Simulado para pruebas de Blargg)
            if (addr == 0xFF01) {
                // cout << (char)value; 
            }

            io[addr - 0xFF00] = value;
            return;
        }

        // High RAM (HRAM)
        if (addr >= 0xFF80 && addr < 0xFFFF) {
            // ELIMINADO: El hack de Tetris que romp�a la escritura.
            internal_ram[addr - 0xFF80] = value;
            return;
        }

        // Interrupt Enable
        if (addr == 0xFFFF) {
            IE = value;
            return;
        }
        break;
    }
}

void MMU::DMATransfer(uint8_t value) {
    uint16_t address = (uint16_t)value << 8;

    for (int i = 0; i < 0xA0; i++) {
        // Usamos read8 para leer de donde sea (ROM/RAM) y escribimos directo a OAM
        // Nota: DMATransfer es instant�neo aqu�, en HW real tarda 160 microsegundos.
        uint8_t data = read8(address + i);
        write8(0xFE00 + i, data);
    }
}

void MMU::push(uint16_t value) {
    // Stack crece hacia abajo. Escribe High, luego Low.
    sp-=1;
    write8(sp, (value >> 8) & 0xFF);
    sp-=1;
    write8(sp, value & 0xFF);
}

void MMU::pop(uint16_t* value) {
    // Lee Low, luego High.
    uint8_t low = read8(sp);
    sp+=1;
    uint8_t high = read8(sp);
    sp+=1;
    *value = (high << 8) | low;
}

void MMU::setRegisters16Bit(GameboyRegisters* reg, const char* regName, uint16_t valueToSet, GameboyFlags* flags) {
    // Esta funci�n auxiliar est� bien, la mantengo igual.
    string name = regName; // Convertir a string para comparar f�cil

    if (name == "AF") {
        reg->A = (valueToSet >> 8);
        uint8_t newF = valueToSet & 0xF0; // Forzar bits bajos a 0
        reg->F = newF;

        // Sincronizar BOOLEANOS desde el nuevo valor de F (Crucial para POP AF)

        reg->AF = (reg->A << 8) | reg->F;
    }
    else if (name == "BC") {
        reg->B = (valueToSet >> 8);
        reg->C = (uint8_t)valueToSet;
        reg->BC = valueToSet;
    }
    else if (name == "DE") {
        reg->D = (valueToSet >> 8);
        reg->E = (uint8_t)valueToSet;
        reg->DE = valueToSet;
    }
    else if (name == "HL") {
        reg->H = (valueToSet >> 8);
        reg->L = (uint8_t)valueToSet;
        reg->HL = valueToSet;
    }
}

void MMU::setRegisters8Bit(GameboyRegisters* reg, const char* regName, uint8_t valueToSet, GameboyFlags* flags) {
    string name = regName;

    if (name == "A") { 
        reg->A = valueToSet; 
        reg->AF = (reg->A << 8) | reg->F; 
    }
    else if (name == "F") {
        reg->F = valueToSet & 0xF0; // Solo los 4 bits altos importan
        reg->AF = (reg->A << 8) | reg->F;
    }
    else if (name == "B") { reg->B = valueToSet; reg->BC = (reg->B << 8) | reg->C; }
    else if (name == "C") { reg->C = valueToSet; reg->BC = (reg->B << 8) | reg->C; }
    else if (name == "D") { reg->D = valueToSet; reg->DE = (reg->D << 8) | reg->E; }
    else if (name == "E") { reg->E = valueToSet; reg->DE = (reg->D << 8) | reg->E; }
    else if (name == "H") { reg->H = valueToSet; reg->HL = (reg->H << 8) | reg->L; }
    else if (name == "L") { reg->L = valueToSet; reg->HL = (reg->H << 8) | reg->L; }
}