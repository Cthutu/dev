from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
FORMAT_ROOTS = (ROOT / "src", ROOT / "tests")
SOURCE_SUFFIXES = {".c", ".h"}


def discover_sources() -> list[Path]:
    sources: list[Path] = []
    for root in FORMAT_ROOTS:
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            if path.suffix not in SOURCE_SUFFIXES:
                continue
            sources.append(path)
    return sorted(sources)


def main() -> int:
    sources = discover_sources()
    if not sources:
        print("No C source files found.")
        return 0

    cmd = ["clang-format", "-i", *map(str, sources)]
    subprocess.run(cmd, check=True)
    print(f"Formatted {len(sources)} file(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
