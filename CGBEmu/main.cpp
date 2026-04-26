#include <SDL.h>
#include <stdio.h>
#include <iostream>
#include "CPU.h"
#include "GUI.h"
#include <bitset>
#include "Timers.h"
#include "APU.h"
#include <iomanip>
#include "portable-file-dialogs.h"
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

using namespace std;

Timer timer;
Timer* globalTimer = &timer;

vector<string> OpenFile() {
	auto fileResult = pfd::open_file("Select a GameBoy ROM", "", { "Gameboy", "*.gb" }, pfd::opt::none);

	std::vector<std::string> result = fileResult.result();

	if (result.empty()) {
		std::cout << "No file selected" << std::endl;
		return {};
	}

	return result;
}

void runApp() {
	//SDL Magic Thing
	SDL_Window* mainWindow = NULL;
	SDL_Renderer *render = NULL;
	SDL_Window* debuggerWindow = NULL;
	SDL_Renderer* debuggerRenderer = NULL;

	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

	mainWindow = SDL_CreateWindow("AvocadoBoy", 500, 500, 160 * 3, 144 * 3, (SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI));
	render = SDL_CreateRenderer(mainWindow, -1, SDL_RENDERER_ACCELERATED);


	int MAXCYCLES = 70224;
	float FPS = 59.73f;
	float DELAY_TIME = 1000.0f / FPS;

	IMGUI_CHECKVERSION();
	ImGuiContext* context1 = ImGui::CreateContext();
	ImGuiContext* context2 = ImGui::CreateContext();
	ImGui::SetCurrentContext(context1);
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	float main_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
	style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
	io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
	io.ConfigDpiScaleViewports = true;      // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForSDLRenderer(mainWindow, render);
	ImGui_ImplSDLRenderer2_Init(render);

	bool isRomLoaded = false;
	bool showDebugger = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	//Instantiate class GPU of the gameboy emulator
	GPU gpu;
	//Instantiate class CPU of the gameboy emulator
	CPU gameboy;

	//Init gameboy with some default values
	gameboy.init();
	gpu.init(render);
	extern void SPU_SetMMU(MMU*);
	SPU_SetMMU(gameboy.getMMUValues());
	initSPU();


	if (mainWindow != NULL) {

		bool isEmuRunning = true;
		SDL_Event sdlEvent;

		//Infinite loop for running gameboy
		while (isEmuRunning) {

			//Handle event
			while (SDL_PollEvent(&sdlEvent)) {
				ImGui_ImplSDL2_ProcessEvent(&sdlEvent);
				//Test things
				if(sdlEvent.type == SDL_WINDOWEVENT) {
					switch (sdlEvent.window.event)
					{
						case SDL_WINDOWEVENT_RESIZED:
							cout << "Window resized" << endl;
							break;
						case SDL_WINDOWEVENT_CLOSE:
							cout << "Closing window" << endl;
							isEmuRunning = false;
							break;
						default:
							break;
					}
						
				}

				if (sdlEvent.type == SDL_KEYDOWN) {
					if (sdlEvent.key.keysym.sym == SDLK_z) {
						std::cout << "Presing A: " << sdlEvent.key.keysym.sym << std::endl;
						gameboy.setKey(0);
					}

					if (sdlEvent.key.keysym.sym == SDLK_x) {
						std::cout << "Presing B: " << sdlEvent.key.keysym.sym << std::endl;
						gameboy.setKey(1);
					}

					if (sdlEvent.key.keysym.sym == SDLK_SPACE) {
						std::cout << "Presing Start: " << sdlEvent.key.keysym.sym << std::endl;
						gameboy.setKey(2);
					}

					if (sdlEvent.key.keysym.sym == SDLK_BACKSPACE) {
						std::cout << "Presing Select: " << sdlEvent.key.keysym.sym << std::endl;
						gameboy.setKey(3);
					}

					if (sdlEvent.key.keysym.sym == SDLK_UP) {
						std::cout << "Presing Up: " << sdlEvent.key.keysym.sym << std::endl;
						gameboy.setKey(6);
					}

					if (sdlEvent.key.keysym.sym == SDLK_DOWN) {
						std::cout << "Presing DOWN: " << sdlEvent.key.keysym.sym << std::endl;
						gameboy.setKey(7);
					}

					if (sdlEvent.key.keysym.sym == SDLK_LEFT) {
						std::cout << "Presing Left: " << sdlEvent.key.keysym.sym << std::endl;
						gameboy.setKey(5);
					}

					if (sdlEvent.key.keysym.sym == SDLK_RIGHT) {
						std::cout << "Presing Right: " << sdlEvent.key.keysym.sym << std::endl;
						gameboy.setKey(4);
					}
				}


				if (sdlEvent.type == SDL_KEYUP) {
					if (sdlEvent.key.keysym.sym == SDLK_z) {
						gameboy.releaseKey(0);
					}

					if (sdlEvent.key.keysym.sym == SDLK_x) {
						gameboy.releaseKey(1);
					}

					if (sdlEvent.key.keysym.sym == SDLK_SPACE) {
						gameboy.releaseKey(2);
					}

					if (sdlEvent.key.keysym.sym == SDLK_BACKSPACE) {
						gameboy.releaseKey(3);
					}

					if (sdlEvent.key.keysym.sym == SDLK_UP) {
						gameboy.releaseKey(6);
					}

					if (sdlEvent.key.keysym.sym == SDLK_DOWN) {
						gameboy.releaseKey(7);
					}

					if (sdlEvent.key.keysym.sym == SDLK_LEFT) {
						gameboy.releaseKey(5);
					}

					if (sdlEvent.key.keysym.sym == SDLK_RIGHT) {
						gameboy.releaseKey(4);
					}
				}
			}

			ImGui_ImplSDLRenderer2_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();
			// 1. Get the exact size of your OS window using the ImGui Viewport
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);

			// 2. Remove window padding and borders so the texture goes edge-to-edge
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

			// 3. Set the flags that make this act like a background layer rather than a floating window
			ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar |
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoNavFocus;

			SDL_SetRenderDrawColor(render, 0, 0, 0, 255);
			SDL_RenderClear(render);
			SDL_RenderClear(debuggerRenderer);

			//gameboy.runCPU(&gpu, render);
			if (isRomLoaded) {
				int cyclesInThisFrame = 0;
				while (cyclesInThisFrame < MAXCYCLES) {
					//logger->info(formatCpuStateLog(&gameboy));
					int cycles = gameboy.step();
					//Logs << "A:00 F:11 B:22 C:33 D:44 E:55 H:66 L:77 SP:8888 PC:9999 PCMEM:AA,BB,CC,DD";
					timer.updateTimer(gameboy.getMMUValues(), gameboy.getInterrupt(), cycles, gameboy.isStoped);
					gpu.step(cycles, gameboy.getMMUValues(), render, gameboy.getInterrupt());
					stepSPU((unsigned char)cycles);

					cyclesInThisFrame += cycles;
				}
			}

			// Volcar framebuffer del emulador al renderer SDL una vez por frame
			if (isRomLoaded) { gpu.renderFramebuffer(render); }
			MMU* mmuValues = gameboy.getMMUValues();

			ImGui::Begin("GameBoy Workspace", nullptr, window_flags);
			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("File")) {
					if (ImGui::MenuItem("Open ROM")) {
						std::cout << "Opening rom" << std::endl;
						vector<string> romPath = OpenFile();

						if (!romPath.empty()) {
							std::cout << "Path selected: " << romPath[0] << std::endl;
							gameboy.init();
							gpu.init(render);
							SPU_SetMMU(gameboy.getMMUValues());
							stopSPU();
							initSPU();
							gameboy.loadGame(romPath[0].c_str());
							isRomLoaded = true;
						}
					}

					if (ImGui::MenuItem("Exit")) {
						std::cout << "Closing" << std::endl;
						isEmuRunning = false;
					}

					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Tools")) {
					if (ImGui::MenuItem("Debugger")) {
						std::cout << "Show debugger" << std::endl;
						debuggerWindow = SDL_CreateWindow("Debugger AvocadoBoy", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 500, 500, (SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI));
						debuggerRenderer = SDL_CreateRenderer(debuggerWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
						IMGUI_CHECKVERSION();
						ImGui::SetCurrentContext(context2);
						ImGuiIO& io = ImGui::GetIO(); (void)io;
						io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

						// Setup Dear ImGui style
						ImGui::StyleColorsDark();

						ImGuiStyle& style = ImGui::GetStyle();
						float main_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
						style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
						style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
						io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
						io.ConfigDpiScaleViewports = true;

						ImGui_ImplSDL2_InitForSDLRenderer(debuggerWindow, debuggerRenderer);
						ImGui_ImplSDLRenderer2_Init(debuggerRenderer);
						ImGui_ImplSDLRenderer2_NewFrame();
						ImGui_ImplSDL2_NewFrame();
						ImGui::NewFrame();
						SDL_RenderClear(debuggerRenderer);
						showDebugger = true;
					}
					ImGui::SetCurrentContext(context1);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("About")) {
					if (ImGui::MenuItem("Info")) {
						SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Creado por 0c0de", "This emulator has been created for educational purposes and we don't provide any ROM", mainWindow);
					}

					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}

			ImGui::PopStyleVar(3);

			if (showDebugger) {

				ImGui::SetCurrentContext(context2);
				drawMMU(mmuValues);
				drawFlags(gameboy.getFlagState(), gameboy.getGameboyRegisters(), &gpu, &gameboy);

				MMU* mmuValues = gameboy.getMMUValues();
				GameboyFlags* flagState = gameboy.getFlagState();
				GameboyRegisters* reg = gameboy.getGameboyRegisters();

				ImGui::Render();
				SDL_RenderSetScale(debuggerRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
				SDL_SetRenderDrawColor(debuggerRenderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
				ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), debuggerRenderer);
				SDL_RenderPresent(debuggerRenderer);

			}

			// 6. Draw the texture
			// GetContentRegionAvail() is the magic function here. It calculates 
			// exactly how much space is left in the window BELOW the menu bar.
			ImVec2 available_size = ImGui::GetContentRegionAvail();

			ImGui::Image(
				(ImTextureID)(intptr_t)gpu.texture,
				available_size // This makes the texture fill the remaining screen space perfectly!
			);
			ImGui::End();

			ImGui::Render();
			SDL_RenderSetScale(render, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
			SDL_SetRenderDrawColor(render, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
			ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), render);
			SDL_RenderPresent(render);
		}
	}

	//SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(mainWindow);
	SDL_Quit();
}


int main(int argc, char* argv[]) {
	if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
		cout << "SDL Initiated correctly" << endl;
		runApp();
	}
	else {
		cout << "An error ocurred when initiating SDL: " << SDL_GetError() << endl;
		SDL_Quit();
	}
	SDL_Delay(500);
	return 0;
}
