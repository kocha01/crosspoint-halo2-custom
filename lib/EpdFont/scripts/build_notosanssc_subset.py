#!/usr/bin/env python3

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


CYRILLIC_UI_CHARS = (
    "ІАБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЫЬЭЯ"
    "абвгдежзийклмнопрстуфхцчшщыьэюяёєіїўғ"
    "ҚқҢңҮүҰұӘәӨө"
)

EXTRA_CHARS = (
    "，。、！？：；（）《》〈〉「」『』【】"
    "—…·・　％＋－～"
    "↺↻‑ţŢ"
)


def _load_common_sc_chars(raw_list_path: Path) -> str:
    capture = False
    chars: list[str] = []
    seen: set[str] = set()

    for line in raw_list_path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if stripped == "1000 characters":
            capture = True
            continue
        if not capture:
            continue

        for ch in line:
            if ch.isspace():
                continue
            if ch not in seen:
                chars.append(ch)
                seen.add(ch)

    return "".join(chars)


def _ordered_union(*parts: str) -> str:
    chars: list[str] = []
    seen: set[str] = set()
    for part in parts:
        for ch in part:
            if ch.isspace():
                continue
            if ch not in seen:
                chars.append(ch)
                seen.add(ch)
    return "".join(chars)


def _load_cjk_chars_from_filenames(*roots: Path) -> str:
    chars: list[str] = []
    seen: set[str] = set()

    for root in roots:
        if not root.exists():
            continue

        for path in sorted(root.iterdir()):
            for ch in path.name:
                if "\u4E00" <= ch <= "\u9FFF" and ch not in seen:
                    chars.append(ch)
                    seen.add(ch)

    return "".join(chars)


def main() -> int:
    pyftsubset = shutil.which("pyftsubset")
    if not pyftsubset:
        print("pyftsubset not found in PATH", file=sys.stderr)
        return 1

    script_dir = Path(__file__).resolve().parent
    source_dir = (script_dir / "../builtinFonts/source/NotoSansSC").resolve()
    raw_regular = source_dir / "NotoSansSC-Regular.otf"
    raw_bold = source_dir / "NotoSansSC-Bold.otf"
    raw_list = source_dir / "1000_characters_SC.txt"
    repo_root = (script_dir / "../../..").resolve()
    simulator_books_dir = repo_root / "simulator/sd_data/books"

    if not raw_regular.exists() or not raw_bold.exists():
        print("Missing Noto Sans SC raw fonts", file=sys.stderr)
        return 1
    if not raw_list.exists():
        print("Missing 1000_characters_SC.txt", file=sys.stderr)
        return 1

    subset_chars = _ordered_union(
        _load_common_sc_chars(raw_list),
        _load_cjk_chars_from_filenames(simulator_books_dir),
        CYRILLIC_UI_CHARS,
        EXTRA_CHARS,
    )
    subset_chars_file = source_dir / "NotoSansSC-SubsetChars.txt"
    subset_chars_file.write_text(subset_chars, encoding="utf-8")

    for source_font, output_name in (
        (raw_regular, "NotoSansSC-Subset-Regular.otf"),
        (raw_bold, "NotoSansSC-Subset-Bold.otf"),
    ):
        output_font = source_dir / output_name
        subprocess.run(
            [
                pyftsubset,
                str(source_font),
                f"--text-file={subset_chars_file}",
                f"--output-file={output_font}",
                "--layout-features=*",
                "--glyph-names",
                "--symbol-cmap",
                "--legacy-cmap",
                "--name-IDs=*",
                "--name-legacy",
                "--name-languages=*",
                "--notdef-glyph",
                "--notdef-outline",
                "--recommended-glyphs",
                "--no-hinting",
            ],
            check=True,
        )
        print(f"Generated {output_font}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
