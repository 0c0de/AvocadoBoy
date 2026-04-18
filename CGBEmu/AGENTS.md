# CGBEmu — Agent Reference

A Game Boy (DMG) emulator written in C++ targeting Windows, using SDL2 for rendering/audio and Dear ImGui for the debug UI.

## Build

**IDE**: Visual Studio 2022 (toolset v143), project file `CGBEmu.vcxproj`.  
Build via Visual Studio or MSBuild — there is no CMake, Makefile, or CLI build system for the emulator itself (the `spdlog/` subdirectory has its own CMakeLists.txt but is not used by the project).

```
msbuild CGBEmu.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild CGBEmu.vcxproj /p:Configuration=Release /p:Platform=x64
```

Output lands in `x64/Debug/` or `x64/Release/`. There is also a `Debug/` folder at root from a Win32 build.

**External dependencies** (must be installed on the machine, not vendored):
- SDL2 — headers at `C:\SDL2-2.0.9\include`, libs at `C:\SDL2-2.0.9\lib\x64`
- GLEW — headers at `C:\GLEW\include\GL`

**Vendored in-tree**:
- Dear ImGui (all `imgui*.cpp/h` files at root — SDL2 + SDLRenderer2 backend)
- `imgui_memory_editor.h` — single-header hex editor widget
- `portable-file-dialogs.h` — single-header file dialog
- `spdlog/` — logging library (headers present; not wired into the emulator code)

## Architecture

```
main.cpp
  └─ runApp()
       ├─ SDL2 window + renderer setup
       ├─ ImGui context init
       ├─ CPU, GPU, APU instantiation + .init()
       └─ Main loop (per-frame):
            ├─ SDL event poll → input → CPU::setKey/releaseKey
            ├─ Frame emulation loop (MAXCYCLES = 70224 T-cycles/frame):
            │    ├─ CPU::step()          → returns cycles consumed
            │    ├─ Timer::updateTimer() → DIV/TIMA, interrupt requests
            │    ├─ GPU::step()          → PPU scanline, mode changes, VBlank IRQ
            │    └─ APU::step()          → audio frame sequencer, sample generation
            └─ ImGui render (debugger windows) + SDL_RenderPresent
```

### Component responsibilities

| File | Class | Role |
|------|-------|------|
| `CPU.cpp/h` | `CPU` | LR35902 fetch-decode-execute, all opcodes, CB prefix, interrupt servicing, IME |
| `mmu.cpp/h` | `MMU` | Memory map (ROM/VRAM/WRAM/OAM/IO/HRAM), read8/write8, DMA transfer, joypad I/O registers |
| `GPU.cpp/h` | `GPU` | PPU: scanline renderer, background + sprites, framebuffer → SDL_Texture |
| `APU.cpp/h` | `APU` | Audio: 2× Square, Wave, Noise channels; SDL2 audio callback with ring buffer |
| `Interrupts.cpp/h` | `Interrupt` | IF/IE flag manipulation, interrupt dispatch (VBlank/LCD/Timer/Joypad) |
| `Timers.cpp/h` | `Timer` | DIV counter, TIMA/TMA/TAC, timer interrupt requests |
| `GUI.cpp/h` | — | ImGui debugger: `drawMMU()` (hex viewer per region), `drawFlags()` (registers/flags) |
| `Joypad.cpp/h` | — | Free functions for button state; the actual state lives in `MMU::directionsButton` / `MMU::actionButton` |

### Global state pattern

Each subsystem is a local variable in `CPU.cpp` with file scope, exposed via accessors:

```cpp
// CPU.cpp (file scope)
MMU mmu;
Interrupt interrupt;
GameboyRegisters reg;
GameboyFlags flags;
```

`CPU::getMMUValues()` returns `&mmu`; `CPU::getInterrupt()` returns `&interrupt`. The `main.cpp` loop passes these pointers into `GPU::step()`, `Timer::updateTimer()`, etc.  
**There is no single global emulator state struct** — callers must thread the MMU/Interrupt pointers through every subsystem call.

The `APU` is wired to `MMU` via a module-level function pointer in `mmu.cpp`:
```cpp
APU* audioSystem = nullptr;   // mmu.cpp global
void AudioWriteHandler(uint16_t addr, uint8_t value);  // called from MMU::write8
```
`APU::init()` must set `audioSystem` before any write8 calls route audio register writes to the APU.

### Memory map (MMU::read8 / write8 switch)

| Range | Region |
|-------|--------|
| 0x0000–0x7FFF | ROM (cartridge, up to 32 KB banked) |
| 0x8000–0x9FFF | VRAM |
| 0xA000–0xBFFF | External RAM (cartridge) |
| 0xC000–0xDFFF | WRAM |
| 0xE000–0xFDFF | Echo RAM (mirrors WRAM) |
| 0xFE00–0xFE9F | OAM (sprite_attrib) |
| 0xFF00–0xFF7F | I/O registers (io[]) |
| 0xFF80–0xFFFE | HRAM (internal_ram[]) |
| 0xFFFF | IE register |

DMA transfer is triggered on write to 0xFF46.

### CPU init state

`CPU::init()` sets the post-BIOS power-on state directly (PC=0x100, no BIOS execution):
- AF=0x01B0, BC=0x0013, DE=0x00D8, HL=0x014D, SP=0xFFFE
- All I/O registers initialised to documented DMG power-on values
- IME=false, isHalted=false

### Timing

- Frame budget: **70224 T-cycles** (≈59.73 Hz)
- `CPU::step()` returns the T-cycle count for the executed instruction
- Timer and GPU are driven by the same cycle count returned from step()
- APU uses a `CYCLES_PER_SAMPLE = 95.0f` accumulator to push samples to a 4096-sample ring buffer

### Input mapping (SDL keycodes → joypad)

Key presses call `CPU::setKey(uint8_t key)` / `CPU::releaseKey(uint8_t key)`:

| key value | Button |
|-----------|--------|
| 0 | A |
| 1 | B |
| 2 | Select |
| 3 | Start |
| 4 | Right |
| 5 | Left |
| 6 | Up |
| 7 | Down |

Low bits of `MMU::actionButton` / `MMU::directionsButton` are cleared (active-low) on press and set on release. Writing 0x10 to FF00 selects direction buttons; 0x20 selects action buttons (P14/P15 lines).

## Non-obvious gotchas

- **`mmu.rom` is `uint16_t[0x8000]`** even though ROM data is bytes. `read8` returns it directly; beware of ROM loading code that writes bytes into a 16-bit array — upper bytes of each element should always be zero.
- **`MMU::stack` is `uint16_t[0xFFFF]`** but is not used for stack emulation — the stack lives inside WRAM addressed by SP. This field appears unused.
- **`Joypad.h` declares globals** (`joyPadState`, `directionsButton`, `actionButton`) at file scope, but the actual joypad state the CPU/MMU use is `MMU::directionsButton` / `MMU::actionButton`. Including `Joypad.h` in multiple translation units will cause multiple-definition linker errors for those globals.
- **`GPU.cpp` may not be UTF-8 encoded** — Visual Studio saves it in the system codepage (Windows-1252). If editing with an external editor, re-save as UTF-8 to avoid corruption.
- **Two ImGui contexts are created** (`context1`, `context2`) in `main.cpp`, but only `context1` is used. `context2` and `debuggerRenderer` are initialised to `nullptr` and never set up properly.
- **`APU::audioSystem` pointer** in `mmu.cpp` must be set before the first `MMU::write8` to an audio register (0xFF10–0xFF3F). If it is null, writes are silently dropped and a warning is printed to stdout.
- **Debug logging in mmu.cpp**: `AudioWriteHandler` prints every audio register write to stdout with a call counter. This is diagnostic code left in and will spam stdout during normal emulation.
- **`spdlog/`** is present as a Git submodule directory but is **not linked** into the project — no spdlog headers are included anywhere in the emulator source.

## Debugger UI

Opened via the ImGui menu bar → Tools → Debugger:
- `drawMMU(mmu)` — hex viewer for ROM/VRAM/WRAM/RAM/HRAM/IO/OAM regions, toggled per-region from a menu bar inside the window. Also shows key I/O registers and stack pointer value.
- `drawFlags(flags, reg, gpu, gameboy)` — shows CPU registers (AF/BC/DE/HL/SP/PC), flags (Z/N/H/C), and GPU state.

## Code style conventions

- Class methods use PascalCase matching the Game Boy opcode name (e.g., `LD_NN_N`, `ADC_A_N`, `CALL_CC_NN`).
- "Cycles" is spelled `cicles` throughout the codebase (Spanish-influenced typo — do not "fix" it, it is used consistently).
- Boolean helpers `isKthBitSet(n, k)` are duplicated across CPU, MMU, Timer, and Interrupt — there is no shared utility header.
- `using namespace std;` is used in all translation units.
