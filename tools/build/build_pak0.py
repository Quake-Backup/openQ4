#!/usr/bin/env python3
"""Build baseoq4/pak0.pk4 and emit the generated integrity header."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from openq4_pak import create_game_pk4


HEADER_TEMPLATE = """\
#ifndef OPENQ4_PAK0_GENERATED_H
#define OPENQ4_PAK0_GENERATED_H

#define OPENQ4_PAK0_MD5 "{md5_hex}"
#define OPENQ4_PAK0_SIZE_BYTES {size_bytes}
#define OPENQ4_PAK0_FILE_COUNT {file_count}

#endif
"""


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build the openQ4 pak0.pk4 runtime pack and generated checksum header."
    )
    parser.add_argument("--source-dir", required=True, help="Source content/baseoq4 directory.")
    parser.add_argument("--pak-out", required=True, help="Output pak0.pk4 path.")
    parser.add_argument("--header-out", required=True, help="Generated C/C++ header path.")
    parser.add_argument(
        "--stage-out",
        default="",
        help="Optional additional baseoq4/pak0.pk4 path to copy for direct builddir launches.",
    )
    return parser.parse_args(argv[1:])


def write_text_if_changed(path: Path, contents: str) -> None:
    if path.is_file() and path.read_text(encoding="utf-8") == contents:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents, encoding="utf-8", newline="\n")


def copy_if_changed(source: Path, destination: Path) -> None:
    if destination.is_file() and source.read_bytes() == destination.read_bytes():
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(source.read_bytes())


def main(argv: list[str]) -> int:
    args = parse_args(argv)

    source_dir = Path(args.source_dir).resolve()
    pak_out = Path(args.pak_out).resolve()
    header_out = Path(args.header_out).resolve()

    result = create_game_pk4(source_dir, pak_out)
    if result.added_files == 0:
        print("error: pak0.pk4 packaging found no eligible files after filtering", file=sys.stderr)
        return 1
    if result.missing_required:
        print("error: pak0.pk4 packaging is missing required runtime files:", file=sys.stderr)
        for relative_path in result.missing_required:
            print(f"  - {relative_path}", file=sys.stderr)
        return 1

    header = HEADER_TEMPLATE.format(
        md5_hex=result.md5_hex,
        size_bytes=result.size_bytes,
        file_count=result.added_files,
    )
    write_text_if_changed(header_out, header)

    if args.stage_out:
        copy_if_changed(pak_out, Path(args.stage_out).resolve())

    print(
        f"built pak0.pk4: md5={result.md5_hex} "
        f"size={result.size_bytes} files={result.added_files}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
