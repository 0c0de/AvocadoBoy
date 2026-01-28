#include "Joypad.h"

uint8_t setBit(uint8_t n, uint8_t a) {
	return n |= 1 << a;
}

uint8_t resetBit(uint8_t n, uint8_t a) {
	return n &= ~(1 << a);
}

bool isBitSet(int n, int k)
{
	return (n & (1 << k));
}

void setButton(uint8_t key, MMU *mmu, Interrupt *interr) {

	//A, B, Start, Select
	if (key <= 3) {
		//A
		if (key == 0) {
			actionButton = resetBit(actionButton, 0);
			interr->requestInterrupt(mmu, 4);
		}

		//B
		if (key == 1) {
			actionButton = resetBit(actionButton, 1);
			interr->requestInterrupt(mmu, 4);
		}

		//Select
		if (key == 2) {
			actionButton = resetBit(actionButton, 2);
			interr->requestInterrupt(mmu, 4);
		}

		//Start
		if (key == 3) {
			actionButton = resetBit(actionButton, 3);
			interr->requestInterrupt(mmu, 4);
		}
	}
	else {

		//Right
		if (key == 4) {
			directionsButton = resetBit(directionsButton, 0);
			interr->requestInterrupt(mmu, 4);
		}

		//Left
		if (key == 5) {
			directionsButton = resetBit(directionsButton, 1);
			interr->requestInterrupt(mmu, 4);
		}

		//Up
		if (key == 6) {
			directionsButton = resetBit(directionsButton, 2);
			interr->requestInterrupt(mmu, 4);
		}

		//Down
		if (key == 7) {
			directionsButton = resetBit(directionsButton, 3);
			interr->requestInterrupt(mmu, 4);
		}
	}
}

void releaseKey(uint8_t key, MMU *mmu, Interrupt *interr) {

	//A, B, Start, Select
	if (key <= 3) {

		//A
		if (key == 0) {
			actionButton = setBit(actionButton, 0);
		}

		//B
		if (key == 1) {
			actionButton = setBit(actionButton, 1);
		}

		//Select
		if (key == 2) {
			actionButton = setBit(actionButton, 2);
		}

		//Start
		if (key == 3) {
			actionButton = setBit(actionButton, 3);
		}
	}
	else {

		//Right
		if (key == 4) {
			directionsButton = setBit(directionsButton, 0);
		}

		//Left
		if (key == 5) {
			directionsButton = setBit(directionsButton, 1);
		}

		//Up
		if (key == 6) {
			directionsButton = setBit(directionsButton, 2);
		}

		//Down
		if (key == 7) {
			directionsButton = setBit(directionsButton, 3);
		}
	}
}