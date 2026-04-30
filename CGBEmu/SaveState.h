#pragma once
#include <string>
#include <cstdint>
#include <vector>

// Forward declarations
class CPU;
class GPU;

// Magic number + version for .sav file validation
static const uint32_t SAVESTATE_MAGIC   = 0x41564F43; // "AVOC"
static const uint16_t SAVESTATE_VERSION = 0x0001;

// Packed binary layout of the full emulator state
#pragma pack(push, 1)
struct SaveStateHeader {
    uint32_t magic;
    uint16_t version;
    char     romTitle[16];
};

struct SaveStateCPU {
    uint16_t pc;
    uint16_t sp;
    uint8_t  A, F, B, C, D, E, H, L;
    bool     IME;
    bool     isHalted;
    bool     isStopped;
};

struct SaveStateMMU {
    // Fixed RAM regions
    uint8_t  vram[0x2000 * 2];     // uint16_t stored as bytes
    uint8_t  wram[0x2000 * 2];
    uint8_t  sprite_attrib[0xA0];
    uint8_t  io[0xFF];
    uint8_t  internal_ram[0x7F];
    uint8_t  IE;

    // MBC state
    uint8_t  typeMBC;
    bool     ramEnabled;
    uint8_t  romBank;
    uint8_t  ramBank;
    bool     mbc1Mode;
    uint8_t  romBankHi;

    // RTC
    uint8_t  rtcS, rtcM, rtcH, rtcDL, rtcDH;
    bool     rtcLatchReady;
    uint8_t  rtcReg;

    // External RAM size (for reading the variable vector below)
    uint32_t ramDataSize;

    // CGB extensions
    bool     cgbMode;
    uint8_t  vramBank[2][0x2000];  // CGB VRAM banks
    uint8_t  wramBank[8][0x1000];  // CGB WRAM banks
    uint8_t  bgPaletteRAM[64];
    uint8_t  objPaletteRAM[64];
    uint8_t  vbk;
    uint8_t  svbk;
    uint8_t  bcps;
    uint8_t  ocps;
};

struct SaveStateGPU {
    uint8_t  mode;
    uint16_t clock;
    uint8_t  line;
    uint8_t  wly;
    bool     checkLYC;
    uint8_t  framebuffer[144][160][3];
};
#pragma pack(pop)

// Returns the save file path for the given ROM title: "title.sav"
std::string getSavePath(const std::string& romTitle);

// Sanitises ROM title bytes into a valid filename
std::string sanitiseTitle(const char title[16]);

// Save full emulator state to disk. Returns true on success.
bool saveState(const std::string& path, CPU& cpu, GPU& gpu);

// Load full emulator state from disk. Returns true on success.
bool loadState(const std::string& path, CPU& cpu, GPU& gpu);

// Returns true if a save file exists at the given full path
bool saveExists(const std::string& fullPath);
