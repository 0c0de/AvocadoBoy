#pragma once
#include <stdio.h>
#include <iostream>
struct CartHeader {
	char* title;
	char* manufacturerCode;
	uint8_t licenseCode;
	uint8_t cartridgeType;
	uint8_t romSize;
	uint8_t ramSize;
};

CartHeader readHeader(uint8_t headerBytes[]);
uint8_t getRomSize(uint8_t headerBytes[]);
uint8_t getRamSize(uint8_t headerBytes[]);
uint8_t getCartridgeType(uint8_t headerBytes[]);
char* getRomName(uint8_t headerBytes[]);
