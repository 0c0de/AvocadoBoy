#include "CPU.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <iomanip>   // Para std::setw, std::setfill, std::hex

MMU mmu;
Interrupt interrupt;
GameboyRegisters reg;
GameboyFlags flags;


uint8_t registerF = 0x00;

int MAXCYCLES = 69905;
float FPS = 59.73f;
float DELAY_TIME = 1000.0f / FPS;

using namespace std;

void CPU::writeFlagsToF() {
	uint8_t n = (flags.Z ? 0x80 : 0) |
		(flags.N ? 0x40 : 0) |
		(flags.H ? 0x20 : 0) |
		(flags.C ? 0x10 : 0);

	mmu.setRegisters8Bit(&reg, "F", n);
}

void CPU::init() {
	pc = 0x100;
	//pc = 0x0; // Descomentar si no usas BIOS real
	mmu.setRegisters16Bit(&reg, "AF", 0x01B0, &flags);
	mmu.setRegisters16Bit(&reg, "BC", 0x0013);
	mmu.setRegisters16Bit(&reg, "DE", 0x00D8);
	mmu.setRegisters16Bit(&reg, "HL", 0x014D);

	mmu.sp = 0xFFFE;

	// Inicialización de registros de I/O (Audio, Video, etc)
	mmu.write8(0xFF00, 0xCF);
	mmu.write8(0xFF05, 0x00);
	mmu.write8(0xFF06, 0x00);
	mmu.write8(0xFF07, 0x00);
	mmu.write8(0xFF10, 0x80);
	mmu.write8(0xFF11, 0xBF);
	mmu.write8(0xFF12, 0xF3);
	mmu.write8(0xFF14, 0xBF);
	mmu.write8(0xFF16, 0x3F);
	mmu.write8(0xFF17, 0x00);
	mmu.write8(0xFF19, 0xBF);
	mmu.write8(0xFF1A, 0x7F);
	mmu.write8(0xFF1B, 0xFF);
	mmu.write8(0xFF1C, 0x9F);
	mmu.write8(0xFF1E, 0xBF);
	mmu.write8(0xFF20, 0xFF);
	mmu.write8(0xFF21, 0x00);
	mmu.write8(0xFF22, 0x00);
	mmu.write8(0xFF23, 0xBF);
	mmu.write8(0xFF24, 0x77);
	mmu.write8(0xFF25, 0xF3);
	mmu.write8(0xFF26, 0xF1);
	mmu.write8(0xFF40, 0x91);
	mmu.write8(0xFF42, 0x00);
	mmu.write8(0xFF43, 0x00);
	mmu.write8(0xFF45, 0x00);
	mmu.write8(0xFF47, 0xFC);
	mmu.write8(0xFF48, 0xFF);
	mmu.write8(0xFF49, 0xFF);
	mmu.write8(0xFF4A, 0x00);
	mmu.write8(0xFF4B, 0x00);
	mmu.write8(0xFFFF, 0x00);
	IME = false;

	// Inicializar estado
	isHalted = false;
	cicles = 0;
}

uint8_t swapNibbles(uint8_t x)
{
	return ((x & 0x0F) << 4 | (x & 0xF0) >> 4);
}

void CPU::addCycles(int ciclesToAdd) {
	cicles += ciclesToAdd;
	//std::cout << static_cast<unsigned>(cicles) << std::endl;
}

void CPU::clearCycles() {
	cicles = 0;
}

/* int n is value and int k is the bit to search */
bool isKthBitSet(int n, int k)
{
	if (n & (1 << k)) {
		return true;
	}
	else {
		return false;
	}
}

MMU *CPU::getMMUValues() {
	return &mmu;
}

Interrupt* CPU::getInterrupt() {
	return &interrupt;
}

uint8_t CPU::setBit(uint8_t n, uint8_t a) {
	return n | 1 << a;
}

uint8_t CPU::resetBit(uint8_t n, uint8_t a) {
	return n & ~(1 << a);
}


void CPU::setKey(uint8_t key) {

	//A, B, Start, Select
		//A
		if (key == 0) {
			mmu.actionButton = resetBit(mmu.actionButton, 0);
			isStoped = false;
			interrupt.requestInterrupt(&mmu, 4);
		}

		//B
		if (key == 1) {
			mmu.actionButton = resetBit(mmu.actionButton, 1);
			isStoped = false;
			interrupt.requestInterrupt(&mmu, 4);
		}

		//Select
		if (key == 2) {
			mmu.actionButton = resetBit(mmu.actionButton, 2);
			isStoped = false;
			interrupt.requestInterrupt(&mmu, 4);
		}

		//Start
		if (key == 3) {
			mmu.actionButton = resetBit(mmu.actionButton, 3);
			isStoped = false;
			interrupt.requestInterrupt(&mmu, 4);
		}

		//Right
		if (key == 4) {
			mmu.directionsButton = resetBit(mmu.directionsButton, 0);
			std::cout << "Apretando derecha: 0x" << static_cast<unsigned>(mmu.directionsButton) << std::hex << std::endl;
			isStoped = false;
			interrupt.requestInterrupt(&mmu, 4);
		}

		//Left
		if (key == 5) {
			std::cout << "Detectada IZQUIERDA. directionsButton antes: " << (int)mmu.directionsButton;
			mmu.directionsButton = resetBit(mmu.directionsButton, 1);
			std::cout << " despues: " << (int)mmu.directionsButton << std::endl;
			isStoped = false;
			interrupt.requestInterrupt(&mmu, 4);
		}

		//Up
		if (key == 6) {
			mmu.directionsButton = resetBit(mmu.directionsButton, 2);
			isStoped = false;
			interrupt.requestInterrupt(&mmu, 4);
		}

		//Down
		if (key == 7) {
			mmu.directionsButton = resetBit(mmu.directionsButton, 3);
			isStoped = false;
			interrupt.requestInterrupt(&mmu, 4);
		}
}

void CPU::releaseKey(uint8_t key) {

	//A, B, Start, Select
	std::cout << "Printing input: " << static_cast<unsigned>(key) << std::endl;
		//A
		if (key == 0) {
			mmu.actionButton = setBit(mmu.actionButton, 0);
		}

		//B
		if (key == 1) {
			mmu.actionButton = setBit(mmu.actionButton, 1);
		}

		//Select
		if (key == 2) {
			mmu.actionButton = setBit(mmu.actionButton, 2);
		}

		//Start
		if (key == 3) {
			mmu.actionButton = setBit(mmu.actionButton, 3);
		}
		//Right
		if (key == 4) {
			mmu.directionsButton = setBit(mmu.directionsButton, 0);
		}

		//Left
		if (key == 5) {
			mmu.directionsButton = setBit(mmu.directionsButton, 1);
		}

		//Up
		if (key == 6) {
			mmu.directionsButton = setBit(mmu.directionsButton, 2);
		}

		//Down
		if (key == 7) {
			mmu.directionsButton = setBit(mmu.directionsButton, 3);
		}
}

// ---------------------------------------------------------
// FUNCIÓN PRINCIPAL REFACTORIZADA
// Ejecuta UNA instrucción y devuelve los ciclos consumidos
// ---------------------------------------------------------
int CPU::step() {
	uint8_t cb_opcode;
	uint8_t opcode;

	/*if (pc == 0x2fa) {
		std::cout << "Breakpoint reached: " << std::hex << static_cast<uint16_t>(pc) << std::endl;
		getchar();
	}*/

	/*if (pc == 0xc073) {
		std::cout << "Breakpoint reached: " << std::hex << static_cast<uint16_t>(pc) << std::endl;
		getchar();
	}*/

	// Revisar interrupciones antes de ejecutar (opcional, depende de la precisión deseada)
	if (interrupt.checkForInterrupts(&mmu, &isHalted, &IME, &pc)) {
		addCycles(20);
		return cicles;
	}

	if (isStoped) {
		addCycles(4);
		return cicles;
	}

	// 2. Manejo de interrupciones y HALT
	if (isHalted) {
		// 1. Simular el paso del tiempo (4 ciclos de reloj para un NOP)
		addCycles(4);

		// 2. LEER REGISTROS DE INTERRUPCIÓN
		// IE (Interrupt Enable) = 0xFFFF
		// IF (Interrupt Flag)   = 0xFF0F
		uint8_t ie = mmu.read8(0xFFFF);
		uint8_t if_reg = mmu.read8(0xFF0F);

		// 3. COMPROBAR SI HAY QUE DESPERTAR
		// Si hay alguna interrupción pendiente (IF) que esté habilitada (IE)
		// (El 0x1F es porque solo se usan los 5 primeros bits)
		if ((ie & if_reg & 0x1F) != 0) {
			isHalted = false; // ¡DESPERTAR!
		}

		// Retornamos los ciclos consumidos esperando.
		// En la siguiente llamada a step(), como isHalted es false, 
		// la CPU procesará la interrupción normalmente.
		return cicles;
	}


	// 1. Reiniciamos el contador de ciclos "delta" para esta instrucción
	clearCycles();


	flags.Z = reg.F & 0x80;
	flags.N = reg.F & 0x40;
	flags.H = reg.F & 0x20;
	flags.C = reg.F & 0x10;



	// 3. Lectura de Opcode (Fetch)
	opcode = mmu.read8(pc);
	//std::cout << "PC: 0x" << static_cast<unsigned>(pc) << std::hex << ", Opcode: 0x" << static_cast<unsigned>(opcode) << std::hex << std::endl;

	/*if (pc == 0xC7EB) {
		std::cout << "Breakpoint reached" << std::endl;
		printf("A: %02x", reg.A);
		printf("FF44: %02x", mmu.read8(0xFF44));
		getchar();
	}*/


	// 5. Decodificación y Ejecución
	switch (opcode) {
	case 0x00: NOP(); break;
	case 0x10: STOP(); break;
	case 0x1F: RRA(); break;
	case 0x07: RLCA(); break;
	case 0x27: DAA(); break;


		// Loads
	case 0x7F: case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E:
	case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x48:
	case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x50: case 0x51:
	case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x58: case 0x59: case 0x36:
	case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x60: case 0x61: case 0x62:
	case 0x63: case 0x64: case 0x65: case 0x66: case 0x68: case 0x69: case 0x6A: case 0x6B:
	case 0x6C: case 0x6D: case 0x6E: case 0x70: case 0x71: case 0x72: case 0x73: case 0x74:
	case 0x75: LD_r1_r2(opcode); break;

	case 0x06: case 0x0E: case 0x16: case 0x1E: case 0x26: case 0x2E: LD_NN_N(opcode); break;
	case 0x0A: case 0x1A: case 0x3E: case 0xFA: LD_A_N(opcode); break;
	case 0x47: case 0x4F: case 0x57: case 0x5F: case 0x67: case 0x6F: case 0x02: case 0x12:
	case 0x77: case 0xEA: LD_N_A(opcode); break;
	case 0x01: case 0x11: case 0x21: case 0x31: LD_N_NN(opcode); break;
	case 0xF0: LDH_A_N(); break;
	case 0xE0: LDH_N_A(); break;
	case 0xE2: LD_regC_A(); break;
	case 0x3A: LDD_A_regHL(); break;
	case 0x32: LDD_regHL_A(); break;
	case 0x2A: LDI_A_regHL(); break;
	case 0x22: LDI_regHL_A(); break;
	case 0xF9: LD_SP_HL(); break;
	case 0xF8: LDHL_SP_N(opcode); break;
	case 0x08: LD_NN_SP(); break;

		// Control Flow
	case 0xC3: JP_NN(opcode); break;
	case 0xF3: DI(); break;
	case 0xFB: EI(); break;
	case 0xCD: CALL_NN(opcode); break;
	case 0xC4: case 0xCC: case 0xD4: case 0xDC: CALL_CC_NN(opcode); break;
	case 0x20: case 0x28: case 0x30: case 0x38: JR_CC_N(opcode); break;
	case 0xCA: case 0xC2: case 0xDA: case 0xD2: JP_CC_NN(opcode); break;
	case 0xE9: JP_regHL(); break;
	case 0x18: JR_N(); break;
	case 0xC9: RET(); break;
	case 0xD9: RETI(); break;
	case 0xC0: case 0xC8: case 0xD0: case 0xD8: RET_CC(opcode); break;
	case 0xC7: case 0xCF: case 0xD7: case 0xDF: case 0xE7: case 0xEF: case 0xF7: case 0xFF: RST_N(opcode); break;
	case 0x76: HALT(); break;

		// Arithmetic / Logic
	case 0xFE: case 0xBF: case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: CP_N(opcode); break;
	case 0x3C: case 0x34: case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24: case 0x2C: INC_N(opcode); break;
	case 0x3D: case 0x35: case 0x05: case 0x0D: case 0x15: case 0x1D: case 0x25: case 0x2D: DEC_N(opcode); break;
	case 0xAF: case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xEE: XOR_N(opcode); break;
	case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7: case 0xF6: OR_N(opcode); break;
	case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xE6: AND_N(opcode); break;
	case 0x0B: case 0x1B: case 0x2B: case 0x3B: DEC_NN(opcode); break;
	case 0x2F: CPL(); break;
	case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87: case 0xC6: ADD_A_N(opcode); break;
	case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F: case 0xCE: ADC_A_N(opcode); break;
	case 0x09: case 0x19: case 0x29: case 0x39: ADD_HL_N(opcode); break;
	case 0xE8: ADD_SP_N(); break;
	case 0x03: case 0x13: case 0x23: case 0x33: INC_NN(opcode); break;
	case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: case 0xD6: SUB_N(opcode); break;

		// Stack
	case 0xF1: case 0xD1: case 0xC1: case 0xE1: POP_NN(opcode); break;
	case 0xF5: case 0xD5: case 0xC5: case 0xE5: PUSH_NN(opcode); break;

		// CB Prefix
	case 0xCB:
		cb_opcode = mmu.read8(pc + 1);
		addCycles(4);
		// IMPORTANTE: CB opcode consume ciclos extra y avanza PC, asegurate que tus funciones internas
		// de CB manejen el incremento de PC o hazlo aquí si es necesario.
		// Asumo que tus funciones CB_SWAP_N, etc. ya manejan addCycles.

		switch (cb_opcode) {
		case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
			RLC_N(cb_opcode);//DONE
			break;
		case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: case 0x0E: case 0x0F:
			RRC_N(cb_opcode); //DONE
			break;
		case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
			RL_N(cb_opcode); // DONE
			break;
		case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
			RR_N(cb_opcode); // DONE
			break;
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
			SLA_N(cb_opcode); break; //DONE
		case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2E: case 0x2F:
			SRA_N(cb_opcode); // DONE
			break;
		case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
			CB_SWAP_N(cb_opcode); break; //DONE
		case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
			SRL_N(cb_opcode); //DONE
			break;
		case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
		case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
		case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
		case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
		case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E: case 0x6F:
		case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
		case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
			BIT_B_R(cb_opcode);
			break;
		case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87: case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F:
		case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97: case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E: case 0x9F:
		case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA6: case 0xA7: case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF:
		case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7: case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
			RES_B_R(cb_opcode); break;
		case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: case 0xC6: case 0xC7: case 0xC8: case 0xC9: case 0xCA: case 0xCB: case 0xCC: case 0xCD: case 0xCE: case 0xCF:
		case 0xD0: case 0xD1: case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD7: case 0xD8: case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF:
		case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: case 0xE6: case 0xE7: case 0xE8: case 0xE9: case 0xEA: case 0xEB: case 0xEC: case 0xED: case 0xEE: case 0xEF:
		case 0xF0: case 0xF1: case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF7: case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFD: case 0xFE: case 0xFF:
			SET_B_R(cb_opcode); break;
		default:
			cout << "Opcode CB " << hex << static_cast<unsigned>(cb_opcode) << " not implemented, PC: " << hex << static_cast<unsigned>(pc) << endl;
			break;
		}
		break;

	default:
		cout << "Opcode: " << hex << static_cast<unsigned>(opcode) << " not implemented, PC: " << hex << static_cast<unsigned>(pc) << endl;
		// Para debugging, puedes pausar aquí o devolver 0.
		break;
	}

	writeFlagsToF();

	if (mmu.cyclesToAdd > 0) {
		addCycles(mmu.cyclesToAdd);
		std::cout << "Añadiendo ciclos de la mmu: " << static_cast<unsigned>(mmu.cyclesToAdd) << std::hex << std::endl;
		mmu.cyclesToAdd = 0;

		std::cout << "Total de ciclos de la cpu: " << static_cast<unsigned>(cicles) << std::hex << std::endl;
	}

	// Devuelve el total de ciclos que consumió esta instrucción (calculado por addCycles interno)
	return cicles;
}

bool isCarry8bit(int n) {
	return n > 0xFF;
}

bool isCarry16bit(int n) {
	return n > 0xFFFF;
}

bool CPU::isHalfCarry(uint8_t a, uint8_t b, std::string type) {
	if (type == "ADD") {
		return (((a & 0x0F) + (b & 0x0F)) & 0x10) == 0x10;
	}
	else {
		return (a & 0x0F) < (b & 0x0F);
	}
}

bool CPU::isHalfCarry16Bit(uint16_t a, uint16_t b, std::string type) {
	return (((a & 0xFFF) + (b & 0xFFF)) & 0x1000) == 0x1000;
}

void CPU::SET_B_R(uint16_t opcode) {
	uint8_t result;
	switch (opcode) {
	case 0xC0:
		result = setBit(reg.B, 0);
		mmu.setRegisters8Bit(&reg, "B", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xC1:
		result = setBit(reg.C, 0);
		mmu.setRegisters8Bit(&reg, "C", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xC2:
		result = setBit(reg.D, 0);
		mmu.setRegisters8Bit(&reg, "D", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xC3:
		result = setBit(reg.E, 0);
		mmu.setRegisters8Bit(&reg, "E", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xC4:
		result = setBit(reg.H, 0);
		mmu.setRegisters8Bit(&reg, "H", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xC5:
		result = setBit(reg.L, 0);
		mmu.setRegisters8Bit(&reg, "L", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xC6:
		result = setBit(mmu.read8(reg.HL), 0);
		mmu.write8(reg.HL, result);
		pc += 2;
		addCycles(16);
		break;
	case 0xC7:
		result = setBit(reg.A, 0);
		mmu.setRegisters8Bit(&reg, "A", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xC8:
		result = setBit(reg.B, 1);
		mmu.setRegisters8Bit(&reg, "B", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xC9:
		result = setBit(reg.C, 1);
		mmu.setRegisters8Bit(&reg, "C", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xCA:
		result = setBit(reg.D, 1);
		mmu.setRegisters8Bit(&reg, "D", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xCB:
		result = setBit(reg.E, 1);
		mmu.setRegisters8Bit(&reg, "E", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xCC:
		result = setBit(reg.H, 1);
		mmu.setRegisters8Bit(&reg, "H", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xCD:
		result = setBit(reg.L, 1);
		mmu.setRegisters8Bit(&reg, "L", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xCE:
		result = setBit(mmu.read8(reg.HL), 1);
		mmu.write8(reg.HL, result);
		pc += 2;
		addCycles(16);
		break;
	case 0xCF:
		result = setBit(reg.A, 1);
		mmu.setRegisters8Bit(&reg, "A", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xD0:
		result = setBit(reg.B, 2);
		mmu.setRegisters8Bit(&reg, "B", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xD1:
		result = setBit(reg.C, 2);
		mmu.setRegisters8Bit(&reg, "C", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xD2:
		result = setBit(reg.D, 2);
		mmu.setRegisters8Bit(&reg, "D", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xD3:
		result = setBit(reg.E, 2);
		mmu.setRegisters8Bit(&reg, "E", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xD4:
		result = setBit(reg.H, 2);
		mmu.setRegisters8Bit(&reg, "H", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xD5:
		result = setBit(reg.L, 2);
		mmu.setRegisters8Bit(&reg, "L", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xD6:
		result = setBit(mmu.read8(reg.HL), 2);
		mmu.write8(reg.HL, result);
		pc += 2;
		addCycles(16);
		break;
	case 0xD7:
		result = setBit(reg.A, 2);
		mmu.setRegisters8Bit(&reg, "A", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xD8:
		result = setBit(reg.B, 3);
		mmu.setRegisters8Bit(&reg, "B", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xD9:
		result = setBit(reg.C, 3);
		mmu.setRegisters8Bit(&reg, "C", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xDA:
		result = setBit(reg.D, 3);
		mmu.setRegisters8Bit(&reg, "D", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xDB:
		result = setBit(reg.E, 3);
		mmu.setRegisters8Bit(&reg, "E", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xDC:
		result = setBit(reg.H, 3);
		mmu.setRegisters8Bit(&reg, "H", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xDD:
		result = setBit(reg.L, 3);
		mmu.setRegisters8Bit(&reg, "L", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xDE:
		result = setBit(mmu.read8(reg.HL), 3);
		mmu.write8(reg.HL, result);
		pc += 2;
		addCycles(16);
		break;
	case 0xDF:
		result = setBit(reg.A, 3);
		mmu.setRegisters8Bit(&reg, "A", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xE0:
		result = setBit(reg.B, 4);
		mmu.setRegisters8Bit(&reg, "B", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xE1:
		result = setBit(reg.C, 4);
		mmu.setRegisters8Bit(&reg, "C", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xE2:
		result = setBit(reg.D, 4);
		mmu.setRegisters8Bit(&reg, "D", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xE3:
		result = setBit(reg.E, 4);
		mmu.setRegisters8Bit(&reg, "E", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xE4:
		result = setBit(reg.H, 4);
		mmu.setRegisters8Bit(&reg, "H", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xE5:
		result = setBit(reg.L, 4);
		mmu.setRegisters8Bit(&reg, "L", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xE6:
		result = setBit(mmu.read8(reg.HL), 4);
		mmu.write8(reg.HL, result);
		pc += 2;
		addCycles(16);
		break;
	case 0xE7:
		result = setBit(reg.A, 4);
		mmu.setRegisters8Bit(&reg, "A", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xE8:
		result = setBit(reg.B, 5);
		mmu.setRegisters8Bit(&reg, "B", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xE9:
		result = setBit(reg.C, 5);
		mmu.setRegisters8Bit(&reg, "C", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xEA:
		result = setBit(reg.D, 5);
		mmu.setRegisters8Bit(&reg, "D", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xEB:
		result = setBit(reg.E, 5);
		mmu.setRegisters8Bit(&reg, "E", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xEC:
		result = setBit(reg.H, 5);
		mmu.setRegisters8Bit(&reg, "H", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xED:
		result = setBit(reg.L, 5);
		mmu.setRegisters8Bit(&reg, "L", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xEE:
		result = setBit(mmu.read8(reg.HL), 5);
		mmu.write8(reg.HL, result);
		pc += 2;
		addCycles(16);
		break;
	case 0xEF:
		result = setBit(reg.A, 5);
		mmu.setRegisters8Bit(&reg, "A", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xF0:
		result = setBit(reg.B, 6);
		mmu.setRegisters8Bit(&reg, "B", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xF1:
		result = setBit(reg.C, 6);
		mmu.setRegisters8Bit(&reg, "C", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xF2:
		result = setBit(reg.D, 6);
		mmu.setRegisters8Bit(&reg, "D", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xF3:
		result = setBit(reg.E, 6);
		mmu.setRegisters8Bit(&reg, "E", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xF4:
		result = setBit(reg.H, 6);
		mmu.setRegisters8Bit(&reg, "H", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xF5:
		result = setBit(reg.L, 6);
		mmu.setRegisters8Bit(&reg, "L", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xF6:
		result = setBit(mmu.read8(reg.HL), 6);
		mmu.write8(reg.HL, result);
		pc += 2;
		addCycles(16);
		break;
	case 0xF7:
		result = setBit(reg.A, 6);
		mmu.setRegisters8Bit(&reg, "A", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xF8:
		result = setBit(reg.B, 7);
		mmu.setRegisters8Bit(&reg, "B", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xF9:
		result = setBit(reg.C, 7);
		mmu.setRegisters8Bit(&reg, "C", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xFA:
		result = setBit(reg.D, 7);
		mmu.setRegisters8Bit(&reg, "D", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xFB:
		result = setBit(reg.E, 7);
		mmu.setRegisters8Bit(&reg, "E", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xFC:
		result = setBit(reg.H, 7);
		mmu.setRegisters8Bit(&reg, "H", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xFD:
		result = setBit(reg.L, 7);
		mmu.setRegisters8Bit(&reg, "L", result);
		pc += 2;
		addCycles(8);
		break;
	case 0xFE:
		result = setBit(mmu.read8(reg.HL), 7);
		mmu.write8(reg.HL, result);
		pc += 2;
		addCycles(16);
		break;
	case 0xFF:
		result = setBit(reg.A, 7);
		mmu.setRegisters8Bit(&reg, "A", result);
		pc += 2;
		addCycles(8);
		break;
	default:
		break;
	}
}

void CPU::HALT() {
	// Leer IE y IF
	uint8_t ie = mmu.read8(0xFFFF);
	uint8_t if_reg = mmu.read8(0xFF0F);

	// Caso 1: IME activado - HALT normal
	if (IME) {
		isHalted = true;
		pc++;
		addCycles(4);
	}
	// Caso 2: IME desactivado PERO hay interrupciones pendientes - HALT bug
	else if ((ie & if_reg & 0x1F) != 0) {
		// HALT bug: no incrementar PC, la siguiente instrucción se ejecuta dos veces
		isHalted = false;
		// NO incrementar pc aquí
		addCycles(4);
	}
	// Caso 3: IME desactivado y NO hay interrupciones - HALT normal (sin IME)
	else {
		isHalted = true;
		pc++;
		addCycles(4);
	}
}

void CPU::DAA() {
	uint8_t adjustment = 0;

	// Caso A: La última operación fue una SUMA (N es falso)
	if (!flags.N) {
		// Si hubo Half-Carry o el nibble bajo es mayor a 9
		if (flags.H || (reg.A & 0x0F) > 0x09) {
			adjustment |= 0x06;
		}
		// Si hubo Carry o el acumulador es mayor a 0x99
		if (flags.C || reg.A > 0x99) {
			adjustment |= 0x60;
			flags.C = true; // El carry se mantiene o se activa
		}
	}
	// Caso B: La última operación fue una RESTA (N es verdadero)
	else {
		if (flags.H) {
			adjustment |= 0x06;
		}
		if (flags.C) {
			adjustment |= 0x60;
		}
		// Nota: En la resta, el flag C no se vuelve a evaluar como en la suma
	}

	// Aplicar el ajuste
	if (flags.N) {
		mmu.setRegisters8Bit(&reg, "A", reg.A - adjustment);
		flags.Z = ((reg.A - adjustment) == 0);
	}
	else {
		mmu.setRegisters8Bit(&reg, "A", reg.A + adjustment);
		flags.Z = ((reg.A + adjustment) == 0);
	}

	// Actualizar Flags
	flags.Z = (reg.A == 0);
	flags.H = false; // El flag H siempre se limpia después de DAA

	addCycles(4);
	pc += 1;
}

void CPU::RLCA() {
	bool hasLastBit = isKthBitSet(reg.A, 7);
	uint8_t result = reg.A << 1;

	if (hasLastBit) {
		result |= 0x01;
	}

	mmu.setRegisters8Bit(&reg, "A", result);

	flags.Z = false;
	flags.H = false;
	flags.N = false;
	flags.C = hasLastBit;

	addCycles(4);
	pc += 1;
}

void CPU::BIT_B_R(uint16_t opcode) {
	switch (opcode)
	{
	case 0x40:
		flags.Z = !isKthBitSet(reg.B, 0);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x41:
		flags.Z = !isKthBitSet(reg.C, 0);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x42:
		flags.Z = !isKthBitSet(reg.D, 0);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x43:
		flags.Z = !isKthBitSet(reg.E, 0);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x44:
		flags.Z = !isKthBitSet(reg.H, 0);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x45:
		flags.Z = !isKthBitSet(reg.L, 0);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x46:
		flags.Z = !isKthBitSet(mmu.read8(reg.HL), 0);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(16);

		break;
	case 0x47:
		flags.Z = !isKthBitSet(reg.A, 0);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x48:
		flags.Z = !isKthBitSet(reg.B, 1);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x49:
		flags.Z = !isKthBitSet(reg.C, 1);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x4A:
		flags.Z = !isKthBitSet(reg.D, 1);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x4B:
		flags.Z = !isKthBitSet(reg.E, 1);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x4C:
		flags.Z = !isKthBitSet(reg.H, 1);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x4D:
		flags.Z = !isKthBitSet(reg.L, 1);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x4E:
		flags.Z = !isKthBitSet(mmu.read8(reg.HL), 1);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(16);

		break;
	case 0x4F:
		flags.Z = !isKthBitSet(reg.A, 1);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x50:
		flags.Z = !isKthBitSet(reg.B, 2);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x51:
		flags.Z = !isKthBitSet(reg.C, 2);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x52:
		flags.Z = !isKthBitSet(reg.D, 2);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x53:
		flags.Z = !isKthBitSet(reg.E, 2);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x54:
		flags.Z = !isKthBitSet(reg.H, 2);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x55:
		flags.Z = !isKthBitSet(reg.L, 2);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x56:
		flags.Z = !isKthBitSet(mmu.read8(reg.HL), 2);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(16);

		break;
	case 0x57:
		flags.Z = !isKthBitSet(reg.A, 2);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x58:
		flags.Z = !isKthBitSet(reg.B, 3);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x59:
		flags.Z = !isKthBitSet(reg.C, 3);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x5A:
		flags.Z = !isKthBitSet(reg.D, 3);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x5B:
		flags.Z = !isKthBitSet(reg.E, 3);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x5C:
		flags.Z = !isKthBitSet(reg.H, 3);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x5D:
		flags.Z = !isKthBitSet(reg.L, 3);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x5E:
		flags.Z = !isKthBitSet(mmu.read8(reg.HL), 3);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(16);

		break;
	case 0x5F:
		flags.Z = !isKthBitSet(reg.A, 3);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x60:
		flags.Z = !isKthBitSet(reg.B, 4);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x61:
		flags.Z = !isKthBitSet(reg.C, 4);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x62:
		flags.Z = !isKthBitSet(reg.D, 4);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x63:
		flags.Z = !isKthBitSet(reg.E, 4);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x64:
		flags.Z = !isKthBitSet(reg.H, 4);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x65:
		flags.Z = !isKthBitSet(reg.L, 4);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x66:
		flags.Z = !isKthBitSet(mmu.read8(reg.HL), 4);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(16);

		break;
	case 0x67:
		flags.Z = !isKthBitSet(reg.A, 4);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x68:
		flags.Z = !isKthBitSet(reg.B, 5);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x69:
		flags.Z = !isKthBitSet(reg.C, 5);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x6A:
		flags.Z = !isKthBitSet(reg.D, 5);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x6B:
		flags.Z = !isKthBitSet(reg.E, 5);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x6C:
		flags.Z = !isKthBitSet(reg.H, 5);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x6D:
		flags.Z = !isKthBitSet(reg.L, 5);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x6E:
		flags.Z = !isKthBitSet(mmu.read8(reg.HL), 5);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(16);

		break;
	case 0x6F:
		flags.Z = !isKthBitSet(reg.A, 5);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x70:
		flags.Z = !isKthBitSet(reg.B, 6);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x71:
		flags.Z = !isKthBitSet(reg.C, 6);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x72:
		flags.Z = !isKthBitSet(reg.D, 6);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x73:
		flags.Z = !isKthBitSet(reg.E, 6);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x74:
		flags.Z = !isKthBitSet(reg.H, 6);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x75:
		flags.Z = !isKthBitSet(reg.L, 6);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x76:
		flags.Z = !isKthBitSet(mmu.read8(reg.HL), 6);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(16);

		break;
	case 0x77:
		flags.Z = !isKthBitSet(reg.A, 6);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x78:
		flags.Z = !isKthBitSet(reg.B, 7);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x79:
		flags.Z = !isKthBitSet(reg.C, 7);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x7A:
		flags.Z = !isKthBitSet(reg.D, 7);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x7B:
		flags.Z = !isKthBitSet(reg.E, 7);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x7C:
		flags.Z = !isKthBitSet(reg.H, 7);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x7D:
		flags.Z = !isKthBitSet(reg.L, 7);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	case 0x7E:
		flags.Z = !isKthBitSet(mmu.read8(reg.HL), 7);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(16);

		break;
	case 0x7F:
		flags.Z = !isKthBitSet(reg.A, 7);
		flags.N = false;
		flags.H = true;

		pc += 2;
		addCycles(8);

		break;
	default:
		break;
	}
}

void CPU::SRA_N(uint16_t opcode) {
	uint8_t lastBit;
	uint8_t result;
	uint8_t valueHL;

	switch (opcode)
	{
	case 0x28:
		lastBit = (reg.B & 0x80);

		flags.C = (reg.B & 0x01);

		result = reg.B >> 1 | lastBit;
		mmu.setRegisters8Bit(&reg, "B", result);

		flags.Z = result == 0;
		flags.N = false;
		flags.H = false;
		pc += 2;
		addCycles(8);
		break;
	case 0x29:
		lastBit = (reg.C & 0x80);

		flags.C = (reg.C & 0x01);

		result = reg.C >> 1 | lastBit;
		mmu.setRegisters8Bit(&reg, "C", result);

		flags.Z = result == 0;
		flags.N = false;
		flags.H = false;
		pc += 2;
		addCycles(8);
		break;
	case 0x2A:
		lastBit = (reg.D & 0x80);

		flags.C = (reg.D & 0x01);

		result = reg.D >> 1 | lastBit;
		mmu.setRegisters8Bit(&reg, "D", result);

		flags.Z = result == 0;
		flags.N = false;
		flags.H = false;
		pc += 2;
		addCycles(8);
		break;
	case 0x2B:
		lastBit = (reg.E & 0x80);

		flags.C = (reg.E & 0x01);

		result = reg.E >> 1 | lastBit;
		mmu.setRegisters8Bit(&reg, "E", result);

		flags.Z = result == 0;
		flags.N = false;
		flags.H = false;
		pc += 2;
		addCycles(8);
		break;
	case 0x2C:
		lastBit = (reg.H & 0x80);

		flags.C = (reg.H & 0x01);

		result = reg.H >> 1 | lastBit;
		mmu.setRegisters8Bit(&reg, "H", result);

		flags.Z = result == 0;
		flags.N = false;
		flags.H = false;
		pc += 2;
		addCycles(8);
		break;
	case 0x2D:
		lastBit = (reg.L & 0x80);

		flags.C = (reg.L & 0x01);

		result = reg.L >> 1 | lastBit;
		mmu.setRegisters8Bit(&reg, "L", result);

		flags.Z = result == 0;
		flags.N = false;
		flags.H = false;
		pc += 2;
		addCycles(8);
		break;
	case 0x2E:
		valueHL = mmu.read8(reg.HL);
		lastBit = (valueHL & 0x80);

		flags.C = (valueHL & 0x01);

		result = valueHL >> 1 | lastBit;
		mmu.write8(reg.HL, result);

		flags.Z = result == 0;
		flags.N = false;
		flags.H = false;
		pc += 2;
		addCycles(16);
		break;
	case 0x2F:
		lastBit = (reg.A & 0x80);

		flags.C = (reg.A & 0x01);

		result = reg.A >> 1 | lastBit;
		mmu.setRegisters8Bit(&reg, "A", result);

		flags.Z = result == 0;
		flags.N = false;
		flags.H = false;
		pc += 2;
		addCycles(8);
		break;
	default:
		break;
	}
}

void CPU::RLC_N(uint16_t opcode) {
	bool is_last_bit_set;
	uint8_t result;

	switch (opcode)
	{
	case 0x00:
		is_last_bit_set = (reg.B & 0x80) != 0;
		result = reg.B << 1;

		if (is_last_bit_set) {
			result = result | 0x01;
		}

		mmu.setRegisters8Bit(&reg, "B", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x01:
		is_last_bit_set = (reg.C & 0x80) != 0;
		result = reg.C << 1;

		if (is_last_bit_set) {
			result = result | 0x01;
		}

		mmu.setRegisters8Bit(&reg, "C", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x02:
		is_last_bit_set = (reg.D & 0x80) != 0;
		result = reg.D << 1;

		if (is_last_bit_set) {
			result = result | 0x01;
		}

		mmu.setRegisters8Bit(&reg, "D", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x03:
		is_last_bit_set = (reg.E & 0x80) != 0;
		result = reg.E << 1;

		if (is_last_bit_set) {
			result = result | 0x01;
		}

		mmu.setRegisters8Bit(&reg, "E", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x04:
		is_last_bit_set = (reg.H & 0x80) != 0;
		result = reg.H << 1;

		if (is_last_bit_set) {
			result = result | 0x01;
		}

		mmu.setRegisters8Bit(&reg, "H", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x05:
		is_last_bit_set = (reg.L & 0x80) != 0;
		result = reg.L << 1;

		if (is_last_bit_set) {
			result = result | 0x01;
		}

		mmu.setRegisters8Bit(&reg, "L", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x06:
		is_last_bit_set = (mmu.read8(reg.HL) & 0x80) != 0;
		result = (mmu.read8(reg.HL)) << 1;

		if (is_last_bit_set) {
			result = result | 0x01;
		}

		mmu.write8(reg.HL, result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(16);
		pc += 2;
		break;
	case 0x07:
		is_last_bit_set = (reg.A & 0x80) != 0;
		result = reg.A << 1;

		if (is_last_bit_set) {
			result = result | 0x01;
		}

		mmu.setRegisters8Bit(&reg, "A", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	default:
		break;
	}
}

void CPU::RRC_N(uint16_t opcode) {
	bool is_last_bit_set;
	uint8_t result;

	switch (opcode)
	{
	case 0x08:
		is_last_bit_set = (reg.B & 0x01) != 0;
		result = reg.B >> 1;

		if (is_last_bit_set) {
			result = result | 0x80;
		}

		mmu.setRegisters8Bit(&reg, "B", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x09:
		is_last_bit_set = (reg.C & 0x01) != 0;
		result = reg.C >> 1;

		if (is_last_bit_set) {
			result = result | 0x80;
		}

		mmu.setRegisters8Bit(&reg, "C", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x0A:
		is_last_bit_set = (reg.D & 0x01) != 0;
		result = reg.D >> 1;

		if (is_last_bit_set) {
			result = result | 0x80;
		}

		mmu.setRegisters8Bit(&reg, "D", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x0B:
		is_last_bit_set = (reg.E & 0x01) != 0;
		result = reg.E >> 1;

		if (is_last_bit_set) {
			result = result | 0x80;
		}

		mmu.setRegisters8Bit(&reg, "E", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x0C:
		is_last_bit_set = (reg.H & 0x01) != 0;
		result = reg.H >> 1;

		if (is_last_bit_set) {
			result = result | 0x80;
		}

		mmu.setRegisters8Bit(&reg, "H", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x0D:
		is_last_bit_set = (reg.L & 0x01) != 0;
		result = reg.L >> 1;

		if (is_last_bit_set) {
			result = result | 0x80;
		}

		mmu.setRegisters8Bit(&reg, "L", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x0E:
		is_last_bit_set = (mmu.read8(reg.HL) & 0x01) != 0;
		result = (mmu.read8(reg.HL)) >> 1;

		if (is_last_bit_set) {
			result = result | 0x80;
		}

		mmu.write8(reg.HL, result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(16);
		pc += 2;
		break;
	case 0x0F:
		is_last_bit_set = (reg.A & 0x01) != 0;
		result = reg.A >> 1;

		if (is_last_bit_set) {
			result = result | 0x80;
		}

		mmu.setRegisters8Bit(&reg, "A", result);

		flags.C = is_last_bit_set;
		flags.Z = false;
		flags.N = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	default:
		break;
	}
}

void CPU::SRL_N(uint16_t opcode) {
	bool bit0;
	uint8_t result;

	switch (opcode)
	{
	case 0x38:
		bit0 = (reg.B & 0x01) != 0;
		result = reg.B >> 1;
		mmu.setRegisters8Bit(&reg, "B", result);

		flags.Z = result == 0;
		flags.C = bit0;
		flags.H = false;
		flags.N = false;
		addCycles(8);
		pc+=2;

		break;
	case 0x39:
		bit0 = (reg.C & 0x01) != 0;
		result = reg.C >> 1;
		mmu.setRegisters8Bit(&reg, "C", result);

		flags.Z = result == 0;
		flags.C = bit0;
		flags.H = false;
		flags.N = false;
		addCycles(8);
		pc += 2;

		break;
	case 0x3A:
		bit0 = (reg.D & 0x01) != 0;
		result = reg.D >> 1;
		mmu.setRegisters8Bit(&reg, "D", result);

		flags.Z = result == 0;
		flags.C = bit0;
		flags.H = false;
		flags.N = false;
		addCycles(8);
		pc += 2;

		break;
	case 0x3B:
		bit0 = (reg.E & 0x01) != 0;
		result = reg.E >> 1;
		mmu.setRegisters8Bit(&reg, "E", result);

		flags.Z = result == 0;
		flags.C = bit0;
		flags.H = false;
		flags.N = false;
		addCycles(8);
		pc += 2;

		break;
	case 0x3C:
		bit0 = (reg.H & 0x01) != 0;
		result = reg.H >> 1;
		mmu.setRegisters8Bit(&reg, "H", result);

		flags.Z = result == 0;
		flags.C = bit0;
		flags.H = false;
		flags.N = false;
		addCycles(8);
		pc += 2;

		break;
	case 0x3D:
		bit0 = (reg.L & 0x01) != 0;
		result = reg.L >> 1;
		mmu.setRegisters8Bit(&reg, "L", result);

		flags.Z = result == 0;
		flags.C = bit0;
		flags.H = false;
		flags.N = false;
		addCycles(8);
		pc += 2;

		break;
	case 0x3E:
		bit0 = (mmu.read8(reg.HL) & 0x01) != 0;
		result = mmu.read8(reg.HL) >> 1;
		mmu.write8(reg.HL, result);

		flags.Z = result == 0;
		flags.C = bit0;
		flags.H = false;
		flags.N = false;
		addCycles(16);
		pc += 2;

		break;
	case 0x3F:
		bit0 = (reg.A & 0x01) != 0;
		result = reg.A >> 1;
		mmu.setRegisters8Bit(&reg, "A", result);

		flags.Z = result == 0;
		flags.C = bit0;
		flags.H = false;
		flags.N = false;
		addCycles(8);
		pc += 2;

		break;
	default:
		break;
	}
}

void CPU::RL_N(uint16_t opcode) {
	uint8_t result;
	bool is_last_bit_set;
	switch (opcode)
	{
	case 0x10:
		is_last_bit_set = (reg.B & 0x80) != 0;
		result = reg.B << 1;

		if (flags.C) {
			result |= 0x01;
		}

		mmu.setRegisters8Bit(&reg, "B", result);

		flags.C = is_last_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x11:
		is_last_bit_set = (reg.C & 0x80) != 0;
		result = reg.C << 1;

		if (flags.C) {
			result |= 0x01;
		}

		mmu.setRegisters8Bit(&reg, "C", result);

		flags.C = is_last_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x12:
		is_last_bit_set = (reg.D & 0x80) != 0;
		result = reg.D << 1;

		if (flags.C) {
			result |= 0x01;
		}

		mmu.setRegisters8Bit(&reg, "D", result);

		flags.C = is_last_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x13:
		is_last_bit_set = (reg.E & 0x80) != 0;
		result = reg.E << 1;

		if (flags.C) {
			result |= 0x01;
		}

		mmu.setRegisters8Bit(&reg, "E", result);

		flags.C = is_last_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x14:
		is_last_bit_set = (reg.H & 0x80) != 0;
		result = reg.H << 1;

		if (flags.C) {
			result |= 0x01;
		}

		mmu.setRegisters8Bit(&reg, "H", result);

		flags.C = is_last_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x15:
		is_last_bit_set = (reg.L & 0x80) != 0;
		result = reg.L << 1;

		if (flags.C) {
			result |= 0x01;
		}

		mmu.setRegisters8Bit(&reg, "L", result);

		flags.C = is_last_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x16:
		is_last_bit_set = (mmu.read8(reg.HL) & 0x80) != 0;
		result = mmu.read8(reg.HL) << 1;

		if (flags.C) {
			result |= 0x01;
		}

		mmu.write8(reg.HL, result);

		flags.C = is_last_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(16);
		pc += 2;
		break;
	case 0x17:
		is_last_bit_set = (reg.A & 0x80) != 0;
		result = reg.A << 1;

		if (flags.C) {
			result |= 0x01;
		}

		mmu.setRegisters8Bit(&reg, "A", result);

		flags.C = is_last_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	default:
		break;
	}
}

void CPU::RR_N(uint16_t opcode) {
	uint8_t result;
	bool is_first_bit_set;
	switch (opcode)
	{
	case 0x18:
		is_first_bit_set = (reg.B & 0x01) != 0;
		result = reg.B >> 1;

		if (flags.C) {
			result |= 0x80;
		}

		mmu.setRegisters8Bit(&reg, "B", result);

		flags.C = is_first_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x19:
		is_first_bit_set = (reg.C & 0x01) != 0;
		result = reg.C >> 1;

		if (flags.C) {
			result |= 0x80;
		}

		mmu.setRegisters8Bit(&reg, "C", result);

		flags.C = is_first_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x1A:
		is_first_bit_set = (reg.D & 0x01) != 0;
		result = reg.D >> 1;

		if (flags.C) {
			result |= 0x80;
		}

		mmu.setRegisters8Bit(&reg, "D", result);

		flags.C = is_first_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x1B:
		is_first_bit_set = (reg.E & 0x01) != 0;
		result = reg.E >> 1;

		if (flags.C) {
			result |= 0x80;
		}

		mmu.setRegisters8Bit(&reg, "E", result);

		flags.C = is_first_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x1C:
		is_first_bit_set = (reg.H & 0x01) != 0;
		result = reg.H >> 1;

		if (flags.C) {
			result |= 0x80;
		}

		mmu.setRegisters8Bit(&reg, "H", result);

		flags.C = is_first_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x1D:
		is_first_bit_set = (reg.L & 0x01) != 0;
		result = reg.L >> 1;

		if (flags.C) {
			result |= 0x80;
		}

		mmu.setRegisters8Bit(&reg, "L", result);

		flags.C = is_first_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	case 0x1E:
		is_first_bit_set = (mmu.read8(reg.HL) & 0x01) != 0;
		result = mmu.read8(reg.HL) >> 1;

		if (flags.C) {
			result |= 0x80;
		}

		mmu.write8(reg.HL, result);

		flags.C = is_first_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(16);
		pc += 2;
		break;
	case 0x1F:
		is_first_bit_set = (reg.A & 0x01) != 0;
		result = reg.A >> 1;

		if (flags.C) {
			result |= 0x80;
		}

		mmu.setRegisters8Bit(&reg, "A", result);

		flags.C = is_first_bit_set;
		flags.N = false;
		flags.H = false;
		flags.Z = result == 0;

		addCycles(8);
		pc += 2;
		break;
	default:
		break;
	}
}

void CPU::LDHL_SP_N(uint16_t opcode) {
	int8_t n = (int8_t)mmu.read8(pc + 1);
	int result = (uint16_t)mmu.sp + n;

	flags.H = ((mmu.sp & 0x0F) + (n & 0x0F)) > 0x0F;
	flags.Z = false;
	flags.N = false;
	flags.C = ((mmu.sp & 0xFF) + (n & 0xFF)) > 0xFF;

	mmu.setRegisters16Bit(&reg, "HL", (uint16_t)result);

	pc+=2;
	addCycles(12);
}

void CPU::BIT(uint16_t opcode) {
	switch (opcode)
	{
	case 0x47:
		break;
	case 0x41:
		break;
	case 0x42:
		break;
	case 0x43:
		break;
	case 0x44:
		break;
	case 0x45:
		break;
	case 0x46:
		break;
	default:
		break;
	}
}

void CPU::SUB_N(uint16_t opcode) {
	uint8_t regA = reg.A;
	uint8_t n;
	flags.N = true;
	switch (opcode)
	{
	case 0x90:
		n = reg.B;
		flags.Z = reg.A - n == 0;
		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		flags.C = reg.A < n;
		mmu.setRegisters8Bit(&reg, "A", reg.A - n);
		pc++;
		addCycles(4);
		break;

	case 0x91:
		n = reg.C;
		flags.Z = reg.A - n == 0;
		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		flags.C = reg.A < n;
		mmu.setRegisters8Bit(&reg, "A", reg.A - n);
		pc++;
		addCycles(4);
		break;

	case 0x92:
		n = reg.D;
		flags.Z = reg.A - n == 0;
		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		flags.C = reg.A < n;
		mmu.setRegisters8Bit(&reg, "A", reg.A - n);
		pc++;
		addCycles(4);
		break;

	case 0x93:
		n = reg.E;
		flags.Z = reg.A - n == 0;
		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		flags.C = reg.A < n;
		mmu.setRegisters8Bit(&reg, "A", reg.A - n);
		pc++;
		addCycles(4);
		break;

	case 0x94:
		n = reg.H;
		flags.Z = reg.A - n == 0;
		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		flags.C = reg.A < n;
		mmu.setRegisters8Bit(&reg, "A", reg.A - n);
		pc++;
		addCycles(4);
		break;

	case 0x95:
		n = reg.L;
		flags.Z = reg.A - n == 0;
		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		flags.C = reg.A < n;
		mmu.setRegisters8Bit(&reg, "A", reg.A - n);
		pc++;
		addCycles(4);
		break;

	case 0x96:
		n = mmu.read8(reg.HL);
		flags.Z = reg.A - n == 0;
		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		flags.C = reg.A < n;
		mmu.setRegisters8Bit(&reg, "A", reg.A - n);
		pc++;
		addCycles(8);
		break;

	case 0x97:
		n = reg.A;
		flags.Z = reg.A - n == 0;
		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		flags.C = reg.A < n;
		mmu.setRegisters8Bit(&reg, "A", reg.A - n);
		pc++;
		addCycles(4);
		break;

	case 0xD6:
		n = mmu.read8(pc+1);
		flags.Z = reg.A - n == 0;
		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		flags.C = reg.A < n;
		mmu.setRegisters8Bit(&reg, "A", reg.A - n);
		pc+=2;
		addCycles(8);
		break;

	default:
		break;
	}
}

void CPU::SLA_N(uint16_t opcode) {
	uint8_t carryBit = 0;
	uint8_t valToSave = 0;
	flags.H = false;
	flags.N = false;

	switch (opcode)
	{
	case 0x27:
		valToSave = reg.A << 1;
		flags.C = isKthBitSet(reg.A, 7);
		flags.Z = valToSave == 0 ? true : false;
		mmu.setRegisters8Bit(&reg, "A", valToSave);
		addCycles(8);
		pc += 2;
		break;

	case 0x20:
		valToSave = reg.B << 1;
		flags.C = isKthBitSet(reg.B, 7);
		flags.Z = valToSave == 0 ? true : false;
		mmu.setRegisters8Bit(&reg, "B", valToSave);
		addCycles(8);
		pc += 2;
		break;

	case 0x21:
		valToSave = reg.C << 1;
		flags.C = isKthBitSet(reg.C, 7);
		flags.Z = valToSave == 0 ? true : false;
		mmu.setRegisters8Bit(&reg, "C", valToSave);
		addCycles(8);
		pc += 2;
		break;

	case 0x22:

		valToSave = reg.D << 1;
		flags.C = isKthBitSet(reg.D, 7);
		flags.Z = valToSave == 0 ? true : false;
		mmu.setRegisters8Bit(&reg, "D", valToSave);
		addCycles(8);
		pc += 2;
		break;

	case 0x23:
		valToSave = reg.E << 1;
		flags.C = isKthBitSet(reg.E, 7);
		flags.Z = valToSave == 0 ? true : false;
		mmu.setRegisters8Bit(&reg, "E", valToSave);
		addCycles(8);
		pc += 2;
		break;

	case 0x24:
		valToSave = reg.H << 1;
		flags.C = isKthBitSet(reg.H, 7);
		flags.Z = valToSave == 0 ? true : false;
		mmu.setRegisters8Bit(&reg, "H", valToSave);
		addCycles(8);
		pc += 2;
		break;

	case 0x25:
		valToSave = reg.L << 1;
		flags.C = isKthBitSet(reg.L, 7);
		flags.Z = valToSave == 0 ? true : false;
		mmu.setRegisters8Bit(&reg, "L", valToSave);
		addCycles(8);
		pc += 2;
		break;

	case 0x26:
		valToSave = mmu.read(reg.HL) << 1;
		flags.C = isKthBitSet(reg.HL, 7);
		flags.Z = valToSave == 0 ? true : false;
		mmu.write8(reg.HL, valToSave);
		addCycles(16);
		pc += 2;
		break;
	default:
		break;
	}
}

void CPU::JP_CC_NN(uint16_t opcode) {
	uint16_t nextAddress = mmu.read(pc + 1);
	switch (opcode)
	{
		case 0xCA:
			if (flags.Z) {
				pc = nextAddress;
				addCycles(16);
			}
			else {
				pc += 3;
				addCycles(12);
			}

			break;

		case 0xC2:
			if (!flags.Z) {
				pc = nextAddress;
				addCycles(16);
			}
			else {
				pc += 3;
				addCycles(12);
			}
			break;

		case 0xDA:
			if (flags.C) {
				pc = nextAddress;
				addCycles(16);
			}
			else {
				pc += 3;
				addCycles(12);
			}
			break;

		case 0xD2:
			if (!flags.C) {
				pc = nextAddress;
				addCycles(16);
			}
			else {
				pc += 3;
				addCycles(12);
			}
			break;
	default:
		break;
	}
}

void CPU::JR_N() {
	int8_t n = (int8_t)mmu.read8(pc + 1);
	//uint16_t nextAddress = pc - (0xff - n) - 1;
	uint16_t nextAddress = pc + n + 2;
	//pc += 2;

	//std::cout << std::hex << "Next address: " << static_cast<unsigned>(nextAddress) << std::endl;
	pc = nextAddress;
	addCycles(12);
}

void CPU::RRA() {
	bool oldCarry = flags.C;
	flags.C = (reg.A & 0x01) != 0;

	uint8_t result = reg.A >> 1;
	if (oldCarry) {
		result |= 0x80;
	}

	mmu.setRegisters8Bit(&reg, "A", result);

	flags.Z = false;  // RRA nunca pone Z
	flags.N = false;
	flags.H = false;

	addCycles(4);
	pc++;
}

void CPU::RET() {
	uint16_t addrPoped = 0;
	mmu.pop(&addrPoped);
	//std::cout << "ADDR POPPED: " << std::hex << static_cast<unsigned>(addrPoped) << std::endl;
	addCycles(16);
	pc = addrPoped;
}

void CPU::RETI() {
	IME = true;
	uint16_t addrPoped;
	mmu.pop(&addrPoped);

	addCycles(16);
	pc = addrPoped;
}

void CPU::LDD_A_regHL() {
	mmu.setRegisters8Bit(&reg, "A", mmu.read8(reg.HL));
	mmu.setRegisters16Bit(&reg, "HL", reg.HL - 1, &flags);
	addCycles(8);
	pc++;
}

void CPU::LDD_regHL_A() {
	//std::cout << "Writing value " << hex << static_cast<unsigned>(reg.A) << ", in address: " << hex << static_cast<unsigned>(reg.HL) << std::endl;
	mmu.write8(reg.HL, reg.A);
	mmu.setRegisters16Bit(&reg, "HL", reg.HL - 1);
	addCycles(8);
	pc++;
}

void CPU::STOP() {
	uint8_t n = mmu.read8(pc + 1);
	if (n == 0x00) {
		pc += 2;
		isStoped = true;
		mmu.write8(0xFF04, 0);
		addCycles(4);
	}
	else {
		isStoped = false;
		pc += 1;
		addCycles(4);
	}
}

void CPU::XOR_N(uint16_t opcode) {
	uint8_t n = 0;
	switch (opcode) {
		case 0xAF:
			n = reg.A ^ reg.A;
			//std::cout << "XOR A " << hex << static_cast<unsigned>(n) << std::endl;
			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			flags.C = false;
			flags.N = false;
			flags.H = false;
			mmu.setRegisters8Bit(&reg, "A", n);
			addCycles(4);
			pc++;
			break; 
		case 0xA8:
			n = reg.B ^ reg.A;
			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			flags.C = false;
			flags.N = false;
			flags.H = false;
			mmu.setRegisters8Bit(&reg, "A", n); 
			addCycles(4);
			pc++;
			break;
		case 0xA9:
			n = reg.C ^ reg.A;
			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			flags.C = false;
			flags.N = false;
			flags.H = false;
			mmu.setRegisters8Bit(&reg, "A", n); 
			addCycles(4);
			pc++;
			break;
		case 0xAA:
			n = reg.D ^ reg.A;
			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			flags.C = false;
			flags.N = false;
			flags.H = false;
			mmu.setRegisters8Bit(&reg, "A", n);
			addCycles(4);
			pc++;
			break;
		case 0xAB:
			n = reg.E ^ reg.A;
			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			flags.C = false;
			flags.N = false;
			flags.H = false;
			mmu.setRegisters8Bit(&reg, "A", n);
			addCycles(4);
			pc++;
			break;
		case 0xAC:
			n = reg.H ^ reg.A;
			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			flags.C = false;
			flags.N = false;
			flags.H = false;
			mmu.setRegisters8Bit(&reg, "A", n);
			addCycles(4);
			pc++;
			break;
		case 0xAD:
			n = reg.L ^ reg.A;
			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			flags.C = false;
			flags.N = false;
			flags.H = false;
			mmu.setRegisters8Bit(&reg, "A", n);
			addCycles(4);
			pc++;
			break;
		case 0xAE:
			n = mmu.read8(reg.HL) ^ reg.A;
			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			flags.C = false;
			flags.N = false;
			flags.H = false;
			mmu.setRegisters8Bit(&reg, "A", n);
			addCycles(8);
			pc++;
			break;
		case 0xEE:
			n = mmu.read8(pc + 1) ^ reg.A;
			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			flags.C = false;
			flags.N = false;
			flags.H = false;
			mmu.setRegisters8Bit(&reg, "A", n);
			addCycles(8);
			pc+=2;
			break;
	}
}

void CPU::OR_N(uint16_t opcode) {
	uint8_t n;
	flags.C = false;
	flags.H = false;
	flags.N = false;
	switch (opcode)
	{
		case 0xB7:
			n = reg.A | reg.A;
			mmu.setRegisters8Bit(&reg, "A", n);

			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0xB0:
			n = reg.B | reg.A;
			mmu.setRegisters8Bit(&reg, "A", n);

			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0xB1:
			n = reg.C | reg.A;
			mmu.setRegisters8Bit(&reg, "A", n);

			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0xB2:
			n = reg.D | reg.A;
			mmu.setRegisters8Bit(&reg, "A", n);

			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0xB3:
			n = reg.E | reg.A;
			mmu.setRegisters8Bit(&reg, "A", n);

			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0xB4:
			n = reg.H | reg.A;
			mmu.setRegisters8Bit(&reg, "A", n);

			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0xB5:
			n = reg.L | reg.A;
			mmu.setRegisters8Bit(&reg, "A", n);

			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0xB6:
			n = mmu.read8(reg.HL) | reg.A;
			mmu.setRegisters8Bit(&reg, "A", n);

			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(8);
			pc++;
			break;
		case 0xF6:
			n = mmu.read8(pc+1) | reg.A;
			mmu.setRegisters8Bit(&reg, "A", n);

			if (n == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(8);
			pc+=2;
			break;
	default:
		break;
	}
}

void CPU::AND_N(uint16_t opcode) {
	uint8_t n;
	switch (opcode)
	{
	case 0xA7:
		n = reg.A & reg.A;
		mmu.setRegisters8Bit(&reg, "A", n);

		if (n == 0) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0xA0:
		n = reg.B & reg.A;
		mmu.setRegisters8Bit(&reg, "A", n);

		if (n == 0) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0xA1:
		n = reg.C & reg.A;
		mmu.setRegisters8Bit(&reg, "A", n);

		if (n == 0) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0xA2:
		n = reg.D & reg.A;
		mmu.setRegisters8Bit(&reg, "A", n);

		if (n == 0) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0xA3:
		n = reg.E & reg.A;
		mmu.setRegisters8Bit(&reg, "A", n);

		if (n == 0) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0xA4:
		n = reg.H & reg.A;
		mmu.setRegisters8Bit(&reg, "A", n);

		if (n == 0) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0xA5:
		n = reg.L & reg.A;
		mmu.setRegisters8Bit(&reg, "A", n);

		if (n == 0) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0xA6:
		n = mmu.read8(reg.HL) & reg.A;
		mmu.setRegisters8Bit(&reg, "A", n);

		if (n == 0) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}
		addCycles(8);
		pc++;
		break;

	case 0xE6:
		n = mmu.read8(pc+1) & reg.A;
		mmu.setRegisters8Bit(&reg, "A", n);

		if (n == 0) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}
		addCycles(8);
		pc+=2;
		break;
	default:
		break;
	}

	flags.C = false;
	flags.H = true;
	flags.N = false;
}

void CPU::LD_NN_N(uint16_t opcode) {
	uint8_t n = mmu.read8(pc+1);

	switch (opcode)
	{

	case 0x06:
		//reg.B = n;
		mmu.setRegisters8Bit(&reg, "B", n);
		addCycles(8);
		pc += 2;
		break;

	case 0x0E:
		//reg.C = n;
		mmu.setRegisters8Bit(&reg, "C", n);
		addCycles(8);
		pc += 2;
		break;

	case 0x16:
		//reg.D = n;
		mmu.setRegisters8Bit(&reg, "D", n);
		addCycles(8);
		pc += 2;
		break;

	case 0x1E:
		//reg.E = n;
		mmu.setRegisters8Bit(&reg, "E", n);
		addCycles(8);
		pc += 2;
		break;

	case 0x26:
		//reg.H = n;
		mmu.setRegisters8Bit(&reg, "H", n);
		addCycles(8);
		pc += 2;
		break;

	case 0x2E:
		//reg.L = n;
		mmu.setRegisters8Bit(&reg, "L", n);
		addCycles(8);
		pc+=2;
		break;

	default:
		break;
	}
}

void CPU::LD_r1_r2(uint16_t opcode) {
	uint8_t n = mmu.read8(pc + 1);
	switch (opcode)
	{
	case 0x7F:
		//reg.A = reg.A;
		mmu.setRegisters8Bit(&reg, "A", reg.A);
		addCycles(4);
		pc += 1;
		break;
	case 0x78:
		//reg.A = reg.B;
		mmu.setRegisters8Bit(&reg, "A", reg.B);
		addCycles(4);
		pc += 1;
		break;
	case 0x79:
		//reg.A = reg.C;
		mmu.setRegisters8Bit(&reg, "A", reg.C);
		addCycles(4);
		pc += 1;
		break;
	case 0x7a:
		//reg.A = reg.D;
		mmu.setRegisters8Bit(&reg, "A", reg.D);
		addCycles(4);
		pc += 1;
		break;
	case 0x7b:
		//reg.A = reg.E;
		mmu.setRegisters8Bit(&reg, "A", reg.E);
		addCycles(4);
		pc += 1;
		break;
	case 0x7c:
		//reg.A = reg.H;
		mmu.setRegisters8Bit(&reg, "A", reg.H);
		addCycles(4);
		pc += 1;
		break;
	case 0x7d:
		//reg.A = reg.L;
		mmu.setRegisters8Bit(&reg, "A", reg.L);
		addCycles(4);
		pc += 1;
		break;
	case 0x7e:
		//reg.A = mmu.read8(reg.HL);
		mmu.setRegisters8Bit(&reg, "A", mmu.read8(reg.HL));
		addCycles(8);
		pc += 1;
		break;
	case 0x40:
		//reg.B = reg.B;
		mmu.setRegisters8Bit(&reg, "B", reg.B);
		addCycles(4);
		pc += 1;
		break;
	case 0x41:
		//reg.B = reg.C;
		mmu.setRegisters8Bit(&reg, "B", reg.C);
		addCycles(4);
		pc += 1;
		break;
	case 0x42:
		//reg.B = reg.D;
		mmu.setRegisters8Bit(&reg, "B", reg.D);
		addCycles(4);
		pc += 1;
		break;
	case 0x43:
		//reg.B = reg.E;
		mmu.setRegisters8Bit(&reg, "B", reg.E);
		addCycles(4);
		pc += 1;
		break;
	case 0x44:
		//reg.B = reg.H;
		mmu.setRegisters8Bit(&reg, "B", reg.H);
		addCycles(4);
		pc += 1;
		break;
	case 0x45:
		//reg.B = reg.L;
		mmu.setRegisters8Bit(&reg, "B", reg.L);
		addCycles(4);
		pc += 1;
		break;
	case 0x46:
		//reg.B = mmu.read8(reg.HL);
		mmu.setRegisters8Bit(&reg, "B", mmu.read8(reg.HL));
		addCycles(8);
		pc += 1;
		break;
	case 0x48:
		//reg.C = reg.B;
		mmu.setRegisters8Bit(&reg, "C", reg.B);
		addCycles(4);
		pc += 1;
		break;
	case 0x49:
		//reg.C = reg.C;
		mmu.setRegisters8Bit(&reg, "C", reg.C);
		addCycles(4);
		pc += 1;
		break;
	case 0x4A:
		//reg.C = reg.D;
		mmu.setRegisters8Bit(&reg, "C", reg.D);
		addCycles(4);
		pc += 1;
		break;
	case 0x4B:
		//reg.C = reg.E;
		mmu.setRegisters8Bit(&reg, "C", reg.E);
		addCycles(4);
		pc += 1;
		break;
	case 0x4C:
		//reg.C = reg.H;
		mmu.setRegisters8Bit(&reg, "C", reg.H);
		addCycles(4);
		pc += 1;
		break;
	case 0x4D:
		//reg.C = reg.L;
		mmu.setRegisters8Bit(&reg, "C", reg.L);
		addCycles(4);
		pc += 1;
		break;
	case 0x4E:
		//reg.C = mmu.read8(reg.HL);
		mmu.setRegisters8Bit(&reg, "C", mmu.read8(reg.HL));
		addCycles(8);
		pc += 1;
		break;
	case 0x50:
		//reg.D = reg.B;
		mmu.setRegisters8Bit(&reg, "D", reg.B);
		addCycles(4);
		pc += 1;
		break;
	case 0x51:
		//reg.D = reg.C;
		mmu.setRegisters8Bit(&reg, "D", reg.C);
		addCycles(4);
		pc += 1;
		break;
	case 0x52:
		//reg.D = reg.D;
		mmu.setRegisters8Bit(&reg, "D", reg.D);
		addCycles(4);
		pc += 1;
		break;
	case 0x53:
		//reg.D = reg.E;
		mmu.setRegisters8Bit(&reg, "D", reg.E);
		addCycles(4);
		pc += 1;
		break;
	case 0x54:
		//reg.D = reg.H;
		mmu.setRegisters8Bit(&reg, "D", reg.H);
		addCycles(4);
		pc += 1;
		break;
	case 0x55:
		//reg.D = reg.L;
		mmu.setRegisters8Bit(&reg, "D", reg.L);
		addCycles(4);
		pc += 1;
		break;
	case 0x56:
		//reg.D = mmu.read8(reg.HL);
		mmu.setRegisters8Bit(&reg, "D", mmu.read8(reg.HL));
		addCycles(8);
		pc += 1;
		break;
	case 0x58:
		//reg.E = reg.B;
		mmu.setRegisters8Bit(&reg, "E", reg.B);
		addCycles(4);
		pc += 1;
		break;
	case 0x59:
		//reg.E = reg.C;
		mmu.setRegisters8Bit(&reg, "E", reg.C);
		addCycles(4);
		pc += 1;
		break;
	case 0x5A:
		//reg.E = reg.D;
		mmu.setRegisters8Bit(&reg, "E", reg.D);
		addCycles(4);
		pc += 1;
		break;
	case 0x5B:
		//reg.E = reg.E;
		mmu.setRegisters8Bit(&reg, "E", reg.E);
		addCycles(4);
		pc += 1;
		break;
	case 0x5C:
		//reg.E = reg.H;
		mmu.setRegisters8Bit(&reg, "E", reg.H);
		addCycles(4);
		pc += 1;
		break;
	case 0x5D:
		//reg.E = reg.L;
		mmu.setRegisters8Bit(&reg, "E", reg.L);
		addCycles(4);
		pc += 1;
		break;
	case 0x5E:
		//reg.E = mmu.read8(reg.HL);
		mmu.setRegisters8Bit(&reg, "E", mmu.read8(reg.HL));
		addCycles(8);
		pc += 1;
		break;
	case 0x60:
		//reg.H = reg.B;
		mmu.setRegisters8Bit(&reg, "H", reg.B);
		addCycles(4);
		pc += 1;
		break;
	case 0x61:
		//reg.H = reg.C;
		mmu.setRegisters8Bit(&reg, "H", reg.C);
		addCycles(4);
		pc += 1;
		break;
	case 0x62:
		//reg.H = reg.D;
		mmu.setRegisters8Bit(&reg, "H", reg.D);
		addCycles(4);
		pc += 1;
		break;
	case 0x63:
		//reg.H = reg.E;
		mmu.setRegisters8Bit(&reg, "H", reg.E);
		addCycles(4);
		pc += 1;
		break;
	case 0x64:
		//reg.H = reg.H;
		mmu.setRegisters8Bit(&reg, "H", reg.H);
		addCycles(4);
		pc += 1;
		break;
	case 0x65:
		//reg.H = reg.L;
		mmu.setRegisters8Bit(&reg, "H", reg.L);
		addCycles(4);
		pc += 1;
		break;
	case 0x66:
		//reg.H = mmu.read8(reg.HL);
		mmu.setRegisters8Bit(&reg, "H", mmu.read8(reg.HL));
		addCycles(8);
		pc += 1;
		break;
	case 0x68:
		//reg.L = reg.B;
		mmu.setRegisters8Bit(&reg, "L", reg.B);
		addCycles(4);
		pc += 1;
		break;
	case 0x69:
		//reg.L = reg.C;
		mmu.setRegisters8Bit(&reg, "L", reg.C);
		addCycles(4);
		pc += 1;
		break;
	case 0x6A:
		//reg.L = reg.D;
		mmu.setRegisters8Bit(&reg, "L", reg.D);
		addCycles(4);
		pc += 1;
		break;
	case 0x6B:
		//reg.L = reg.E;
		mmu.setRegisters8Bit(&reg, "L", reg.E);
		addCycles(4);
		pc += 1;
		break;
	case 0x6C:
		//reg.L = reg.H;
		mmu.setRegisters8Bit(&reg, "L", reg.H);
		addCycles(4);
		pc += 1;
		break;
	case 0x6D:
		//reg.L = reg.L;
		mmu.setRegisters8Bit(&reg, "L", reg.L);
		addCycles(4);
		pc += 1;
		break;
	case 0x6E:
		//reg.L = mmu.read8(reg.HL);
		mmu.setRegisters8Bit(&reg, "L", mmu.read8(reg.HL));
		addCycles(8);
		pc += 1;
		break;
	case 0x70:
		mmu.write8(reg.HL, reg.B);
		addCycles(8);
		pc += 1;
		break;
	case 0x71:
		mmu.write8(reg.HL, reg.C);
		addCycles(8);
		pc += 1;
		break;
	case 0x72:
		mmu.write8(reg.HL, reg.D);
		addCycles(8);
		pc += 1;
		break;
	case 0x73:
		mmu.write8(reg.HL, reg.E);
		addCycles(8);
		pc += 1;
		break;
	case 0x74:
		mmu.write8(reg.HL, reg.H);
		addCycles(8);
		pc += 1;
		break;
	case 0x75:
		mmu.write8(reg.HL, reg.L);
		addCycles(8);
		pc += 1;
		break;
	case 0x36:
		mmu.write8(reg.HL, n);
		addCycles(12);
		pc += 2;
		break;
	default:
		break;
	}
}

void CPU::LD_A_N(uint16_t opcode) {
	uint16_t nn = mmu.read(pc + 1);
	uint8_t n = mmu.read8(pc + 1);
	switch (opcode)
	{
	
	case 0x0A:
		//std::cout << "LD A, (BC)" << endl;
		//reg.A = mmu.read8(reg.BC);
		mmu.setRegisters8Bit(&reg, "A", mmu.read8(reg.BC));
		pc+=1;
		addCycles(8);
		break;
	case 0x1A:
		//std::cout << "LD A, (DE)" << endl;
		//reg.A = mmu.read8(reg.DE);
		mmu.setRegisters8Bit(&reg, "A", mmu.read8(reg.DE));
		pc+=1;
		addCycles(8);
		break;
	case 0xFA:
		//std::cout << "LD A, (" << hex << static_cast<unsigned>(nn) << "h)" << endl;
		//reg.A = mmu.read8(nn);
		mmu.setRegisters8Bit(&reg, "A", mmu.read(nn));
		pc += 3;
		addCycles(16);
		break;
	case 0x3E:
		//std::cout << "LD A, " << hex << static_cast<unsigned>(n) << "h)" << endl;
		//reg.A = mmu.read8(n);
		//std::cout << "Value to set in A: " << hex << static_cast<unsigned>(n) << std::endl;
		mmu.setRegisters8Bit(&reg, "A", n);
		pc+=2;
		addCycles(8);
		break;
	default:
		break;
	}
}

void CPU::LD_N_A(uint16_t opcode) {
	uint16_t nn = mmu.read(pc + 1);
	switch (opcode)
	{
	case 0x47:
		//cout << "LD B, A" << endl;
		//reg.B = reg.A;
		mmu.setRegisters8Bit(&reg, "B", reg.A);
		pc += 1;
		addCycles(4);
		break;
	case 0x4F:
		//cout << "LD C, A" << endl;
		//reg.C = reg.A;
		mmu.setRegisters8Bit(&reg, "C", reg.A);
		pc += 1;
		addCycles(4);
		break;
	case 0x57:
		//cout << "LD D, A" << endl;
		mmu.setRegisters8Bit(&reg, "D", reg.A);
		//reg.D = reg.A;
		pc += 1;
		addCycles(4);
		break;
	case 0x5F:
		//cout << "LD E, A" << endl;
		mmu.setRegisters8Bit(&reg, "E", reg.A);
		//reg.E = reg.A;
		pc += 1;
		addCycles(4);
		break;
	case 0x67:
		//cout << "LD H, A" << endl;
		mmu.setRegisters8Bit(&reg, "H", reg.A);
		//reg.H = reg.A;
		pc += 1;
		addCycles(4);
		break;
	case 0x6F:
		//cout << "LD L, A" << endl;
		mmu.setRegisters8Bit(&reg, "L", reg.A);
		//reg.L = reg.A;
		pc += 1;
		addCycles(4);
		break;
	case 0x02:
		//cout << "LD (BC), A" << endl;
		mmu.write8(reg.BC, reg.A);
		pc += 1;
		addCycles(8);
		break;
	case 0x12:
		//cout << "LD (DE), A" << endl;
		mmu.write8(reg.DE, reg.A);
		pc += 1;
		addCycles(8);
		break;
	case 0x77:
		//cout << "LD (HL), A" << endl;
		mmu.write8(reg.HL, reg.A);
		pc += 1;
		addCycles(8);
		break;
	case 0xEA:
		//cout << "LD (" << hex << static_cast<unsigned>(nn) << "h), A" << endl;
		//reg.B = reg.A;
		mmu.write8(nn, reg.A);
		pc += 3; 
		addCycles(16);
		break;
	}
}

void CPU::LD_N_NN(uint16_t opcode) {
	uint16_t nn = mmu.read(pc + 1);
	switch (opcode)
	{
	case 0x01:
		//reg.BC = nn;
		//cout << "LD BC, " << hex << nn << endl;
		mmu.setRegisters16Bit(&reg, "BC", nn);
		pc += 3;
		addCycles(12);
		break;
	case 0x11:
		//reg.DE = nn;
		//cout << "LD DE, " << hex << nn << endl;
		mmu.setRegisters16Bit(&reg, "DE", nn);
		pc += 3;
		addCycles(12);
		break;
	case 0x21:
		//reg.HL = nn;
		//cout << "LD HL, " << hex << nn << endl;
		mmu.setRegisters16Bit(&reg, "HL", nn);
		pc += 3;
		addCycles(12);
		break;
	case 0x31:
		mmu.sp = nn;
		//cout << "LD SP, " << hex << nn << endl;
		pc += 3;
		addCycles(12);
		break;
	}
}

void CPU::NOP() {
	pc++;
	//cout << "NOP" << endl;
	addCycles(4);
}

void CPU::JP_NN(uint16_t opcode) {
	pc = mmu.read(pc+1);
	//cout << "JP, " << hex << pc << "h" << endl;
	addCycles(16);
}

void CPU::JP_regHL() {
	pc = reg.HL;
	addCycles(4);
}

void CPU::DI() {
	IME = false;
	//cout << "DI" << endl;
	addCycles(4);
	pc++;
}

void CPU::EI() {
	IME = true;
	//cout << "EI" << endl;
	addCycles(4);
	pc++;
}

void CPU::CALL_CC_NN(uint16_t opcode) {
	switch (opcode)
	{
	case 0xC4:
		if (!flags.Z) {
			uint16_t nn = mmu.read(pc + 1);
			//cout << "CALL, " << static_cast<unsigned>(nn) << "h" << endl;
			mmu.push(pc + 3);
			//std::cout << "CALL, " << hex << nn << "h, PC: " << hex << static_cast<unsigned>(pc) << std::endl;
			pc = nn;
			addCycles(24);
		}
		else {
			pc += 3;
			addCycles(12);
		}
		break;

	case 0xCC:
		if (flags.Z) {
			uint16_t nn = mmu.read(pc + 1);
			//cout << "CALL, " << static_cast<unsigned>(nn) << "h" << endl;
			mmu.push(pc + 3);
			//std::cout << "CALL, " << hex << nn << "h, PC: " << hex << static_cast<unsigned>(pc) << std::endl;
			pc = nn;
			addCycles(24);
		}
		else {
			pc += 3;
			addCycles(12);
		}
		break;

	case 0xD4:
		if (!flags.C) {
			uint16_t nn = mmu.read(pc + 1);
			//cout << "CALL, " << static_cast<unsigned>(nn) << "h" << endl;
			mmu.push(pc + 3);
			//std::cout << "CALL, " << hex << nn << "h, PC: " << hex << static_cast<unsigned>(pc) << std::endl;
			pc = nn;
			addCycles(24);
		}
		else {
			pc += 3;
			addCycles(12);
		}
		break;

	case 0xDC:
		if (flags.C) {
			uint16_t nn = mmu.read(pc + 1);
			//cout << "CALL, " << static_cast<unsigned>(nn) << "h" << endl;
			mmu.push(pc + 3);
			//std::cout << "CALL, " << hex << nn << "h, PC: " << hex << static_cast<unsigned>(pc) << std::endl;
			pc = nn;
			addCycles(24);
		}
		else {
			pc += 3;
			addCycles(12);
		}
		break;

	default:
		break;
	}
}

void CPU::CALL_NN(uint16_t opcode) {
	uint16_t nn = mmu.read(pc+1);
	//cout << "CALL, " << static_cast<unsigned>(nn) << "h" << endl;
	mmu.push(pc+3);
	//std::cout << "CALL, " << hex << nn << "h, PC: " << hex << static_cast<unsigned>(pc) << std::endl;
	pc = nn;
	addCycles(24);
}

void CPU::LDH_N_A() {
	uint8_t n = mmu.read8(pc + 1);
	uint16_t memoryAdress = 0xFF00 + n;
	uint8_t readedMemory = mmu.read8(memoryAdress);
	//std::cout << "Readed value from ff00: " << std::hex << static_cast<unsigned>(readedMemory) << std::endl;
	mmu.write8(memoryAdress, reg.A);
	addCycles(12);
	pc += 2;
}

void CPU::LDH_A_N() {
	uint8_t n = mmu.read8(pc + 1);
	uint16_t memoryAdress = 0xFF00 + n;
	//std::cout << "LDH A," << hex << " (" << hex << static_cast<unsigned>(memoryAdress) << "h) so A will be " << static_cast<unsigned>(mmu.read8(memoryAdress))<< std::endl;
	mmu.setRegisters8Bit(&reg, "A", mmu.read8(memoryAdress)); 
	addCycles(12);
	pc += 2;
}

void CPU::CP_N(uint16_t opcode) {
	uint8_t n;
	uint16_t nn;
	
	switch (opcode) {
	case 0xFE:
		n = mmu.read8(pc+1);
		if (reg.A == n) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}

		flags.N = true;

		if (reg.A < n) {
			flags.C = true;
		}
		else {
			flags.C = false;
		}

		flags.H = (reg.A & 0x0F) < (n & 0x0F);

		pc += 2;
		addCycles(8);
		//cout << "CP, A " << hex << static_cast<unsigned>(n) << "h" << endl;
		break;
	case 0xBF:
		n = reg.A;
		if (reg.A == n) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}

		flags.N = true;

		if (reg.A < n) {
			flags.C = true;
		}
		else {
			flags.C = false;
		}


		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		pc += 1;
		addCycles(4);
		//cout << "CP, A " << hex << static_cast<unsigned>(n) << "h" << endl;
		break;
	case 0xB8:
		n = reg.B;
		if (reg.A == n) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}

		flags.N = true;

		if (reg.A < n) {
			flags.C = true;
		}
		else {
			flags.C = false;
		}


		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		pc += 1;
		addCycles(4);
		//cout << "CP, A " << hex << static_cast<unsigned>(n) << "h" << endl;
		break;
	case 0xB9:
		n = reg.C;
		if (reg.A == n) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}

		flags.N = true;

		if (reg.A < n) {
			flags.C = true;
		}
		else {
			flags.C = false;
		}

		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		pc += 1;
		addCycles(4);
		//cout << "CP, A " << hex << static_cast<unsigned>(n) << "h" << endl;
		break;
	case 0xBA:
		n = reg.D;
		if (reg.A == n) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}

		flags.N = true;

		if (reg.A < n) {
			flags.C = true;
		}
		else {
			flags.C = false;
		}


		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		pc += 1;
		addCycles(4);
		//cout << "CP, A " << hex << static_cast<unsigned>(n) << "h" << endl;
		break;
	case 0xBB:
		n = reg.E;
		if (reg.A == n) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}

		flags.N = true;

		if (reg.A < n) {
			flags.C = true;
		}
		else {
			flags.C = false;
		}


		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		pc += 1;
		addCycles(4);
		//cout << "CP, A " << hex << static_cast<unsigned>(n) << "h" << endl;
		break;
	case 0xBC:
		n = reg.H;
		if (reg.A == n) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}

		flags.N = true;

		if (reg.A < n) {
			flags.C = true;
		}
		else {
			flags.C = false;
		}

		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		pc += 1;
		addCycles(4);
		//cout << "CP, A " << hex << static_cast<unsigned>(n) << "h" << endl;
		break;
	case 0xBD:
		n = reg.L;
		if (reg.A == n) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}

		flags.N = true;

		if (reg.A < n) {
			flags.C = true;
		}
		else {
			flags.C = false;
		}

		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		pc += 1;
		addCycles(4);
		//cout << "CP, A " << hex << static_cast<unsigned>(n) << "h" << endl;
		break;
	case 0xBE:
		n = mmu.read8(reg.HL);
		if (reg.A == n) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}

		flags.N = true;

		if (reg.A < n) {
			flags.C = true;
		}
		else {
			flags.C = false;
		}


		flags.H = (reg.A & 0x0F) < (n & 0x0F);
		pc += 1;
		addCycles(8);
		//cout << "CP, A " << hex << static_cast<unsigned>(nn) << "h" << endl;
		break;
	default:
		pc+=2;
	}

}

void CPU::JR_CC_N(uint16_t opcode) {
	int8_t n = (int8_t)mmu.read8(pc+1); 
	uint16_t nextAddress = pc + n + 2;
	//uint16_t nextAddress = pc - (0xFF - n) - 1;
	switch (opcode) {
		//JR NZ, N
	case 0x20:
		if (!flags.Z) {

				pc = nextAddress;
				addCycles(12);
				//std::cout << "PC: " << pc << endl;
			}
			else {
				pc += 2;
				addCycles(8);
			}
			//cout << "JR NZ, " << hex << pc << endl;
		
		break;
		//JR Z, N
	case 0x28: 
			if (flags.Z) {
				pc = nextAddress;
				addCycles(12);
			}
			else {
				pc += 2;
				addCycles(8);
			}
			//cout << "JR Z, " << hex << pc << endl;
		
		break;
		//JR NC, N
	case 0x30: 
			if (!flags.C) {
				pc = nextAddress;
				addCycles(12);
			}
			else {
				pc += 2;
				addCycles(8);
			}
			//cout << "JR NC, " << hex << pc << endl;
		
		break;
		//JR C, N
	case 0x38: 
			if (flags.C) {
				pc = nextAddress;
				addCycles(12);
			}
			else {
				pc += 2;
				addCycles(8);
			}
			//cout << "JR C, " << hex << pc << endl;
		
		break;

	}
}

void CPU::INC_N(uint16_t opcode) {
	flags.N = false;
	switch (opcode)
	{
		case 0x3C:
			//std::cout << "INC A" << endl;			
			if (isHalfCarry(reg.A, 1, "ADD")) {
				flags.H = true;
			}
			else {
				flags.H = false;
			}
			mmu.setRegisters8Bit(&reg, "A", reg.A + 1);
			if (reg.A == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0x04:
			//std::cout << "INC B" << endl;
			if (isHalfCarry(reg.B, 1, "ADD")) {
				flags.H = true;
			}
			else {
				flags.H = false;
			}
			mmu.setRegisters8Bit(&reg, "B", reg.B + 1);
			if (reg.B == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0x0C:
			//std::cout << "INC C" << endl;
			if (isHalfCarry(reg.C, 1, "ADD")) {
				flags.H = true;
			}
			else {
				flags.H = false;
			}
			mmu.setRegisters8Bit(&reg, "C", reg.C + 1);
			if (reg.C == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}

			addCycles(4);
			pc++;
			break;
		case 0x14:
			//std::cout << "INC D" << endl;			
			if (isHalfCarry(reg.D, 1, "ADD")) {
				flags.H = true;
			}
			else {
				flags.H = false;
			}
			mmu.setRegisters8Bit(&reg, "D", reg.D + 1);
			if (reg.D == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0x1C:
			//std::cout << "INC E" << endl;
			if (isHalfCarry(reg.E, 1, "ADD")) {
				flags.H = true;
			}
			else {
				flags.H = false;
			}
			mmu.setRegisters8Bit(&reg, "E", reg.E + 1);
			if (reg.E == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0x24:
			//std::cout << "INC H" << endl;			
			if (isHalfCarry(reg.H, 1, "ADD")) {
				flags.H = true;
			}
			else {
				flags.H = false;
			}
			mmu.setRegisters8Bit(&reg, "H", reg.H + 1);
			if (reg.H == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0x2C:
			//std::cout << "INC L" << endl;
			if (isHalfCarry(reg.L, 1, "ADD")) {
				flags.H = true;
			}
			else {
				flags.H = false;
			}
			mmu.setRegisters8Bit(&reg, "L", reg.L + 1);
			if (reg.L == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(4);
			pc++;
			break;
		case 0x34:
			//std::cout << "INC (HL)" << endl;
			if (isHalfCarry(mmu.read8(reg.HL), 1, "ADD")) {
				flags.H = true;
			}
			else {
				flags.H = false;
			}
			mmu.write8(reg.HL, mmu.read8(reg.HL) + 1);
			if (mmu.read8(reg.HL) == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(12);
			pc++;
			break;
		default:
			break;
	}
}

void CPU::DEC_N(uint16_t opcode) {

	flags.N = true;
	switch (opcode)
	{
	case 0x3D:
		//std::cout << "DEC A" << endl;
		if (isHalfCarry(reg.A, 1, "SUB")) {
			flags.H = true;
		}
		else {
			flags.H = false;
		}
		mmu.setRegisters8Bit(&reg, "A", reg.A - 1);
		if (reg.A == 0) {
			flags.Z = true;
		}else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0x05:
		//std::cout << "DEC B" << endl;

		if (isHalfCarry(reg.B, 1, "SUB")) {
			flags.H = true;
		}
		else {
			flags.H = false;
		}
		mmu.setRegisters8Bit(&reg, "B", reg.B - 1);
		if (reg.B == 0) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0x0D:
		//std::cout << "DEC C" << endl;
		if (isHalfCarry(reg.C, 1, "SUB")) {
			flags.H = true;
		}
		else {
			flags.H = false;
		}
		mmu.setRegisters8Bit(&reg, "C", reg.C - 1);
		if (reg.C == 0) {
			flags.Z = true;
		}else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0x15:
		//std::cout << "DEC D" << endl;
		if (isHalfCarry(reg.D, 1, "SUB")) {
			flags.H = true;
		}
		else {
			flags.H = false;
		}
		mmu.setRegisters8Bit(&reg, "D", reg.D - 1);
		if (reg.D == 0) {
			flags.Z = true;
		}else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0x1D:
		//std::cout << "DEC E" << endl;
		if (isHalfCarry(reg.E, 1, "SUB")) {
			flags.H = true;
		}
		else {
			flags.H = false;
		}
		mmu.setRegisters8Bit(&reg, "E", reg.E - 1);
		if (reg.E == 0) {
			flags.Z = true;
		}else {
			flags.Z = false;
		}

		addCycles(4);
		pc++;
		break;
	case 0x25:
		//std::cout << "DEC H" << endl;
		if (isHalfCarry(reg.H, 1, "SUB")) {
			flags.H = true;
		}
		else {
			flags.H = false;
		}

		mmu.setRegisters8Bit(&reg, "H", reg.H - 1);
		if (reg.H == 0) {
			flags.Z = true;
		}else {
			flags.Z = false;
		}

		addCycles(4);
		pc++;
		break;
	case 0x2D:
		//std::cout << "DEC L" << endl;

		if (isHalfCarry(reg.L, 1, "SUB")) {
			flags.H = true;
		}
		else {
			flags.H = false;
		}

		mmu.setRegisters8Bit(&reg, "L", reg.L - 1);
		if (reg.L == 0) {
			flags.Z = true;
		}else {
			flags.Z = false;
		}
		addCycles(4);
		pc++;
		break;
	case 0x35:
		//std::cout << "DEC (HL)" << endl;		
		if (isHalfCarry(mmu.read8(reg.HL), 1, "SUB")) {
			flags.H = true;
		}
		else {
			flags.H = false;
		}
		mmu.write8(reg.HL, mmu.read8(reg.HL) - 1);
		if (mmu.read8(reg.HL) == 0) {
			flags.Z = true;
		}
		else {
			flags.Z = false;
		}
		addCycles(12);
		pc++;
		break;
	default:
		break;
	}
}

void CPU::LDI_regHL_A() {
	//std::cout << "LDI (HL), A" << endl;
	mmu.write8(reg.HL, reg.A);
	mmu.setRegisters16Bit(&reg, "HL", reg.HL + 1);
	addCycles(8);
	pc++;
}

void CPU::LDI_A_regHL() {
	//std::cout << "LDI A, (HL)" << endl;
	mmu.setRegisters8Bit(&reg, "A", mmu.read8(reg.HL));
	mmu.setRegisters16Bit(&reg, "HL", reg.HL + 1);
	addCycles(8);
	pc++;
}

void CPU::RST_N(uint16_t opcode) {
	switch (opcode)
	{
		case 0xC7:
			mmu.push(pc+1);
			pc = 0x0000;
			addCycles(16);
			break;
		case 0xCF:
			mmu.push(pc + 1);
			pc = 0x0008;
			addCycles(16);
			break;
		case 0xD7:
			mmu.push(pc + 1);
			pc = 0x0010;
			addCycles(16);
			break;
		case 0xDF:
			mmu.push(pc + 1);
			pc = 0x0018;
			addCycles(16);
			break;
		case 0xE7:
			mmu.push(pc + 1);
			pc = 0x0020;
			addCycles(16);
			break;
		case 0xEF:
			mmu.push(pc + 1);
			pc = 0x0028;
			addCycles(16);
			break;
		case 0xF7:
			mmu.push(pc + 1);
			pc = 0x0030;
			addCycles(16);
			break;
		case 0xFF:
			mmu.push(pc + 1);
			pc = 0x0038;
			addCycles(16);
			break;
	default:
		break;
	}
}

void CPU::LD_regC_A() {
	uint16_t n = 0xFF00 + reg.C;
	mmu.write8(n, reg.A);
	pc++;
	addCycles(8);
}

void CPU::DEC_NN(uint16_t opcode) {
	switch (opcode)
	{
		case 0x0B:
			mmu.setRegisters16Bit(&reg, "BC", reg.BC - 1);
			pc++;
			addCycles(8);
			break;
		case 0x1B:
			mmu.setRegisters16Bit(&reg, "DE", reg.DE - 1);
			pc++;
			addCycles(8);
			break;
		case 0x2B:
			mmu.setRegisters16Bit(&reg, "HL", reg.HL - 1);
			pc++;
			addCycles(8);
			break;
		case 0x3B:
			mmu.sp-=1;
			pc++;
			addCycles(8);
			break;
	default:
		break;
	}
}

void CPU::CPL() {
	uint8_t registerA = reg.A;
	uint8_t regAFlipped = ~registerA;
	mmu.setRegisters8Bit(&reg, "A", regAFlipped);	
	addCycles(4);
	flags.N = true;
	flags.H = true;
	pc++;
}

void CPU::CB_SWAP_N(uint16_t opcode) {
	uint8_t swappedNibles;

	flags.N = false;
	flags.H = false;
	flags.C = false;

	switch (opcode)
	{
		case 0x37:
			swappedNibles = swapNibbles(reg.A);
			mmu.setRegisters8Bit(&reg, "A", swappedNibles);
			if (swappedNibles == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(8);
			pc+=2;
			break;
		case 0x30:
			swappedNibles = swapNibbles(reg.B);
			mmu.setRegisters8Bit(&reg, "B", swappedNibles);
			if (swappedNibles == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(8);
			pc += 2;
			break;
		case 0x31:
			swappedNibles = swapNibbles(reg.C);
			mmu.setRegisters8Bit(&reg, "C", swappedNibles);
			if (swappedNibles == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(8);
			pc += 2;
			break;
		case 0x32:
			swappedNibles = swapNibbles(reg.D);
			mmu.setRegisters8Bit(&reg, "D", swappedNibles);
			if (swappedNibles == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(8);
			pc += 2;
			break;
		case 0x33:
			swappedNibles = swapNibbles(reg.E);
			mmu.setRegisters8Bit(&reg, "E", swappedNibles);
			if (swappedNibles == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(8);
			pc += 2;
			break;
		case 0x34:
			swappedNibles = swapNibbles(reg.H);
			mmu.setRegisters8Bit(&reg, "H", swappedNibles);
			if (swappedNibles == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(8);
			pc += 2;
			break;
		case 0x35:
			swappedNibles = swapNibbles(reg.L);
			mmu.setRegisters8Bit(&reg, "L", swappedNibles);
			if (swappedNibles == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(8);
			pc += 2;
			break;
		case 0x36:
			swappedNibles = swapNibbles(mmu.read8(reg.HL));
			mmu.write8(reg.HL, swappedNibles);
			if (swappedNibles == 0) {
				flags.Z = true;
			}
			else {
				flags.Z = false;
			}
			addCycles(16);
			pc += 2;
			break;
	default:
		break;
	}
}

void CPU::ADD_A_N(uint16_t opcode){
	uint8_t n;
	uint16_t result;
	switch (opcode)
	{
		case 0x80:
			n = reg.B;
			result = reg.A + n;
			flags.H = ((reg.A & 0x0F) + (n & 0x0F)) > 0x0F;
			flags.Z = ((result & 0xFF) == 0);
			flags.C = result > 0xFF;
			flags.N = false;

			mmu.setRegisters8Bit(&reg, "A", (uint8_t)(result & 0xFF));

			addCycles(4);
			pc++;
			break;
		case 0x81:
			n = reg.C;
			result = reg.A + n;
			flags.H = ((reg.A & 0x0F) + (n & 0x0F)) > 0x0F;
			flags.Z = ((result & 0xFF) == 0);
			flags.C = result > 0xFF;
			flags.N = false;

			mmu.setRegisters8Bit(&reg, "A", (uint8_t)(result & 0xFF));

			addCycles(4);
			pc++;
			break;
		case 0x82:
			n = reg.D;
			result = reg.A + n;
			flags.H = ((reg.A & 0x0F) + (n & 0x0F)) > 0x0F;
			flags.Z = ((result & 0xFF) == 0);
			flags.C = result > 0xFF;
			flags.N = false;

			mmu.setRegisters8Bit(&reg, "A", (uint8_t)(result & 0xFF));

			addCycles(4);
			pc++;
			break;
		case 0x83:
			n = reg.E;
			result = reg.A + n;
			flags.H = ((reg.A & 0x0F) + (n & 0x0F)) > 0x0F;
			flags.Z = ((result & 0xFF) == 0);
			flags.C = result > 0xFF;
			flags.N = false;

			mmu.setRegisters8Bit(&reg, "A", (uint8_t)(result & 0xFF));

			addCycles(4);
			pc++;
			break;
		case 0x84:
			n = reg.H;
			result = reg.A + n;
			flags.H = ((reg.A & 0x0F) + (n & 0x0F)) > 0x0F;
			flags.Z = ((result & 0xFF) == 0);
			flags.C = result > 0xFF;
			flags.N = false;

			mmu.setRegisters8Bit(&reg, "A", (uint8_t)(result & 0xFF));

			addCycles(4);
			pc++;
			break;
		case 0x85:
			n = reg.L;
			result = reg.A + n;
			flags.H = ((reg.A & 0x0F) + (n & 0x0F)) > 0x0F;
			flags.Z = ((result & 0xFF) == 0);
			flags.C = result > 0xFF;
			flags.N = false;

			mmu.setRegisters8Bit(&reg, "A", (uint8_t)(result & 0xFF));

			addCycles(4);
			pc++;
			break;
		case 0x86:
			n = mmu.read8(reg.HL);
			result = reg.A + n;
			flags.H = ((reg.A & 0x0F) + (n & 0x0F)) > 0x0F;
			flags.Z = ((result & 0xFF) == 0);
			flags.C = result > 0xFF;
			flags.N = false;

			mmu.setRegisters8Bit(&reg, "A", (uint8_t)(result & 0xFF));

			addCycles(8);
			pc++;
			break;
		case 0x87:
			n = reg.A;
			result = reg.A + n;
			flags.H = ((reg.A & 0x0F) + (n & 0x0F)) > 0x0F;
			flags.Z = ((result & 0xFF) == 0);
			flags.C = result > 0xFF;
			flags.N = false;

			mmu.setRegisters8Bit(&reg, "A", (uint8_t)(result & 0xFF));

			addCycles(4);
			pc++;
			break;
		case 0xC6:
			n = mmu.read8(pc+1);
			result = reg.A + n;
			flags.H = ((reg.A & 0x0F) + (n & 0x0F)) > 0x0F;
			flags.Z = ((result & 0xFF) == 0);
			flags.C = result > 0xFF;
			flags.N = false;

			mmu.setRegisters8Bit(&reg, "A", (uint8_t)(result & 0xFF));

			addCycles(8);
			pc+=2;
			break;
	default:
		break;
	}
}

void CPU::ADC_A_N(uint16_t opcode) {
	int n;
	int is_carry = flags.C ? 1 : 0;

	switch (opcode)
	{
		case 0x88:
			n = reg.A + reg.B + is_carry;
			flags.H = ((reg.A & 0xF) + (reg.B & 0xF) + is_carry) > 0xF;
			flags.C = n > 0xFF;
			flags.Z = ((n & 0xFF) == 0);
			flags.N = false;
			mmu.setRegisters8Bit(&reg, "A", (uint8_t)n);
			addCycles(4);
			pc++;
			break;
		case 0x89:
			n = reg.A + reg.C + is_carry;
			flags.H = ((reg.A & 0xF) + (reg.C & 0xF) + is_carry) > 0xF;
			flags.C = n > 0xFF;
			flags.Z = ((n & 0xFF) == 0);
			flags.N = false;
			mmu.setRegisters8Bit(&reg, "A", (uint8_t)n);
			addCycles(4);
			pc++;
			break;
		case 0x8A:
			n = reg.A + reg.D + is_carry;
			flags.H = ((reg.A & 0xF) + (reg.D & 0xF) + is_carry) > 0xF;
			flags.C = n > 0xFF;
			flags.Z = ((n & 0xFF) == 0);
			flags.N = false;
			mmu.setRegisters8Bit(&reg, "A", (uint8_t)n);
			addCycles(4);
			pc++;
			break;
		case 0x8B:
			n = reg.A + reg.E + is_carry;
			flags.H = ((reg.A & 0xF) + (reg.E & 0xF) + is_carry) > 0xF;
			flags.C = n > 0xFF;
			flags.Z = ((n & 0xFF) == 0);
			flags.N = false;
			mmu.setRegisters8Bit(&reg, "A", (uint8_t)n);
			addCycles(4);
			pc++;
			break;
		case 0x8C:
			n = reg.A + reg.H + is_carry;
			flags.H = ((reg.A & 0xF) + (reg.H & 0xF) + is_carry) > 0xF;
			flags.C = n > 0xFF;
			flags.Z = ((n & 0xFF) == 0);
			flags.N = false;
			mmu.setRegisters8Bit(&reg, "A", (uint8_t)n);
			addCycles(4);
			pc++;
			break;
		case 0x8D:
			n = reg.A + reg.L + is_carry;
			flags.H = ((reg.A & 0xF) + (reg.L & 0xF) + is_carry) > 0xF;
			flags.C = n > 0xFF;
			flags.Z = ((n & 0xFF) == 0);
			flags.N = false;
			mmu.setRegisters8Bit(&reg, "A", (uint8_t)n);
			addCycles(4);
			pc++;
			break;
		case 0x8E:
			n = reg.A + mmu.read8(reg.HL) + is_carry;
			flags.H = ((reg.A & 0xF) + (mmu.read8(reg.HL) & 0xF) + is_carry) > 0xF;
			flags.C = n > 0xFF;
			flags.Z = ((n & 0xFF) == 0);
			flags.N = false;
			mmu.setRegisters8Bit(&reg, "A", (uint8_t)n);
			addCycles(8);
			pc++;
			break;
		case 0x8F:
			n = reg.A + reg.A + is_carry;
			flags.H = ((reg.A & 0xF) + (reg.A & 0xF) + is_carry) > 0xF;
			flags.C = n > 0xFF;
			flags.Z = ((n & 0xFF) == 0);
			flags.N = false;
			mmu.setRegisters8Bit(&reg, "A", (uint8_t)n);
			addCycles(4);
			pc++;
			break;
		case 0xCE:
			n = reg.A + mmu.read8(pc+1) + is_carry;
			flags.H = ((reg.A & 0xF) + (mmu.read8(pc + 1) & 0xF) + is_carry) > 0xF;
			flags.C = n > 0xFF;
			flags.Z = ((n & 0xFF) == 0);
			flags.N = false;
			mmu.setRegisters8Bit(&reg, "A", (uint8_t)n);
			addCycles(8);
			pc+=2;
			break;
	default:
		break;
	}
}

void CPU::POP_NN(uint16_t opcode) {
	uint16_t nn;
	switch (opcode)
	{
		case 0xF1:
			mmu.pop(&nn);
			//std::cout << "Poping values from stack to AF register: " << std::hex << static_cast<unsigned>(nn) << std::endl;
			mmu.setRegisters16Bit(&reg, "AF", nn);

			flags.Z = (reg.F & 0x80) != 0;
			flags.N = (reg.F & 0x40) != 0;
			flags.H = (reg.F & 0x20) != 0;
			flags.C = (reg.F & 0x10) != 0;

			pc++;
			addCycles(12);
			break;
		case 0xC1:
			mmu.pop(&nn);
			mmu.setRegisters16Bit(&reg, "BC", nn);
			pc++;
			addCycles(12);
			break;
		case 0xD1:
			mmu.pop(&nn);
			mmu.setRegisters16Bit(&reg, "DE", nn);
			//std::cout << "POP HL, PC value is: " << hex << static_cast<unsigned>(pc) << ", Value is: " << hex << static_cast<unsigned>(nn) << std::endl;
			pc++;
			addCycles(12);
			break;
		case 0xE1:
			mmu.pop(&nn);
			mmu.setRegisters16Bit(&reg, "HL", nn);
			//std::cout << "POP HL, PC value is: " << hex << static_cast<unsigned>(pc) << ", Value is: " << hex << static_cast<unsigned>(nn) << std::endl;
			pc++;
			addCycles(12);
			break;
	default:
		break;
	}
}

void CPU::PUSH_NN(uint16_t opcode) {
	uint16_t nn;
	switch (opcode)
	{
	case 0xF5:
		//std::cout << "Pushing AF: " << hex << static_cast<unsigned>(reg.AF) << std::endl;
		nn = (flags.Z ? 0x80 : 0) |
			(flags.N ? 0x40 : 0) |
			(flags.H ? 0x20 : 0) |
			(flags.C ? 0x10 : 0);
		nn &= 0xF0;
		mmu.setRegisters8Bit(&reg, "F", nn);
		mmu.push(reg.AF);
		pc++;
		addCycles(16);
		break;
	case 0xC5:
		//std::cout << "Pushing BC: " << hex << static_cast<unsigned>(reg.BC) << std::endl;
		mmu.push(reg.BC);
		pc++;
		addCycles(16);
		break;
	case 0xD5:
		//std::cout << "Pushing DE: " << hex << static_cast<unsigned>(reg.DE) << std::endl;
		mmu.push(reg.DE);
		pc++;
		addCycles(16);
		break;
	case 0xE5:
		//std::cout << "Pushing HL: " << hex << static_cast<unsigned>(reg.HL) << std::endl;
		mmu.push(reg.HL);
		pc++;
		addCycles(16);
		break;
	default:
		break;
	}
}

void CPU::LD_SP_HL() {
	mmu.sp = reg.HL;
	pc++;
	addCycles(8);
}

void CPU::LD_NN_SP() {
	uint16_t nn = mmu.read(pc + 1);
	mmu.write(nn, mmu.sp);
	pc += 3;
	addCycles(20);
}

void CPU::ADD_HL_N(uint16_t opcode) {
	uint32_t nn;
	switch (opcode)
	{
		case 0x09:
			nn = (uint32_t)reg.HL + (uint32_t)reg.BC;
			flags.H = ((reg.HL & 0x0FFF) + (mmu.sp & 0x0FFF)) > 0x0FFF;
			flags.C = (nn > 0xFFFF);
			flags.N = false;
			mmu.setRegisters16Bit(&reg, "HL", nn);
			addCycles(8);
			pc++;
			break;
		case 0x19:
			nn = (uint32_t)reg.HL + (uint32_t)reg.DE;
			flags.H = ((reg.HL & 0x0FFF) + (mmu.sp & 0x0FFF)) > 0x0FFF;
			flags.C = (nn > 0xFFFF);
			flags.N = false;
			mmu.setRegisters16Bit(&reg, "HL", nn);
			addCycles(8);
			pc++;
			break;
		case 0x29:
			nn = (uint32_t)reg.HL + (uint32_t)reg.HL;
			flags.H = ((reg.HL & 0x0FFF) + (mmu.sp & 0x0FFF)) > 0x0FFF;
			flags.C = (nn > 0xFFFF);
			flags.N = false;
			mmu.setRegisters16Bit(&reg, "HL", nn);
			addCycles(8);
			pc++;
			break;
		case 0x39:
			nn = (uint32_t)reg.HL + (uint32_t)mmu.sp;
			flags.H = ((reg.HL & 0x0FFF) + (mmu.sp & 0x0FFF)) > 0x0FFF;
			flags.C = (nn > 0xFFFF);
			flags.N = false;
			mmu.setRegisters16Bit(&reg, "HL", nn);
			addCycles(8);
			pc++;
			break;
	default:
		break;
	}
}

void CPU::ADD_SP_N() {
	int8_t n = (int8_t)mmu.read8(pc + 1);

	// ERROR ESTABA AQUI: No hagas cast a (uint8_t) aquí. Usa todo el SP.
	int result = (uint16_t)mmu.sp + n;

	// Para los flags, usamos variables temporales para limpieza
	uint16_t sp_val = mmu.sp;

	// Casting a uint8_t en 'n' es importante para evitar problemas de signo en la suma de bits
	// H: Acarreo del bit 3
	flags.H = ((sp_val & 0x0F) + ((uint8_t)n & 0x0F)) > 0x0F;

	// C: Acarreo del bit 7
	flags.C = ((sp_val & 0xFF) + ((uint8_t)n & 0xFF)) > 0xFF;

	flags.Z = false;
	flags.N = false;

	// Actualizamos SP con el resultado de 16 bits
	mmu.sp = (uint16_t)result;

	addCycles(16);
	pc += 2;
}

void CPU::INC_NN(uint16_t opcode) {
	switch (opcode) {
		case 0x03:
			mmu.setRegisters16Bit(&reg, "BC", reg.BC + 1);
			addCycles(8);
			pc++;
			break;
		case 0x13:
			mmu.setRegisters16Bit(&reg, "DE", reg.DE + 1);
			addCycles(8);
			pc++;
			break;
		case 0x23:
			mmu.setRegisters16Bit(&reg, "HL", reg.HL + 1);
			addCycles(8);
			pc++;
			break;
		case 0x33:
			mmu.sp+=1;
			addCycles(8);
			pc++;
			break;
	}

}

void CPU::RES_B_R(uint16_t opcode) {
	uint8_t n;
	switch (opcode)
	{
		case 0x80:
			n = reg.B & ~(1 << 0);
			mmu.setRegisters8Bit(&reg, "B", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x81:
			n = reg.C & ~(1 << 0);
			mmu.setRegisters8Bit(&reg, "C", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x82:
			n = reg.D & ~(1 << 0);
			mmu.setRegisters8Bit(&reg, "D", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x83:
			n = reg.E & ~(1 << 0);
			mmu.setRegisters8Bit(&reg, "E", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x84:
			n = reg.H & ~(1 << 0);
			mmu.setRegisters8Bit(&reg, "H", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x85:
			n = reg.L & ~(1 << 0);
			mmu.setRegisters8Bit(&reg, "L", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x86:
			n = mmu.read8(reg.HL) & ~(1 << 0);
			mmu.write8(reg.HL, n);
			pc+=2;
			addCycles(16);
			break;
		case 0x87:
			n = (reg.A & ~(1 << 0));
			//std::cout << "Reseting bit 0 of A: " << hex << static_cast<unsigned>(reg.A) << " with reg.A & ~(1 << 0): " << hex << static_cast<unsigned>(n) << std::endl;
			mmu.setRegisters8Bit(&reg, "A", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x88:
			n = reg.B & ~(1 << 1);
			mmu.setRegisters8Bit(&reg, "B", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x89:
			n = reg.C & ~(1 << 1);
			mmu.setRegisters8Bit(&reg, "C", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x8A:
			n = reg.D & ~(1 << 1);
			mmu.setRegisters8Bit(&reg, "D", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x8B:
			n = reg.E & ~(1 << 1);
			mmu.setRegisters8Bit(&reg, "E", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x8C:
			n = reg.H & ~(1 << 1);
			mmu.setRegisters8Bit(&reg, "H", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x8D:
			n = reg.L & ~(1 << 1);
			mmu.setRegisters8Bit(&reg, "L", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x8E:
			n = mmu.read8(reg.HL) & ~(1 << 0);
			mmu.write8(reg.HL, n);
			pc+=2;
			addCycles(16);
			break;
		case 0x8F:
			n = reg.A & ~(1 << 1);
			mmu.setRegisters8Bit(&reg, "A", n);
			pc+=2;
			addCycles(8);
			break;		
		case 0x90:
			n = reg.B & ~(1 << 2);
			mmu.setRegisters8Bit(&reg, "B", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x91:
			n = reg.C & ~(1 << 2);
			mmu.setRegisters8Bit(&reg, "C", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x92:
			n = reg.D & ~(1 << 2);
			mmu.setRegisters8Bit(&reg, "D", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x93:
			n = reg.E & ~(1 << 2);
			mmu.setRegisters8Bit(&reg, "E", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x94:
			n = reg.H & ~(1 << 2);
			mmu.setRegisters8Bit(&reg, "H", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x95:
			n = reg.L & ~(1 << 2);
			mmu.setRegisters8Bit(&reg, "L", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x96:
			n = mmu.read8(reg.HL) & ~(1 << 2);
			mmu.write8(reg.HL, n);
			pc+=2;
			addCycles(16);
			break;
		case 0x97:
			n = reg.A & ~(1 << 2);
			mmu.setRegisters8Bit(&reg, "A", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x98:
			n = reg.B & ~(1 << 3);
			mmu.setRegisters8Bit(&reg, "B", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x99:
			n = reg.C & ~(1 << 3);
			mmu.setRegisters8Bit(&reg, "C", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x9A:
			n = reg.D & ~(1 << 3);
			mmu.setRegisters8Bit(&reg, "D", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x9B:
			n = reg.E & ~(1 << 3);
			mmu.setRegisters8Bit(&reg, "E", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x9C:
			n = reg.H & ~(1 << 3);
			mmu.setRegisters8Bit(&reg, "H", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x9D:
			n = reg.L & ~(1 << 3);
			mmu.setRegisters8Bit(&reg, "L", n);
			pc+=2;
			addCycles(8);
			break;
		case 0x9E:
			n = mmu.read8(reg.HL) & ~(1 << 3);
			mmu.write8(reg.HL, n);
			pc+=2;
			addCycles(16);
			break;
		case 0x9F:
			n = reg.A & ~(1 << 3);
			mmu.setRegisters8Bit(&reg, "A", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xA0:
			n = reg.B & ~(1 << 4);
			mmu.setRegisters8Bit(&reg, "B", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xA1:
			n = reg.C & ~(1 << 4);
			mmu.setRegisters8Bit(&reg, "C", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xA2:
			n = reg.D & ~(1 << 4);
			mmu.setRegisters8Bit(&reg, "D", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xA3:
			n = reg.E & ~(1 << 4);
			mmu.setRegisters8Bit(&reg, "E", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xA4:
			n = reg.H & ~(1 << 4);
			mmu.setRegisters8Bit(&reg, "H", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xA5:
			n = reg.L & ~(1 << 4);
			mmu.setRegisters8Bit(&reg, "L", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xA6:
			n = mmu.read8(reg.HL) & ~(1 << 4);
			mmu.write8(reg.HL, n);
			pc+=2;
			addCycles(16);
			break;
		case 0xA7:
			n = reg.A & ~(1 << 4);
			mmu.setRegisters8Bit(&reg, "A", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xA8:
			n = reg.B & ~(1 << 5);
			mmu.setRegisters8Bit(&reg, "B", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xA9:
			n = reg.C & ~(1 << 5);
			mmu.setRegisters8Bit(&reg, "C", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xAA:
			n = reg.D & ~(1 << 5);
			mmu.setRegisters8Bit(&reg, "D", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xAB:
			n = reg.E & ~(1 << 5);
			mmu.setRegisters8Bit(&reg, "E", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xAC:
			n = reg.H & ~(1 << 5);
			mmu.setRegisters8Bit(&reg, "H", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xAD:
			n = reg.L & ~(1 << 5);
			mmu.setRegisters8Bit(&reg, "L", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xAE:
			n = mmu.read8(reg.HL) & ~(1 << 5);
			mmu.write8(reg.HL, n);
			pc+=2;
			addCycles(16);
			break;
		case 0xAF:
			n = reg.A & ~(1 << 5);
			mmu.setRegisters8Bit(&reg, "A", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xB0:
			n = reg.B & ~(1 << 6);
			mmu.setRegisters8Bit(&reg, "B", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xB1:
			n = reg.C & ~(1 << 6);
			mmu.setRegisters8Bit(&reg, "C", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xB2:
			n = reg.D & ~(1 << 6);
			mmu.setRegisters8Bit(&reg, "D", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xB3:
			n = reg.E & ~(1 << 6);
			mmu.setRegisters8Bit(&reg, "E", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xB4:
			n = reg.H & ~(1 << 6);
			mmu.setRegisters8Bit(&reg, "H", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xB5:
			n = reg.L & ~(1 << 6);
			mmu.setRegisters8Bit(&reg, "L", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xB6:
			n = mmu.read8(reg.HL) & ~(1 << 6);
			mmu.write8(reg.HL, n);
			pc+=2;
			addCycles(16);
			break;
		case 0xB7:
			n = reg.A & ~(1 << 6);
			mmu.setRegisters8Bit(&reg, "A", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xB8:
			n = reg.B & ~(1 << 7);
			mmu.setRegisters8Bit(&reg, "B", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xB9:
			n = reg.C & ~(1 << 7);
			mmu.setRegisters8Bit(&reg, "C", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xBA:
			n = reg.D & ~(1 << 7);
			mmu.setRegisters8Bit(&reg, "D", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xBB:
			n = reg.E & ~(1 << 7);
			mmu.setRegisters8Bit(&reg, "E", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xBC:
			n = reg.H & ~(1 << 7);
			mmu.setRegisters8Bit(&reg, "H", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xBD:
			n = reg.L & ~(1 << 7);
			mmu.setRegisters8Bit(&reg, "L", n);
			pc+=2;
			addCycles(8);
			break;
		case 0xBE:
			n = mmu.read8(reg.HL) & ~(1 << 7);
			mmu.write8(reg.HL, n);
			pc+=2;
			addCycles(16);
			break;
		case 0xBF:
			n = reg.A & ~(1 << 7);
			mmu.setRegisters8Bit(&reg, "A", n);
			pc+=2;
			addCycles(8);
			break;
	default:
		break;
	}
}

void CPU::RET_CC(uint16_t opcode) {
	uint16_t addrPoped;
	switch (opcode)
	{
	case 0xC0:
		if (!flags.Z) {
			mmu.pop(&addrPoped);
			pc = addrPoped;
			addCycles(20);
		}
		else {
			pc++;
			addCycles(8);
		}
		break;

	case 0xC8:
		if (flags.Z) {
			mmu.pop(&addrPoped);
			pc = addrPoped;
			addCycles(20);
		}
		else {
			pc++;
			addCycles(8);
		}
		break;

	case 0xD0:
		if (!flags.C) {
			mmu.pop(&addrPoped);
			pc = addrPoped;
			addCycles(20);
		}
		else {
			pc++;
			addCycles(8);
		}
		break;

	case 0xD8:
		if (flags.C) {
			mmu.pop(&addrPoped);
			pc = addrPoped;
			addCycles(20);
		}
		else {
			pc++;
			addCycles(8);
		}
		break;

	default:
		break;
	}
}

GameboyFlags *CPU::getFlagState() {
	return &flags;
}

GameboyRegisters *CPU::getGameboyRegisters() {
	return &reg;
}

//Load the game into memory
void CPU::loadGame(const char* path) {
	//Holds the game
	ifstream game;
	game.open(path, ifstream::binary);
	
	//Set pos at the end
	game.seekg(0, ifstream::end);
	
	//Holds the size of the game
	int gamesize = game.tellg();
	cout << "Size is: " << gamesize << endl;
	
	//Set pos at the beginning
	game.seekg(0, ifstream::beg);
	char* tempGame = new char[gamesize];

	//Dump content into the buffer we created
	game.read(tempGame, gamesize);
	
	//Now dump the content in memory space desired
	for (int x = 0; x < gamesize; x++) {
		mmu.rom[x] = tempGame[x];
		//std::cout << "Address is " << x << ", Value of that is: " << hex << static_cast<unsigned>(mmu.rom[x]) << endl;
	}
	std::cout << "Game loaded into memory" << endl;
	game.close();
}


void CPU::loadBIOS() {
	const uint8_t bios[0x100] = {
	0x31, 0xFE, 0xFF, 0xAF, 0x21, 0xFF, 0x9F, 0x32, 0xCB, 0x7C, 0x20, 0xFB, 0x21, 0x26, 0xFF, 0x0E,
	0x11, 0x3E, 0x80, 0x32, 0xE2, 0x0C, 0x3E, 0xF3, 0xE2, 0x32, 0x3E, 0x77, 0x77, 0x3E, 0xFC, 0xE0,
	0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 0xCD, 0x95, 0x00, 0xCD, 0x96, 0x00, 0x13, 0x7B,
	0xFE, 0x34, 0x20, 0xF3, 0x11, 0xD8, 0x00, 0x06, 0x08, 0x1A, 0x13, 0x22, 0x23, 0x05, 0x20, 0xF9,
	0x3E, 0x19, 0xEA, 0x10, 0x99, 0x21, 0x2F, 0x99, 0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20,
	0xF9, 0x2E, 0x0F, 0x18, 0xF3, 0x67, 0x3E, 0x64, 0x57, 0xE0, 0x42, 0x3E, 0x91, 0xE0, 0x40, 0x04,
	0x1E, 0x02, 0x0E, 0x0C, 0xF0, 0x44, 0xFE, 0x90, 0x20, 0xFA, 0x0D, 0x20, 0xF7, 0x1D, 0x20, 0xF2,
	0x0E, 0x13, 0x24, 0x7C, 0x1E, 0x83, 0xFE, 0x62, 0x28, 0x06, 0x1E, 0xC1, 0xFE, 0x64, 0x20, 0x06,
	0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xF2, 0xF0, 0x42, 0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20,
	0x4F, 0x16, 0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17,
	0x05, 0x20, 0xF5, 0x22, 0x23, 0x22, 0x23, 0xC9, 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
	0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
	0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
	0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 0x3c, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x4C,
	0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x20, 0xFE, 0x23, 0x7D, 0xFE, 0x34, 0x20,
	0xF5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x20, 0xFE, 0x3E, 0x01, 0xE0, 0x50
	};

	//Dump into the 256 byte rom memory of gameboy
	for (int x = 0; x < 0x100; x++) {
		mmu.bios[x] = bios[x];
	}
}