"""A GameCube disc image made out of a game folder, to test the tools that read discs.

Una imagen de disco de GameCube armada con una carpeta de juego, para probar las herramientas
que leen discos.

ModernGekko's launcher turns a disc image into a game folder (`ModernGekko --extract disc.gcm`).
This module goes the other way for the made-up game of synthetic_disc.py, so that the extraction
can be tested on a machine that has no disc: it packs a folder with `sys/` (the boot files and
the executable) and `files/` (the game's files) into an image. Nothing here comes from a game.

The image is laid out the way a GameCube disc is, and the way Dolphin's folder disc is:

    0x0000  boot.bin       the disc header; it also says where the executable and the file table are
    0x0440  bi2.bin        the second header
    0x2440  apploader.img  the program that loads the executable
     ...    main.dol       the executable, aligned
     ...    fst.bin        the file table: a 12-byte row for the root folder and for each file, then
                           the names, each ending in a zero byte
     ...    the files, each aligned

Only files directly inside `files/` are packed, no folders: that is all a test needs.

El launcher de ModernGekko convierte una imagen de disco en una carpeta de juego
(`ModernGekko --extract disc.gcm`). Este módulo hace lo contrario para el juego inventado de
synthetic_disc.py, para poder probar la extracción en una máquina sin disco: empaqueta una carpeta
con `sys/` (los archivos de arranque y el ejecutable) y `files/` (los archivos del juego) en una
imagen. Nada de esto viene de un juego.

La imagen tiene la distribución de un disco de GameCube, y la del disco de carpeta de Dolphin:

    0x0000  boot.bin       el encabezado del disco; también dice dónde están el ejecutable y la tabla
    0x0440  bi2.bin        el segundo encabezado
    0x2440  apploader.img  el programa que carga el ejecutable
     ...    main.dol       el ejecutable, alineado
     ...    fst.bin        la tabla de archivos: una fila de 12 bytes para la carpeta raíz y para cada
                           archivo, y luego los nombres, cada uno terminado en un byte cero
     ...    los archivos, cada uno alineado

Solo se empaquetan los archivos que están directamente en `files/`, sin carpetas: es todo lo que
una prueba necesita.

Usage / Uso:
    python3 tools/synthetic_disc_image.py GAME_ROOT DISC_IMAGE
"""

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from synthetic_disc import (  # noqa: E402
    APPLOADER_DISC_OFFSET,
    BI2_BIN_SIZE,
    BOOT_BIN_SIZE,
    DISC_ALIGNMENT,
    align_up,
    dol_address_on_disc,
)

BI2_BIN_DISC_OFFSET = BOOT_BIN_SIZE  # the second header follows the first

# Where, inside the disc header, it says where the executable and the file table are.
HEADER_DOL_OFFSET = 0x420
HEADER_FST_OFFSET = 0x424
HEADER_FST_SIZE = 0x428
HEADER_FST_MAXIMUM_SIZE = 0x42C  # how much room the table may take; here, as much as it does

FST_ROOT_FOLDER_FLAG = 1  # the first byte of a row: 1 for a folder, 0 for a file


def file_table(names: list[str], disc_offsets: list[int], lengths: list[int]) -> bytes:
    """The file table of a disc whose root folder holds the files `names`, and nothing else.

    A row is a flag byte and a three-byte offset of the name inside the names that follow the
    rows, then two words: for a file, where it is on the disc and how long it is. The first row is
    the root folder, whose two words are its parent (itself) and the number of rows in all.
    """
    encoded_names = [name.encode("utf-8") + b"\0" for name in names]
    row_count = 1 + len(names)
    root_row = struct.pack(">III", FST_ROOT_FOLDER_FLAG << 24, 0, row_count)
    rows = [root_row]
    name_offset = 0
    for encoded_name, disc_offset, length in zip(encoded_names, disc_offsets, lengths, strict=True):
        rows.append(struct.pack(">III", name_offset, disc_offset, length))
        name_offset += len(encoded_name)
    return b"".join(rows) + b"".join(encoded_names)


def disc_image(game_root: Path) -> bytes:
    """The bytes of a disc image that holds `game_root`: its `sys/` folder and the files of `files/`."""
    sys_directory = game_root / "sys"
    header = bytearray((sys_directory / "boot.bin").read_bytes())
    second_header = (sys_directory / "bi2.bin").read_bytes()
    apploader = (sys_directory / "apploader.img").read_bytes()
    executable = (sys_directory / "main.dol").read_bytes()
    if len(header) != BOOT_BIN_SIZE or len(second_header) != BI2_BIN_SIZE:
        raise ValueError(f"{sys_directory}: boot.bin or bi2.bin does not have the size of a disc header")
    files = sorted(path for path in (game_root / "files").iterdir())
    if any(not path.is_file() for path in files):
        raise ValueError(f"{game_root / 'files'} may only hold files, not folders")

    # The table's size has to be known before the files are placed, because they go after it.
    table_size = len(file_table([path.name for path in files], [0] * len(files), [0] * len(files)))
    executable_offset = dol_address_on_disc(len(apploader))
    table_offset = align_up(executable_offset + len(executable), DISC_ALIGNMENT)
    next_file_offset = align_up(table_offset + table_size, DISC_ALIGNMENT)
    file_contents, file_offsets = [], []
    for path in files:
        file_contents.append(path.read_bytes())
        file_offsets.append(next_file_offset)
        next_file_offset = align_up(next_file_offset + len(file_contents[-1]), DISC_ALIGNMENT)
    table = file_table(
        [path.name for path in files], file_offsets, [len(content) for content in file_contents]
    )
    assert len(table) == table_size

    struct.pack_into(">I", header, HEADER_DOL_OFFSET, executable_offset)
    struct.pack_into(">I", header, HEADER_FST_OFFSET, table_offset)
    struct.pack_into(">I", header, HEADER_FST_SIZE, len(table))
    struct.pack_into(">I", header, HEADER_FST_MAXIMUM_SIZE, len(table))

    image = bytearray(next_file_offset)
    image[:BOOT_BIN_SIZE] = header
    image[BI2_BIN_DISC_OFFSET : BI2_BIN_DISC_OFFSET + BI2_BIN_SIZE] = second_header
    image[APPLOADER_DISC_OFFSET : APPLOADER_DISC_OFFSET + len(apploader)] = apploader
    image[executable_offset : executable_offset + len(executable)] = executable
    image[table_offset : table_offset + len(table)] = table
    for offset, content in zip(file_offsets, file_contents, strict=True):
        image[offset : offset + len(content)] = content
    return bytes(image)


def write_disc_image(game_root: Path, image_path: Path) -> None:
    """Write the disc image of `game_root` to `image_path`."""
    image_path.write_bytes(disc_image(game_root))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "game_root", type=Path, help="a game folder with sys/ and files/ (see synthetic_disc.py)"
    )
    parser.add_argument("image_path", type=Path, help="the disc image to write")
    arguments = parser.parse_args(argv)
    try:
        write_disc_image(arguments.game_root, arguments.image_path)
    except (OSError, ValueError) as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    print(f"disc image: {arguments.image_path} ({arguments.image_path.stat().st_size:,} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
