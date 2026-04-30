#include "GPU.h"
#include <cstring>

/*
0xFF40 Memory Address:
Bit 7 - LCD Display Enable(0 = Off, 1 = On)
Bit 6 - Window Tile Map Display Select(0 = 9800 - 9BFF, 1 = 9C00 - 9FFF)
Bit 5 - Window Display Enable(0 = Off, 1 = On)
Bit 4 - BG & Window Tile Data Select(0 = 8800 - 97FF, 1 = 8000 - 8FFF)
Bit 3 - BG Tile Map Display Select(0 = 9800 - 9BFF, 1 = 9C00 - 9FFF)
Bit 2 - OBJ(Sprite) Size(0 = 8x8, 1 = 8x16)
Bit 1 - OBJ(Sprite) Display Enable(0 = Off, 1 = On)
Bit 0 - BG Display(for CGB see below) (0 = Off, 1 = On)
*/

bool GPU::isKthBitSet(uint8_t n, uint8_t k) {
	if (n & (1 << k)) {
		return true;
	}
	else {
		return false;
	}
}

uint8_t GPU::BitGetVal(uint8_t valueToGet, uint8_t bitToDisplace) {
	uint8_t lMsk = 1 << bitToDisplace;
	return (valueToGet & lMsk) ? 1 : 0;
}

int GPU::SDL_CalculatePitch(Uint32 format, int width)
{
	int pitch;

	if (SDL_ISPIXELFORMAT_FOURCC(format) || SDL_BITSPERPIXEL(format) >= 8) {
		pitch = (width * SDL_BYTESPERPIXEL(format));
	}
	else {
		pitch = ((width * SDL_BITSPERPIXEL(format)) + 7) / 8;
	}
	pitch = (pitch + 3) & ~3;   /* 4-byte aligning for speed */
	return pitch;
}

uint8_t GPU::getSCX(MMU *mmu) {
	return mmu->read8(0xFF42);
}

uint8_t GPU::getSCY(MMU* mmu) {
	return mmu->read8(0xFF43);
}

bool GPU::isSpriteBig(MMU* mmu) {
	uint8_t n = mmu->read8(0xFF40);
	return isKthBitSet(n, 2);
}

bool GPU::isScreenEnabled(MMU *mmu) {
	uint8_t n = mmu->read8(0xFF40);
	return isKthBitSet(n, 7);
}

bool GPU::bgUsed(MMU* mmu) {
	uint8_t n = mmu->read8(0xFF40);
	return isKthBitSet(n, 3);
}

uint8_t GPU::clearBit(uint8_t value, uint8_t bitToReset) {
	uint8_t bitCleared = value & ~(1 << bitToReset);

	return bitCleared;
}

uint8_t GPU::setBit(uint8_t value, uint8_t bitToSet) {
	uint8_t bitSet = value | (1 << bitToSet);

	return bitSet;
}

void GPU::changeModeGPU(MMU* mmu, uint8_t gpuMode, Interrupt* interr) {
	mode = gpuMode;

	// 1. Leemos el valor actual del registro STAT
	uint8_t lcdStatValue = mmu->read8(0xFF41);

	// 2. Limpiamos los bits 0 y 1 (Modo antiguo)
	lcdStatValue &= 0xFC;

	// 3. Aplicamos el nuevo modo al VALOR (no a la dirección)
	lcdStatValue |= (mode & 0x03);

	// 4. Escribimos de vuelta en memoria
	mmu->io[0x41] = lcdStatValue;

	// 5. Manejo de Interrupciones STAT (Opcional pero recomendado aquí)
	// Si el modo nuevo coincide con la interrupción seleccionada en STAT, solicitarla.
	bool interruptTriggered = false;
	if ((mode == 0) && (lcdStatValue & 0x08)) interruptTriggered = true; // Mode 0 HBlank check
	if ((mode == 1) && (lcdStatValue & 0x10)) interruptTriggered = true; // Mode 1 VBlank check
	if ((mode == 2) && (lcdStatValue & 0x20)) interruptTriggered = true; // Mode 2 OAM check
	if (interruptTriggered) {
		interr->requestInterrupt(mmu, 1);
	}
	if (gpuMode == 0 && mmu->cgbMode) mmu->hdmaPendingHBlank = true;
}

/*
EASTER EGG:
Este es un comentario en ESPAÑOL para los putos guiris que intente copiar el código al motelu el pajas VR
*/

void GPU::init(SDL_Renderer* render) {
	texture = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 160, 144);
}

void GPU::renderFramebuffer(SDL_Renderer *render) {
	SDL_UpdateTexture(texture, NULL, framebuffer, 160 * 3);
	//SDL_RenderPresent(render);
}

uint8_t GPU::getColour(uint8_t colourNum, uint16_t address, MMU *mmu) {
	uint8_t res = 0; //Full white
	uint8_t palette = mmu->read8(address);
	uint8_t hi = 0;
	uint8_t lo = 0;

	// which bits of the colour palette does the colour id map to?
	switch (colourNum)
	{
	case 0: hi = 1; lo = 0; break;
	case 1: hi = 3; lo = 2; break;
	case 2: hi = 5; lo = 4; break;
	case 3: hi = 7; lo = 6; break;
	}

	// use the palette to get the colour
	uint8_t colour = 0;
	colour = BitGetVal(palette, hi) << 1;
	colour |= BitGetVal(palette, lo);

	// convert the game colour to emulator colour

	res = colour;

	return res;

}

void GPU::cgbPaletteToRGB(const uint8_t* palRAM, int paletteNum, int colorIdx, uint8_t& r, uint8_t& g, uint8_t& b) {

    int idx = (paletteNum * 8) + (colorIdx * 2);

    uint16_t rgb555 = palRAM[idx] | ((uint16_t)palRAM[idx + 1] << 8);

    r = (uint8_t)(((rgb555 >> 0) & 0x1F) * 255 / 31);

    g = (uint8_t)(((rgb555 >> 5) & 0x1F) * 255 / 31);

    b = (uint8_t)(((rgb555 >> 10) & 0x1F) * 255 / 31);

}


void GPU::DrawScanline(MMU* mmu) {
	// Clear BG color index buffer each scanline so sprite priority is never stale
	memset(bgColorIndex, 0, sizeof(bgColorIndex));
	uint8_t n = mmu->read8(0xFF40);
	//Display Background
	//std::cout << "Rendering background" << std::endl;
	bool bgEnable = isKthBitSet(n, 0);
	if (mmu->cgbMode || bgEnable) {
		renderBackground(mmu);
	}
	//Display Sprites
	//std::cout << "Rendering sprites" << std::endl;
	if (isKthBitSet(n, 1)) {
		renderSprites(mmu);
	}
	
}

static inline uint8_t readVRAMBank(MMU* mmu, int bank, uint16_t offset) {
	if (mmu->cgbMode)
		return mmu->vramBank[bank & 1][offset];
	return (uint8_t)mmu->vram[offset];
}
void GPU::renderBackground(MMU* mmu) {
 	uint8_t lcdControl = mmu->read8(0xFF40);
 	uint8_t ly = mmu->read8(0xFF44);
 	uint8_t scrollY = mmu->read8(0xFF42);
 	uint8_t scrollX = mmu->read8(0xFF43);
 	uint8_t windowY = mmu->read8(0xFF4A);
 	uint8_t windowX = (uint8_t)(mmu->read8(0xFF4B) - 7);
 
 	bool windowEnabled    = isKthBitSet(lcdControl, 5);
 	bool tileDataUnsigned = isKthBitSet(lcdControl, 4);
 	bool bgMapSelect      = isKthBitSet(lcdControl, 3);
 	bool winMapSelect     = isKthBitSet(lcdControl, 6);
 
 	uint16_t bgMapBase  = bgMapSelect  ? 0x9C00 : 0x9800;
 	uint16_t winMapBase = winMapSelect ? 0x9C00 : 0x9800;
 
 	bool windowDrawnThisLine = false;
 
 	for (int pixel = 0; pixel < 160; pixel++) {
 
 		bool usingWindow = false;
 		if (windowEnabled && ly >= windowY && pixel >= (int)(uint8_t)windowX) {
 			usingWindow = true;
 			windowDrawnThisLine = true;
 		}
 
 		uint16_t mapBase;
 		uint8_t yPos, xPos;
 		if (usingWindow) {
 			mapBase = winMapBase;
 			yPos = wly;
 			xPos = (uint8_t)(pixel - (int)(uint8_t)windowX);
 		} else {
 			mapBase = bgMapBase;
 			yPos = scrollY + ly;
 			xPos = scrollX + pixel;
 		}
 
 		uint16_t tileRow     = (yPos / 8) * 32;
 		uint16_t tileCol     = (xPos / 8);
 		uint16_t tileAddress = mapBase + tileRow + tileCol;
 
 		// Always read tile index from VRAM bank 0
 		uint8_t tileIndex = readVRAMBank(mmu, 0, tileAddress - 0x8000);
 
 		// CGB: tile attributes from VRAM bank 1
 		uint8_t tileAttr    = 0;
 		bool cgbYFlip   = false;
 		bool cgbXFlip   = false;
 		int  cgbVramBank = 0;
 		int  cgbPalette  = 0;
 		if (mmu->cgbMode) {
 			tileAttr    = readVRAMBank(mmu, 1, tileAddress - 0x8000);
 			cgbYFlip    = (tileAttr & 0x40) != 0;
 			cgbXFlip    = (tileAttr & 0x20) != 0;
 			cgbVramBank = (tileAttr & 0x08) ? 1 : 0;
 			cgbPalette  = (tileAttr & 0x07);
 		}
 
 		uint16_t tileLocation;
 		if (tileDataUnsigned)
 			tileLocation = 0x8000 + ((uint16_t)tileIndex * 16);
 		else
 			tileLocation = (uint16_t)(0x9000 + ((int8_t)tileIndex * 16));
 
 		uint8_t tileLineOfs = (yPos % 8);
 		if (cgbYFlip) tileLineOfs = 7 - tileLineOfs;
 		uint8_t tileLine = tileLineOfs * 2;
 
 		// Read tile data from correct VRAM bank
 		uint16_t tileOfs = tileLocation - 0x8000;
 		uint8_t data1 = readVRAMBank(mmu, cgbVramBank, tileOfs + tileLine);
 		uint8_t data2 = readVRAMBank(mmu, cgbVramBank, tileOfs + tileLine + 1);
 
 		int colorBit = cgbXFlip ? (xPos % 8) : (7 - (xPos % 8));
 		uint8_t colorNum = (uint8_t)((!!(data2 & (1 << colorBit)) << 1) | !!(data1 & (1 << colorBit)));
 
 		uint8_t red = 0, green = 0, blue = 0;
 		if (mmu->cgbMode) {
 			cgbPaletteToRGB(mmu->bgPaletteRAM, cgbPalette, colorNum, red, green, blue);
 		} else {
 			uint8_t col = getColour(colorNum, 0xFF47, mmu);
 			red   = GB_PALETTES[currentPalette].color[col][0];
 			green = GB_PALETTES[currentPalette].color[col][1];
 			blue  = GB_PALETTES[currentPalette].color[col][2];
 		}
 
 		if (ly < 144 && pixel < 160) {
 			bgColorIndex[pixel] = colorNum;
 			framebuffer[ly][pixel][0] = red;
 			framebuffer[ly][pixel][1] = green;
 			framebuffer[ly][pixel][2] = blue;
 		}
 	}
 	if (windowDrawnThisLine) wly++;
 }
 
 void GPU::renderSprites(MMU *mmu) {
 	bool use8x16 = isKthBitSet(mmu->read8(0xFF40), 2);
 	int ysize    = use8x16 ? 16 : 8;
 	int scanline = mmu->read8(0xFF44);
 
 	for (int sprite = 39; sprite >= 0; sprite--) {
 		uint8_t index      = sprite * 4;
 		int     yPos       = (int)mmu->read8(0xFE00 + index)     - 16;
 		int     xPos       = (int)mmu->read8(0xFE00 + index + 1) - 8;
 		uint8_t tileLoc    = mmu->read8(0xFE00 + index + 2);
 		uint8_t attributes = mmu->read8(0xFE00 + index + 3);
 
 		bool yFlip    = isKthBitSet(attributes, 6);
 		bool xFlip    = isKthBitSet(attributes, 5);
 		bool priority = isKthBitSet(attributes, 7);
 
 		if (scanline < yPos || scanline >= yPos + ysize) continue;
 
 		if (use8x16) tileLoc &= 0xFE;
 
 		int line = scanline - yPos;
 		if (yFlip) line = (ysize - 1) - line;
 
 		int cgbSpVramBank = mmu->cgbMode ? ((attributes & 0x08) ? 1 : 0) : 0;
 		int cgbSpPalette  = mmu->cgbMode ? (attributes & 0x07) : 0;
 
 		uint16_t dataOfs = (uint16_t)(tileLoc * 16) + (uint16_t)(line * 2);
 		uint8_t  data1 = readVRAMBank(mmu, cgbSpVramBank, dataOfs);
 		uint8_t  data2 = readVRAMBank(mmu, cgbSpVramBank, dataOfs + 1);
 
 		uint16_t paletteAddr = isKthBitSet(attributes, 4) ? 0xFF49 : 0xFF48;
 
 		for (int tilePixel = 7; tilePixel >= 0; tilePixel--) {
 			int colourbit = xFlip ? (7 - tilePixel) : tilePixel;
 			int colourNum = (BitGetVal(data2, colourbit) << 1) | BitGetVal(data1, colourbit);
 			if (colourNum == 0) continue;
 			int pixel = xPos + (7 - tilePixel);
 			if (scanline < 0 || scanline > 143 || pixel < 0 || pixel > 159) continue;
 			if (priority && bgColorIndex[pixel] != 0) continue;
 			uint8_t red = 0, green = 0, blue = 0;
 			if (mmu->cgbMode) {
 				cgbPaletteToRGB(mmu->objPaletteRAM, cgbSpPalette, colourNum, red, green, blue);
 			} else {
 				uint8_t col = getColour(colourNum, paletteAddr, mmu);
 				red   = GB_PALETTES[currentPalette].color[col][0];
 				green = GB_PALETTES[currentPalette].color[col][1];
 				blue  = GB_PALETTES[currentPalette].color[col][2];
 			}
 			framebuffer[scanline][pixel][0] = red;
 			framebuffer[scanline][pixel][1] = green;
 			framebuffer[scanline][pixel][2] = blue;
 		}
 	}
 }
void GPU::step(uint16_t cycles, MMU *mmu, SDL_Renderer *render, Interrupt *interr) {
	// 1. LEER EL REGISTRO DE CONTROL (LCDC)
	uint8_t lcdc = mmu->io[0x40];

	// 2. COMPROBAR EL BIT 7 (LCD ENABLE)
	bool isLCDEnabled = (lcdc & 0x80); // 0x80 es 10000000 en binario

	if (!isLCDEnabled) {
		// --- LA PANTALLA ESTÁ APAGADA ---

		// El hardware real resetea LY a 0 cuando se apaga el LCD
		clock = 0;      // Reseteamos el contador de ciclos internos
		line = 0;       // LY vuelve a 0
		mode = 1;       // Modo HBlank (o estado inicial)

		// Escribimos el estado en memoria para que la CPU lo lea correctamente
		mmu->io[0x44] = 0; // LY = 0

		// Importante: También se resetean los bits de modo en STAT (bits 0-1)
		uint8_t stat = mmu->io[0x41];
		stat &= 0xFC; // Borrar bits 0 y 1
		mmu->io[0x41] = stat;

		return; // ¡SALIR! No procesar nada más mientras esté apagada
	}

	clock += cycles;
	switch (mode)
		{
		case 0:
			//Horizontal Blanking
			//std::cout << "Entering Horizontal Blanking" << std::endl;
			if (clock >= 204) {
				clock -= 204;
				line++;
				mmu->io[0x44] = line;
				checkLYC = false;

				if (line == 144) {
					//Enter in Vertical Blanking Mode
					changeModeGPU(mmu, 1, interr);
					interr->requestInterrupt(mmu, 0);

					//TODO: Write a function that write data into the SDL Render
				// renderFramebuffer moved to main loop
					//std::cout << "Writing data from framebuffer" << std::endl;
				}
				else {
					changeModeGPU(mmu, 2, interr);
				}
			}
			break;
		case 1:
			//Vertical Blanking
			//std::cout << "Entering Vertical Blanking" << std::endl;
			if (clock >= 456) {

				clock -= 456;
				line++;

				if (line > 153) {
				line = 0;
				wly = 0;
				checkLYC = false;
				mmu->io[0x44] = 0;
				changeModeGPU(mmu, 2, interr);
			} else {
				checkLYC = false;
				mmu->io[0x44] = line;
			}
			}
			break;
		case 2:
			//Read OAM
			//std::cout << "Reading OAM Mode" << std::endl;
			if (clock >= 80) {
				clock -= 80;
				changeModeGPU(mmu, 3, interr);
			}
			break;
		case 3:
			//Read VRAM for generate picture
			//std::cout << "Reading VRAM for generate picture" << std::endl;
			if (clock >= 172) {

				// Draw scanline with registers valid during Mode 3, THEN enter HBlank
				clock -= 172;
				DrawScanline(mmu);
				changeModeGPU(mmu, 0, interr); // HBlank STAT interrupt fires after draw
			}
			break;
		}
	

	//Comparación LYC(Coincidence Flag)
	uint8_t currentLY = mmu->io[0x44];
	uint8_t currentLYC = mmu->io[0x45];

	// La Game Boy comprueba si LY == LYC y lanza interrupción si está habilitada
	if (currentLY == currentLYC) {
		mmu->io[0x41] |= 0x04; // Set Coincidence Bit (Bit 2)

		// Solo pedimos la interrupción si NO la habíamos pedido ya para esta coincidencia
		if (checkLYC == false) {
			// Comprobar si la interrupción STAT por LYC está habilitada (Bit 6 de STAT)
			if (mmu->io[0x41] & 0x40) {
				interr->requestInterrupt(mmu, 1);
			}
			checkLYC = true; // Marcamos que ya hemos gestionado esta coincidencia
		}
	}
	else {
		mmu->io[0x41] &= ~0x04; // Reset Coincidence Bit
		checkLYC = false; // Reseteamos el flag para la próxima vez que coincidan
	}
}
