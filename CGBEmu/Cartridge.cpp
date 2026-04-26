#include "Cartridge.h"

CartHeader readHeader(uint8_t headerBytes[]) {
	CartHeader header;
	header.title = getRomName(headerBytes);
	header.cartridgeType = getCartridgeType(headerBytes);
	header.ramSize = getRamSize(headerBytes);
	header.romSize = getRomSize(headerBytes);

	std::cout << "Final rom name: " << header.title << std::endl;
	std::cout << "Cartridge type: 0x" << static_cast<unsigned>(header.cartridgeType) << std::hex << std::endl;
	std::cout << "Rom size: 0x" << static_cast<unsigned>(header.romSize) << std::hex << std::endl;
	std::cout << "Ram size: 0x" << static_cast<unsigned>(header.ramSize) << std::hex << std::endl;

	return header;
}

uint8_t getRomSize(uint8_t headerBytes[]) {
	return headerBytes[0x48];
}

uint8_t getRamSize(uint8_t headerBytes[]) {
	return headerBytes[0x49];
}

uint8_t getCartridgeType(uint8_t headerBytes[]) {
	return headerBytes[0x47];
}

char* getRomName(uint8_t headerBytes[]) {
	char* romName = new char[0x10];
	for (int x = 0; x < 0x10; x++) {
		romName[x] = (char)headerBytes[0x34 + x];
	}
	return romName;
}