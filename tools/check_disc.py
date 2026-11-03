#!/usr/bin/env python3
"""Check that an extracted disc matches the target binary (G4AEE9 rev 0).

Comprueba que un disco extraído coincide con el binario objetivo (G4AEE9 rev 0).

Usage / Uso:
    python3 tools/check_disc.py orig/G4AEE9
"""

import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from dol import Dol  # noqa: E402

EXPECTED_GAME_ID = b"G4AEE9"
EXPECTED_REVISION = 0
EXPECTED_DOL_SHA1 = "5977972cd798d60ecfcf5c1f26a86dcecc36446f"


def sha1_of(path: Path) -> str:
    hasher = hashlib.sha1()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1 << 20), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    disc = Path(sys.argv[1])
    boot_path = disc / "sys" / "boot.bin"
    dol_path = disc / "sys" / "main.dol"

    if not boot_path.is_file() or not dol_path.is_file():
        print(f"{disc} does not look like an extracted disc (missing sys/boot.bin or sys/main.dol)")
        return 1

    # boot.bin starts with the disc header: game ID at 0x00, revision at
    # 0x07, internal title at 0x20.
    disc_header = boot_path.read_bytes()
    game_id = disc_header[0:6]
    revision = disc_header[7]
    title = disc_header[0x20:0x60].split(b"\0", 1)[0].decode("ascii", "replace")

    matches_target = True
    print(f"Title:    {title}")
    print(f"Game ID:  {game_id.decode()}  (expected {EXPECTED_GAME_ID.decode()})")
    print(f"Revision: {revision}  (expected {EXPECTED_REVISION})")
    if game_id != EXPECTED_GAME_ID or revision != EXPECTED_REVISION:
        matches_target = False

    dol_sha1 = sha1_of(dol_path)
    print(f"main.dol: {dol_sha1}")
    if dol_sha1 != EXPECTED_DOL_SHA1:
        print(f"          expected {EXPECTED_DOL_SHA1}")
        matches_target = False

    # The DOL layout, handy when comparing against docs/DISC_ANALYSIS.md.
    dol = Dol(dol_path)
    print(f"entry:    0x{dol.entry:08X}   bss: 0x{dol.bss_address:08X} (+0x{dol.bss_size:X})")
    for section in dol.sections:
        print(
            f"  {section.name:6} file 0x{section.file_offset:08X}  "
            f"address 0x{section.address:08X}  size 0x{section.size:08X}"
        )

    if matches_target:
        print("\nOK: this is the target binary.")
        return 0
    print("\nMISMATCH: this is not the target binary.")
    return 2


if __name__ == "__main__":
    sys.exit(main())
