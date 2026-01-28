#pragma once
#include "mmu.h"
#include "Interrupts.h"

class Timer {
public:
	int divCounter;
	int timaCounter;
	uint8_t div = 0xFF04;
	uint8_t tima = 0xFF05;
	uint8_t tma = 0xFF06;
	uint8_t TAC = 0xFF07;
	void updateTimer(MMU* mmu, Interrupt* interrupt, int cicles, bool isStoped);
private:
	bool isKthBitSet(int n, int k);
	int clockSelect(uint8_t n);
};