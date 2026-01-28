#pragma once
#include "Interrupts.h"


	uint8_t joyPadState;
	uint8_t directionsButton;
	uint8_t actionButton;
	bool isBitSet(int n, int k);
	void setButton(uint8_t key, MMU* mmu, Interrupt* interr);
	void releaseKey(uint8_t key, MMU* mmu, Interrupt* interr);
