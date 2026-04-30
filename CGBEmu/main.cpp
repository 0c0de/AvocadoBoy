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
#include "SaveState.h"
#include <windows.h>
#include <timeapi.h>
#include <direct.h>
#include <filesystem>
#include <regex>
#pragma comment(lib, "winmm.lib")

using namespace std;

Timer timer;
Timer* globalTimer = &timer;

vector<string> OpenFile() {
	auto fileResult = pfd::open_file("Select a GameBoy ROM", "", { "All files", "*" }, pfd::opt::none);

	std::vector<std::string> result = fileResult.result();

	if (result.empty()) {
		std::cout << "No file selected" << std::endl;
		return {};
	}
	return result;
}

std::string regex_replace_all(
	const std::string& str,
	const std::string& from,
	const std::string& to) {
	return std::regex_replace(
		str, std::regex(from), to
	);
}

void runApp() {
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
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	float main_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
	style.ScaleAllSizes(main_scale);
	style.FontScaleDpi = main_scale;
	io.ConfigDpiScaleFonts = true;
	io.ConfigDpiScaleViewports = true;
	ImGui_ImplSDL2_InitForSDLRenderer(mainWindow, render);
	ImGui_ImplSDLRenderer2_Init(render);
	bool isRomLoaded = false;
	bool showDebugger = false;
	bool showLoadSavePopup = false;
	char actualPath[1024];
	std::string savesDir;
	std::string currentSavePath;
	if (_getcwd(actualPath, 1024) != NULL) {
		std::string uncleaned = actualPath;
		uncleaned += "\\saves";
		savesDir = regex_replace_all(uncleaned, "/", "\\");
		if (_mkdir(savesDir.c_str()) != -1) {
			std::cout << "[SaveState] Saves folder created: " << savesDir << std::endl;
		}
		else if (errno != EEXIST) {
			std::cout << "[SaveState] Warning: could not create saves folder." << std::endl;
		}
	}
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	GPU gpu;
	CPU gameboy;
	gameboy.init();
	gpu.init(render);
	extern void SPU_SetMMU(MMU*);
	SPU_SetMMU(gameboy.getMMUValues());
	initSPU();
	if (mainWindow != NULL) {
		bool isEmuRunning = true;
		SDL_Event sdlEvent;
		while (isEmuRunning) {
			while (SDL_PollEvent(&sdlEvent)) {
				ImGui_ImplSDL2_ProcessEvent(&sdlEvent);
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
					if (sdlEvent.key.keysym.sym == SDLK_F5 && isRomLoaded) {
						if (saveState(currentSavePath, gameboy, gpu))
							SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Save State", "Game saved!", mainWindow);
					}
					if (sdlEvent.key.keysym.sym == SDLK_F8 && isRomLoaded) {
						if (loadState(currentSavePath, gameboy, gpu))
							SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Load State", "Game loaded!", mainWindow);
					}
					if (sdlEvent.key.keysym.sym == SDLK_z) { gameboy.setKey(0); }
					if (sdlEvent.key.keysym.sym == SDLK_x) { gameboy.setKey(1); }
					if (sdlEvent.key.keysym.sym == SDLK_SPACE) { gameboy.setKey(2); }
					if (sdlEvent.key.keysym.sym == SDLK_BACKSPACE) { gameboy.setKey(3); }
					if (sdlEvent.key.keysym.sym == SDLK_UP) { gameboy.setKey(6); }
					if (sdlEvent.key.keysym.sym == SDLK_DOWN) { gameboy.setKey(7); }
					if (sdlEvent.key.keysym.sym == SDLK_LEFT) { gameboy.setKey(5); }
					if (sdlEvent.key.keysym.sym == SDLK_RIGHT) { gameboy.setKey(4); }
				}
				if (sdlEvent.type == SDL_KEYUP) {
					if (sdlEvent.key.keysym.sym == SDLK_z) { gameboy.releaseKey(0); }
					if (sdlEvent.key.keysym.sym == SDLK_x) { gameboy.releaseKey(1); }
					if (sdlEvent.key.keysym.sym == SDLK_SPACE) { gameboy.releaseKey(2); }
					if (sdlEvent.key.keysym.sym == SDLK_BACKSPACE) { gameboy.releaseKey(3); }
					if (sdlEvent.key.keysym.sym == SDLK_UP) { gameboy.releaseKey(6); }
					if (sdlEvent.key.keysym.sym == SDLK_DOWN) { gameboy.releaseKey(7); }
					if (sdlEvent.key.keysym.sym == SDLK_LEFT) { gameboy.releaseKey(5); }
					if (sdlEvent.key.keysym.sym == SDLK_RIGHT) { gameboy.releaseKey(4); }
				}
			}
			ImGui_ImplSDLRenderer2_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
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

			if (isRomLoaded) {
				int cyclesInThisFrame = 0;
				while (cyclesInThisFrame < MAXCYCLES) {
					int cycles = gameboy.step();
					timer.updateTimer(gameboy.getMMUValues(), gameboy.getInterrupt(), cycles, gameboy.isStoped);
					gpu.step(cycles, gameboy.getMMUValues(), render, gameboy.getInterrupt());
					stepSPU((unsigned char)cycles);
					// CGB HBlank HDMA: fire exactly one 16-byte block per HBlank entry
					if (gameboy.getMMUValues()->cgbMode && gameboy.getMMUValues()->hdmaPendingHBlank) {
					    gameboy.getMMUValues()->hdmaPendingHBlank = false;
					    gameboy.getMMUValues()->stepHDMA();
					}

					cyclesInThisFrame += cycles;
				}
			}

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
							gameboy.loadGame(romPath[0].c_str());
							gameboy.init();
							gpu.init(render);
							SPU_SetMMU(gameboy.getMMUValues());
							stopSPU();
							initSPU();
							isRomLoaded = true;
							currentSavePath = savesDir + "\\" + getSavePath(gameboy.getRomTitle());
							showLoadSavePopup = saveExists(currentSavePath);
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
						ImGui::StyleColorsDark();
						ImGuiStyle& style = ImGui::GetStyle();
						float main_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
						style.ScaleAllSizes(main_scale);
						style.FontScaleDpi = main_scale;
						io.ConfigDpiScaleFonts = true;
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
				if (isRomLoaded && ImGui::BeginMenu("Save State")) {
					if (ImGui::MenuItem("Save (F5)")) {
						if (saveState(currentSavePath, gameboy, gpu))
							SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Save State", "Game saved!", mainWindow);
					}
					if (ImGui::MenuItem("Load (F8)")) {
						if (loadState(currentSavePath, gameboy, gpu))
							SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Load State", "Game loaded!", mainWindow);
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Palette")) {
					const char* paletteNames[] = { "DMG Green", "Red", "Black & White", "Default Gray" };
					for (int i = 0; i < 4; i++) {
						bool selected = (gpu.currentPalette == i);
						if (ImGui::MenuItem(paletteNames[i], nullptr, selected))
							gpu.currentPalette = i;
					}
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
			if (showLoadSavePopup) {
				ImGui::OpenPopup("Load Save?");
				showLoadSavePopup = false;
			}
			if (ImGui::BeginPopupModal("Load Save?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::Text("A save file was found for this game.");
				ImGui::Text("Would you like to continue from your last save?");
				ImGui::Separator();
				if (ImGui::Button("Yes", ImVec2(120, 0))) {
					loadState(currentSavePath, gameboy, gpu);
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("No", ImVec2(120, 0))) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
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
			ImVec2 available_size = ImGui::GetContentRegionAvail();
			ImGui::Image(
				(ImTextureID)(intptr_t)gpu.texture,
				available_size
			);
			ImGui::End();
			ImGui::Render();
			SDL_RenderSetScale(render, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
			SDL_SetRenderDrawColor(render, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
			ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), render);
			SDL_RenderPresent(render);
		}
	}

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
