#include "Language.h"

#include <fstream>
#include <system_error>
#include <vector>

#include <rex/logging.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

REXCVAR_DEFINE_STRING(tip_language, "en", "TiP",
                      "Game text language: en, ru (ru needs assets/Beta/bundles/russian.bnl)");

namespace tip_language {
namespace {

constexpr const char* kBundle = "englishus.bnl";
// englishus.bnl from the tested disc (Title ID 4D53085F).
constexpr const char* kDiscSha1 = "6af790d0e5fd71e9e788283e8f59a32c3c6fa281";

std::filesystem::path BundleDir(const std::filesystem::path& game_root) {
  return game_root / "Beta" / "bundles";
}

std::string Sha1Hex(const std::filesystem::path& file) {
  std::string result;
#ifdef _WIN32
  std::ifstream in(file, std::ios::binary);
  if (!in) return result;
  BCRYPT_ALG_HANDLE alg = nullptr;
  if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0))) {
    return result;
  }
  BCRYPT_HASH_HANDLE hash = nullptr;
  if (BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0))) {
    std::vector<char> buffer(1 << 16);
    bool ok = true;
    while (ok && in) {
      in.read(buffer.data(), std::streamsize(buffer.size()));
      const std::streamsize n = in.gcount();
      if (n > 0) {
        ok = BCRYPT_SUCCESS(BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), ULONG(n), 0));
      }
    }
    UCHAR digest[20] = {};
    if (ok && BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0))) {
      static const char kHex[] = "0123456789abcdef";
      for (UCHAR b : digest) {
        result += kHex[b >> 4];
        result += kHex[b & 0xF];
      }
    }
    BCryptDestroyHash(hash);
  }
  BCryptCloseAlgorithmProvider(alg, 0);
#else
  (void)file;
#endif
  return result;
}

}  // namespace

bool RussianAvailable(const std::filesystem::path& game_root) {
  std::error_code ec;
  return !game_root.empty() && std::filesystem::is_regular_file(BundleDir(game_root) / "russian.bnl", ec);
}

void Apply(const std::filesystem::path& game_root) {
  namespace fs = std::filesystem;
  if (game_root.empty()) return;
  const fs::path dir = BundleDir(game_root);
  const fs::path current = dir / kBundle;
  const fs::path ru = dir / "russian.bnl";
  fs::path orig = current;
  orig += ".orig";

  const bool want_russian = REXCVAR_GET(tip_language) == "ru";
  const bool russian = want_russian && RussianAvailable(game_root);
  if (want_russian && !russian) {
    REXLOG_WARN("tip_language: ru requested, but {} is missing (tools/tip_text.py build)", ru.string());
  }

  std::error_code ec;
  if (!fs::exists(orig, ec)) {
    if (!russian) return;  // never switched: the disc file is in place
    if (Sha1Hex(current) != kDiscSha1) {
      REXLOG_WARN("tip_language: {} is not the disc file and has no .orig backup; left as is",
                  current.string());
      return;
    }
    if (!fs::copy_file(current, orig, ec)) {
      REXLOG_WARN("tip_language: cannot back up {}: {}", current.string(), ec.message());
      return;
    }
  }
  const fs::path& source = russian ? ru : orig;
  if (!fs::copy_file(source, current, fs::copy_options::overwrite_existing, ec)) {
    REXLOG_WARN("tip_language: cannot install {} as {}: {}", source.string(), current.string(),
                ec.message());
    return;
  }
  REXLOG_INFO("tip_language: game text {}", russian ? "Russian" : "English");
}

}  // namespace tip_language
