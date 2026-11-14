# AGENTS.md — instructions for AI agents

This file is written for AI coding agents (Claude Code, Codex, Cursor, …). A person has probably given you the link to this repository and asked you to install the game, or to work on the code.

- **Installing the game for a player** → section 1.
- **Changing the code of this project** → section 2.

Talk to the user in their own language. Many users of this project speak Russian.

---

## 1. Install Viva Piñata: Trouble in Paradise Recomp for a player (Windows)

**Goal:** build `out\build\local-win-relwithdebinfo\retip.exe` from this repository and the user's own game disc image, then start it.

### 1.0 Ground rules

- **Never download the game** or any game files from the internet. The game must come from the user's own disc image; ask for the path to their `.iso`.
- Ask before you start installers (Visual Studio, Git, Python) and before accepting licences.
- The project path must contain **only ASCII characters**, for example `C:\Games\VivaPinataTiPRecomp`. The ReXGlue code generator crashes with `0xC0000409` on paths with non-ASCII characters.
- Do not commit or redistribute `assets\` (except `assets\README.md`), `assets\Beta\bundles\russian.bnl`, `translation\work\` or `translation\check_report.txt`: they contain game text.
- Build the configuration **`local-win-relwithdebinfo`**. `local-win-debug` links the SDK's debug runtime, which crashes a few seconds after **PLAY** (section 1.9).

### 1.1 Check the machine

- Windows 10/11 x64, a GPU with Direct3D 12 (`dxdiag`; the Accurate render mode also needs Vulkan with fragment shader interlock), about 25 GB free: Visual Studio ~12 GB, game files ~5 GB, the ISO ~8 GB (it can be deleted after unpacking).

### 1.2 Install the tools

| Tool | How | Notes |
| :-- | :-- | :-- |
| Git | `winget install -e --id Git.Git` | |
| Visual Studio 2026 (v18) Community | `winget search Microsoft.VisualStudio`, take the 2026 Community package, then add `--override "--passive --wait --add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --add Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset --includeRecommended"` | Needs the C++ workload, clang (the *C++ Clang tools for Windows* component), CMake and Ninja (come with the workload). |
| extract-xiso | Download `extract-xiso-Win64_Release.zip` from https://github.com/XboxDev/extract-xiso/releases/latest and unzip it | Unpacks the Xbox 360 ISO |
| Python 3 | `winget install -e --id Python.Python.3.12` | Only for the Russian language and the dev tools |

### 1.3 Get the project

```powershell
git clone https://github.com/crabinacrabic/VivaPinataTroubleInParadiseRecomp.git C:\Games\VivaPinataTiPRecomp
```

### 1.4 Check and unpack the game (before the first build)

The ISO must be **Viva Pinata - Trouble in Paradise (World)**, Title ID `4D53085F`, Media ID `76A44A5D`, version `0.0.0.4`.

```powershell
(Get-FileHash "C:\path\to\game.iso" -Algorithm MD5).Hash      # expected AAD50E5418E22E8A9B92D2C0C258079E
extract-xiso -x -d C:\Games\VivaPinataTiPRecomp\assets "C:\path\to\game.iso"
(Get-FileHash C:\Games\VivaPinataTiPRecomp\assets\default.xex -Algorithm SHA1).Hash
                                                              # expected C5970941FCB201DDA99DE0D35B623827241D6B8B
Test-Path C:\Games\VivaPinataTiPRecomp\assets\Beta\bundles\englishus.bnl   # expected True
```

- `default.xex` and `Beta\` must be directly inside `assets\`. If extract-xiso created a subfolder, move its contents up.
- A different ISO hash is only a warning. A different `default.xex` SHA-1 means the recompiled code will not match: tell the user this edition is not supported.
- Unpack **before** the first CMake configure: configuring runs the code generator on `assets\default.xex`.

### 1.5 Build

On the first configure, CMake downloads ReXGlue SDK `0.10.0.30-dev.g8fdf638` into `rexglue\win-amd64\` ([`cmake/fetch-rexglue-sdk.cmake`](cmake/fetch-rexglue-sdk.cmake)): the GitHub release `red-edition-lava-20261102-8fdf638` of the fork [crabinacrabic/rexglue-sdk-RED-Edition-LAVA](https://github.com/crabinacrabic/rexglue-sdk-RED-Edition-LAVA), upstream ReXGlue plus Vulkan with the exact Xbox 360 EDRAM at full speed (the same SDK as Viva Piñata 1). If `rexglue\win-amd64\` holds another version, it is moved to `rexglue\win-amd64.old-<version>-<time>\` and the pinned one is downloaded; the old folder can be deleted. Then it runs `rexglue codegen retip_manifest.toml` once, which writes `generated\default\` (163 C++ files). Expect 10–40 minutes in total.

**A. Visual Studio (tested; ask the user to do this):**
1. Open Visual Studio 2026 → **File → Open → Folder…** → `C:\Games\VivaPinataTiPRecomp`.
2. Wait for "CMake generation finished" in the Output window.
3. In the toolbar, select the configuration **`local-win-relwithdebinfo`**.
4. **Build → Build All** (`F7`).

**B. Command line (only when the user asks you to build without the IDE):**

```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * `
      -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath
Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
# The presets use clang / clang++ from PATH; CMake and Ninja come with Visual Studio.
$env:PATH = "$vs\VC\Tools\Llvm\x64\bin;$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;" +
            "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"
Set-Location C:\Games\VivaPinataTiPRecomp
cmake --preset local-win-relwithdebinfo
cmake --build out/build/local-win-relwithdebinfo
```

Result: `out\build\local-win-relwithdebinfo\retip.exe`. The build also copies `packaged\` (`retip.toml`, `mods\`, the launcher wallpaper) next to it; an existing `retip.toml` is kept.

### 1.6 Run

Start `out\build\local-win-relwithdebinfo\retip.exe` (or `Ctrl+F5` in Visual Studio). It opens in a window. Settings are read from `retip.toml` next to the exe, the game from `assets\`, and logs go to `out\build\local-win-relwithdebinfo\logs\`.

| Launcher | Meaning |
| :-- | :-- |
| **PLAY**, or `Enter` | Start the game. Grey with "Game files not found" = `assets\` is wrong |
| **OPTIONS** | Aspect ratio, resolution (720p / 1440p), quality preset, Graphics → Render mode (**Fast** = Direct3D 12, the default / **Accurate** = Vulkan with the Xbox 360 EDRAM emulated exactly: ~31 FPS on the title screen on an RX 6600, Fast 63–72; switching restarts the game once), fullscreen, Show FPS, Lock FPS, Skip Intros, Language → Game text |
| Top-right button | Fullscreen / window |
| **Show this launcher on startup** | `ShowLaunchMenu` |

The launcher font is Latin-only, so its labels are English.

Command-line flags:
- `--ShowLaunchMenu=false` starts the game directly;
- `--tip_language=ru|en` sets the game text language;
- `--fullscreen=true|false`;
- `--graphics_backend=any|d3d12|vulkan` picks the GPU API (`any` = Direct3D 12 on Windows). The exact Xbox 360 EDRAM (as Viva Piñata 1's "Accurate" mode): `--graphics_backend=vulkan --render_target_path_vulkan=fsi --readback_resolve=lazy --edram_interlock_optimizations=true` (needs Vulkan with fragment shader interlock; tested only on an AMD RX 6600). The same keys work in `retip.toml`;
- `--tip_log_fps=5` logs the frame rate and the present pacing every 5 s (`[tip] ticks: …`, the format of Viva Piñata's `--vp_tools_log_ticks`).

In game: hold **Back** on the controller for 0.5 s for the ReTiP tools menu; `Shift+Esc` or `End` asks to quit.

### 1.7 Optional: Russian language

The translation (ZoG Team names and lines, used with the team's permission, plus lines translated with Google Gemini) is distributed as a pack that holds only the Russian strings: **[VivaPinataTiP_Russian_v1.zip](https://disk.yandex.ru/d/BMFSpKGapzwuaw)** on Yandex Disk (326 833 bytes, MD5 `afdc9351ec8ff10041b8068c8b88c56c`). It is not game data; ask the user before downloading it. Needs Python 3.

```powershell
cd C:\Games\VivaPinataTiPRecomp
$api = 'https://cloud-api.yandex.net/v1/disk/public/resources/download?public_key=' + [uri]::EscapeDataString('https://disk.yandex.ru/d/BMFSpKGapzwuaw')
Invoke-WebRequest (Invoke-RestMethod $api).href -OutFile $env:TEMP\VivaPinataTiP_Russian_v1.zip
(Get-FileHash $env:TEMP\VivaPinataTiP_Russian_v1.zip -Algorithm MD5).Hash   # expected AFDC9351EC8FF10041B8068C8B88C56C
Expand-Archive $env:TEMP\VivaPinataTiP_Russian_v1.zip -DestinationPath . -Force   # -> translation\tip_russian.json
python tools\tip_text.py build
# expected: "9019/9019 strings translated, 0 errors ..." and
#           "...russian.bnl: 15632 strings in Russian, 321 unchanged ..." (a few seconds)
```

Translators with the work files `translation\work\part_*.json` (gitignored) build from those instead; `tools\tip_text.py export` writes the pack from them.

Then in the launcher choose **OPTIONS → Language → Game text → Russian (fan translation)** and press **PLAY** (or start with `--tip_language=ru`). Before the game starts, the launcher copies `russian.bnl` over `Beta\bundles\englishus.bnl` (the file the game reads) and keeps the disc file as `englishus.bnl.orig` (backed up only if its SHA-1 is the disc's `6af790d0…`). Choosing English restores it.

### 1.8 Verify

- [ ] `assets\default.xex` has SHA-1 `C5970941…6D8B`, and `assets\Beta\` exists.
- [ ] `out\build\local-win-relwithdebinfo\retip.exe` exists.
- [ ] The launcher shows an active **PLAY** button.
- [ ] After **PLAY** and the intro videos, the title screen looks like [`docs/images/title_screen.webp`](docs/images/title_screen.webp) ("Press START", or «Жмите START» in Russian).
- [ ] After **START** the game reaches the garden, like [`docs/images/garden.webp`](docs/images/garden.webp).

### 1.9 Troubleshooting

| Symptom | Cause | Fix |
| :-- | :-- | :-- |
| CMake: `ReXGlue SDK not found` or `download failed` | No network, or a partial download | Delete `rexglue\win-amd64\` and configure again |
| Codegen exits with `0xC0000409` | Non-ASCII characters in the project path | Move the project to an ASCII path |
| Launcher: "Game files not found", **PLAY** grey | `assets` layout is wrong | `default.xex` and `Beta\` must be directly in `assets\` |
| `local-win-debug`: the game closes ~8 s after **PLAY**, `0xC000001D` in `rexruntimed.dll` | SDK bug: `D3D12ImmediateDrawer::Begin` keeps a `std::deque` iterator after `pop_back()` invalidated it; the debug STL traps, release builds survive | Build `local-win-relwithdebinfo` |
| `Microsoft Visual C/C++ Version differs in precompiled file`, or `file '…' has been modified since the precompiled header … was built` | Visual Studio or the Windows SDK was updated | Delete `out\build\local-win-relwithdebinfo` and build again |
| Accurate: a yellow warning under the render mode in OPTIONS, or the FPS overlay shows the mode in yellow (`D3D12 (Vulkan unavailable)`, `Vulkan, FBO (no FSI support)`) | No Vulkan, or no `VK_EXT_fragment_shader_interlock` on this GPU / driver (tested only on an AMD RX 6600). The game then runs another path instead of failing | Choose **Fast**; please report the GPU in an issue |
| Black window after **PLAY**, the log says `gpu_plugin not set`, every setting back to its default | `retip.toml` is not valid TOML any more: the SDK's `rex::cvar::SaveConfig` (the launcher saves on **PLAY**, the tools menu on Save) writes string values without escaping, so a Windows path given on the command line or in `retip.toml` (`--user_data_root=C:\…`) becomes `"C:\…"`, an invalid escape, and the next start ignores the whole file | Write paths with forward slashes (`C:/Games/…`); remove the broken line from `retip.toml` |
| CLI build: `clang++` not found | LLVM is not on PATH, or the component is missing | Add `<VS>\VC\Tools\Llvm\x64\bin` to PATH; install the *C++ Clang tools for Windows* component |
| Visual Studio breaks on `0xC0000005` under `F5` | The SDK's GPU write-watch (a handled first-chance AV) | Not a crash: untick Win32 Exceptions → `0xC0000005` in Exception Settings, or run without the debugger |
| `tip_text.py build`: `cannot build a …-byte text stream` | The text does not compress to the original size | Install [7-Zip](https://www.7-zip.org/) (its deflate encoder is tried after zlib), or shorten the longest lines |
| Russian text broken after editing a `.bnl` by hand | Rare CAFF streams are inflated in place | Use only `tools\tip_text.py build` (it builds exact-size streams) |

---

## 2. Working on the code (development rules)

The maintainer builds and runs everything in Visual Studio. The maintainer's working language is Russian; code, comments and commit messages are in English.

### 2.1 Rules

1. **Do not run builds or code generation from the terminal** (`cmake --build`, `ninja`, `rexglue codegen`, compilers) unless the user explicitly asks. Launching the already-built exe to test it is fine.
2. **`generated\` is read-only.** Codegen rewrites it whenever the manifest, any `config\*.toml` or the XEX changes. Fix things in one of two places:
   - `src\`: `REX_PPC_HOOK` / `REX_HOOK_RAW` overrides of guest functions (`src\tip_engine\*.cpp`, macros in `src\tip_engine\rex_macros.h`), mid-asm hook functions in `src\main.cpp`;
   - `config\*.toml`: function names and boundaries (`retip_hooks.toml`), `[[midasm_hook]]` entries (`retip_midasm.toml`), native CRT (`retip_crt.toml`).
3. After editing the manifest or `config\*.toml`, run `python tools\validate_manifest.py` before building.
4. Push only when the user asks. Never commit game files, `russian.bnl` or the translation work files.
5. Do not change the defaults in `packaged\retip.toml` (windowed start, `lock_fps` off, D3D12) unless the user asks.

### 2.2 Map

```
retip_manifest.toml          codegen manifest, includes config/*.toml
config/                      named functions, ctx flags, midasm hooks, native CRT, variables
src/main.cpp                 entry point, mid-asm hook functions
src/retip_app.h              ReXApp: asset path, input gating, dialogs, launcher gating, language install
src/tip_engine/              guest hooks (camera, cursor, graphics, sleep), Language.{h,cpp}, SDK 0.10 Compat,
                             RenderMode.h (Fast / Accurate, restart), VulkanFsiProbe.h (can this PC run Accurate)
src/tip_engine/Overlays/     launcher (LaunchMenu), quit menu, FPS, ReTiP tools pages
packaged/                    retip.toml defaults, mods/, launcher wallpaper; staged next to retip.exe
tools/tip_text.py            game text: extract / check / build (russian.bnl)
tools/validate_manifest.py   manifest pre-flight check
translation/GEMINI_BRIEF.md  translator brief; translation/work/ is gitignored
docs/PORTING_0.10.md         port from the original SDK fork to ReXGlue SDK 0.10
docs/images/                 README icon and screenshots
assets/  rexglue/  generated/  out/     not in git (except assets/README.md, generated/rexglue.cmake)
```

### 2.3 Facts worth knowing

- XEX: Title ID `4D53085F`, image `0x82000000–0x83FA0000`, code `0x821A0000–0x82B776C4`, entry `0x82B0AEA8`; 30 000 functions in 163 generated units.
- The project is [SolarCookies/TiP-Recomp](https://github.com/SolarCookies/TiP-Recomp), built originally on an SDK 0.8 fork; [`docs/PORTING_0.10.md`](docs/PORTING_0.10.md) lists what the 0.10 port changed (`tip_compat` stand-ins for fork-only APIs, dialogs, immediate drawer). Since then, guest pad input is gated with `InputSystem::SetActiveCallback` (`src\retip_app.h`) and the cursor with `Window::SetCursorVisibility` (`src\tip_engine\Globals.h`).
- Mods: only data mods (`mods\<name>\data\*.vdat`, through the `assetManOpen` hook) work on SDK 0.10. Texture and shader mods needed the fork's GPU layer and are inactive.
- With the SDK's defaults the game reads `Beta\bundles\englishus.bnl`. `english.bnl` has the same blocks and hashes but different UK strings and stream sizes; only `englishus.bnl` is swapped.
- **Text bundles are Rare CAFF files inflated in place.** The text stream must keep exactly its compressed size (stream 0 records it), and every LBSL block keeps its size, so a block's strings share its space. See `compress_exact()` and `relayout()` in `tools\tip_text.py`. The font cache already has Cyrillic.
- Unlike Viva Piñata 1, TiP has no in-place `vpkd3d128` (all its `vpkd3d128` have `vD != vB`), so the codegen sign-loss workaround is not needed.
- `Flag 'mnk_sensitivity': value 0 below min (0.01)` in the log is expected: the mouse drives the camera through `CursorHooks`, not the SDK's mouse-to-stick path.
- **Render modes** (`src\tip_engine\RenderMode.h`, the launcher's OPTIONS → Graphics): Accurate sets `graphics_backend = "vulkan"`, `render_target_path_vulkan = "fsi"`, `readback_resolve = "lazy"`, `edram_interlock_optimizations = true`, `vulkan_allow_present_mode_immediate = false`; Fast resets those keys to their defaults (Direct3D 12; readback stays off except while the in-game camera is open, the game's own `one_hook` / `two_hook`). The GPU API is chosen and the GPU system set up before the launcher appears, so `StartGame` saves `retip.toml` and, for another mode, starts the game again and closes: the new process skips the launcher once through the environment variable `RETIP_SKIP_LAUNCHER_ONCE` (not a cvar, because `rex::cvar::SaveConfig` writes every non-default value, command-line ones included). `VulkanFsiProbe.h` (the same check as Viva Piñata 1) warns in OPTIONS when Accurate can't work; the FPS overlay shows the mode as it really runs (`CommandProcessor::render_target_path()` of our SDK), yellow when Accurate fell back. `REX_VK_DISABLE_FSI=1` (environment) makes the SDK and the probe treat interlock as unsupported, to test the fallback.
- **GPU API and the exact EDRAM.** `graphics_backend` is this project's cvar: `RetipApp::OnPreSetup` loads the GPU plugin with it (`rex::system::LoadGpuPlugin`), otherwise the SDK picks Direct3D 12. Measured 2026-11-14 on an RX 6600, 1280×720 window, title screen ("Press START", the heaviest scene of the no-input attract loop), `--tip_log_fps=5`: Fast (Direct3D 12 RTV, readback off as the game sets it) 63–72 FPS (with `readback_resolve = "fast"` and before the repaint fix below: 47–57); Vulkan FSI + lazy readback + `edram_interlock_optimizations` (the exact EDRAM of our SDK) ~31 FPS at 96 % GPU, frames 31.8 ± 0.3 ms apart; the exact paths without our SDK's speedups: Vulkan FSI + full readback 5.6 FPS, Direct3D 12 ROV + full readback 7.6 FPS. No errors in any mode. The first Vulkan FSI run compiles its pipelines (stalls of up to 6.6 s in the first seconds); later runs load them from `<user data>\cache\shaders\shareable\4D53085F.fsi.vk.xpso`. Not checked yet: a per-pixel A/B in TiP, saving and loading, long play.
- **Dialogs that stay alive must return false from `NeedsContinuousRepaint()`** unless they need input at the display rate (the FPS overlay always; the launcher, the quit prompt and the tools menu only while open). Before this, the always-on FPS overlay made the host present 180–290 times per second: on Vulkan only ~20 of 31 game frames per second reached the screen (43 ± 42 ms apart). Keys read in `OnDraw` (`QuitMenu.cpp`) also use bit `0x0001` of `GetAsyncKeyState`, so a tap shorter than a frame still counts; only while the key was up at both checks, because Windows sets that bit again on every auto-repeat of a held key.
- The launcher and the tools menu save `retip.toml` to the path the SDK loads it from (`PathConfig::config_path`, `<exe folder>\retip.toml`, taken in `OnConfigurePaths`), not relative to the working directory: the render mode restart relies on the new process reading what the launcher saved.
- Benchmarks: `C:\Recompiles\Knowlage_BASE\rov_amd_scripts\tip_bench.ps1` (maintainer's PC) runs one mode without input and prints the frame rate per 5 s, present pacing and GPU load. The no-input loop (title screen, intro movie, demo garden) does not repeat at the same times, so compare runs by scene (screenshots: `-ShotAt`), not by time.
- Testing without the launcher: `retip.exe --ShowLaunchMenu=false --tip_language=en` (careful: `--tip_language` installs or restores `englishus.bnl` in `assets\`, so pass the language the user has chosen). Guest crashes show `Unhandled guest access violation` in the log; host crashes appear in the Windows Application event log, and in `%LOCALAPPDATA%\CrashDumps` when Windows Error Reporting local dumps are enabled.
