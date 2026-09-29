// Language.h - game text language
//
// The game reads its text from Beta\bundles\englishus.bnl. tools/tip_text.py
// build writes a Russian bundle next to it as russian.bnl (same schema, same
// size). Apply() copies it over englishus.bnl and keeps the disc file as
// englishus.bnl.orig; choosing English copies the backup back. A backup is
// made only from a file whose SHA-1 matches the disc, so a replaced file is
// never mistaken for the original.
#pragma once

#include <filesystem>
#include <string>

#include <rex/cvar.h>

REXCVAR_DECLARE(std::string, tip_language);

namespace tip_language {

bool RussianAvailable(const std::filesystem::path& game_root);

// Installs the bundle for the tip_language cvar ("en" / "ru"). Call before the
// game module starts.
void Apply(const std::filesystem::path& game_root);

}  // namespace tip_language
