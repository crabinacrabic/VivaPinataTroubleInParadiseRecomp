# Porting ReTiP to ReXGlue SDK 0.10

> Date: 2026-09-26. Base: ReTiP (commit `757f550`). Port to upstream ReXGlue SDK 0.10.

## 1. The project

**ReTiP** is a recompilation of *Viva Piñata: Trouble in Paradise* (Xbox 360, 2008) on ReXGlue.
- The original has a launcher, TiPTools overlays (spawning piñatas, plants, player, sours, shovel, graphics, upscaling, settings), a mouse cursor and raw input, an FPS overlay, camera and render hooks (aspect ratio, frustum, letterbox), mods (textures, HLSL shaders, `.vdat` data) and Discord Rich Presence.
- Size: 56 source files, about 13,200 lines of C++; 24 mid-asm hooks; 46 named functions; 20 `[rexcrt]` entries (heap and file I/O).

### How the original was built
- On a fork of SDK 0.8.1. CI cloned and built the SDK from source.
- The code was already written against an API close to 0.10: `REX_HOOK`, `REX_EXTERN`, `REX_HOOK_RAW`, `rex::ppc::GuestToHostFunction`, `REX_DEFINE_APP`. The main porting work is the fork-only functions that upstream lacks.

### License
The original has **no license file**. By default that means "all rights reserved by the author". This code cannot be published in a new public repository without the author's permission, so the project is published as a GitHub fork of [SolarCookies/TiP-Recomp](https://github.com/SolarCookies/TiP-Recomp) (GitHub's terms allow that), keeping the author's whole commit history.

## 2. The game

| | |
| :-- | :-- |
| Disc (Redump) | `Viva Pinata - Trouble in Paradise (World).iso`, MD5 `aad50e5418e22e8a9b92d2c0c258079e` (matches Redump) |
| Title ID / Media ID | `4D53085F` / `76A44A5D` |
| XEX | version `0.0.0.4`, image at `0x82000000`, entry point `0x82B0AEA8`, SHA-1 `c5970941…6b8b` |
| Where | `assets/` (5.0 GB, unpacked) |

## 3. What this commit did (first pass)

| Area | Change |
| :-- | :-- |
| SDK | Prebuilt ReXGlue `0.10.0.8-dev.g1406e1b` in `rexglue/win-amd64`; `cmake/fetch-rexglue-sdk.cmake` downloads it if the folder is missing |
| CMake | `CMakeLists.txt` in the 0.10 style: SDK download, one codegen at the first configure, without the fork options `REXGLUE_ENABLE_TEXTURES/SHADERS`. After the build `packaged/` (retip.toml, mods, launcher wallpaper) is copied next to the exe; an existing `retip.toml` is not overwritten. `CMakePresets.json` and `generated/rexglue.cmake` come from the 0.10 template; `CMakeUserPresets.json` adds `local-win-relwithdebinfo` |
| Manifest | `sdk_version = "0.10.0"`; `config/retip_variables.toml` (`[globals]`, a fork feature) is no longer included; `tools/validate_manifest.py` passes clean |
| Codegen flags | All default (`false`) for the first run; the fork's values (`skip_lr`, `ctr_as_local` etc. = true) are kept in comments. Function boundaries marked "crash fix" are kept |
| Discord RPC | `tip_compat::discord_rpc` is a stub (`src/tip_engine/Compat.h`) |
| Mods | `tip_compat::GetEnabledModDirs` and the settings `mods_data_root`, `enabled_mods`, `default_mods` (`Compat.cpp`). Data mods (`mods/<name>/data/*.vdat`) work through the project's own `assetManOpen` hook. Texture and shader mods need the fork's GPU layer and are inactive for now |
| SolarRenderer | Settings `OverlaySolarRenderer`, `SolarRendererPreview`, `rgb_cursor` are defined off |
| `retip_globals.h` | Removed (the fork generated it); its only user already reads the address directly |
| Dialogs | In 0.10 `ImGuiDialog` registers itself in its constructor: 4 extra `AddDialog` calls removed, otherwise everything was drawn twice |
| Immediate drawer | In 0.10 `ImGuiDrawer` has no access to it: `g_immediate_drawer = immediate_drawer()` in `OnCreateDialogs`, used by the launcher |
| Paths | In 0.10 `game_data_root()` is filled only in `ConstructRuntime`, after the dialogs are created. The "game installed" check moved to `OnConfigurePaths`, otherwise the launcher thought the game was missing. `assets/` is searched upward from the exe folder, not the current folder |

## 4. What to check at the first build

1. **Compilation.** The port was not built at the time (Visual Studio only). Errors are possible where the 0.10 API differs from the fork; fix them from the build log.
2. **SDL3.** The launcher calls `SDL_OpenURL`. If the linker does not find SDL3, add the library explicitly.
3. **Mid-asm hooks** (24, `config/retip_midasm.toml`): all functions are in `src/`; their signatures must match the declarations 0.10 generates.
4. **Known SDK 0.10 bug:** `vpkd3d128` FLOAT16 writing to the same register (in place) loses the sign bit (rexglue-sdk issue #457). Add a mid-asm workaround if needed for mesh coordinate packing. (Later checked: TiP has no in-place `vpkd3d128`, see `AGENTS.md`.)
5. **CAFF inflated in place:** if `.bnl`/CAFF files are modified (a translation, for example), the stream must keep exactly its original size (see `compress_exact()` in `tools/tip_text.py`).

## 5. How to run

1. Visual Studio → **File → Open → Folder…** → this project's folder.
2. Wait for CMake generation to finish. The first time it runs codegen on `assets/default.xex`, a few minutes.
3. Choose the configuration **`local-win-relwithdebinfo`**, press **F7**.
4. Before the first **F5**: Debug → Exception Settings → Win32 Exceptions → untick `0xC0000005` (not crashes: the SDK's GPU memory protection).
5. **F5** opens the ReTiP launcher → PLAY.
