#include "Timers.h"

bool Timer::isKthBitSet(int n, int k){
	return n & (1 << k);
}

int Timer::clockSelect(uint8_t n) {
	if (!isKthBitSet(n, 0) && !isKthBitSet(n, 1)) {
		return 1024;
	} else if (!isKthBitSet(n, 0) && isKthBitSet(n, 1)) {
		return 16;
	} else if (isKthBitSet(n, 0) && !isKthBitSet(n, 1)) {
		return 64;
	} else if (isKthBitSet(n, 0) && isKthBitSet(n, 1)) {
		return 256;
	}
	else {
		return 1024;
	}
}

void Timer::updateTimer(MMU* mmu, Interrupt* interrupt, int cycles, bool isStopped) {
    // Actualizar DIV (siempre se actualiza)
    divCounter += cycles;
    while (divCounter >= 256) {
        divCounter -= 256;
        mmu->io[0x04]++;
    }

    // Leer TAC
    uint8_t tac = mmu->read8(0xFF07);
    bool timerEnabled = (tac & 0x04) != 0;

    if (!timerEnabled) {
        return;  // No resetear timaCounter aquí
    }

    // Timer habilitado
    int clockThreshold = clockSelect(tac);
    timaCounter += cycles;

    while (timaCounter >= clockThreshold) {
        timaCounter -= clockThreshold;

        uint8_t tima = mmu->read8(0xFF05);

        if (tima == 0xFF) {
            // Overflow
            mmu->io[0x05] = mmu->read8(0xFF06);  // Cargar TMA
            interrupt->requestInterrupt(mmu, 2);
        }
        else {
            mmu->io[0x05] = tima + 1;
        }
    }
}