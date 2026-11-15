#!/usr/bin/env python3
"""Let the native runtime choose the size of the farm's mesh of tilled soil.

Deja que el runtime nativo elija el tamaño de la malla de tierra labrada de la granja.

The tilled and watered soil is not part of the farm's map: an object the game calls
GROUND draws it over the ground, as a mesh of 20 by 20 points around the player kept in
tables at fixed addresses (runtime/src/ground_mesh.h tells the whole story). With the
far_camera mod the camera sees past that square, and the soil there appeared as the player
walked. This changes the translated C++ of the GROUND functions, once, after the translator
and before the compiler: every place where they use the mesh's size (20 points to a side, 19
squares, the rows of 240 bytes, the slot of a point as its position modulo 20, the 2166
corners of the triangles of one drawing, the 9 tiles the mesh starts west and north of the
player) or a table's address takes it from a variable of the runtime instead
(hmmm_ground_mesh_*, defined in ground_mesh.cpp). Around the two drawings it adds the calls
that give them their own room for the display list (the game's room keeps lists of 64 KiB at
most). Those variables hold the game's own values unless the mod is on, so without it the
translated code does exactly what it did.

Each change names the function, the statement as the translator writes it, what replaces
it, and how many times the function has that statement. A statement that is missing or
found another number of times stops the build: the translation would not be the one this
was written for (WiiCompiled at the version scripts/build_native_runtime.sh pins). Running
the tool again does nothing new: every changed function carries a marker.

Cambia el C++ traducido de las funciones de GROUND para que tomen el tamaño de la malla y
las direcciones de sus tablas de variables del runtime, que guardan los valores del juego
salvo con el mod far_camera.

Usage / Uso:
    python3 tools/size_ground_mesh.py --translation build/native/translation/game
"""

import argparse
import sys
from pathlib import Path

MARKER = "// ground mesh sized by the runtime"
DECLARATIONS = [
    f"{MARKER}: the variables and functions of runtime/src/ground_mesh.h",
    'extern "C" uint32_t hmmm_ground_mesh_size;',
    'extern "C" uint32_t hmmm_ground_mesh_squares;',
    'extern "C" uint32_t hmmm_ground_mesh_points;',
    'extern "C" uint32_t hmmm_ground_mesh_row;',
    'extern "C" uint32_t hmmm_ground_mesh_corners;',
    'extern "C" float hmmm_ground_mesh_behind;',
    'extern "C" uint32_t hmmm_ground_mesh_positions;',
    'extern "C" uint32_t hmmm_ground_mesh_soil;',
    'extern "C" uint32_t hmmm_ground_mesh_marks;',
    'extern "C" void hmmm_ground_mesh_reset();',
    'extern "C" void hmmm_ground_mesh_list_begins();',
    'extern "C" void hmmm_ground_mesh_list_ends();',
]

SIZE = "hmmm_ground_mesh_size"  # 20 points to a side
SQUARES = "hmmm_ground_mesh_squares"  # 19 squares to a side
ROW = "hmmm_ground_mesh_row"  # 240: 20 points of 3 floats
POSITIONS = "hmmm_ground_mesh_positions"
SOIL = "hmmm_ground_mesh_soil"
MARKS = "hmmm_ground_mesh_marks"

# The game's tables are the points' positions at 0x802B9008 (the translator writes it as
# 0x802C0000 + -28664), then a byte for each point at + 4800 (the soil) and + 5200 (the
# marks over it).
BEGIN_LIST = "MKW_STATIC_TRANSLATED_CALL(0x801C8958u, func_801C8958, ctx);"
END_LIST = "MKW_STATIC_TRANSLATED_CALL(0x801C8924u, func_801C8924, ctx);"


def loop_end(register: str, game_end: int, name: str, times: int = 1) -> tuple[str, str, int]:
    """A loop over the rows or the columns, which compares its counter with 20 or 19."""
    return (
        f"SetCRResident(cr, xer, 0, static_cast<uint32_t>({register}), static_cast<uint32_t>({game_end}));",
        f"SetCRResident(cr, xer, 0, static_cast<uint32_t>({register}), static_cast<uint32_t>({name}));",
        times,
    )


# For each function: (statement as translated, replacement, times in the function).
CHANGES: dict[int, list[tuple[str, str, int]]] = {
    # The slot of a point, its position modulo 20: the game divides by 20 with a multiply
    # by 0xCCCCCCCD and a shift by 4 (here the masked rotate), then multiplies back.
    0x8006D094: [
        ("r4 = (r4_rot_0 & 268435455);", f"r4 = (r29 / {SIZE});", 1),
        ("r4 = (r4 * 20);", f"r4 = (r4 * {SIZE});", 1),
        ("r0 = (r0_rot_0 & 268435455);", f"r0 = (r3 / {SIZE});", 1),
        ("r0 = (r0 * 20);", f"r0 = (r0 * {SIZE});", 1),
    ],
    # Makes the tables' points (a vector of 400, as the game is started).
    0x80064B88: [
        ("r3 = (r3 + -28664);", f"r3 = {POSITIONS};", 1),
        ("r7 = 400;", "r7 = hmmm_ground_mesh_points;", 1),
    ],
    # Starts the tables over and makes the GROUND object, when the farm is loaded. Its
    # unrolled loops still start the game's tables; the runtime starts its own.
    0x8006490C: [
        ("r3 = (r3 + -28664);", "r3 = (r3 + -28664); hmmm_ground_mesh_reset();", 1),
    ],
    # The update: where the mesh starts (the player's position less 9), and for every point
    # the soil and the marks of its tile and, for the points that came in, its height.
    0x80064108: [
        ("r30 = (r4 + -28664);", f"r30 = {POSITIONS};", 1),
        ("r28 = (r4 + 19);", f"r28 = (r4 + {SQUARES});", 1),
        ("r26 = (r5 + 19);", f"r26 = (r5 + {SQUARES});", 1),
        ("f2.d = MemoryInline::FlatReadFloat32((r2 + -30496));", "f2.d = hmmm_ground_mesh_behind;", 1),
        ("r3 = (r30 + 4800);", f"r3 = {SOIL};", 12),
        ("r3 = (r30 + 5200);", f"r3 = {MARKS};", 8),
        ("r4 = (r4 * 20);", f"r4 = (r4 * {SIZE});", 20),
        ("r4 = (r4 * 240);", f"r4 = (r4 * {ROW});", 1),
        loop_end("r22", 20, SIZE),
        loop_end("r23", 20, SIZE),
    ],
    # The drawing: the squares of the mesh as triangles, in two display lists (the soil,
    # then the marks over it).
    0x80063554: [
        ("r31 = (r4 + -28664);", f"r31 = {POSITIONS};", 1),
        ("r27 = (r31 + 4800);", f"r27 = {SOIL};", 1),
        ("r28 = (r31 + 5200);", f"r28 = {MARKS};", 1),
        ("r5 = 2166;", "r5 = hmmm_ground_mesh_corners;", 2),
        ("r0 = (r3 * 20);", f"r0 = (r3 * {SIZE});", 2),
        ("r3 = (r3 * 240);", f"r3 = (r3 * {ROW});", 12),
        (BEGIN_LIST, f"hmmm_ground_mesh_list_begins(); {BEGIN_LIST}", 2),
        (END_LIST, f"{END_LIST} hmmm_ground_mesh_list_ends();", 2),
        loop_end("r17", 19, SQUARES, times=2),
        loop_end("r18", 19, SQUARES, times=2),
    ],
}

# What no changed function may still have: a comparison of a loop with the game's size
# (but fn_8006490C, whose loops start the game's own tables).
LEFT_LOOPS = ("static_cast<uint32_t>(20));", "static_cast<uint32_t>(19));")
GAME_LOOPS_KEPT = {0x8006490C}

FUNCTION_HEADER = 'extern "C" void func_{:08X}(CpuContext* MKW_RESTRICT ctx)'


def closing_brace(lines: list[str], open_brace: int) -> int:
    """The line that closes the brace opened on line `open_brace`."""
    depth = 0
    for i in range(open_brace, len(lines)):
        depth += lines[i].count("{") - lines[i].count("}")
        if depth == 0:
            return i
    raise SystemExit("error: a translated function does not end")


def change_function(lines: list[str], address: int) -> bool:
    """Applies the changes of one function in place; False when it was already changed."""
    header = FUNCTION_HEADER.format(address)
    start = lines.index(header)
    open_brace = start + 1
    if lines[open_brace] != "{":
        raise SystemExit(f"error: unexpected layout of func_{address:08X}")
    if lines[open_brace + 1].strip() == MARKER:
        return False
    close_brace = closing_brace(lines, open_brace)
    body = range(open_brace + 1, close_brace)
    for old, new, times in CHANGES[address]:
        found = [i for i in body if lines[i].strip() == old]
        if len(found) != times:
            raise SystemExit(
                f"error: func_{address:08X} has {len(found)} times, not {times}, the statement:\n    {old}"
            )
        for i in found:
            indent = lines[i][: len(lines[i]) - len(lines[i].lstrip())]
            lines[i] = indent + new
    left = [
        i
        for i in body
        if address not in GAME_LOOPS_KEPT
        and lines[i].strip().startswith("SetCRResident(")
        and lines[i].strip().endswith(LEFT_LOOPS)
    ]
    if left:
        raise SystemExit(
            f"error: func_{address:08X} still compares with the game's size:\n    {lines[left[0]].strip()}"
        )
    lines.insert(open_brace + 1, f"    {MARKER}")
    return True


def add_declarations(lines: list[str]) -> None:
    if DECLARATIONS[0] in lines:
        return
    last_include = max(i for i, line in enumerate(lines[:30]) if line.startswith("#include"))
    lines[last_include + 1 : last_include + 1] = DECLARATIONS


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--translation", type=Path, required=True, help="the translated game (build/native/translation/game)"
    )
    arguments = parser.parse_args()

    shards = sorted((arguments.translation / "build_shards" / "base_common").glob("*.cpp"))
    texts = {path: path.read_text(encoding="utf-8") for path in shards}
    changed = 0
    for address in CHANGES:
        name = f"{address:08X}"
        header = FUNCTION_HEADER.format(address)
        holders = [path for path, text in texts.items() if header + "\n{" in text]
        if len(holders) != 1:
            raise SystemExit(f"error: func_{name} is translated in {len(holders)} places, not one")
        # A copy of the function elsewhere would keep the game's mesh.
        if any(
            f"inline leaf 0x{name}" in text or f"func_{name}_statefree" in text for text in texts.values()
        ):
            raise SystemExit(f"error: the translator copied 0x{name} into other functions")
        path = holders[0]
        lines = texts[path].split("\n")
        if change_function(lines, address):
            add_declarations(lines)
            changed += 1
        texts[path] = "\n".join(lines)
    for path in shards:
        if texts[path] != path.read_text(encoding="utf-8"):
            path.write_text(texts[path], encoding="utf-8")
    print(
        f"ground mesh: {changed} of {len(CHANGES)} functions changed"
        if changed
        else "ground mesh: already changed"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
