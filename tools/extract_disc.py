#!/usr/bin/env python3
"""Extract a GameCube disc image (ISO/GCM/RVZ) into orig/G4AEE9 using nodtool.

Extrae una imagen de disco de GameCube (ISO/GCM/RVZ) a orig/G4AEE9 usando nodtool.

orig/<game id>/ is where decomp-toolkit projects keep the user's own copy of
the game, so the layout is familiar to anyone coming from another decomp.

orig/<game id>/ es donde los proyectos de decomp-toolkit guardan la copia del
juego del usuario, así que la estructura le resulta familiar a cualquiera que
venga de otro decomp.

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
    output_directory = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("orig/G4AEE9")

    if not image.is_file():
        print(f"Disc image not found: {image}")
        return 1

    if shutil.which("nodtool") is None:
        print("nodtool is not installed. Run: cargo install nodtool")
        return 1

    if (output_directory / "sys").exists():
        print(f"{output_directory} already holds an extracted disc")
        return 1

    print(f"Extracting {image} -> {output_directory}")
    subprocess.run(["nodtool", "extract", str(image), str(output_directory)], check=True)
    print("Done. Now run: python3 tools/check_disc.py", output_directory)
    return 0


if __name__ == "__main__":
    sys.exit(main())
