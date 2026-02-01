#include "GPU.h"

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

void GPU::changeModeGPU(MMU* mmu, uint8_t gpuMode) {
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
		// Necesitas pasar tu puntero de interrupciones a esta función o hacerlo fuera
		// interrupt->requestInterrupt(mmu, 1); 
	}
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
	SDL_RenderCopy(render, texture, NULL, NULL);
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

void GPU::DrawScanline(MMU* mmu) {
	uint8_t n = mmu->read8(0xFF40);
	if (isKthBitSet(n, 7)) {
		//Display Background
		//std::cout << "Rendering background" << std::endl;
		if (isKthBitSet(n, 0)) {
			renderBackground(mmu);
		}
		//Display Sprites
		//std::cout << "Rendering sprites" << std::endl;
		if (isKthBitSet(n, 1)) {
			renderSprites(mmu);
		}
	}
}

void GPU::renderBackground(MMU* mmu) {
	uint8_t lcdControl = mmu->read8(0xFF40);
	uint8_t ly = mmu->read8(0xFF44); // Línea actual (Scanline)

	// Coordenadas de Scroll y Window
	uint8_t scrollY = mmu->read8(0xFF42);
	uint8_t scrollX = mmu->read8(0xFF43);
	uint8_t windowY = mmu->read8(0xFF4A);
	uint8_t windowX = mmu->read8(0xFF4B) - 7; // WX tiene un offset de 7

	// Flags del LCDC
	bool windowEnabled = isKthBitSet(lcdControl, 5);
	bool tileDataUnsigned = isKthBitSet(lcdControl, 4); // Bit 4: 1=8000-8FFF, 0=8800-97FF
	bool bgMapSelect = isKthBitSet(lcdControl, 3);      // Bit 3: Mapa Fondo (9800 vs 9C00)
	bool winMapSelect = isKthBitSet(lcdControl, 6);     // Bit 6: Mapa Ventana (9800 vs 9C00)

	// Direcciones base de los mapas
	uint16_t bgMapBase = bgMapSelect ? 0x9C00 : 0x9800;
	uint16_t winMapBase = winMapSelect ? 0x9C00 : 0x9800;

	// --- BUCLE DE PÍXELES (0 a 159) ---
	for (int pixel = 0; pixel < 160; pixel++) {

		// 1. Decidir si dibujamos VENTANA o FONDO en este píxel específico
		bool usingWindow = false;

		if (windowEnabled) {
			// La ventana se dibuja si estamos dentro de su rango Y y X
			if (ly >= windowY && pixel >= windowX) {
				usingWindow = true;
			}
		}

		// 2. Calcular las coordenadas en el mapa de tiles (VRAM)
		uint16_t mapBase = 0;
		uint8_t yPos = 0;
		uint8_t xPos = 0;

		if (usingWindow) {
			mapBase = winMapBase;
			yPos = ly - windowY;       // Y relativo a la ventana
			xPos = pixel - windowX;    // X relativo a la ventana
		}
		else {
			mapBase = bgMapBase;
			yPos = scrollY + ly;       // Y relativo al fondo (con scroll)
			xPos = scrollX + pixel;    // X relativo al fondo (con scroll)
		}

		// 3. Obtener el ID del Tile
		// (yPos / 8) * 32  -> Fila del tile (32 tiles de ancho)
		// (xPos / 8)       -> Columna del tile
		uint16_t tileRow = (yPos / 8) * 32;
		uint16_t tileCol = (xPos / 8);
		uint16_t tileAddress = mapBase + tileRow + tileCol;

		// Leemos el índice del tile (puede ser signed o unsigned)
		uint8_t tileIndex = mmu->read8(tileAddress);

		// 4. Calcular la dirección de los datos del tile (AQUÍ FALLABA TETRIS)
		uint16_t tileLocation = 0;

		if (tileDataUnsigned) {
			// MODO 8000 (Unsigned): 0 a 255
			// Dr. Mario usa esto
			tileLocation = 0x8000 + (tileIndex * 16);
		}
		else {
			// MODO 8800 (Signed): -128 a 127
			// Tetris usa esto. Usamos 0x9000 como base para simplificar la matemática.
			// Casteamos explícitamente a int8_t para obtener el signo correcto.
			tileLocation = 0x9000 + ((int8_t)tileIndex * 16);
		}

		// 5. Obtener los bytes del tile (Plano bajo y alto)
		uint8_t line = (yPos % 8) * 2; // Que línea del tile (0-7) * 2 bytes
		uint8_t data1 = mmu->read8(tileLocation + line);
		uint8_t data2 = mmu->read8(tileLocation + line + 1);

		// 6. Decodificar el color (Bit flip)
		// El pixel 0 es el Bit 7, el pixel 7 es el Bit 0.
		int colorBit = 7 - (xPos % 8);

		// Combinamos los bits
		// (Bit del byte 2 << 1) | (Bit del byte 1)
		uint8_t colorNum = !!(data2 & (1 << colorBit)) << 1;
		colorNum |= !!(data1 & (1 << colorBit));

		// 7. Colorear (Paleta)
		uint8_t col = getColour(colorNum, 0xFF47, mmu);

		// Asignar RGB (Tu lógica original)
		uint8_t red = 0, green = 0, blue = 0;
		switch (col) {
		case 0: red = 255; green = 255; blue = 255; break;
		case 1: red = 204; green = 204; blue = 204; break;
		case 2: red = 119; green = 119; blue = 119; break;
		case 3: red = 0;   green = 0;   blue = 0;   break;
		}

		// Escribir al framebuffer (Ojo con los límites)
		if (ly < 144 && pixel < 160) {
			framebuffer[ly][pixel][0] = red;
			framebuffer[ly][pixel][1] = green;
			framebuffer[ly][pixel][2] = blue;
		}
	}
}

void GPU::renderSprites(MMU *mmu) {

	bool use8x16 = false;
	uint8_t lcdControl = mmu->read8(0xFF40);

	if (isKthBitSet(lcdControl, 2))
		use8x16 = true;

	for (int sprite = 0; sprite < 40; sprite++)
	{
		// sprite occupies 4 bytes in the sprite attributes table
		uint8_t index = sprite * 4;
		uint8_t yPos = mmu->read8(0xFE00 + index) - 16;
		uint8_t xPos = mmu->read8(0xFE00 + index + 1) - 8;
		//std::cout << "Index sprite: " << sprite << "Pixel value: " << static_cast<unsigned>(xPos) << std::endl;
		uint8_t tileLocation = mmu->read8(0xFE00 + index + 2);
		uint8_t attributes = mmu->read8(0xFE00 + index + 3);

		bool yFlip = isKthBitSet(attributes, 6);
		bool xFlip = isKthBitSet(attributes, 5);

		int scanline = mmu->read8(0xFF44);

		int ysize = 8;
		if (use8x16)
			ysize = 16;

		if ((scanline >= yPos) && (scanline < (yPos + ysize))) {
			int line = scanline - yPos;

			// read the sprite in backwards in the y axis
			if (yFlip)
			{
				line -= ysize;
				line *= -1;
			}

			line *= 2; // same as for tiles
			uint16_t dataAddress = (0x8000 + (tileLocation * 16)) + line;
			//std::cout << std::hex << "Tile Location: 0x" << static_cast<unsigned>(tileLocation) << "Line is: 0x" << static_cast<unsigned>(line) << std::endl;
			uint8_t data1 = mmu->read8(dataAddress);
			uint8_t data2 = mmu->read8(dataAddress + 1);

			// its easier to read in from right to left as pixel 0 is
			// bit 7 in the colour data, pixel 1 is bit 6 etc...
			for (int tilePixel = 7; tilePixel >= 0; tilePixel--)
			{
				int colourbit = tilePixel;
				// read the sprite in backwards for the x axis
				if (xFlip)
				{
					colourbit -= 7;
					colourbit *= -1;
				}

				// the rest is the same as for tiles
				int colourNum = BitGetVal(data2, colourbit);
				colourNum <<= 1;
				colourNum |= BitGetVal(data1, colourbit);

				uint16_t colourAddress = isKthBitSet(attributes, 4) ? 0xFF49 : 0xFF48;
				uint8_t col = getColour(colourNum, colourAddress, mmu);

				// white is transparent for sprites.
				if (col == 0) {
					continue;
				}

				uint8_t red = 0;
				uint8_t green = 0;
				uint8_t blue = 0;

				
				switch (col)
				{
				case 0: red = 255; green = 255; blue = 255; break;
				case 1: red = 0xCC; green = 0xCC; blue = 0xCC; break;
				case 2: red = 0x77; green = 0x77; blue = 0x77; break;
				}

				int xPix = 0 - tilePixel;
				xPix += 7;

				int pixel = xPos + xPix;

				if ((scanline < 0) || (scanline > 143) || (pixel < 0) || (pixel > 159))
				{
					continue;
				}

				framebuffer[scanline][pixel][0] = red;
				framebuffer[scanline][pixel][1] = green;
				framebuffer[scanline][pixel][2] = blue;
			}
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
		mode = 0;       // Modo HBlank (o estado inicial)

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
					changeModeGPU(mmu, 1);
					interr->requestInterrupt(mmu, 0);

					//TODO: Write a function that write data into the SDL Render
					renderFramebuffer(render);
					//std::cout << "Writing data from framebuffer" << std::endl;
				}
				else {
					changeModeGPU(mmu, 2);
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
					changeModeGPU(mmu, 2);
				}

				mmu->io[0x44] = line;
			}
			break;
		case 2:
			//Read OAM
			//std::cout << "Reading OAM Mode" << std::endl;
			if (clock >= 80) {
				clock -= 80;
				changeModeGPU(mmu, 3);
			}
			break;
		case 3:
			//Read VRAM for generate picture
			//std::cout << "Reading VRAM for generate picture" << std::endl;
			if (clock >= 172) {

				//Enter Horizontal Blanking
				clock -= 172;
				changeModeGPU(mmu, 0);

				//Write a line to framebuffer based in the VRAM
				//TODO: Write a function that does this
				//renderScanBackground(mmu, line);

				DrawScanline(mmu);
				//gameboy->requestInterrupt(mmu, 1);
				//renderSprites(mmu);
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