// Compat.cpp - see Compat.h.
#include "tip_engine/Compat.h"

#include <sstream>
#include <system_error>

REXCVAR_DEFINE_STRING(mods_data_root, "", "MODS", "Folder with mods (default: <exe dir>/mods)");
REXCVAR_DEFINE_STRING(enabled_mods, "", "MODS", "Comma-separated list of enabled mods");
REXCVAR_DEFINE_STRING(default_mods, "", "MODS", "Comma-separated list of mods enabled by default");

// Declared by tip_engine/Overlays/TiPTools/SettingsMenu.h; the fork's SDK
// defined them for its SolarRenderer, which upstream 0.10 does not have.
REXCVAR_DEFINE_BOOL(OverlaySolarRenderer, false, "TiP/Renderer", "SolarRenderer overlay (fork only, inactive)");
REXCVAR_DEFINE_BOOL(SolarRendererPreview, false, "TiP/Renderer", "SolarRenderer preview (fork only, inactive)");
REXCVAR_DEFINE_BOOL(rgb_cursor, false, "TiP/Renderer", "RGB cursor (fork only, inactive)");

namespace tip_compat
{
  std::vector<std::filesystem::path> GetEnabledModDirs(const std::filesystem::path &mods_root)
  {
    std::vector<std::filesystem::path> dirs;
    std::stringstream list(REXCVAR_GET(enabled_mods));
    std::string name;
    while (std::getline(list, name, ','))
    {
      const auto first = name.find_first_not_of(" \t");
      const auto last = name.find_last_not_of(" \t");
      if (first == std::string::npos)
      {
        continue;
      }
      const std::filesystem::path dir = mods_root / name.substr(first, last - first + 1);
      std::error_code ec;
      if (std::filesystem::is_directory(dir, ec))
      {
        dirs.push_back(dir);
      }
    }
    return dirs;
  }
} // namespace tip_compat
