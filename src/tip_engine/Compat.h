// Compat.h - stand-ins for extended ReXGlue APIs that upstream ReXGlue SDK 0.10 does not have.
//
// What each piece replaces:
//   <rex/discord_rpc.h>      -> tip_compat::discord_rpc (no-op; no Discord in 0.10)
//   <rex/mods.h>             -> tip_compat::GetEnabledModDirs + mods_* cvars. Data mods
//                               (mods/<name>/data/*.vdat, injected by the assetManOpen
//                               hook) work; texture/shader mods needed the fork's GPU
//                               replacement layer and are inactive.
//   <rex/ppc/guest_global.h> -> nothing: the only [globals] user reads the address directly.
//   SolarRenderer cvars      -> defined here, off (the renderer was part of the fork).
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <rex/cvar.h>

REXCVAR_DECLARE(std::string, mods_data_root);
REXCVAR_DECLARE(std::string, enabled_mods);
REXCVAR_DECLARE(std::string, default_mods);

namespace tip_compat
{
  // Folders mods/<name> for every name in the comma-separated `enabled_mods`
  // cvar that exists under `mods_root`, in the listed order.
  std::vector<std::filesystem::path> GetEnabledModDirs(const std::filesystem::path &mods_root);

  namespace discord_rpc
  {
    struct Presence
    {
      std::string details_;
      std::string state_;
      std::string large_image_key_;
      std::string large_image_text_;
    };

    inline void Start(const char * /*application_id*/, const Presence & /*presence*/) {}
    inline void SetDetails(const std::string & /*details*/) {}
    inline void SetState(const std::string & /*state*/) {}
    inline void Shutdown() {}
  } // namespace discord_rpc
} // namespace tip_compat
