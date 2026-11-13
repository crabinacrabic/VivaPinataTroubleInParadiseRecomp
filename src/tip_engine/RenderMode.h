// RenderMode.h - the launcher's render mode (OPTIONS > Graphics), as in Viva Pinata 1:
// - Fast: Direct3D 12 with the PC GPU's own render targets, as before (the SDK
//   defaults; the game turns resolve readback on only for the in-game camera).
// - Accurate: Vulkan with the Xbox 360 EDRAM emulated exactly in the pixel shader
//   (fragment shader interlock), the exact speedups of our SDK build
//   (edram_interlock_optimizations) and lazy readback (the same resolve data as a
//   full sync, copied when the CPU reads it). About 31 FPS on the title screen on an
//   RX 6600, where Fast runs 63-72 (2026-11-14). Presented with vsync (FIFO
//   relaxed), as Viva Pinata 1's Accurate mode.
// The GPU API is chosen before the launcher appears (RetipApp::OnPreSetup) and the
// GPU system is set up right after it, so another mode takes effect after a
// restart: StartGame saves retip.toml and starts the game once more, skipping the
// launcher (RestartSkippingLauncher).
#pragma once

#include <string>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <rex/cvar.h>
#include <rex/graphics/command_processor.h>
#include <rex/graphics/graphics_system.h>
#include <rex/logging.h>
#include <rex/runtime.h>

namespace tip_render {

enum Mode : int { kFast = 0, kAccurate = 1 };

// What Accurate sets; Fast resets the same keys to their defaults.
inline constexpr std::pair<const char*, const char*> kAccurateValues[] = {
    {"graphics_backend", "vulkan"},
    {"render_target_path_vulkan", "fsi"},
    {"readback_resolve", "lazy"},
    {"edram_interlock_optimizations", "true"},
    {"vulkan_allow_present_mode_immediate", "false"},
};

// The mode the settings ask for now (Vulkan FSI = Accurate, anything else = Fast).
inline Mode CurrentSetting() {
  return rex::cvar::Query<std::string>("graphics_backend") == "vulkan" &&
                 rex::cvar::Query<std::string>("render_target_path_vulkan") == "fsi"
             ? kAccurate
             : kFast;
}

inline void Select(Mode mode) {
  for (const auto& [key, value] : kAccurateValues) {
    if (mode == kAccurate) {
      rex::cvar::SetFlagByName(key, value);
    } else {
      rex::cvar::ResetToDefault(key);
    }
  }
}

// Set by RetipApp::OnPreSetup: the mode and API the GPU plugin was loaded with.
inline Mode& Active() {
  static Mode mode = kFast;
  return mode;
}
inline std::string& ActiveBackend() {
  static std::string backend = "d3d12";
  return backend;
}
// Vulkan was asked for but could not be used: the SDK fell back to Direct3D 12.
inline bool& BackendFallback() {
  static bool fallback = false;
  return fallback;
}

// What actually runs, for the FPS overlay. degraded: Accurate was chosen, but the
// PC cannot run it (no Vulkan, or no fragment shader interlock).
inline std::string ActualText(bool& degraded) {
  degraded = false;
  if (BackendFallback()) {
    degraded = true;
    return "D3D12 (Vulkan unavailable)";
  }
  using Path = rex::graphics::CommandProcessor::RenderTargetPath;
  rex::Runtime* runtime = rex::Runtime::instance();
  auto* graphics = runtime ? static_cast<rex::graphics::GraphicsSystem*>(runtime->graphics_system()) : nullptr;
  const rex::graphics::CommandProcessor* processor = graphics ? graphics->command_processor() : nullptr;
  const bool vulkan = ActiveBackend() == "vulkan";
  switch (processor ? processor->render_target_path() : Path::kUnknown) {
    case Path::kPixelShaderInterlock:
      return vulkan ? "Vulkan, exact EDRAM (FSI)" : "D3D12, exact EDRAM (ROV)";
    case Path::kHostRenderTargets:
      if (Active() == kAccurate) {
        degraded = true;
        return "Vulkan, FBO (no FSI support)";
      }
      return vulkan ? "Vulkan, FBO" : "D3D12, RTV";
    default:
      return vulkan ? "Vulkan" : "D3D12";
  }
}

#if defined(_WIN32)
// Set in the environment of a game started by RestartSkippingLauncher. Not a cvar:
// retip.toml would keep it (SaveConfig writes every non-default value).
inline constexpr wchar_t kSkipLauncherVariable[] = L"RETIP_SKIP_LAUNCHER_ONCE";
#endif

// Starts the game again with the same command line, skipping the launcher once.
// The caller closes this instance.
inline bool RestartSkippingLauncher() {
#if defined(_WIN32)
  wchar_t exe[MAX_PATH];
  if (!GetModuleFileNameW(nullptr, exe, MAX_PATH)) {
    return false;
  }
  std::wstring command_line = GetCommandLineW();
  STARTUPINFOW startup_info = {};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info = {};
  SetEnvironmentVariableW(kSkipLauncherVariable, L"1");
  const BOOL started = CreateProcessW(exe, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                                      &startup_info, &process_info);
  SetEnvironmentVariableW(kSkipLauncherVariable, nullptr);
  if (!started) {
    REXLOG_ERROR("launcher: restart failed ({})", GetLastError());
    return false;
  }
  CloseHandle(process_info.hThread);
  CloseHandle(process_info.hProcess);
  REXLOG_INFO("launcher: render mode changed, the game restarts");
  return true;
#else
  return false;
#endif
}

// True in a game started by RestartSkippingLauncher (read once, then cleared, so
// a game started from this one shows the launcher again).
inline bool SkipLauncherOnce() {
  static const bool skip = [] {
#if defined(_WIN32)
    wchar_t value[4] = {};
    const bool set = GetEnvironmentVariableW(kSkipLauncherVariable, value, 4) > 0;
    if (set) {
      SetEnvironmentVariableW(kSkipLauncherVariable, nullptr);
    }
    return set;
#else
    return false;
#endif
  }();
  return skip;
}

}  // namespace tip_render
