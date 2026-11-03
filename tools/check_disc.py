#!/usr/bin/env python3
"""Check that an extracted disc matches the target binary (G4AEE9 rev 0).

Comprueba que un disco extraído coincide con el binario objetivo (G4AEE9 rev 0).

Usage / Uso:
    python3 tools/check_disc.py disc/
"""

import hashlib
import struct
import sys
from pathlib import Path

EXPECTED_GAME_ID = b"G4AEE9"
EXPECTED_REVISION = 0
EXPECTED_DOL_SHA1 = "5977972cd798d60ecfcf5c1f26a86dcecc36446f"


def sha1_of(path: Path) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    disc = Path(sys.argv[1])
    boot = disc / "sys" / "boot.bin"
    dol = disc / "sys" / "main.dol"

    if not boot.is_file() or not dol.is_file():
        print(f"{disc} does not look like an extracted disc (missing sys/boot.bin or sys/main.dol)")
        return 1

    header = boot.read_bytes()
    game_id = header[0:6]
    revision = header[7]
    title = header[0x20:0x60].split(b"\0", 1)[0].decode("ascii", "replace")

    ok = True
    print(f"Title:    {title}")
    print(f"Game ID:  {game_id.decode()}  (expected {EXPECTED_GAME_ID.decode()})")
    print(f"Revision: {revision}  (expected {EXPECTED_REVISION})")
    if game_id != EXPECTED_GAME_ID or revision != EXPECTED_REVISION:
        ok = False

    digest = sha1_of(dol)
    print(f"main.dol: {digest}")
    if digest != EXPECTED_DOL_SHA1:
        print(f"          expected {EXPECTED_DOL_SHA1}")
        ok = False

    # Print the DOL layout; handy when comparing against the docs.
    data = dol.read_bytes()
    offsets = struct.unpack(">18I", data[0:72])
    addrs = struct.unpack(">18I", data[72:144])
    sizes = struct.unpack(">18I", data[144:216])
    bss_addr, bss_size, entry = struct.unpack(">3I", data[216:228])
    print(f"entry:    0x{entry:08X}   bss: 0x{bss_addr:08X} (+0x{bss_size:X})")
    for i in range(18):
        if sizes[i]:
            kind = f"text{i}" if i < 7 else f"data{i - 7}"
            print(f"  {kind:6} file 0x{offsets[i]:08X}  addr 0x{addrs[i]:08X}  size 0x{sizes[i]:08X}")

    print("\nOK: this is the target binary." if ok else "\nMISMATCH: this is not the target binary.")
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
