#include "Timers.h"

bool Timer::isKthBitSet(int n, int k){
	return n & (1 << k);
}

int Timer::clockSelect(uint8_t n) {
	// TAC bits 1-0 select TIMA increment period in T-cycles (per Pan Docs)
	switch (n & 0x03) {
	case 0: return 1024; // 4096 Hz
	case 1: return 16;   // 262144 Hz
	case 2: return 64;   // 65536 Hz
	case 3: return 256;  // 16384 Hz
	default: return 1024;
	}
}

void Timer::updateTimer(MMU* mmu, Interrupt* interrupt, int cycles, bool isStopped) {
    // DIV always counts, regardless of TAC enable
    divCounter += cycles;
    while (divCounter >= 256) {
        divCounter -= 256;
        mmu->io[0x04]++;
    }

    uint8_t tac = mmu->read8(0xFF07);
    bool timerEnabled = (tac & 0x04) != 0;

    if (!timerEnabled) {
        timaCounter = 0;
        return;
    }

    int clockThreshold = clockSelect(tac);
    timaCounter += cycles;

    while (timaCounter >= clockThreshold) {
        timaCounter -= clockThreshold;

        uint8_t tima = mmu->read8(0xFF05);

        if (tima == 0xFF) {
            // Overflow: reload from TMA and request interrupt
            mmu->io[0x05] = mmu->read8(0xFF06);
            interrupt->requestInterrupt(mmu, 2);
        }
        else {
            mmu->io[0x05] = tima + 1;
        }
    }
}