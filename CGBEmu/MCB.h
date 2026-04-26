#pragma once
#include <stdio.h>
#include <iostream>

class MCB1 {
	uint8_t read(uint16_t address);
	void write(uint16_t address, uint8_t valueToWrite);
	bool ramOn;
	int bankSelected = 0;
	int ramSelected = 0;
	bool hasBattery = false;
};

class MCB3 {
	uint8_t read(uint16_t address);
	void write(uint16_t address, uint8_t valueToWrite);
};