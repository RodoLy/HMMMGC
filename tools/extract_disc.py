#!/usr/bin/env python3
"""Extract a GameCube disc image (ISO/GCM/RVZ) into ./disc using nodtool.

Extrae una imagen de disco de GameCube (ISO/GCM/RVZ) a ./disc usando nodtool.

Usage / Uso:
    python3 tools/extract_disc.py path/to/disc.rvz [output_dir]
"""

import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    image = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("disc")

    if not image.is_file():
        print(f"Disc image not found: {image}")
        return 1

    if shutil.which("nodtool") is None:
        print("nodtool is not installed. Run: cargo install nodtool")
        return 1

    if out.exists():
        print(f"Output directory already exists: {out}")
        return 1

    print(f"Extracting {image} -> {out}")
    subprocess.run(["nodtool", "extract", str(image), str(out)], check=True)
    print("Done. Now run: python3 tools/check_disc.py", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
