#!/usr/bin/env python3
"""Extract a GameCube disc image (ISO/GCM/RVZ) into orig/G4AEE9 using nodtool.

Extrae una imagen de disco de GameCube (ISO/GCM/RVZ) a orig/G4AEE9 usando nodtool.

orig/<game id>/ is where decomp-toolkit projects keep the user's own copy of
the game, so the layout is familiar to anyone coming from another decomp.

orig/<game id>/ es donde los proyectos de decomp-toolkit guardan la copia del
juego del usuario, así que la estructura le resulta familiar a cualquiera que
venga de otro decomp.

Usage / Uso:
    python3 tools/extract_disc.py path/to/disc.rvz               # into orig/G4AEE9
    python3 tools/extract_disc.py path/to/disc.rvz other/folder
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from project import DISC_DIRECTORY  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract a GameCube disc image with nodtool.")
    parser.add_argument("image", type=Path, help="your disc image (.iso, .gcm or .rvz)")
    parser.add_argument(
        "output_directory",
        type=Path,
        nargs="?",
        default=DISC_DIRECTORY,
        help=f"where to extract it (default: {DISC_DIRECTORY})",
    )
    arguments = parser.parse_args()

    if not arguments.image.is_file():
        parser.error(f"disc image not found: {arguments.image}")
    if shutil.which("nodtool") is None:
        parser.error("nodtool is not installed. Run: cargo install nodtool")
    if (arguments.output_directory / "sys").exists():
        parser.error(f"{arguments.output_directory} already holds an extracted disc")

    print(f"Extracting {arguments.image} -> {arguments.output_directory}")
    arguments.output_directory.mkdir(parents=True, exist_ok=True)
    subprocess.run(["nodtool", "extract", str(arguments.image), str(arguments.output_directory)], check=True)
    print(f"Done. Now run: python3 tools/check_disc.py {arguments.output_directory}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
