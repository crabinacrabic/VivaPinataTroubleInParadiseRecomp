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

Text format (same Rare CAFF container as Viva Pinata 1, see
tools/make_russian_bnl.py): the third zlib stream holds LBSL blocks, each an
entry table {u16 hash, u32 offset in UTF-16 chars} and big-endian UTF-16
strings. english.bnl and englishus.bnl share one layout (same blocks, same
hashes); the game reads englishus.bnl.

Length budget: the rebuilt bundle keeps every block at its original size (the
stream directory in stream 0 is not rewritten), so the strings of one block
share the block's space: sum(len(ru) + 1) must not exceed sum(len(en) + 1)
over the block. Strings may borrow room from each other within a block.

The work files hold game text and third-party translation text: they are
gitignored and must never be committed.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import struct
import sys
import zlib
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUNDLE = ROOT / "assets" / "Beta" / "bundles" / "englishus.bnl"
WORK = ROOT / "translation" / "work"
REPORT = ROOT / "translation" / "check_report.txt"

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
        self.text = zlib.decompressobj().decompress(self.raw[self.text_offset:])


def parse_blocks(data: bytes, big: bool) -> list[list[tuple[int, int, int, str]]]:
    """Blocks of (hash, byte offset, slot length in chars, text with terminator)."""
    tag = b"LBSL" if big else b"LSBL"
    e16, e32 = (">H", ">I") if big else ("<H", "<I")
    enc = "utf-16-be" if big else "utf-16-le"
    blocks = []
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
        strings = []
        for k, (h, o) in enumerate(entries):
            end = entries[k + 1][1] if k + 1 < count else total
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

def cmd_extract(args) -> int:
    if WORK.exists() and any(WORK.glob("part_*.json")) and not args.force:
        print(f"error: {WORK} already has work files; --force overwrites them (translations are lost)",
              file=sys.stderr)
        return 1
    blocks = parse_blocks(Caff(BUNDLE).text, True)

    zog: dict[str, set[str]] = defaultdict(set)
    if args.pc_ru and args.pc_en:
        rb = parse_blocks(Caff(args.pc_ru).text, False)
        eb = parse_blocks(Caff(args.pc_en).text, False)
        for b1, b2 in zip(eb, rb):
            for (_, _, _, en), (_, _, _, ru) in zip(b1, b2):
                if body(en):
                    zog[norm(en)].add(body(ru))

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


def cmd_check(args) -> int:
    entries, errors = load_work()
    warnings: list[str] = []
    blocks = parse_blocks(Caff(BUNDLE).text, True)
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
    for line in errors[:args.show]:
        print("error:", line)
    if len(errors) > args.show:
        print(f"... {len(errors) - args.show} more errors in {REPORT}")
    print(summary)
    print(f"full report: {REPORT}")
    return 1 if errors else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    ex = sub.add_parser("extract", help="write translation/work/part_NNN.json")
    ex.add_argument("--pc-ru", type=Path, help="Viva Pinata 1 PC bundles/english.bnl with the ZoG translation")
    ex.add_argument("--pc-en", type=Path, help="its original, Install_Rus/backup/bundles/english.bnl")
    ex.add_argument("--force", action="store_true", help="overwrite existing work files")
    ch = sub.add_parser("check", help="validate translation/work/*.json")
    ch.add_argument("--show", type=int, default=40, help="errors to print (all go to the report)")
    args = ap.parse_args()
    if not BUNDLE.exists():
        print(f"error: {BUNDLE} not found (unpack the game into assets/)", file=sys.stderr)
        return 1
    return cmd_extract(args) if args.cmd == "extract" else cmd_check(args)


if __name__ == "__main__":
    sys.exit(main())
