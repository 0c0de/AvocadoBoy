#pragma once
#include <stdio.h>
#include <cstdio>
#include <iostream>
#include <vector>

//16 Bit registers
		//AF
		//BC
		//DE
		//HL
//8 Bit registers
		//A -> 0
		//F -> 1
		//B -> 2
		//C -> 3
		//D -> 4
		//E -> 5
		//H -> 6
		//L -> 7
struct GameboyRegisters
{
	uint8_t A;
	uint8_t F;
	uint8_t B;
	uint8_t C;
	uint8_t D;
	uint8_t E;
	uint8_t H;
	uint8_t L;
	uint16_t AF;
	uint16_t BC;
	uint16_t DE;
	uint16_t HL;

};

struct GameboyFlags {
	//Carry Flag
	bool C;
	//Zero Flag
	bool Z;
	//Substraction Flag
	bool N;
	//Half Carry flag
	bool H;
};

// MBC type constants (cartridge header byte 0x147)
enum MBCType : uint8_t {
	MBC_ROM_ONLY      = 0x00,
	MBC_MBC1          = 0x01,
	MBC_MBC1_RAM      = 0x02,
	MBC_MBC1_RAM_BAT  = 0x03,
	MBC_MBC2          = 0x05,
	MBC_MBC2_BAT      = 0x06,
	MBC_MBC3_TIMER_BAT       = 0x0F,
	MBC_MBC3_TIMER_RAM_BAT   = 0x10,
	MBC_MBC3          = 0x11,
	MBC_MBC3_RAM      = 0x12,
	MBC_MBC3_RAM_BAT  = 0x13,
	MBC_MBC5          = 0x19,
	MBC_MBC5_RAM      = 0x1A,
	MBC_MBC5_RAM_BAT  = 0x1B,
	MBC_MBC5_RUMBLE         = 0x1C,
	MBC_MBC5_RUMBLE_RAM     = 0x1D,
	MBC_MBC5_RUMBLE_RAM_BAT = 0x1E,
};

class MMU {
	public:
		//Memory Map (fixed regions)
		uint8_t bios[0xFF];
		uint16_t vram[0x2000];
		uint16_t wram[0x2000];
		uint16_t echo_ram[0x1E00];
		uint8_t sprite_attrib[0xA0];
		uint8_t io[0xFF];
		uint8_t internal_ram[0x7F];
		uint8_t IE = 0x00;

		uint16_t stack[0xFFFF];

		// Dynamic ROM/RAM (allocated on loadROM)
		std::vector<uint8_t> romData;
		std::vector<uint8_t> ramData;

		// --- MBC state ---
		uint8_t  typeMBC       = MBC_ROM_ONLY;
		bool     ramEnabled    = false;

		// MBC1 / MBC3 shared
		uint8_t  romBank       = 1;   // selected ROM bank (1-based default)
		uint8_t  ramBank       = 0;   // selected RAM bank
		bool     mbc1Mode      = false; // false=ROM banking, true=RAM banking

		// MBC5 extra: 9-bit ROM bank (bit 8 stored separately)
		uint8_t  romBankHi     = 0;   // bit 8 of MBC5 bank number

		// MBC3 RTC
		uint8_t  rtcS = 0, rtcM = 0, rtcH = 0;
		uint8_t  rtcDL = 0, rtcDH = 0;
		bool     rtcLatchReady = false; // latch sequence: written 0x00 first
		uint8_t  rtcReg        = 0;    // currently mapped RTC register (0x08-0x0C) or 0xFF = none

		int      cyclesToAdd   = 0;

		//Stack Pointer
		uint16_t sp;

		//Is in bios
		bool isInBios = false;

		//Read 16 Bit address
		uint16_t read(uint16_t addr);

		//Read 8 Bit address
		uint8_t read8(uint16_t addr);

		//Write 16 Bits value
		void write(uint16_t addr, uint16_t value);

		//Write 8 Bit Value
		void write8(uint16_t addr, uint8_t value);

		//Push a value to the RAM STACK
		void push(uint16_t value);

		//Pops out a value from the RAM STACK
		void pop(uint16_t *value);

		//Function for setting 16bit registers
		void setRegisters16Bit(GameboyRegisters *reg, const char *regName, uint16_t valueToSet, GameboyFlags *flags = NULL);

		//Function for setting 8bit registers
		void setRegisters8Bit(GameboyRegisters *reg, const char *regName, uint8_t valueToSet, GameboyFlags *flags = NULL);

		uint8_t resetBit(uint8_t n, uint8_t a);

		uint8_t setBit(uint8_t n, uint8_t a);

		uint8_t directionsButton = 0x0F;
		uint8_t actionButton     = 0x0F;

		// Load a full ROM image into romData and configure MBC state
		void loadROM(const std::vector<uint8_t>& data);

	private:
		bool isInBIOS = false;

		void DMATransfer(uint8_t data);

		// MBC register handlers (called from write8 for 0x0000-0x7FFF writes)
		void handleMBCWrite(uint16_t addr, uint8_t value);

		// ROM/RAM address resolvers
		uint8_t readROM(uint16_t addr);
		uint8_t readRAM(uint16_t addr);
		void    writeRAM(uint16_t addr, uint8_t value);
};
