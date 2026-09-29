#!/usr/bin/env python3
"""tip_text.py - Russian translation workflow for Viva Pinata: Trouble in Paradise.

Subcommands
-----------
  extract  Unique strings of assets/Beta/bundles/englishus.bnl, in game order,
           -> translation/work/part_NNN.json. With --pc-ru/--pc-en, strings
           that Viva Pinata 1 (PC) also has are prefilled from the ZoG Team
           Russian translation (src "zog"); translation/work/glossary.json
           lists its short terms.
  check    Validates translation/work/*.json: JSON shape, markup tokens,
           leading/trailing whitespace, characters of the game font, and the
           per-block length budget (see below). Writes
           translation/check_report.txt. Exit 0 = no errors.
  build    Runs the checks, then writes assets/Beta/bundles/russian.bnl: the
           englishus.bnl text stream with every block re-laid out around the
           Russian strings, recompressed to its exact original size (7-Zip is
           tried when zlib is not enough). The launcher installs it over
           englishus.bnl. Without work files it builds from the translation
           pack translation/tip_russian.json (or --pack FILE).
  export   Runs the checks, then writes the translation pack for players:
           only the Russian strings, keyed by id and a CRC32 of the English
           string, so it contains no game text.

Text format (same Rare CAFF container as Viva Pinata 1, see
tools/make_russian_bnl.py in Viva Pinata Recomp): the third zlib stream holds LBSL blocks, each an
entry table {u16 hash, u32 offset in UTF-16 chars} and big-endian UTF-16
strings. english.bnl and englishus.bnl share one layout (same blocks, same
hashes); the game reads englishus.bnl.

Length budget: the rebuilt bundle keeps every block at its original size (the
stream directory in stream 0 is not rewritten), so the strings of one block
share the block's space: sum(len(ru) + 1) must not exceed sum(len(en) + 1)
over the block. Strings may borrow room from each other within a block: build
rewrites the block's offset table and puts the spare characters as extra
terminators after its last string.

The text stream must also keep its exact compressed size: stream 0 records it
and the game inflates the stream in place (see compress_exact()).

The work files hold game text and third-party translation text: they are
gitignored and must never be committed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUNDLES = ROOT / "assets" / "Beta" / "bundles"
BUNDLE = BUNDLES / "englishus.bnl"
# The launcher replaces englishus.bnl with the Russian file and keeps the
# original as englishus.bnl.orig; the tools always read the original.
BUNDLE_ORIG = BUNDLES / "englishus.bnl.orig"
OUTPUT = BUNDLES / "russian.bnl"
WORK = ROOT / "translation" / "work"
REPORT = ROOT / "translation" / "check_report.txt"
PACK = ROOT / "translation" / "tip_russian.json"
PACK_FORMAT = "tip-russian-1"
PACK_CREDITS = [
    "Names and about 4400 lines: ZoG Team (zoneofgames.ru), Russian translation of Viva Pinata (PC, 2007), "
    "used with the team's permission",
    "Other lines: translated with Google Gemini for Viva Pinata: Trouble in Paradise Recomp",
]

# englishus.bnl from the tested disc (Redump, Title ID 4D53085F).
ENGLISHUS_SHA1 = "6af790d0e5fd71e9e788283e8f59a32c3c6fa281"

MAGIC = b"CAFF07.08.06.003"
PART_SIZE = 400

# Markup the game interprets: HTML-like tags, button/icon/name tokens,
# placeholders, printf formats, HTML entities. Tags are matched whole, so
# <font color=[RED]> is one token.
TOKEN_RE = re.compile(r"<[^<>]*>|\[[^\[\]]*\]|\{[^{}]*\}|%[-+ #0-9.]*[sdifuxXc%]|&[a-zA-Z]+;|&#\d+;")

# Characters the Russian text may use (all present in fontcacheabc/02dd2188,
# the englishus font), plus any character of the English source string.
ALLOWED = (set(chr(c) for c in range(0x20, 0x7F))
           | set(chr(c) for c in range(0x410, 0x450)) | {"Ё", "ё"}
           | set("«»—–…№“”„’‘ \n\t"))


# --- CAFF / LBSL --------------------------------------------------------------

class Caff:
    """Rare CAFF container: header, stream0, stream1, stream2 (text)."""

    def __init__(self, path: Path):
        self.raw = path.read_bytes()
        if not self.raw.startswith(MAGIC):
            raise ValueError(f"{path}: not a CAFF file")
        self.big = self.raw[0x48] == 1  # 1 = big-endian (Xbox 360), 0 = PC
        e32 = ">I" if self.big else "<I"
        header_end = struct.unpack_from(e32, self.raw, 0x14)[0]
        comp0 = struct.unpack_from(e32, self.raw, 0x60)[0]
        comp1 = struct.unpack_from(e32, self.raw, 0x74)[0]
        self.text_offset = header_end + comp0 + comp1
        if self.raw[self.text_offset:self.text_offset + 1] != b"\x78":
            raise ValueError(f"{path}: no zlib stream at 0x{self.text_offset:X}")
        self.text = zlib.decompressobj().decompress(self.raw[self.text_offset:])
        self.text_size = len(self.raw) - self.text_offset  # compressed size the game expects


def source_bundle() -> Path:
    return BUNDLE_ORIG if BUNDLE_ORIG.exists() else BUNDLE


def block_tables(data: bytes, big: bool) -> list[tuple[int, int, int, list[tuple[int, int]]]]:
    """Per LBSL block: (entry table offset, string base, total chars, [(hash, char offset)])."""
    tag = b"LBSL" if big else b"LSBL"
    e16, e32 = (">H", ">I") if big else ("<H", "<I")
    tables = []
    pos = 0
    while (t := data.find(tag, pos)) >= 0:
        pos = t + 4
        count = struct.unpack_from(e32, data, t + 0x20)[0]
        ent = t + 0x24
        if count > 20000 or struct.unpack_from(e16, data, ent + 6 * count)[0] != 0xFFFF:
            continue
        total = struct.unpack_from("<I", data, ent + 6 * count + 2)[0]  # LE on both platforms
        base = ent + 6 * (count + 1)
        entries = [(struct.unpack_from(e16, data, ent + 6 * k)[0],
                    struct.unpack_from(e32, data, ent + 6 * k + 2)[0]) for k in range(count)]
        tables.append((ent, base, total, entries))
    return tables


def parse_blocks(data: bytes, big: bool) -> list[list[tuple[int, int, int, str]]]:
    """Blocks of (hash, byte offset, slot length in chars, text with terminator)."""
    enc = "utf-16-be" if big else "utf-16-le"
    blocks = []
    for _, base, total, entries in block_tables(data, big):
        strings = []
        for k, (h, o) in enumerate(entries):
            end = entries[k + 1][1] if k + 1 < len(entries) else total
            a, b = base + 2 * o, base + 2 * end
            strings.append((h, a, end - o, data[a:b].decode(enc, errors="replace")))
        blocks.append(strings)
    return blocks


def body(s: str) -> str:
    """Visible text: without the terminator (and ZoG's space padding)."""
    return s.rstrip("\x00").rstrip(" ")


def norm(s: str) -> str:
    s = body(s).strip()
    for a, b in (("ñ", "n"), ("Ñ", "N"), ("’", "'"), ("‘", "'"), ("“", '"'), ("”", '"'),
                 ("…", "..."), (" ", " ")):
        s = s.replace(a, b)
    return " ".join(s.split()).lower()


# --- checks -------------------------------------------------------------------

def tokens(s: str) -> Counter:
    c = Counter(TOKEN_RE.findall(s))
    c["\\n"] = s.count("\n")
    c["\\t"] = s.count("\t")
    return +c


def edges(s: str) -> tuple[str, str]:
    return s[:len(s) - len(s.lstrip())], s[len(s.rstrip()):]


def string_errors(en: str, ru: str) -> list[str]:
    errs = []
    if tokens(en) != tokens(ru):
        missing, extra = tokens(en) - tokens(ru), tokens(ru) - tokens(en)
        errs.append("markup differs"
                    + (f"; missing {dict(missing)}" if missing else "")
                    + (f"; extra {dict(extra)}" if extra else ""))
    if edges(en) != edges(ru):
        errs.append(f"leading/trailing whitespace differs: en {edges(en)!r}, ru {edges(ru)!r}")
    bad = sorted({ch for ch in ru if ch not in ALLOWED and ch not in en})
    if bad:
        errs.append("characters not in the font: " + " ".join(f"U+{ord(c):04X}" for c in bad))
    return errs


def soft_limit(en: str) -> int:
    return max(len(en) + 2, math.ceil(len(en) * 1.10))


# --- extract ------------------------------------------------------------------

def unique_strings(blocks) -> tuple[list[str], dict[str, dict]]:
    """Unique non-empty strings in game order; string n has the id t{n:05d}."""
    order: list[str] = []
    info: dict[str, dict] = {}
    for blk in blocks:
        texts = [body(t) for _, _, _, t in blk]
        for i, en in enumerate(texts):
            if not en:
                continue
            if en in info:
                info[en]["uses"] += 1
                continue
            ctx = [t[:100] for t in (texts[i - 1] if i > 0 else "", texts[i + 1] if i + 1 < len(texts) else "")
                   if t]
            info[en] = {"uses": 1, "ctx": ctx}
            order.append(en)
    return order, info


def cmd_extract(args) -> int:
    if WORK.exists() and any(WORK.glob("part_*.json")) and not args.force:
        print(f"error: {WORK} already has work files; --force overwrites them (translations are lost)",
              file=sys.stderr)
        return 1
    blocks = parse_blocks(Caff(source_bundle()).text, True)

    zog: dict[str, set[str]] = defaultdict(set)
    if args.pc_ru and args.pc_en:
        rb = parse_blocks(Caff(args.pc_ru).text, False)
        eb = parse_blocks(Caff(args.pc_en).text, False)
        for b1, b2 in zip(eb, rb):
            for (_, _, _, en), (_, _, _, ru) in zip(b1, b2):
                if body(en):
                    zog[norm(en)].add(body(ru))

    order, info = unique_strings(blocks)

    entries, prefilled, hints, glossary = [], 0, 0, {}
    for n, en in enumerate(order, 1):
        e = {"id": f"t{n:05d}", "en": en, "ru": "", "src": "", "limit": soft_limit(en),
             "uses": info[en]["uses"], "ctx": info[en]["ctx"], "note": ""}
        cand = zog.get(norm(en), set())
        if len(cand) == 1:
            ru = next(iter(cand))
            if not string_errors(en, ru):
                e["ru"], e["src"] = ru, "zog"
                prefilled += 1
                if len(en) <= 30 and not TOKEN_RE.search(en) and en[:1].isupper():
                    glossary[en] = ru
            else:
                e["hint"] = ru
                hints += 1
        entries.append(e)

    WORK.mkdir(parents=True, exist_ok=True)
    for old in WORK.glob("part_*.json"):
        old.unlink()
    for k in range(0, len(entries), PART_SIZE):
        path = WORK / f"part_{k // PART_SIZE + 1:03d}.json"
        path.write_text(json.dumps(entries[k:k + PART_SIZE], ensure_ascii=False, indent=1) + "\n",
                        encoding="utf-8", newline="\n")
    if glossary:
        (WORK / "glossary.json").write_text(json.dumps(dict(sorted(glossary.items())), ensure_ascii=False,
                                                       indent=1) + "\n", encoding="utf-8", newline="\n")
    parts = math.ceil(len(entries) / PART_SIZE)
    print(f"{len(entries)} unique strings in {parts} files under {WORK}; "
          f"{prefilled} prefilled from ZoG, {hints} with a ZoG hint only, glossary {len(glossary)} terms")
    return 0


# --- check --------------------------------------------------------------------

def load_work() -> tuple[list[dict], list[str]]:
    entries, errors = [], []
    files = sorted(WORK.glob("part_*.json"))
    if not files:
        errors.append(f"no work files in {WORK} (run extract first)")
    for path in files:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as ex:
            errors.append(f"{path.name}: invalid JSON: {ex}")
            continue
        if not isinstance(data, list):
            errors.append(f"{path.name}: top level must be a list")
            continue
        for e in data:
            if not isinstance(e, dict) or not all(isinstance(e.get(k), str) for k in ("id", "en", "ru")):
                errors.append(f"{path.name}: entry without string id/en/ru: {str(e)[:80]}")
                continue
            e["_file"] = path.name
            entries.append(e)
    return entries, errors


def run_checks(entries: list[dict], errors: list[str], show: int) -> int:
    warnings: list[str] = []
    blocks = parse_blocks(Caff(source_bundle()).text, True)
    game_texts = {body(t) for b in blocks for _, _, _, t in b if body(t)}

    by_en: dict[str, dict] = {}
    ids = Counter(e["id"] for e in entries)
    for i, n in ids.items():
        if n > 1:
            errors.append(f"{i}: duplicate id")
    translated = 0
    for e in entries:
        where = f"{e['_file']} {e['id']}"
        en, ru = e["en"], e["ru"]
        if en not in game_texts:
            errors.append(f"{where}: 'en' was changed (not found in the game): {en[:60]!r}")
            continue
        by_en[en] = e
        if not ru:
            continue
        translated += 1
        errors += [f"{where}: {m}" for m in string_errors(en, ru)]
        if len(ru) > e.get("limit", soft_limit(en)):
            warnings.append(f"{where}: {len(ru)} chars, soft limit {e.get('limit', soft_limit(en))}")
    missing = game_texts - by_en.keys()
    if missing:
        errors.append(f"{len(missing)} game strings are missing from the work files")

    over = []
    for bi, blk in enumerate(blocks):
        budget = sum(n for _, _, n, _ in blk)
        need, ids_in = 0, []
        for _, _, n, t in blk:
            en = body(t)
            e = by_en.get(en)
            if not en:
                need += n
            else:
                need += len((e["ru"] if e else "") or en) + 1
                if e and e["ru"]:
                    ids_in.append(e["id"])
        if need > budget:
            over.append((need - budget, bi, budget, ids_in))
    over.sort(reverse=True)
    for extra, bi, budget, ids_in in over:
        errors.append(f"block {bi}: {extra} chars over its budget of {budget} "
                      f"(shorten some of: {', '.join(ids_in[:12])}{' ...' if len(ids_in) > 12 else ''})")

    total = len(by_en)
    summary = (f"{translated}/{total} strings translated, {len(errors)} errors "
               f"({len(over)} blocks over budget), {len(warnings)} soft-limit warnings")
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    with REPORT.open("w", encoding="utf-8", newline="\n") as f:
        f.write(summary + "\n\nERRORS\n" + "\n".join(errors) + "\n\nWARNINGS\n" + "\n".join(warnings) + "\n")
    for line in errors[:show]:
        print("error:", line)
    if len(errors) > show:
        print(f"... {len(errors) - show} more errors in {REPORT}")
    print(summary)
    print(f"full report: {REPORT}")
    return 1 if errors else 0


def cmd_check(args) -> int:
    entries, errors = load_work()
    return run_checks(entries, errors, args.show)


# --- translation pack -----------------------------------------------------------
# The file players download: only the Russian strings, keyed by id plus a CRC32
# of the English string (no game text). build --pack rebuilds the English side
# from the player's own englishus.bnl and verifies every CRC.

def en_crc(en: str) -> str:
    return f"{zlib.crc32(en.encode('utf-8')):08x}"


def cmd_export(args) -> int:
    entries, errors = load_work()
    if run_checks(entries, errors, args.show) != 0:
        print("error: fix the errors above before exporting", file=sys.stderr)
        return 1
    strings = [{"id": e["id"], "crc": en_crc(e["en"]), "ru": e["ru"]}
               for e in entries if e["ru"] and e["ru"] != e["en"]]
    pack = {
        "format": PACK_FORMAT,
        "game": "Viva Pinata: Trouble in Paradise (Xbox 360), Title ID 4D53085F",
        "source": "Beta/bundles/englishus.bnl",
        "source_sha1": ENGLISHUS_SHA1,
        "credits": PACK_CREDITS,
        "strings": strings,
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(pack, ensure_ascii=False, indent=1) + "\n", encoding="utf-8", newline="\n")
    print(f"{args.out}: {len(strings)} Russian strings")
    return 0


def entries_from_pack(path: Path) -> tuple[list[dict], list[str]]:
    """Work-file entries rebuilt from a pack and the player's englishus.bnl."""
    try:
        pack = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as ex:
        return [], [f"{path}: cannot read the translation pack: {ex}"]
    if not isinstance(pack, dict) or pack.get("format") != PACK_FORMAT:
        return [], [f"{path}: not a {PACK_FORMAT} translation pack"]
    src = source_bundle()
    if hashlib.sha1(src.read_bytes()).hexdigest() != pack.get("source_sha1"):
        print(f"warning: {src} is not the englishus.bnl the pack was made for; "
              "strings are matched by id and checksum", file=sys.stderr)
    order, _ = unique_strings(parse_blocks(Caff(src).text, True))
    by_id = {f"t{n:05d}": en for n, en in enumerate(order, 1)}
    ru_by_id, errors = {}, []
    for s in pack.get("strings", []):
        en = by_id.get(s.get("id"))
        if en is None or en_crc(en) != s.get("crc"):
            errors.append(f"{path.name} {s.get('id')}: does not match this englishus.bnl")
            continue
        ru_by_id[s["id"]] = s.get("ru", "")
    entries = [{"id": i, "en": en, "ru": ru_by_id.get(i, ""), "_file": path.name} for i, en in by_id.items()]
    return entries, errors


# --- build --------------------------------------------------------------------

def find_7zip() -> str | None:
    for p in (shutil.which("7z"), r"C:\Program Files\7-Zip\7z.exe", r"C:\Program Files (x86)\7-Zip\7z.exe"):
        if p and Path(p).exists():
            return p
    return None


def raw_deflate_zlib(data: bytes) -> bytes:
    c = zlib.compressobj(9, zlib.DEFLATED, -15, 9)
    return c.compress(data) + c.flush()


def raw_deflate_7zip(data: bytes, seven_zip: str) -> bytes | None:
    """Raw deflate stream from 7-Zip's encoder (gzip container stripped)."""
    with tempfile.TemporaryDirectory() as tmp:
        src, gz = Path(tmp) / "text.bin", Path(tmp) / "text.gz"
        src.write_bytes(data)
        r = subprocess.run([seven_zip, "a", "-tgzip", "-mx9", str(gz), str(src)], capture_output=True)
        if r.returncode != 0 or not gz.exists():
            return None
        b = gz.read_bytes()
    if b[:3] != b"\x1f\x8b\x08":
        return None
    flags, i = b[3], 10
    if flags & 4:
        i += 2 + struct.unpack_from("<H", b, i)[0]
    for bit in (8, 16):  # file name, comment
        if flags & bit:
            i = b.index(b"\x00", i) + 1
    if flags & 2:
        i += 2
    return b[i:-8]


def stored_blocks(data: bytes) -> bytes:
    """Non-final deflate stored blocks (byte-aligned, 5 bytes of header each)."""
    out = bytearray()
    for i in range(0, len(data), 65535):
        chunk = data[i:i + 65535]
        out += b"\x00" + struct.pack("<HH", len(chunk), len(chunk) ^ 0xFFFF) + chunk
    return bytes(out)


EMPTY_STORED_BLOCK = b"\x00\x00\x00\xff\xff"


def compress_exact(data: bytes, size: int) -> bytes | None:
    """A zlib stream of exactly `size` bytes (same as tools/make_russian_bnl.py
    in Viva Pinata Recomp).

    The game inflates the text stream in place: the compressed bytes sit at the
    end of the output buffer and the output catches up with them only at the
    very end. A shorter stream padded with zeros ends early, so its last bytes
    are overwritten before they are read (garbage text, crashes). The stream
    must fill the region exactly: the first k bytes of text go into stored
    (uncompressed) blocks, the rest is deflated, and empty stored blocks absorb
    the last few bytes. Stored data up front keeps the in-place order safe.
    Cyrillic UTF-16 compresses worse than English, so 7-Zip's deflate encoder
    is needed; zlib -9 is tried first."""
    encoders = [("zlib", raw_deflate_zlib)]
    seven_zip = find_7zip()
    if seven_zip:
        encoders.append(("7-Zip", lambda d: raw_deflate_7zip(d, seven_zip)))
    trailer = struct.pack(">I", zlib.adler32(data))
    for name, encode in encoders:
        cache: dict[int, int] = {}

        def total(k: int) -> int:
            if k not in cache:
                deflated = encode(data[k:])
                cache[k] = (1 << 62) if deflated is None else \
                    2 + len(stored_blocks(data[:k])) + len(deflated) + 4
            return cache[k]

        if total(0) > size:
            print(f"  {name}: {total(0)} bytes, {total(0) - size} over the {size}-byte stream")
            continue
        # total(k) grows ~0.8 byte per byte moved into the stored prefix.
        lo, hi = 0, min(len(data), int((size - total(0)) / 0.6) + 1)
        while total(hi) <= size and hi < len(data):
            lo, hi = hi, min(len(data), hi * 2)
        while hi - lo > 1:  # largest k with total(k) <= size
            mid = (lo + hi) // 2
            if total(mid) <= size:
                lo = mid
            else:
                hi = mid
        for k in range(lo, max(-1, lo - 200), -1):
            if total(k) > size or (size - total(k)) % 5:
                continue
            payload = stored_blocks(data[:k]) + encode(data[k:])
            stream = b"\x78\x9c" + EMPTY_STORED_BLOCK * ((size - total(k)) // 5) + payload + trailer
            if len(stream) == size and zlib.decompress(stream) == data:
                print(f"  {name}: {k} bytes stored, {len(stream)} bytes total")
                return stream
    print(f"error: cannot build a {size}-byte text stream"
          + ("" if seven_zip else " (install 7-Zip: zlib alone is not enough for Cyrillic)"),
          file=sys.stderr)
    return None


def relayout(text: bytes, by_en: dict[str, str]) -> tuple[bytes, list[list[str]], Counter]:
    """The text stream with the Russian strings; every block keeps its size."""
    out = bytearray(text)
    expected: list[list[str]] = []
    stats: Counter = Counter()
    for ent, base, total, entries in block_tables(text, True):
        strings = []
        for k, (h, o) in enumerate(entries):
            end = entries[k + 1][1] if k + 1 < len(entries) else total
            raw = text[base + 2 * o:base + 2 * end].decode("utf-16-be")
            en = body(raw)
            if not en:
                strings.append(raw)  # empty slot: keep as is
                continue
            ru = by_en.get(en, "")
            stats["translated" if ru else "english"] += 1
            strings.append((ru or en) + "\x00")
        used = sum(len(s) for s in strings)
        if used > total:
            raise ValueError(f"block at 0x{ent:X}: {used} chars, budget {total}")
        strings[-1] += "\x00" * (total - used)
        off = 0
        for k, s in enumerate(strings):
            struct.pack_into(">I", out, ent + 6 * k + 2, off)
            off += len(s)
        out[base:base + 2 * total] = "".join(strings).encode("utf-16-be")
        expected.append(strings)
    return bytes(out), expected, stats


def cmd_build(args) -> int:
    # Work files (translators) first; otherwise the downloaded pack (players).
    pack = args.pack
    if pack is None and not any(WORK.glob("part_*.json")) and PACK.exists():
        pack = PACK
    if pack is not None:
        print(f"using the translation pack {pack}")
        entries, errors = entries_from_pack(pack)
    else:
        entries, errors = load_work()
    if run_checks(entries, errors, args.show) != 0:
        print("error: fix the errors above before building", file=sys.stderr)
        return 1
    src = source_bundle()
    caff = Caff(src)
    if hashlib.sha1(caff.raw).hexdigest() != ENGLISHUS_SHA1:
        print(f"warning: {src} is not the tested englishus.bnl; continuing", file=sys.stderr)
    by_en = {e["en"]: e["ru"] for e in entries if e["ru"]}

    text, expected, stats = relayout(caff.text, by_en)
    if len(text) != len(caff.text):
        print("error: text stream changed size", file=sys.stderr)
        return 1
    print(f"packing {len(text)} bytes of text into {caff.text_size} bytes...")
    packed = compress_exact(text, caff.text_size)
    if packed is None:
        return 1
    result = caff.raw[:caff.text_offset] + packed

    # Self-check: same file size, same block tables (hashes, totals), every
    # string reads back as written.
    back = zlib.decompressobj().decompress(result[caff.text_offset:])
    shape = lambda d: [(ent, base, total, [h for h, _ in e]) for ent, base, total, e in block_tables(d, True)]
    got = [[t for _, _, _, t in b] for b in parse_blocks(back, True)]
    if len(result) != len(caff.raw) or back != text or shape(back) != shape(caff.text) or got != expected:
        print("error: self-check failed", file=sys.stderr)
        return 1

    args.out.write_bytes(result)
    print(f"{args.out}: {stats['translated']} strings in Russian, {stats['english']} unchanged "
          "(names and credits that stay the same, or untranslated)")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    ex = sub.add_parser("extract", help="write translation/work/part_NNN.json")
    ex.add_argument("--pc-ru", type=Path, help="Viva Pinata 1 PC bundles/english.bnl with the ZoG translation")
    ex.add_argument("--pc-en", type=Path, help="its original, Install_Rus/backup/bundles/english.bnl")
    ex.add_argument("--force", action="store_true", help="overwrite existing work files")
    ch = sub.add_parser("check", help="validate translation/work/*.json")
    ch.add_argument("--show", type=int, default=40, help="errors to print (all go to the report)")
    bu = sub.add_parser("build", help="check, then write assets/Beta/bundles/russian.bnl")
    bu.add_argument("--show", type=int, default=40, help="errors to print (all go to the report)")
    bu.add_argument("--out", type=Path, default=OUTPUT, help=f"output file (default {OUTPUT})")
    bu.add_argument("--pack", type=Path,
                    help=f"build from a translation pack (default: {PACK} when there are no work files)")
    exp = sub.add_parser("export", help="check, then write the translation pack (Russian strings only)")
    exp.add_argument("--show", type=int, default=40, help="errors to print (all go to the report)")
    exp.add_argument("--out", type=Path, default=PACK, help=f"output file (default {PACK})")
    args = ap.parse_args()
    if not source_bundle().exists():
        print(f"error: {BUNDLE} not found (unpack the game into assets/)", file=sys.stderr)
        return 1
    return {"extract": cmd_extract, "check": cmd_check, "build": cmd_build,
            "export": cmd_export}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
