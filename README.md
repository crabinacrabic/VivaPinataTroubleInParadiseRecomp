# Viva Piñata: Trouble in Paradise - PC Recompilation

A native PC recompilation of **Viva Piñata: Trouble in Paradise** (Xbox 360, 2008), powered by the **ReXGlue SDK** (v0.10).

---

## Overview

This project statically recompiles the original Xbox 360 PowerPC binary into native x86-64 code, running natively on Windows without traditional full-system emulation overhead.

### Key Features
- **DirectX 12 Backend**: Native hardware rendering on modern GPUs.
- **Configurable Framerate**: Support for high refresh rates as well as authentic 30/60 FPS frame locks to preserve original game physics and mini-games.
- **Mouse & Keyboard Support**: Full desktop navigation and cursor controls.
- **Custom Aspect Ratios & Resolutions**: Support for ultrawide and arbitrary display aspect ratios.
- **Integrated Mod Loading**: Support for community shaders, texture packs, and data modifications.

---

## Requirements

### System Requirements
- **OS**: Windows 10 / 11 (64-bit)
- **Processor**: Modern 4-core x86-64 CPU (Intel Core i5 / AMD Ryzen 3 or better)
- **RAM**: 8 GB RAM minimum (16 GB recommended)
- **Graphics**: DirectX 12 compatible GPU (Nvidia GeForce GTX 1060 / AMD Radeon RX 580 or newer)
- **Storage**: ~15 GB available space

### Game Assets
This repository contains only the recompilation source code and tooling. You must provide your own legally dumped retail disc copy of *Viva Piñata: Trouble in Paradise* (Xbox 360).

---

## Setup & Building

### 1. Extract Game Assets
Extract your game disc (`default.xex` and the `Beta` folder) directly into the `assets/` directory:

```text
VivaPinataTroubleInParadiseRecomp/
├── assets/
│   ├── default.xex
│   ├── Beta/
│   └── ...
├── src/
├── config/
└── CMakeLists.txt
```

### 2. Build with Visual Studio
1. Open Visual Studio.
2. Select **File -> Open -> Folder...** and choose the root directory of this repository.
3. Allow CMake generation to finish (on first configure, codegen will run against `assets/default.xex`).
4. Select the preset **`local-win-relwithdebinfo`** (or `local-win-release`).
5. Press **F7** to build `retip.exe`.

### 3. Launching
- Press **F5** in Visual Studio or run `retip.exe` directly from the build output directory.
- Use **F4** in-game to access graphics and overlay options.

---

## Credits & Acknowledgments
- **Rare Ltd. & Microsoft Game Studios**: Creators of *Viva Piñata*.
- **ReXGlue Project**: Static recompilation framework and runtime.
- **Project Xenia**: Groundbreaking Xbox 360 emulation research and shader pipelines.
- **Community Contributors**: Patches, fixes, and reverse-engineering contributions.

---

## Disclaimer
This project is an unofficial fan recompilation created for preservation and educational purposes. It is not affiliated with, endorsed by, or sponsored by Microsoft, Xbox, or Rare Ltd. No copyrighted game assets or proprietary code are distributed within this repository.
