#include "APU.h"
#include "mmu.h"
#include <SDL.h>
#include <iostream>
#include <vector>
#include <cstdint>


// ─── MMU access ──────────────────────────────────────────────────────────────

static MMU* g_mmu = nullptr;
void SPU_SetMMU(MMU* mmu) { g_mmu = mmu; }

static inline uint8_t reg(uint16_t addr) {
    if (!g_mmu) return 0xFF;
    return g_mmu->io[addr - 0xFF00];
}
static inline void writereg(uint16_t addr, uint8_t val) {
    if (g_mmu) g_mmu->io[addr - 0xFF00] = val;
}

// ─── Toggle / volume helpers ──────────────────────────────────────────────────

static bool  useSC1 = true, useSC2 = true, useSC3 = true, useSC4 = true;
static float volume = 0.5f;
static bool  SoundIsPlaying = false;

void toggleAudio()       { useSC1 = useSC2 = useSC3 = useSC4 = !useSC1; }
void toggleSC1()         { useSC1 = !useSC1; }
void toggleSC2()         { useSC2 = !useSC2; }
void toggleSC3()         { useSC3 = !useSC3; }
void toggleSC4()         { useSC4 = !useSC4; }
void toggleRemix()       {}
void setVolume(float v)  { volume = v; }

// ─── Duty table ──────────────────────────────────────────────────────────────

static const uint8_t duties[4][8] = {
    {0,0,0,0,0,0,0,1},  // 12.5%
    {1,0,0,0,0,0,0,1},  // 25%
    {1,0,0,0,0,1,1,1},  // 50%
    {0,1,1,1,1,1,1,0}   // 75%
};

static const uint32_t SC4divisorTable[8] = { 8,16,32,48,64,80,96,112 };

// ─── APU state ───────────────────────────────────────────────────────────────

// Shared frame sequencer
static uint32_t g_fsCycles = 0;
static uint8_t  g_fsStep   = 0;

// Downsampler
// 4194304 / 44100 ≈ 95.1058 T-cycles per output sample
static const double CYCLES_PER_SAMPLE = 4194304.0 / 44100.0;
static double g_sampleAcc = 0.0;

// CH1
static uint32_t ch1Timer     = 0;
static uint8_t  ch1DutyPos   = 0;
static uint16_t ch1Length    = 0;
static uint8_t  ch1Volume    = 0;
static uint8_t  ch1EnvTimer  = 0;
static bool     ch1EnvEn     = false;
static bool     ch1On        = false;
static bool     ch1SweepEn   = false;
static uint8_t  ch1SweepTimer= 0;
static uint16_t ch1SweepShadow = 0;

// CH2
static uint32_t ch2Timer    = 0;
static uint8_t  ch2DutyPos  = 0;
static uint16_t ch2Length   = 0;
static uint8_t  ch2Volume   = 0;
static uint8_t  ch2EnvTimer = 0;
static bool     ch2EnvEn    = false;
static bool     ch2On       = false;

// CH3
static uint32_t ch3Timer    = 0;
static uint8_t  ch3WavePos  = 0;
static uint16_t ch3Length   = 0;
static bool     ch3On       = false;

// CH4
static uint32_t ch4Timer    = 0;
static uint16_t ch4Lfsr     = 0x7FFF;
static uint16_t ch4Length   = 0;
static uint8_t  ch4Volume   = 0;
static uint8_t  ch4EnvTimer = 0;
static bool     ch4EnvEn    = false;
static bool     ch4On       = false;

// Audio output buffer
static std::vector<float> audioBuf;

// ─── Frame sequencer events ───────────────────────────────────────────────────

static void clockLength() {
    // CH1
    if ((reg(0xFF14) >> 6) & 1) {
        if (ch1Length > 0 && --ch1Length == 0) ch1On = false;
    }
    // CH2
    if ((reg(0xFF19) >> 6) & 1) {
        if (ch2Length > 0 && --ch2Length == 0) ch2On = false;
    }
    // CH3
    if ((reg(0xFF1E) >> 6) & 1) {
        if (ch3Length > 0 && --ch3Length == 0) ch3On = false;
    }
    // CH4
    if ((reg(0xFF23) >> 6) & 1) {
        if (ch4Length > 0 && --ch4Length == 0) ch4On = false;
    }
}

static void clockSweep() {
    if (!ch1SweepEn) return;
    if (ch1SweepTimer > 0) --ch1SweepTimer;
    if (ch1SweepTimer == 0) {
        uint8_t nr10  = reg(0xFF10);
        uint8_t pace  = (nr10 >> 4) & 7;
        ch1SweepTimer = (pace > 0) ? pace : 8;

        uint8_t shift = nr10 & 7;
        if (pace > 0 && shift > 0) {
            int     neg     = ((nr10 >> 3) & 1) ? -1 : 1;
            int32_t newfreq = (int32_t)ch1SweepShadow + neg * (ch1SweepShadow >> shift);
            if (newfreq > 2047) {
                ch1On = ch1SweepEn = false;
            } else {
                ch1SweepShadow = (uint16_t)newfreq;
                writereg(0xFF13, ch1SweepShadow & 0xFF);
                writereg(0xFF14, (reg(0xFF14) & 0xF8) | ((ch1SweepShadow >> 8) & 7));
                // Second overflow check
                int32_t chk = (int32_t)ch1SweepShadow + neg * (ch1SweepShadow >> shift);
                if (chk > 2047) ch1On = ch1SweepEn = false;
            }
        }
    }
}

static void clockEnvelope() {
    // CH1
    if (ch1EnvEn && (reg(0xFF12) & 7)) {
        if (ch1EnvTimer > 0) --ch1EnvTimer;
        if (ch1EnvTimer == 0) {
            ch1EnvTimer = reg(0xFF12) & 7;
            int16_t nv = ch1Volume + (((reg(0xFF12) >> 3) & 1) ? 1 : -1);
            if (nv >= 0 && nv <= 15) ch1Volume = (uint8_t)nv;
            else ch1EnvEn = false;
        }
    }
    // CH2
    if (ch2EnvEn && (reg(0xFF17) & 7)) {
        if (ch2EnvTimer > 0) --ch2EnvTimer;
        if (ch2EnvTimer == 0) {
            ch2EnvTimer = reg(0xFF17) & 7;
            int16_t nv = ch2Volume + (((reg(0xFF17) >> 3) & 1) ? 1 : -1);
            if (nv >= 0 && nv <= 15) ch2Volume = (uint8_t)nv;
            else ch2EnvEn = false;
        }
    }
    // CH4
    if (ch4EnvEn && (reg(0xFF21) & 7)) {
        if (ch4EnvTimer > 0) --ch4EnvTimer;
        if (ch4EnvTimer == 0) {
            ch4EnvTimer = reg(0xFF21) & 7;
            int16_t nv = ch4Volume + (((reg(0xFF21) >> 3) & 1) ? 1 : -1);
            if (nv >= 0 && nv <= 15) ch4Volume = (uint8_t)nv;
            else ch4EnvEn = false;
        }
    }
}

// ─── Per-T-cycle channel ticks (timers only) ─────────────────────────────────

static inline void tickCH1() {
    if (ch1Timer > 0) { ch1Timer--; return; }
    uint16_t period = (uint16_t)(((reg(0xFF14) & 7) << 8) | reg(0xFF13));
    ch1Timer = (2048 - period) * 4;
    ch1DutyPos = (ch1DutyPos + 1) & 7;
}

static inline void tickCH2() {
    if (ch2Timer > 0) { ch2Timer--; return; }
    uint16_t period = (uint16_t)(((reg(0xFF19) & 7) << 8) | reg(0xFF18));
    ch2Timer = (2048 - period) * 4;
    ch2DutyPos = (ch2DutyPos + 1) & 7;
}

static inline void tickCH3() {
    if (ch3Timer > 0) { ch3Timer--; return; }
    uint16_t period = (uint16_t)(((reg(0xFF1E) & 7) << 8) | reg(0xFF1D));
    ch3Timer = (2048 - period) * 2;
    ch3WavePos = (ch3WavePos + 1) & 31;
}

static inline void tickCH4() {
    if (ch4Timer > 0) { ch4Timer--; return; }
    uint8_t nr22  = reg(0xFF22);
    uint8_t div   = nr22 & 7;
    uint8_t shift = nr22 >> 4;
    ch4Timer = (div == 0 ? 4 : SC4divisorTable[div]) << shift;

    uint8_t xr   = (ch4Lfsr & 1) ^ ((ch4Lfsr >> 1) & 1);
    ch4Lfsr >>= 1;
    ch4Lfsr |= (xr << 14);
    if ((nr22 >> 3) & 1) {
        ch4Lfsr &= ~(1 << 6);
        ch4Lfsr |= (xr << 6);
    }
}

// ─── Sample generation ────────────────────────────────────────────────────────

static inline float sampleCH1() {
    if (!ch1On || (reg(0xFF12) & 0xF8) == 0) return 0.0f;
    int duty = reg(0xFF11) >> 6;
    return duties[duty][ch1DutyPos] ? (ch1Volume / 15.0f) : 0.0f;
}

static inline float sampleCH2() {
    if (!ch2On || (reg(0xFF17) & 0xF8) == 0) return 0.0f;
    int duty = reg(0xFF16) >> 6;
    return duties[duty][ch2DutyPos] ? (ch2Volume / 15.0f) : 0.0f;
}

static inline float sampleCH3() {
    if (!ch3On || (reg(0xFF1A) & 0x80) == 0) return 0.0f;
    uint8_t sample = g_mmu->io[0x30 + (ch3WavePos >> 1)];
    sample = (ch3WavePos & 1) ? (sample & 0x0F) : (sample >> 4);
    uint8_t vol = (reg(0xFF1C) >> 5) & 3;
    if      (vol == 0) return 0.0f;
    else if (vol == 2) sample >>= 1;
    else if (vol == 3) sample >>= 2;
    return sample / 15.0f;
}

static inline float sampleCH4() {
    if (!ch4On || (reg(0xFF21) & 0xF8) == 0) return 0.0f;
    return (ch4Lfsr & 1) ? 0.0f : (ch4Volume / 15.0f);
}

// ─── Public step ─────────────────────────────────────────────────────────────

void stepSPU(unsigned char cycles) {
    if (!(reg(0xFF26) & 0x80)) return;  // APU master off

    const uint8_t nr25 = reg(0xFF25);

    for (int i = 0; i < cycles; i++) {
        // Frame sequencer: 8192 T-cycles per step (512 Hz)
        g_fsCycles++;
        if (g_fsCycles >= 8192) {
            g_fsCycles -= 8192;
            if (g_fsStep % 2 == 0) clockLength();
            if (g_fsStep == 2 || g_fsStep == 6) clockSweep();
            if (g_fsStep == 7) clockEnvelope();
            g_fsStep = (g_fsStep + 1) & 7;
        }

        tickCH1();
        tickCH2();
        tickCH3();
        tickCH4();

        // Downsample
        g_sampleAcc += 1.0;
        if (g_sampleAcc >= CYCLES_PER_SAMPLE) {
            g_sampleAcc -= CYCLES_PER_SAMPLE;

            float ch1 = useSC1 ? sampleCH1() : 0.0f;
            float ch2 = useSC2 ? sampleCH2() : 0.0f;
            float ch3 = useSC3 ? sampleCH3() : 0.0f;
            float ch4 = useSC4 ? sampleCH4() : 0.0f;

            // Panning: NR51 (FF25)
            // Right: bits 3,2,1,0 = CH4,CH3,CH2,CH1
            // Left:  bits 7,6,5,4 = CH4,CH3,CH2,CH1
            float left  = ((nr25 >> 4) & 1 ? ch1 : 0) + ((nr25 >> 5) & 1 ? ch2 : 0)
                        + ((nr25 >> 6) & 1 ? ch3 : 0) + ((nr25 >> 7) & 1 ? ch4 : 0);
            float right = ((nr25 >> 0) & 1 ? ch1 : 0) + ((nr25 >> 1) & 1 ? ch2 : 0)
                        + ((nr25 >> 2) & 1 ? ch3 : 0) + ((nr25 >> 3) & 1 ? ch4 : 0);

            // 4 channels per side × max 1.0 → normalise to ±1
            left  *= volume * 0.25f;
            right *= volume * 0.25f;

            audioBuf.push_back(left);
            audioBuf.push_back(right);
        }
    }

    // Queue when we have at least 512 stereo frames (~11ms)
    if (audioBuf.size() >= 1024) {
        static const uint32_t MAX_QUEUED = (uint32_t)(44100 * 2 * sizeof(float) * 0.2);
        if (SDL_GetQueuedAudioSize(1) < MAX_QUEUED)
            SDL_QueueAudio(1, audioBuf.data(), (uint32_t)(audioBuf.size() * sizeof(float)));
        audioBuf.clear();
    }
}

// ─── Init / stop ─────────────────────────────────────────────────────────────

void initSPU() {
    SDL_setenv("SDL_AUDIODRIVER", "directsound", 1);
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_AudioSpec spec = {};
    spec.freq     = 44100;
    spec.format   = AUDIO_F32SYS;
    spec.channels = 2;
    spec.samples  = 2048;
    SDL_OpenAudio(&spec, nullptr);
    if (!SoundIsPlaying) {
        SDL_PauseAudio(0);
        SoundIsPlaying = true;
    }
}

void stopSPU() {
    SDL_CloseAudio();
    audioBuf.clear();
    SoundIsPlaying = false;
    g_fsCycles = g_fsStep = 0;
    g_sampleAcc = 0.0;
    ch1Timer = ch1DutyPos = ch1Volume = ch1EnvTimer = 0;
    ch1Length = ch1SweepShadow = 0;
    ch1EnvEn = ch1On = ch1SweepEn = false; ch1SweepTimer = 0;
    ch2Timer = ch2DutyPos = ch2Volume = ch2EnvTimer = 0; ch2Length = 0;
    ch2EnvEn = ch2On = false;
    ch3Timer = ch3WavePos = 0; ch3Length = ch3On = false;
    ch4Timer = ch4Volume = ch4EnvTimer = 0; ch4Length = 0;
    ch4Lfsr = 0x7FFF; ch4EnvEn = ch4On = false;
}

// ─── Channel trigger functions (called from mmu.cpp on NRx4 bit 7) ───────────

void resetSC1length(uint8_t val) {
    if (ch1Length == 0) ch1Length = 64 - (val & 0x3F);
    ch1On       = true;
    ch1Volume   = reg(0xFF12) >> 4;
    ch1EnvTimer = reg(0xFF12) & 7;
    ch1EnvEn    = true;
    uint16_t period  = (uint16_t)(((reg(0xFF14) & 7) << 8) | reg(0xFF13));
    ch1Timer         = (2048 - period) * 4;
    ch1SweepShadow   = period;
    uint8_t nr10     = reg(0xFF10);
    uint8_t pace     = (nr10 >> 4) & 7;
    uint8_t shift    = nr10 & 7;
    ch1SweepTimer    = (pace > 0) ? pace : 8;
    ch1SweepEn       = (pace != 0 || shift != 0);
    if (shift != 0) {
        int neg = ((nr10 >> 3) & 1) ? -1 : 1;
        if ((int32_t)ch1SweepShadow + neg * (ch1SweepShadow >> shift) > 2047)
            ch1On = ch1SweepEn = false;
    }
    if ((reg(0xFF12) & 0xF8) == 0) ch1On = false;
}

void resetSC2length(uint8_t val) {
    if (ch2Length == 0) ch2Length = 64 - (val & 0x3F);
    ch2On       = true;
    ch2Volume   = reg(0xFF17) >> 4;
    ch2EnvTimer = reg(0xFF17) & 7;
    ch2EnvEn    = true;
    uint16_t period = (uint16_t)(((reg(0xFF19) & 7) << 8) | reg(0xFF18));
    ch2Timer        = (2048 - period) * 4;
    if ((reg(0xFF17) & 0xF8) == 0) ch2On = false;
}

void resetSC3length(uint8_t val) {
    if (ch3Length == 0) ch3Length = 256 - val;
    ch3On      = true;
    ch3WavePos = 0;
    uint16_t period = (uint16_t)(((reg(0xFF1E) & 7) << 8) | reg(0xFF1D));
    ch3Timer        = (2048 - period) * 2;
    if ((reg(0xFF1A) & 0x80) == 0) ch3On = false;
}

void resetSC4length(uint8_t val) {
    if (ch4Length == 0) ch4Length = 64 - (val & 0x3F);
    ch4On       = true;
    ch4Lfsr     = 0x7FFF;
    ch4Volume   = reg(0xFF21) >> 4;
    ch4EnvTimer = reg(0xFF21) & 7;
    ch4EnvEn    = true;
    uint8_t nr22 = reg(0xFF22);
    uint8_t div  = nr22 & 7;
    ch4Timer     = (div == 0 ? 4 : SC4divisorTable[div]) << (nr22 >> 4);
    if ((reg(0xFF21) & 0xF8) == 0) ch4On = false;
}
