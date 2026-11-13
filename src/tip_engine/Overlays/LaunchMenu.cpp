#include "LaunchMenu.h"
#include "tip_engine/Globals.h"
#include "tip_engine/Language.h"
#include "tip_engine/RenderMode.h"
#include "tip_engine/VulkanFsiProbe.h"
#include "tip_engine/version.h"
#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/ui/image_decode.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <optional>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

REXCVAR_DEFINE_BOOL(ShowLaunchMenu, true, "TiP", "Show the launcher on startup");

REXCVAR_DECLARE(bool, EnableShadows);
REXCVAR_DECLARE(bool, EnableReflections);
REXCVAR_DECLARE(bool, EnableOcclusions);
REXCVAR_DECLARE(bool, EnableTransparency);
REXCVAR_DECLARE(bool, EnableModels);
REXCVAR_DECLARE(bool, EnableSceneEffects);
REXCVAR_DECLARE(bool, EnableBloom);
REXCVAR_DECLARE(bool, SkipIntros);
REXCVAR_DECLARE(bool, UseAspectRatioFromConfig);
REXCVAR_DECLARE(double, AspectRatio);
REXCVAR_DECLARE(bool, fullscreen);
REXCVAR_DECLARE(bool, show_fps);
REXCVAR_DECLARE(bool, lock_fps);

#define CVAR_CHECKBOX(label, cvar) { bool v = REXCVAR_GET(cvar); if (ImGui::Checkbox(label, &v)) REXCVAR_SET(cvar, v); }

enum AspectChoice { kAspect16x9 = 0, kAspect16x10, kAspect21x9, kAspect32x9, kAspectNative, kAspectCustom, kAspectCount };
static constexpr const char* kAspectNames[kAspectCount] = {
    "16:9 Widescreen", "16:10 Widescreen", "21:9 Widescreen", "32:9 Widescreen", "Native", "Custom"
};
static double AspectRatioForChoice(int choice) {
    switch (choice) {
        case kAspect16x10: return 16.0 / 10.0;
        case kAspect21x9:  return 21.0 / 9.0;
        case kAspect32x9:  return 32.0 / 9.0;
        default:           return 16.0 / 9.0;
    }
}

static constexpr const char* kResolutionNames[2] = { "720p", "1440p" };
static constexpr int kResolutionScales[2] = { 1, 2 };

struct QualityPreset {
    bool shadows, reflections, occlusions, transparency, models, sceneEffects, bloom;
};
static constexpr const char* kQualityNames[4] = { "Low", "Medium", "High", "Ultra" };
static constexpr QualityPreset kQualityPresets[4] = {
    /* Low */ { false, false, false, true, true, false, false },
    /* Medium */ { false, false, true,  true, true, false, true  },
    /* High */ { true,  false, true,  true, true, false, true  },
    /* Ultra */ { true,  true,  true,  true, true, true,  true  },
};

static void ApplyQualityPreset(int index) {
    const QualityPreset& p = kQualityPresets[index];
    REXCVAR_SET(EnableShadows, p.shadows);
    REXCVAR_SET(EnableReflections, p.reflections);
    REXCVAR_SET(EnableOcclusions, p.occlusions);
    REXCVAR_SET(EnableTransparency, p.transparency);
    REXCVAR_SET(EnableModels, p.models);
    REXCVAR_SET(EnableSceneEffects, p.sceneEffects);
    REXCVAR_SET(EnableBloom, p.bloom);
}

static int DetectQualityPreset() {
    for (int i = 0; i < 4; i++) {
        const QualityPreset& p = kQualityPresets[i];
        if (REXCVAR_GET(EnableShadows) == p.shadows &&
            REXCVAR_GET(EnableReflections) == p.reflections &&
            REXCVAR_GET(EnableOcclusions) == p.occlusions &&
            REXCVAR_GET(EnableTransparency) == p.transparency &&
            REXCVAR_GET(EnableModels) == p.models &&
            REXCVAR_GET(EnableSceneEffects) == p.sceneEffects &&
            REXCVAR_GET(EnableBloom) == p.bloom) {
            return i;
        }
    }
    return 3;
}

// OPTIONS > Graphics > Render mode (tip_engine/RenderMode.h). The launcher font is
// Latin-only, so the text stays in English.
static constexpr const char* kRenderModeNames[2] = {
    "Fast (Direct3D 12)",
    "Accurate (Vulkan, exact Xbox 360 EDRAM)",
};
static constexpr const char* kRenderModeNote =
    "Accurate emulates the Xbox 360 video memory exactly, as on the console, and needs a stronger GPU "
    "(an RX 6600 runs about 31 FPS on the title screen, Fast 63-72). Changing the mode restarts the game once.";

// Whether Accurate can work on this PC (VulkanFsiProbe.h). Started the first time
// OPTIONS shows the render mode, so players who never open it don't load the Vulkan
// driver; nullptr until the worker thread is done. The thread is detached: PLAY
// never waits for a slow driver.
namespace {
struct FsiProbeSlot {
    std::atomic<bool> done{false};
    tip_render::VulkanFsiProbe probe;  // written by the probe thread before done
};
std::shared_ptr<FsiProbeSlot> g_fsiProbeSlot;
std::optional<tip_render::VulkanFsiProbe> g_fsiProbe;

const tip_render::VulkanFsiProbe* FsiProbe() {
    if (!g_fsiProbeSlot) {
        // An unregistered cvar reads as "": automatic device selection then.
        const std::string device = rex::cvar::Query<std::string>("vulkan_device");
        const int32_t deviceIndex = device.empty() ? -1 : std::atoi(device.c_str());
        g_fsiProbeSlot = std::make_shared<FsiProbeSlot>();
        std::thread([slot = g_fsiProbeSlot, deviceIndex] {
            slot->probe = tip_render::ProbeVulkanFsi(deviceIndex);
            slot->done.store(true, std::memory_order_release);
        }).detach();
    }
    if (!g_fsiProbe && g_fsiProbeSlot->done.load(std::memory_order_acquire)) {
        g_fsiProbe = g_fsiProbeSlot->probe;
        static constexpr const char* kResultNames[] = {"unknown", "no Vulkan", "no interlock", "supported"};
        REXLOG_INFO("launcher: Vulkan FSI probe: {} ({})", kResultNames[int(g_fsiProbe->result)], g_fsiProbe->device);
    }
    return g_fsiProbe ? &*g_fsiProbe : nullptr;
}
}  // namespace

static constexpr float kPad = 24.0f;
static constexpr float kBarHeight = 116.0f;
static constexpr float kWallpaperVerticalShift = 160.0f;
static constexpr float kBtnHeight = 46.0f;
static constexpr float kRowBottomPad = 22.0f;
static constexpr float kCheckboxGap = 10.0f;

bool LaunchMenuDialog::WillShowOnStartup() {
    // Not after a restart for another render mode: the player already pressed PLAY.
    return REXCVAR_GET(ShowLaunchMenu) && !tip_render::SkipLauncherOnce();
}

void LaunchMenuDialog::ReleaseWallpaper() {
    wallpaper_.reset();
    wallpaperPixels_.clear();
    wallpaperPixels_.shrink_to_fit();
    icon_.reset();
}

void LaunchMenuDialog::EnsureIcon() {
    if (icon_ || iconLoadFailed_ || !g_immediate_drawer) return;
    auto path = rex::filesystem::GetExecutableFolder() / "retip" / "icon.png";
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        iconLoadFailed_ = true;
        return;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    int w = 0, h = 0;
    std::vector<uint8_t> pixels = rex::ui::DecodeImageRGBA(bytes.data(), bytes.size(), w, h);
    if (pixels.empty()) {
        iconLoadFailed_ = true;
        return;
    }
    icon_ = g_immediate_drawer->CreateTexture(w, h, rex::ui::ImmediateTextureFilter::kLinear, false, pixels.data());
    iconLoadFailed_ = !icon_;
}

void LaunchMenuDialog::EnsureWallpaper() {
    if (wallpaper_ || wallpaperLoadFailed_) return;

    if (wallpaperPixels_.empty()) {
        auto path = rex::filesystem::GetExecutableFolder() / "retip" / "wallpaper.png";
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            wallpaperLoadFailed_ = true;
            return;
        }
        std::vector<uint8_t> bytes(static_cast<size_t>(file.tellg()));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());

        wallpaperPixels_ = rex::ui::DecodeImageRGBA(bytes.data(), bytes.size(), wallpaperW_, wallpaperH_);
        if (wallpaperPixels_.empty()) {
            wallpaperLoadFailed_ = true;
            return;
        }
    }

    auto* immediate = g_immediate_drawer;  // SDK 0.10: set by RetipApp::OnCreateDialogs
    if (!immediate) return;
    wallpaper_ = immediate->CreateTexture(wallpaperW_, wallpaperH_, rex::ui::ImmediateTextureFilter::kLinear, false, wallpaperPixels_.data());
    if (wallpaper_) {
        wallpaperPixels_.clear();
        wallpaperPixels_.shrink_to_fit();
    }
}

void LaunchMenuDialog::SyncOptionsFromCVars() {
    if (REXCVAR_GET(UseAspectRatioFromConfig)) {
        double ar = REXCVAR_GET(AspectRatio);
        if (fabs(ar - AspectRatioForChoice(kAspect16x10)) < 0.01) aspectIndex_ = kAspect16x10;
        else if (fabs(ar - AspectRatioForChoice(kAspect21x9)) < 0.01) aspectIndex_ = kAspect21x9;
        else if (fabs(ar - AspectRatioForChoice(kAspect32x9)) < 0.01) aspectIndex_ = kAspect32x9;
        else { aspectIndex_ = kAspectCustom; customAspectRatio_ = static_cast<float>(ar); }
    } else {
        aspectIndex_ = kAspect16x9;
    }

    resolutionIndex_ = (rex::cvar::Query<int32_t>("resolution_scale") >= kResolutionScales[1]) ? 1 : 0;
    qualityIndex_ = DetectQualityPreset();
    russianAvailable_ = tip_language::RussianAvailable(gameRoot_);
}

double LaunchMenuDialog::EffectiveAspectRatio() const {
    if (aspectIndex_ == kAspectCustom) return static_cast<double>(customAspectRatio_);
    if (aspectIndex_ == kAspect16x9 || aspectIndex_ == kAspectNative) return 16.0 / 9.0;
    return AspectRatioForChoice(aspectIndex_);
}

void LaunchMenuDialog::ResizeWindowToAspect(double aspectRatio) {
#ifdef _WIN32
    if (!window_ || window_->IsFullscreen()) return;
    HWND hwnd = reinterpret_cast<HWND>(window_->GetNativeWindowHandle());
    if (!hwnd || IsZoomed(hwnd)) return;

    int height = kResolutionScales[resolutionIndex_] * 720;
    int width = static_cast<int>(std::lround(height * aspectRatio));

    RECT rect{0, 0, width, height};
    DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
#endif
}

void LaunchMenuDialog::StartGame() {
    REXCVAR_SET(ShowLaunchMenu, showOnStartup);
    rex::cvar::SaveConfig(configPath_);
    visible_ = false;
    optionsOpen_ = false;
    g_LaunchMenuOpen = false;
    // Another render mode: the GPU API and the render target path were set up
    // before the launcher appeared, so the game starts once more in the new mode
    // (retip.toml is saved above) and this instance closes without starting it.
    if (tip_render::CurrentSetting() != tip_render::Active() && tip_render::RestartSkippingLauncher()) {
        onClosed_ = nullptr;
        if (window_) window_->RequestClose();
        return;
    }
    if (onClosed_) {
        auto cb = std::move(onClosed_);
        onClosed_ = nullptr;
        cb();
    }
}

static bool BrandButton(const char* label, ImVec4 color, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 1.15f, color.y * 1.15f, color.z * 1.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 0.85f, color.y * 0.85f, color.z * 0.85f, 1.0f));
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

void LaunchMenuDialog::OnDraw(ImGuiIO& io) {
    if (!initialized_) {
        initialized_ = true;
        showOnStartup = REXCVAR_GET(ShowLaunchMenu);
        visible_ = WillShowOnStartup();
    }

    g_LaunchMenuOpen = visible_;
    if (!visible_) {
        ReleaseWallpaper();
        return;
    }

    EnsureWallpaper();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##ReTiPLauncher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
    ImGui::PopStyleVar(3);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 disp = io.DisplaySize;

    if (wallpaper_) {
        float scale = std::max(disp.x / (float)wallpaperW_, disp.y / (float)wallpaperH_);
        ImVec2 size(wallpaperW_ * scale, wallpaperH_ * scale);
        float posY = (disp.y - size.y) * 0.5f - kWallpaperVerticalShift;
        posY = std::clamp(posY, disp.y - size.y, 0.0f);
        ImVec2 pos((disp.x - size.x) * 0.5f, posY);
        dl->AddImage(reinterpret_cast<ImTextureID>(wallpaper_.get()), pos, ImVec2(pos.x + size.x, pos.y + size.y));
    } else {
        dl->AddRectFilled(ImVec2(0, 0), disp, ImColor(14, 16, 20, 255));
    }

    if (window_) {
        const ImVec2 fsSize(120.0f, 34.0f);
        const char* fsLabel = window_->IsFullscreen() ? "Windowed" : "Fullscreen";
        ImGui::SetCursorPos(ImVec2(disp.x - kPad - fsSize.x, kPad));
        if (BrandButton(fsLabel, ImVec4(0.0f, 0.0f, 0.0f, 0.55f), fsSize)) {
            rex::cvar::SetFlagByName("fullscreen", window_->IsFullscreen() ? "false" : "true");
        }
    }

    float barTop = disp.y - kBarHeight;
    dl->AddRectFilledMultiColor(ImVec2(0, barTop - 70.0f), ImVec2(disp.x, barTop), ImColor(0, 0, 0, 0), ImColor(0, 0, 0, 0), ImColor(0, 0, 0, 200), ImColor(0, 0, 0, 200));
    dl->AddRectFilled(ImVec2(0, barTop), disp, ImColor(0, 0, 0, 200));

    EnsureIcon();
    float titleX = kPad;
    if (icon_) {
        const float iconSize = kBarHeight - 28.0f;
        const ImVec2 iconPos(kPad, barTop + (kBarHeight - iconSize) * 0.5f);
        dl->AddImage(reinterpret_cast<ImTextureID>(icon_.get()), iconPos,
                     ImVec2(iconPos.x + iconSize, iconPos.y + iconSize));
        titleX = kPad + iconSize + 16.0f;
    }
    ImGui::SetCursorPos(ImVec2(titleX, barTop + 20.0f));
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextUnformatted("ReTiP");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SetCursorPos(ImVec2(titleX + 2.0f, barTop + 58.0f));
    ImGui::TextDisabled("Version %s", retipversion.c_str());

    float rowY = disp.y - kRowBottomPad - kBtnHeight;

    const float kOptionsGap = 16.0f;
    const ImVec2 playSize(170.0f, kBtnHeight);
    const ImVec2 optionsSize(170.0f, kBtnHeight);
    float playX = disp.x - kPad - playSize.x;
    float optionsX = playX - kOptionsGap - optionsSize.x;

    ImGui::SetCursorPos(ImVec2(playX, rowY));
    ImGui::SetWindowFontScale(1.15f);
    ImGui::BeginDisabled(!gameInstalled_);
    bool play = BrandButton("PLAY", ImVec4(0.0f, 0.62f, 0.36f, 1.0f), playSize);
    ImGui::EndDisabled();
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetCursorPos(ImVec2(optionsX, rowY));
    ImGui::SetWindowFontScale(1.15f);
    if (BrandButton("OPTIONS", ImVec4(0.22f, 0.45f, 0.60f, 1.0f), optionsSize)) {
        bool wasOpen = optionsOpen_;
        optionsOpen_ = !optionsOpen_;
        if (optionsOpen_ && !wasOpen) SyncOptionsFromCVars();
    }
    ImGui::SetWindowFontScale(1.0f);

    float checkboxY = rowY - ImGui::GetFrameHeightWithSpacing() - kCheckboxGap;
    if (gameInstalled_) {
        ImGui::SetCursorPos(ImVec2(playX, checkboxY));
        if (ImGui::Checkbox("Show this launcher on startup", &showOnStartup)) {
            REXCVAR_SET(ShowLaunchMenu, showOnStartup);
        }
    } else {
        static constexpr const char* kMissing =
            "Game files not found: unpack your disc into assets/ (see assets/README.md)";
        ImGui::SetCursorPos(ImVec2(disp.x - kPad - ImGui::CalcTextSize(kMissing).x, checkboxY));
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", kMissing);
    }

    ImGui::End();

    DrawOptionsWindow(io);

    if (gameInstalled_ && (play || ImGui::IsKeyPressed(ImGuiKey_Enter))) {
        StartGame();
    }
}

void LaunchMenuDialog::DrawOptionsWindow(ImGuiIO& io) {
    if (!optionsOpen_) return;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.45f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("Options", &optionsOpen_,ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse)) {
        static constexpr float kItemWidth = 240.0f;

        ImGui::SeparatorText("Display");

        ImGui::TextUnformatted("Aspect Ratio");
        ImGui::SetNextItemWidth(kItemWidth);
        if (ImGui::Combo("##AspectRatio", &aspectIndex_, kAspectNames, kAspectCount)) {
            if (aspectIndex_ == kAspect16x9 || aspectIndex_ == kAspectNative) {
                REXCVAR_SET(UseAspectRatioFromConfig, false);
            } else if (aspectIndex_ == kAspectCustom) {
                REXCVAR_SET(UseAspectRatioFromConfig, true);
                REXCVAR_SET(AspectRatio, static_cast<double>(customAspectRatio_));
            } else {
                REXCVAR_SET(UseAspectRatioFromConfig, true);
                REXCVAR_SET(AspectRatio, AspectRatioForChoice(aspectIndex_));
            }
            rex::cvar::SetFlagByName("fullscreen_span_monitors", aspectIndex_ == kAspect32x9 ? "true" : "false");
            if (aspectIndex_ != kAspectNative) ResizeWindowToAspect(EffectiveAspectRatio());
        }

        if (aspectIndex_ == kAspectCustom) {
            ImGui::SetNextItemWidth(kItemWidth);
            if (ImGui::InputFloat("##CustomAspectRatio", &customAspectRatio_, 0.0f, 0.0f, "%.4f")) {
                if (customAspectRatio_ < 0.5f) customAspectRatio_ = 0.5f;
                if (customAspectRatio_ > 4.0f) customAspectRatio_ = 4.0f;
                REXCVAR_SET(AspectRatio, static_cast<double>(customAspectRatio_));
                ResizeWindowToAspect(customAspectRatio_);
            }
        }

        ImGui::TextUnformatted("Resolution");
        ImGui::SetNextItemWidth(kItemWidth);
        if (ImGui::Combo("##Resolution", &resolutionIndex_, kResolutionNames, 2)) {
            rex::cvar::SetFlagByName("resolution_scale", std::to_string(kResolutionScales[resolutionIndex_]));
            if (aspectIndex_ != kAspectNative) ResizeWindowToAspect(EffectiveAspectRatio());
        }

        ImGui::TextUnformatted("Quality");
        ImGui::SetNextItemWidth(kItemWidth);
        if (ImGui::Combo("##Quality", &qualityIndex_, kQualityNames, 4)) {
            ApplyQualityPreset(qualityIndex_);
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Graphics");
        ImGui::TextUnformatted("Render mode");
        static constexpr float kModeWidth = 360.0f;
        int mode = tip_render::CurrentSetting();
        ImGui::SetNextItemWidth(kModeWidth);
        if (ImGui::Combo("##RenderMode", &mode, kRenderModeNames, 2)) {
            tip_render::Select(static_cast<tip_render::Mode>(mode));
        }
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kModeWidth + 60.0f);
        ImGui::TextDisabled("%s", kRenderModeNote);
        const tip_render::VulkanFsiProbe* probe = FsiProbe();
        if (mode == tip_render::kAccurate && probe) {
            using Result = tip_render::VulkanFsiProbe::Result;
            if (probe->result == Result::kNoVulkan) {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s",
                                   "No Vulkan on this PC: Accurate would run on Direct3D 12 without the exact "
                                   "EDRAM. Choose Fast instead.");
            } else if (probe->result == Result::kNoInterlock) {
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
                                   "%s has no fragment shader interlock: Accurate would run on Vulkan without "
                                   "the exact EDRAM. Choose Fast instead.",
                                   probe->device.c_str());
            }
        }
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::SeparatorText("General");

        if (window_) {
            bool fs = window_->IsFullscreen();
            if (ImGui::Checkbox("Fullscreen", &fs)) {
                rex::cvar::SetFlagByName("fullscreen", fs ? "true" : "false");
            }
        }
        CVAR_CHECKBOX("Show FPS", show_fps);
        CVAR_CHECKBOX("Lock FPS", lock_fps);
        CVAR_CHECKBOX("Skip Intros", SkipIntros);

        // Applied when the game starts (tip_language::Apply in RetipApp::LaunchModule).
        // The launcher font is Latin-only, so the names stay in English.
        ImGui::Spacing();
        ImGui::SeparatorText("Language");
        static constexpr const char* kEnglish = "English";
        static constexpr const char* kRussian = "Russian (fan translation)";
        const bool russian = russianAvailable_ && REXCVAR_GET(tip_language) == "ru";
        ImGui::TextUnformatted("Game text");
        ImGui::SetNextItemWidth(kItemWidth);
        if (ImGui::BeginCombo("##GameText", russian ? kRussian : kEnglish)) {
            if (ImGui::Selectable(kEnglish, !russian)) {
                rex::cvar::SetFlagByName("tip_language", "en");
            }
            if (ImGui::Selectable(kRussian, russian, russianAvailable_ ? 0 : ImGuiSelectableFlags_Disabled)) {
                rex::cvar::SetFlagByName("tip_language", "ru");
            }
            ImGui::EndCombo();
        }
        if (!russianAvailable_) {
            ImGui::TextDisabled("Russian: build russian.bnl with tools/tip_text.py build");
        }
    }
    ImGui::End();
}
