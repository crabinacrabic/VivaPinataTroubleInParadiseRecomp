// retip - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include <rex/rex_app.h>

#ifdef _WIN32
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif
#include "tip_engine/Globals.h"
#include "tip_engine/Compat.h"  // 0.10: discord_rpc stand-in
#include "tip_engine/Language.h"
#include <rex/filesystem.h>

#include "tip_engine/hooks.h"
#include "tip_engine/Overlays/Fps.h"
#include "tip_engine/Overlays/LaunchMenu.h"
#include "tip_engine/Overlays/QuitMenu.h"
#include "tip_engine/Overlays/TiPTools.h"
#include "tip_engine/Overlays/TiPTools/SpawnMenu.h"
#include "tip_engine/Overlays/TiPTools/GraphicsMenu.h"
#include "tip_engine/Overlays/TiPTools/UpscalingMenu.h"
#include "tip_engine/Overlays/TiPTools/SettingsMenu.h"
#include "tip_engine/Overlays/TiPTools/PinataMenu.h"
#include "tip_engine/Overlays/TiPTools/PlantMenu.h"
#include "tip_engine/Overlays/TiPTools/PlayerMenu.h"
#include "tip_engine/Overlays/TiPTools/RuffianMenu.h"
#include "tip_engine/Overlays/TiPTools/ShovelMenu.h"

class RetipApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<RetipApp>(new RetipApp(ctx, "retip", PPCImageConfig));
  }

  void OnPostSetup() override {
    tip_compat::discord_rpc::Presence rpc;

    rpc.details_ = "";
    rpc.state_ = "";
    rpc.large_image_key_ = "10979_viva_piata_trouble_in_paradise";
    rpc.large_image_text_ = "ReTiP";

    tip_compat::discord_rpc::Start("1497091207132876860", rpc);

    //optimization tom suggested
    timeBeginPeriod(1);

    //Force MnK driver on regardless of toml, since EXE may run from build dir without retip.toml
    rex::cvar::SetFlagByName("mnk_mode", "true");

    //Silence MnK's mouse->right-stick path; CursorHooks consumes mouse delta directly
    rex::cvar::SetFlagByName("mnk_sensitivity", "0");

    g_input_system = static_cast<rex::input::InputSystem*>(runtime()->input_system());

    // Replaces the fork's InputMode::kUIOnly: no guest pad input while a retip
    // menu owns the UI (SetRetipInputUiMode). Keeps the SDK's default checks.
    if (g_input_system) {
      g_input_system->SetActiveCallback([this]() {
        if (g_RetipInputUiMode) return false;
        if (window() && !window()->HasFocus()) return false;
        return !imgui_drawer() || !imgui_drawer()->GetIO().WantCaptureMouse;
      });
    }

    auto* w = window();
    if (!w) return;
    auto listener = std::make_unique<TipMouseListener>(w);
    w->AddInputListener(listener.get(), 1);
    g_mouse_listener = std::move(listener);

#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(w->GetNativeWindowHandle());
    auto raw = std::make_unique<TipRawMouse>();
    if (raw->Setup(hwnd)) {
      g_raw_mouse = std::move(raw);
    }
#endif
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    // SDK 0.10: ImGuiDialog registers itself in its constructor (no AddDialog),
    // and the immediate drawer is owned by ReXApp, not reachable from ImGuiDrawer.
    g_immediate_drawer = immediate_drawer();

    fps_dialog_ = std::make_unique<FpsOverlayDialog>(drawer);
    fps_dialog_->fpsManager = &fpsManager;

    // gameInstalled_ is set in OnConfigurePaths: in SDK 0.10 game_data_root()
    // is only filled in ConstructRuntime, after the dialogs are created.
    launch_dialog_ = std::make_unique<LaunchMenuDialog>(drawer, window(), "retip.toml", gameInstalled_, gameRoot_);

    quit_dialog_ = std::make_unique<QuitMenuDialog>(drawer, window());

    tools_dialog_ = std::make_unique<TipToolsDialog>(drawer, "retip.toml");
    tools_dialog_->pages.push_back(std::make_unique<SpawnMenuPage>());
    tools_dialog_->pages.push_back(std::make_unique<PlayerMenuPage>());
    tools_dialog_->pages.push_back(std::make_unique<PinataMenuPage>());
    tools_dialog_->pages.push_back(std::make_unique<PlantMenuPage>());
    tools_dialog_->pages.push_back(std::make_unique<RuffianMenuPage>());
    tools_dialog_->pages.push_back(std::make_unique<ShovelMenuPage>());
    tools_dialog_->pages.push_back(std::make_unique<GraphicsMenuPage>());
    tools_dialog_->pages.push_back(std::make_unique<UpscalingMenuPage>());
    tools_dialog_->pages.push_back(std::make_unique<SettingsMenuPage>());
  }

  void LaunchModule() override {
    // The text language is installed right before the game starts, so a
    // change in the launcher's options applies to this run.
    if (launch_dialog_ && (LaunchMenuDialog::WillShowOnStartup() || !gameInstalled_)) {
      launch_dialog_->SetOnClosed([this] {
        tip_language::Apply(gameRoot_);
        rex::ReXApp::LaunchModule();
      });
      return;
    }
    tip_language::Apply(gameRoot_);
    rex::ReXApp::LaunchModule();
  }

  void OnShutdown() override {
    if (launch_dialog_) launch_dialog_->ReleaseWallpaper();
  }

  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults,
      std::function<void(rex::PathConfig)> resume) override {
    (void)resume;
    if (!gameInstalled_) {
      return std::nullopt;
    }
    return defaults;
  }

  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;

  void OnConfigurePaths(rex::PathConfig &paths) override {
    if (paths.game_data_root.empty()) {
      // assets/ next to the exe (release zip), or in the repository root when
      // running from out/build/<preset>/: walk up from the exe folder, so it
      // does not depend on the working directory.
      std::filesystem::path dir = rex::filesystem::GetExecutableFolder();
      for (int i = 0; i < 6 && !dir.empty(); ++i) {
        if (std::filesystem::exists(dir / "assets" / "default.xex")) {
          paths.game_data_root = dir / "assets";
          break;
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) {
          break;
        }
        dir = dir.parent_path();
      }
    }
    // SDK 0.10: OnConfigurePaths runs before OnCreateDialogs, game_data_root()
    // only after ConstructRuntime - decide here whether the game is present.
    gameInstalled_ = !paths.game_data_root.empty() &&
                     std::filesystem::exists(paths.game_data_root / "default.xex");
    gameRoot_ = gameInstalled_ ? paths.game_data_root : std::filesystem::path();
  }

 private:
  bool gameInstalled_ = true;
  std::filesystem::path gameRoot_;
  std::unique_ptr<FpsOverlayDialog> fps_dialog_;
  std::unique_ptr<LaunchMenuDialog> launch_dialog_;
  std::unique_ptr<QuitMenuDialog> quit_dialog_;
  std::unique_ptr<TipToolsDialog> tools_dialog_;
};

REX_STUB(__imp__XUsbcamSetView)
REX_STUB(__imp__XUsbcamSetCaptureMode)
REX_STUB(__imp__XUsbcamSetConfig)
REX_STUB(__imp__XUsbcamReadFrame)
REX_STUB(__imp__XUsbcamDestroy)
REX_STUB(__imp__XUsbcamCreate)
//REX_STUB(__imp__XUsbcamGetState) in main.cpp I have a empty hook that silences the log that there is a stub