#!/usr/bin/env python3
"""
Batch-insert Thai zero-width break markers into EPUB text nodes.

This script:
1. opens each EPUB in a folder
2. tokenizes Thai runs with PyThaiNLP newmm
3. inserts U+200B (zero-width space) between Thai words
4. writes a processed EPUB copy to an output folder

Dependencies:
  pip install EbookLib beautifulsoup4 pythainlp lxml
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path
from itertools import count


def require_deps():
    try:
        import ebooklib  # noqa: F401
        from ebooklib import ITEM_DOCUMENT, epub  # noqa: F401
        from bs4 import BeautifulSoup, NavigableString  # noqa: F401
        from pythainlp.tokenize import word_tokenize  # noqa: F401
    except ImportError as exc:  # pragma: no cover - user environment dependency
        raise SystemExit(
            "Missing dependency: "
            f"{exc}. Install with: pip install EbookLib beautifulsoup4 pythainlp lxml"
        ) from exc


require_deps()

import ebooklib
from ebooklib import ITEM_DOCUMENT, epub
from bs4 import BeautifulSoup, NavigableString
from pythainlp.tokenize import word_tokenize

ZWSP = "\u200B"
THAI_RUN_RE = re.compile(r"[\u0E00-\u0E7F]+")
SKIP_PARENTS = {"script", "style"}


def tokenize_thai_run(run: str) -> str:
    if ZWSP in run:
        return run

    tokens = [tok for tok in word_tokenize(run, engine="newmm") if tok]
    if len(tokens) <= 1:
        return run
    return ZWSP.join(tokens)


def preprocess_thai_text(text: str) -> str:
    return THAI_RUN_RE.sub(lambda match: tokenize_thai_run(match.group(0)), text)


def build_soup(content: bytes) -> BeautifulSoup:
    for parser in ("lxml-xml", "xml", "html.parser"):
        try:
            return BeautifulSoup(content, parser)
        except Exception:
            continue
    raise RuntimeError("Unable to parse XHTML/HTML content with BeautifulSoup")


def process_document_item(item) -> bool:
    soup = build_soup(item.get_content())
    changed = False

    for node in soup.find_all(string=True):
        if not isinstance(node, NavigableString):
            continue
        if not node.strip():
            continue
        if node.parent and node.parent.name in SKIP_PARENTS:
            continue

        original = str(node)
        updated = preprocess_thai_text(original)
        if updated != original:
            node.replace_with(updated)
            changed = True

    if changed:
        item.set_content(str(soup).encode("utf-8"))

    return changed


def assign_missing_uids(book) -> None:
    uid_counter = count(1)

    def ensure_uid(entry) -> None:
        if hasattr(entry, "uid") and getattr(entry, "uid", None) is None:
            setattr(entry, "uid", f"zwsp_uid_{next(uid_counter)}")

    def walk_toc(node) -> None:
        if isinstance(node, list):
            for child in node:
                walk_toc(child)
            return

        if isinstance(node, tuple):
            # EbookLib TOC commonly stores nested sections as (entry, children)
            if len(node) == 2 and isinstance(node[1], (list, tuple)):
                ensure_uid(node[0])
                walk_toc(node[1])
                return

            for child in node:
                walk_toc(child)
            return

        ensure_uid(node)

    for item in book.get_items():
        ensure_uid(item)

    walk_toc(book.toc)


def process_epub(input_file: Path, output_file: Path) -> tuple[int, int]:
    book = epub.read_epub(str(input_file))
    total_docs = 0
    changed_docs = 0

    for item in book.get_items_of_type(ITEM_DOCUMENT):
        total_docs += 1
        if process_document_item(item):
            changed_docs += 1

    assign_missing_uids(book)
    epub.write_epub(str(output_file), book)
    return total_docs, changed_docs


def batch_process(folder: Path, output_dir: Path, prefix: str) -> int:
    output_dir.mkdir(parents=True, exist_ok=True)
    processed_count = 0

    for entry in sorted(folder.iterdir()):
        if not entry.is_file() or entry.suffix.lower() != ".epub":
            continue

        output_path = output_dir / f"{prefix}{entry.name}"
        total_docs, changed_docs = process_epub(entry, output_path)
        print(f"[OK] {entry.name} -> {output_path.name} ({changed_docs}/{total_docs} docs changed)")
        processed_count += 1

    return processed_count


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Insert Thai zero-width spaces into EPUB files using PyThaiNLP newmm."
    )
    parser.add_argument("input_folder", help="Folder containing source EPUB files")
    parser.add_argument(
        "--output-dir",
        default=None,
        help="Folder for processed EPUB files (default: <input>/processed_zwsp)",
    )
    parser.add_argument(
        "--prefix",
        default="processed_",
        help="Prefix for output EPUB filenames (default: processed_)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    input_folder = Path(args.input_folder).expanduser().resolve()
    if not input_folder.is_dir():
        print(f"Input folder not found: {input_folder}", file=sys.stderr)
        return 1

    output_dir = (
        Path(args.output_dir).expanduser().resolve()
        if args.output_dir
        else input_folder / "processed_zwsp"
    )

    processed_count = batch_process(input_folder, output_dir, args.prefix)
    if processed_count == 0:
        print(f"No EPUB files found in {input_folder}", file=sys.stderr)
        return 1

    print(f"Processed {processed_count} EPUB file(s) into {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
