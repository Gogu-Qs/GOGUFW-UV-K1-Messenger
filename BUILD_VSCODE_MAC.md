# GOGUFW 1.2.0 - VS Code build (macOS)

## Requirements
- CMake
- Ninja
- ARM GNU embedded toolchain (`arm-none-eabi-gcc`)
- VS Code extension: CMake Tools

The included toolchain file searches both PATH and common Homebrew locations:
- `/opt/homebrew/bin` (Apple Silicon)
- `/usr/local/bin` (Intel/Homebrew)

## Recommended build
1. Open this project folder in VS Code.
2. Install the recommended **CMake Tools** extension if prompted.
3. Press `Cmd+Shift+B`.
4. Select/default task: **GGFW: Build Fusion**.

The task always runs `cmake --preset Fusion` before building, so a fresh ZIP does not need a pre-existing `build/` directory.

The same build can be run directly from Terminal:

```bash
cmake --preset Fusion
cmake --build --preset Fusion
```

## Clean build
Command Palette -> `Tasks: Run Task` -> **GGFW: Clean + Build Fusion**.

## Important
Do not copy an old `build/` directory between machines or project folders. CMake caches absolute paths.

The generated firmware files are written to `build/Fusion/` with the `gogufw` target name.
