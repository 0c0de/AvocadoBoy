#include "SaveState.h"
#include "CPU.h"
#include "GPU.h"
#include "mmu.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>

std::string sanitiseTitle(const char title[16]) {
    std::string s;
    for (int i = 0; i < 16; i++) {
        char c = title[i];
        if (c == '\0') break;
        // Replace any character illegal in filenames with '_'
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
        s += c;
    }
    if (s.empty()) s = "UNKNOWN";
    return s;
}

std::string getSavePath(const std::string& romTitle) {
    return sanitiseTitle(romTitle.c_str()) + ".sav";
}

bool saveExists(const std::string& fullPath) {
    std::ifstream f(fullPath, std::ios::binary);
    return f.good();
}

bool saveState(const std::string& path, CPU& cpu, GPU& gpu) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        std::cout << "[SaveState] Cannot open for writing: " << path << std::endl;
        return false;
    }

    MMU* mmu = cpu.getMMUValues();

    // --- Header ---
    SaveStateHeader hdr{};
    hdr.magic   = SAVESTATE_MAGIC;
    hdr.version = SAVESTATE_VERSION;
    // Copy ROM title from bytes 0x134-0x143 of ROM
    if (mmu->romData.size() > 0x143) {
        for (int i = 0; i < 16; i++)
            hdr.romTitle[i] = (char)mmu->romData[0x134 + i];
    }
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    // --- CPU state ---
    SaveStateCPU cs{};
    cs.pc        = cpu.pc;
    cs.sp        = mmu->sp;
    cs.A         = cpu.getGameboyRegisters()->A;
    cs.F         = cpu.getGameboyRegisters()->F;
    cs.B         = cpu.getGameboyRegisters()->B;
    cs.C         = cpu.getGameboyRegisters()->C;
    cs.D         = cpu.getGameboyRegisters()->D;
    cs.E         = cpu.getGameboyRegisters()->E;
    cs.H         = cpu.getGameboyRegisters()->H;
    cs.L         = cpu.getGameboyRegisters()->L;
    cs.IME       = cpu.IME;
    cs.isHalted  = cpu.isHalted;
    cs.isStopped = cpu.isStoped;
    f.write(reinterpret_cast<const char*>(&cs), sizeof(cs));

    // --- MMU state ---
    SaveStateMMU ms{};
    // VRAM/WRAM are uint16_t arrays — copy raw bytes
    std::memcpy(ms.vram,          mmu->vram,          sizeof(mmu->vram));
    std::memcpy(ms.wram,          mmu->wram,          sizeof(mmu->wram));
    std::memcpy(ms.sprite_attrib, mmu->sprite_attrib, sizeof(mmu->sprite_attrib));
    std::memcpy(ms.io,            mmu->io,            sizeof(mmu->io));
    std::memcpy(ms.internal_ram,  mmu->internal_ram,  sizeof(mmu->internal_ram));
    ms.IE            = mmu->IE;
    ms.typeMBC       = mmu->typeMBC;
    ms.ramEnabled    = mmu->ramEnabled;
    ms.romBank       = mmu->romBank;
    ms.ramBank       = mmu->ramBank;
    ms.mbc1Mode      = mmu->mbc1Mode;
    ms.romBankHi     = mmu->romBankHi;
    ms.rtcS          = mmu->rtcS;
    ms.rtcM          = mmu->rtcM;
    ms.rtcH          = mmu->rtcH;
    ms.rtcDL         = mmu->rtcDL;
    ms.rtcDH         = mmu->rtcDH;
    ms.rtcLatchReady = mmu->rtcLatchReady;
    ms.rtcReg        = mmu->rtcReg;
    ms.ramDataSize   = (uint32_t)mmu->ramData.size();
    // CGB
    ms.cgbMode = mmu->cgbMode;
    std::memcpy(ms.vramBank,     mmu->vramBank,     sizeof(mmu->vramBank));
    std::memcpy(ms.wramBank,     mmu->wramBank,     sizeof(mmu->wramBank));
    std::memcpy(ms.bgPaletteRAM, mmu->bgPaletteRAM, sizeof(mmu->bgPaletteRAM));
    std::memcpy(ms.objPaletteRAM,mmu->objPaletteRAM,sizeof(mmu->objPaletteRAM));
    ms.vbk  = mmu->vbk;
    ms.svbk = mmu->svbk;
    ms.bcps = mmu->bcps;
    ms.ocps = mmu->ocps;
    f.write(reinterpret_cast<const char*>(&ms), sizeof(ms));

    // External RAM (variable size)
    if (ms.ramDataSize > 0)
        f.write(reinterpret_cast<const char*>(mmu->ramData.data()), ms.ramDataSize);

    // --- GPU state ---
    SaveStateGPU gs{};
    gs.mode     = gpu.mode;
    gs.clock    = gpu.clock;
    gs.line     = gpu.line;
    gs.wly      = gpu.wly;
    gs.checkLYC = gpu.checkLYC;
    std::memcpy(gs.framebuffer, gpu.framebuffer, sizeof(gpu.framebuffer));
    f.write(reinterpret_cast<const char*>(&gs), sizeof(gs));

    f.flush();
    std::cout << "[SaveState] Saved to: " << path << std::endl;
    return true;
}

bool loadState(const std::string& path, CPU& cpu, GPU& gpu) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cout << "[SaveState] Cannot open: " << path << std::endl;
        return false;
    }

    MMU* mmu = cpu.getMMUValues();

    // --- Header ---
    SaveStateHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (hdr.magic != SAVESTATE_MAGIC || hdr.version != SAVESTATE_VERSION) {
        std::cout << "[SaveState] Invalid file or version mismatch." << std::endl;
        return false;
    }

    // --- CPU state ---
    SaveStateCPU cs{};
    f.read(reinterpret_cast<char*>(&cs), sizeof(cs));
    cpu.pc       = cs.pc;
    mmu->sp      = cs.sp;
    cpu.getGameboyRegisters()->A  = cs.A;
    cpu.getGameboyRegisters()->F  = cs.F;
    cpu.getGameboyRegisters()->B  = cs.B;
    cpu.getGameboyRegisters()->C  = cs.C;
    cpu.getGameboyRegisters()->D  = cs.D;
    cpu.getGameboyRegisters()->E  = cs.E;
    cpu.getGameboyRegisters()->H  = cs.H;
    cpu.getGameboyRegisters()->L  = cs.L;
    // Rebuild 16-bit pairs
    cpu.getGameboyRegisters()->AF = ((uint16_t)cs.A << 8) | cs.F;
    cpu.getGameboyRegisters()->BC = ((uint16_t)cs.B << 8) | cs.C;
    cpu.getGameboyRegisters()->DE = ((uint16_t)cs.D << 8) | cs.E;
    cpu.getGameboyRegisters()->HL = ((uint16_t)cs.H << 8) | cs.L;
    cpu.IME       = cs.IME;
    cpu.isHalted  = cs.isHalted;
    cpu.isStoped  = cs.isStopped;

    // --- MMU state ---
    SaveStateMMU ms{};
    f.read(reinterpret_cast<char*>(&ms), sizeof(ms));
    std::memcpy(mmu->vram,          ms.vram,          sizeof(mmu->vram));
    std::memcpy(mmu->wram,          ms.wram,          sizeof(mmu->wram));
    std::memcpy(mmu->sprite_attrib, ms.sprite_attrib, sizeof(mmu->sprite_attrib));
    std::memcpy(mmu->io,            ms.io,            sizeof(mmu->io));
    std::memcpy(mmu->internal_ram,  ms.internal_ram,  sizeof(mmu->internal_ram));
    mmu->IE            = ms.IE;
    mmu->typeMBC       = (MBCType)ms.typeMBC;
    mmu->ramEnabled    = ms.ramEnabled;
    mmu->romBank       = ms.romBank;
    mmu->ramBank       = ms.ramBank;
    mmu->mbc1Mode      = ms.mbc1Mode;
    mmu->romBankHi     = ms.romBankHi;
    mmu->rtcS          = ms.rtcS;
    mmu->rtcM          = ms.rtcM;
    mmu->rtcH          = ms.rtcH;
    mmu->rtcDL         = ms.rtcDL;
    mmu->rtcDH         = ms.rtcDH;
    mmu->rtcLatchReady = ms.rtcLatchReady;
    mmu->rtcReg        = ms.rtcReg;
    // CGB
    mmu->cgbMode = ms.cgbMode;
    std::memcpy(mmu->vramBank,     ms.vramBank,     sizeof(mmu->vramBank));
    std::memcpy(mmu->wramBank,     ms.wramBank,     sizeof(mmu->wramBank));
    std::memcpy(mmu->bgPaletteRAM, ms.bgPaletteRAM, sizeof(mmu->bgPaletteRAM));
    std::memcpy(mmu->objPaletteRAM,ms.objPaletteRAM,sizeof(mmu->objPaletteRAM));
    mmu->vbk  = ms.vbk;
    mmu->svbk = ms.svbk;
    mmu->bcps = ms.bcps;
    mmu->ocps = ms.ocps;

    // External RAM
    if (ms.ramDataSize > 0) {
        mmu->ramData.resize(ms.ramDataSize);
        f.read(reinterpret_cast<char*>(mmu->ramData.data()), ms.ramDataSize);
    }

    // --- GPU state ---
    SaveStateGPU gs{};
    f.read(reinterpret_cast<char*>(&gs), sizeof(gs));
    gpu.mode     = gs.mode;
    gpu.clock    = gs.clock;
    gpu.line     = gs.line;
    gpu.wly      = gs.wly;
    gpu.checkLYC = gs.checkLYC;
    std::memcpy(gpu.framebuffer, gs.framebuffer, sizeof(gpu.framebuffer));

    std::cout << "[SaveState] Loaded from: " << path << std::endl;
    return true;
}
