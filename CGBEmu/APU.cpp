#include "APU.h"
#include "mmu.h"
#include <SDL.h>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <cmath>

// ─── MMU access ───────────────────────────────────────────────────────────────

static MMU* g_mmu = nullptr;
void SPU_SetMMU(MMU* mmu) { g_mmu = mmu; }

static inline uint8_t reg(uint16_t addr) {
    if (!g_mmu) return 0xFF;
    return g_mmu->io[addr - 0xFF00];
}
static inline void writereg(uint16_t addr, uint8_t val) {
    if (g_mmu) g_mmu->io[addr - 0xFF00] = val;
}

// ─── Toggle / volume ──────────────────────────────────────────────────────────

static bool  useSC1 = true, useSC2 = true, useSC3 = true, useSC4 = true;
static float masterVolume = 0.5f;
static bool  SoundIsPlaying = false;

void toggleAudio()       { useSC1 = useSC2 = useSC3 = useSC4 = !useSC1; }
void toggleSC1()         { useSC1 = !useSC1; }
void toggleSC2()         { useSC2 = !useSC2; }
void toggleSC3()         { useSC3 = !useSC3; }
void toggleSC4()         { useSC4 = !useSC4; }
void toggleRemix()       {}
void setVolume(float v)  { masterVolume = v; }

// ─── Ring buffer ──────────────────────────────────────────────────────────────
// Stereo floats. Size = power of 2. 4096 floats = 2048 stereo frames = ~46ms.
// The callback reads it; stepSPU writes it and blocks when full — this is the
// emulator's natural speed limiter (no SDL_Delay in the main loop needed).

static const int RING_SIZE = 16384;
static const int RING_MASK = RING_SIZE - 1;
static const int ONE_FRAME_SAMPLES = 1470;
static float     ringBuf[RING_SIZE];
static volatile int ringWrite = 0;
static volatile int ringRead  = 0;    // only written by callback

static inline int ringAvailable() { return (ringWrite - ringRead) & RING_MASK; }
static inline int ringFree()      { return RING_MASK - ringAvailable(); }

static inline void ringPush(float l, float r) {
    ringBuf[ ringWrite              & RING_MASK] = l;
    ringBuf[(ringWrite + 1)         & RING_MASK] = r;
    ringWrite = (ringWrite + 2) & RING_MASK;
}
static inline bool ringPop(float& l, float& r) {
    if (ringAvailable() < 2) { l = r = 0.0f; return false; }
    l = ringBuf[ ringRead              & RING_MASK];
    r = ringBuf[(ringRead + 1)         & RING_MASK];
    ringRead = (ringRead + 2) & RING_MASK;
    return true;
}

// ─── SDL audio callback ───────────────────────────────────────────────────────

static void audioCallback(void*, Uint8* stream, int len) {
    float* out   = reinterpret_cast<float*>(stream);
    int    count = len / (sizeof(float) * 2);
    for (int i = 0; i < count; i++) {
        float l, r;
        ringPop(l, r);
        out[i * 2    ] = l;
        out[i * 2 + 1] = r;
    }
}

// ─── Duty table ───────────────────────────────────────────────────────────────

static const uint8_t duties[4][8] = {
    {0,0,0,0,0,0,0,1},  // 12.5%
    {1,0,0,0,0,0,0,1},  // 25%
    {1,0,0,0,0,1,1,1},  // 50%
    {0,1,1,1,1,1,1,0}   // 75%
};

static const uint32_t SC4divisorTable[8] = { 8,16,32,48,64,80,96,112 };

// ─── APU state ────────────────────────────────────────────────────────────────

static uint32_t g_fsCycles = 0;
static uint8_t  g_fsStep   = 0;

static const double CYCLES_PER_SAMPLE = 4194304.0 / 44100.0;
static double       g_sampleAcc       = 0.0;

static const float HPF_CHARGE = 0.999958f;
static float hpfCapL = 0.0f, hpfCapR = 0.0f;

static uint32_t ch1Timer=0; static uint8_t ch1DutyPos=0,ch1Volume=0,ch1EnvTimer=0;
static uint16_t ch1Length=0,ch1SweepShadow=0;
static bool     ch1EnvEn=false,ch1On=false,ch1SweepEn=false; static uint8_t ch1SweepTimer=0;

static uint32_t ch2Timer=0; static uint8_t ch2DutyPos=0,ch2Volume=0,ch2EnvTimer=0;
static uint16_t ch2Length=0; static bool ch2EnvEn=false,ch2On=false;

static uint32_t ch3Timer=0; static uint8_t ch3WavePos=0;
static uint16_t ch3Length=0; static bool ch3On=false;

static uint32_t ch4Timer=0; static uint16_t ch4Lfsr=0x7FFF,ch4Length=0;
static uint8_t  ch4Volume=0,ch4EnvTimer=0; static bool ch4EnvEn=false,ch4On=false;

// ─── Frame sequencer ─────────────────────────────────────────────────────────

static void clockLength() {
    if ((reg(0xFF14)>>6)&1) { if (ch1Length>0 && --ch1Length==0) ch1On=false; }
    if ((reg(0xFF19)>>6)&1) { if (ch2Length>0 && --ch2Length==0) ch2On=false; }
    if ((reg(0xFF1E)>>6)&1) { if (ch3Length>0 && --ch3Length==0) ch3On=false; }
    if ((reg(0xFF23)>>6)&1) { if (ch4Length>0 && --ch4Length==0) ch4On=false; }
}

static void clockSweep() {
    if (!ch1SweepEn) return;
    if (ch1SweepTimer > 0) --ch1SweepTimer;
    if (ch1SweepTimer == 0) {
        uint8_t nr10 = reg(0xFF10);
        uint8_t pace = (nr10>>4)&7, shift = nr10&7;
        ch1SweepTimer = pace ? pace : 8;
        if (pace && shift) {
            int neg = ((nr10>>3)&1)?-1:1;
            int32_t nf = (int32_t)ch1SweepShadow + neg*(ch1SweepShadow>>shift);
            if (nf > 2047) { ch1On=ch1SweepEn=false; }
            else {
                ch1SweepShadow=(uint16_t)nf;
                writereg(0xFF13, ch1SweepShadow&0xFF);
                writereg(0xFF14, (reg(0xFF14)&0xF8)|((ch1SweepShadow>>8)&7));
                if ((int32_t)ch1SweepShadow+neg*(ch1SweepShadow>>shift)>2047)
                    ch1On=ch1SweepEn=false;
            }
        }
    }
}

static void clockEnvelope() {
    auto tick=[](bool&en,uint8_t&vol,uint8_t&timer,uint8_t nr2){
        if(!en||!(nr2&7))return;
        if(--timer==0){
            timer=(nr2&7)?(nr2&7):8;
            int16_t nv=vol+(((nr2>>3)&1)?1:-1);
            if(nv>=0&&nv<=15)vol=(uint8_t)nv; else en=false;
        }
    };
    tick(ch1EnvEn,ch1Volume,ch1EnvTimer,reg(0xFF12));
    tick(ch2EnvEn,ch2Volume,ch2EnvTimer,reg(0xFF17));
    tick(ch4EnvEn,ch4Volume,ch4EnvTimer,reg(0xFF21));
}

// ─── Channel timers ───────────────────────────────────────────────────────────

static inline void tickCH1(){
    if(ch1Timer>0){--ch1Timer;return;}
    uint16_t p=(uint16_t)(((reg(0xFF14)&7)<<8)|reg(0xFF13));
    ch1Timer=(2048-p)*4; ch1DutyPos=(ch1DutyPos+1)&7;
}
static inline void tickCH2(){
    if(ch2Timer>0){--ch2Timer;return;}
    uint16_t p=(uint16_t)(((reg(0xFF19)&7)<<8)|reg(0xFF18));
    ch2Timer=(2048-p)*4; ch2DutyPos=(ch2DutyPos+1)&7;
}
static inline void tickCH3(){
    if(ch3Timer>0){--ch3Timer;return;}
    uint16_t p=(uint16_t)(((reg(0xFF1E)&7)<<8)|reg(0xFF1D));
    ch3Timer=(2048-p)*2; ch3WavePos=(ch3WavePos+1)&31;
}
static inline void tickCH4(){
    if(ch4Timer>0){--ch4Timer;return;}
    uint8_t nr22=reg(0xFF22),div=nr22&7;
    ch4Timer=(div==0?4:SC4divisorTable[div])<<(nr22>>4);
    uint8_t xr=(ch4Lfsr&1)^((ch4Lfsr>>1)&1);
    ch4Lfsr>>=1; ch4Lfsr|=(xr<<14);
    if((nr22>>3)&1){ch4Lfsr&=~(1<<6);ch4Lfsr|=(xr<<6);}
}

// ─── Sample generation ────────────────────────────────────────────────────────

static inline float sampleCH1(){
    if(!ch1On||(reg(0xFF12)&0xF8)==0)return 0.f;
    return duties[reg(0xFF11)>>6][ch1DutyPos]?ch1Volume/15.f:0.f;
}
static inline float sampleCH2(){
    if(!ch2On||(reg(0xFF17)&0xF8)==0)return 0.f;
    return duties[reg(0xFF16)>>6][ch2DutyPos]?ch2Volume/15.f:0.f;
}
static inline float sampleCH3(){
    if(!ch3On||(reg(0xFF1A)&0x80)==0)return 0.f;
    uint8_t s=g_mmu->io[0x30+(ch3WavePos>>1)];
    s=(ch3WavePos&1)?(s&0x0F):(s>>4);
    uint8_t vol=(reg(0xFF1C)>>5)&3;
    if(vol==0)return 0.f;
    if(vol==2)s>>=1;
    if(vol==3)s>>=2;
    return s/15.f;
}
static inline float sampleCH4(){
    if(!ch4On||(reg(0xFF21)&0xF8)==0)return 0.f;
    return (ch4Lfsr&1)?0.f:ch4Volume/15.f;
}

// ─── Public step ─────────────────────────────────────────────────────────────

void stepSPU(unsigned char rawCycles) {
    if(!(reg(0xFF26)&0x80))return;

    const int cyc = (int)rawCycles;

    for(int i=0;i<cyc;i++){
        if(++g_fsCycles>=8192){
            g_fsCycles-=8192;
            if((g_fsStep&1)==0)           clockLength();
            if(g_fsStep==2||g_fsStep==6)  clockSweep();
            if(g_fsStep==7)               clockEnvelope();
            g_fsStep=(g_fsStep+1)&7;
        }

        tickCH1(); tickCH2(); tickCH3(); tickCH4();

        if((g_sampleAcc+=1.0)>=CYCLES_PER_SAMPLE){
            g_sampleAcc-=CYCLES_PER_SAMPLE;

            float c1=useSC1?sampleCH1():0.f;
            float c2=useSC2?sampleCH2():0.f;
            float c3=useSC3?sampleCH3():0.f;
            float c4=useSC4?sampleCH4():0.f;

            uint8_t nr25 = reg(0xFF25);
            float L=((nr25>>4)&1?c1:0.f)+((nr25>>5)&1?c2:0.f)+((nr25>>6)&1?c3:0.f)+((nr25>>7)&1?c4:0.f);
            float R=((nr25>>0)&1?c1:0.f)+((nr25>>1)&1?c2:0.f)+((nr25>>2)&1?c3:0.f)+((nr25>>3)&1?c4:0.f);

            // NR50: master volume (bits 2-0 right, bits 6-4 left), 0=1x .. 7=8x
            uint8_t nr50 = reg(0xFF24);
            float volL = ((nr50>>4)&7)+1;  // 1..8
            float volR = (nr50&7)+1;

            L*=(volL/8.f)*0.25f*masterVolume;
            R*=(volR/8.f)*0.25f*masterVolume;

            // High-pass filter (Pan Docs formula): out = in - cap; cap = out * charge
            float outL=L-hpfCapL; hpfCapL=outL*HPF_CHARGE;
            float outR=R-hpfCapR; hpfCapR=outR*HPF_CHARGE;

            while(ringFree() < 2) SDL_Delay(1);
            ringPush(outL,outR);
        }
    }
}

// ─── Init / stop ─────────────────────────────────────────────────────────────

void initSPU(){
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    memset(ringBuf,0,sizeof(ringBuf));
    ringRead=ringWrite=0;
    hpfCapL=hpfCapR=0.f;

    SDL_AudioSpec want={},got={};
    want.freq=44100; want.format=AUDIO_F32SYS; want.channels=2;
    want.samples=1024; want.callback=audioCallback;
    if(SDL_OpenAudio(&want,&got)<0){
        std::cout<<"[APU] SDL_OpenAudio: "<<SDL_GetError()<<std::endl;
        return;
    }
    SDL_PauseAudio(0);
    SoundIsPlaying=true;
}

void stopSPU(){
    SDL_PauseAudio(1);
    SDL_CloseAudio();
    SoundIsPlaying=false;
    memset(ringBuf,0,sizeof(ringBuf));
    ringRead=ringWrite=0;
    g_fsCycles=g_fsStep=0; g_sampleAcc=0.0; hpfCapL=hpfCapR=0.f;
    ch1Timer=ch1DutyPos=ch1Volume=ch1EnvTimer=0;ch1Length=ch1SweepShadow=0;
    ch1EnvEn=ch1On=ch1SweepEn=false;ch1SweepTimer=0;
    ch2Timer=ch2DutyPos=ch2Volume=ch2EnvTimer=0;ch2Length=0;ch2EnvEn=ch2On=false;
    ch3Timer=ch3WavePos=0;ch3Length=0;ch3On=false;
    ch4Timer=ch4Volume=ch4EnvTimer=0;ch4Length=0;
    ch4Lfsr=0x7FFF;ch4EnvEn=ch4On=false;
}

// ─── Trigger functions ────────────────────────────────────────────────────────

void resetSC1length(uint8_t val){
    if(ch1Length==0)ch1Length=64;
    ch1On=true; ch1Volume=reg(0xFF12)>>4;
    ch1EnvTimer=(reg(0xFF12)&7)?(reg(0xFF12)&7):8; ch1EnvEn=true;
    uint16_t p=(uint16_t)(((reg(0xFF14)&7)<<8)|reg(0xFF13));
    ch1Timer=(2048-p)*4; ch1SweepShadow=p;
    uint8_t nr10=reg(0xFF10),pace=(nr10>>4)&7,shift=nr10&7;
    ch1SweepTimer=pace?pace:8; ch1SweepEn=(pace||shift);
    if(shift){int neg=((nr10>>3)&1)?-1:1;if((int32_t)ch1SweepShadow+neg*(ch1SweepShadow>>shift)>2047)ch1On=ch1SweepEn=false;}
    if((reg(0xFF12)&0xF8)==0)ch1On=false;
}
void resetSC2length(uint8_t val){
    if(ch2Length==0)ch2Length=64;
    ch2On=true; ch2Volume=reg(0xFF17)>>4;
    ch2EnvTimer=(reg(0xFF17)&7)?(reg(0xFF17)&7):8; ch2EnvEn=true;
    uint16_t p=(uint16_t)(((reg(0xFF19)&7)<<8)|reg(0xFF18));
    ch2Timer=(2048-p)*4;
    if((reg(0xFF17)&0xF8)==0)ch2On=false;
}
void resetSC3length(uint8_t val){
    if(ch3Length==0)ch3Length=256;
    ch3On=true; ch3WavePos=0;
    uint16_t p=(uint16_t)(((reg(0xFF1E)&7)<<8)|reg(0xFF1D));
    ch3Timer=(2048-p)*2;
    if((reg(0xFF1A)&0x80)==0)ch3On=false;
}
void resetSC4length(uint8_t val){
    if(ch4Length==0)ch4Length=64;
    ch4On=true; ch4Lfsr=0x7FFF; ch4Volume=reg(0xFF21)>>4;
    ch4EnvTimer=(reg(0xFF21)&7)?(reg(0xFF21)&7):8; ch4EnvEn=true;
    uint8_t nr22=reg(0xFF22),div=nr22&7;
    ch4Timer=(div==0?4:SC4divisorTable[div])<<(nr22>>4);
    if((reg(0xFF21)&0xF8)==0)ch4On=false;
}
