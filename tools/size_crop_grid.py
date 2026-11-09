#!/usr/bin/env python3
"""Let the native runtime choose the size of the farm's grid of crops and items.

Deja que el runtime nativo elija el tamaño de la cuadrícula de cultivos y objetos de la
granja.

The game draws what stands on the farm's tiles (crops, weeds, stones, branches) with an
object it calls CROP, which keeps a square of 25 by 25 tiles around the player in tables
at fixed addresses (runtime/src/crop_grid.h tells the whole story). Zoomed out, the camera
sees past that square. This changes the translated C++ of the CROP functions, once, after
the translator and before the compiler: every place where they use the square's size (25
tiles to a side, the rows of 50, 100 and 800 bytes, the slot of a tile as its position
modulo 25, the last tile of the part with all items) or a table's address takes it from a
variable of the runtime instead (hmmm_crop_grid_*, defined in crop_grid.cpp). Those
variables hold the game's own values unless the player zoomed out, so with the game's
camera the translated code does exactly what it did.

Each change names the function, the statement as the translator writes it, what replaces
it, and how many times the function has that statement. A statement that is missing or
found another number of times stops the build: the translation would not be the one this
was written for (WiiCompiled at the version scripts/build_native_runtime.sh pins). Running
the tool again does nothing new: every changed function carries a marker.

Cambia el C++ traducido de las funciones de CROP para que tomen el tamaño de la
cuadrícula y las direcciones de sus tablas de variables del runtime, que guardan los
valores del juego salvo con el zoom alejado.

Usage / Uso:
    python3 tools/size_crop_grid.py --translation build/native/translation/game
"""

import argparse
import sys
from pathlib import Path

MARKER = "// crop grid sized by the runtime"
DECLARATIONS = [
    f"{MARKER}: the variables of runtime/src/crop_grid.h",
    'extern "C" uint32_t hmmm_crop_grid_size;',
    'extern "C" uint32_t hmmm_crop_grid_inner_end;',
    'extern "C" uint32_t hmmm_crop_grid_model_row;',
    'extern "C" uint32_t hmmm_crop_grid_item_row;',
    'extern "C" uint32_t hmmm_crop_grid_word_row;',
    'extern "C" uint32_t hmmm_crop_grid_models;',
    'extern "C" uint32_t hmmm_crop_grid_attachments;',
    'extern "C" uint32_t hmmm_crop_grid_items;',
    'extern "C" uint32_t hmmm_crop_grid_kinds;',
    'extern "C" uint32_t hmmm_crop_grid_effects;',
    'extern "C" uint32_t hmmm_crop_grid_flags;',
    'extern "C" uint32_t hmmm_crop_grid_heights;',
]

# The tables, as the game addresses them: their first bytes from 0x802D0000 (0x802CF1D8,
# the models, and 0x802D3FF8, what they carry), from 0x802E0000 (0x802DA684, the flags), or
# from the models' address + 0x10000 (the items 0x802D8E18, the kinds 0x802D92FC, the
# effects 0x802D9CC0, the flags and the heights 0x802DB048).
MODELS = "hmmm_crop_grid_models"
ATTACHMENTS = "hmmm_crop_grid_attachments"
ITEMS = "hmmm_crop_grid_items"
KINDS = "hmmm_crop_grid_kinds"
EFFECTS = "hmmm_crop_grid_effects"
FLAGS = "hmmm_crop_grid_flags"
HEIGHTS = "hmmm_crop_grid_heights"
SIZE = "hmmm_crop_grid_size"
MODEL_ROW = "hmmm_crop_grid_model_row"  # 800: 25 tiles of 8 words
ITEM_ROW = "hmmm_crop_grid_item_row"  # 50: 25 halfwords
WORD_ROW = "hmmm_crop_grid_word_row"  # 100: 25 words


def table(register: str, game_expression: str, name: str, times: int = 1) -> tuple[str, str, int]:
    return (f"{register} = ({game_expression});", f"{register} = {name};", times)


def row(register: str, game_row: int, name: str, times: int = 1) -> tuple[str, str, int]:
    """A pointer moved to the next row of a table."""
    return (f"{register} = ({register} + {game_row});", f"{register} = ({register} + {name});", times)


def row_offset(register: str, index: str, game_row: int, name: str, times: int = 1) -> tuple[str, str, int]:
    """The offset of row `index` of a table."""
    return (f"{register} = ({index} * {game_row});", f"{register} = ({index} * {name});", times)


def loop_end(register: str) -> list[tuple[str, str, int]]:
    """A loop over the rows or the columns, which compares its counter with 25. The
    translator writes the comparison in one of two forms: the condition register kept for a
    later branch, or the branch itself."""
    return [
        (
            f"SetCRResident(cr, xer, 0, static_cast<uint32_t>({register}), static_cast<uint32_t>(25));",
            f"SetCRResident(cr, xer, 0, static_cast<uint32_t>({register}), static_cast<uint32_t>({SIZE}));",
            1,
        ),
        (
            f"if ((static_cast<uint32_t>({register}) < static_cast<uint32_t>(25))) {{",
            f"if ((static_cast<uint32_t>({register}) < static_cast<uint32_t>({SIZE}))) {{",
            1,
        ),
    ]


def tile_slot(quotient: str, rotated: str, position: str, product: str) -> list[tuple[str, str, int]]:
    """The slot of a tile, its position modulo 25: the game divides by 25 with a multiply by
    0x51EB851F and a shift by 3 (here the masked rotate), then multiplies back."""
    return [
        (f"{quotient} = ({rotated} & 536870911);", f"{quotient} = ({position} / {SIZE});", 1),
        (f"{product} = ({quotient} * 25);", f"{product} = ({quotient} * {SIZE});", 1),
    ]


# For each function: (statement as translated, replacement, times in the function). Of the
# two forms loop_end() gives for each loop, the one the translator did not use is skipped
# (is_loop_comparison below), and no comparison with 25 may be left.
CHANGES: dict[int, list[tuple[str, str, int]]] = {
    # The slot of a tile in the tables (called by fn_800727B8 and fn_8007351C).
    0x8006D1F0: tile_slot("r4", "r4_rot_0", "r30", "r4") + tile_slot("r0", "r0_rot_0", "r31", "r0"),
    # Sets the tables up and makes the CROP object, when the farm is loaded.
    0x80073628: [
        table("r4", "r3 + -3624", MODELS),
        table("r4", "r4 + 20000", ATTACHMENTS),
        table("r5", "r9 + -25536", ITEMS),
        table("r6", "r9 + -24284", KINDS),
        table("r7", "r9 + -21784", EFFECTS),
        table("r8", "r9 + -19284", FLAGS),
        table("r9", "r9 + -16784", HEIGHTS),
        ("r0 = 25;", f"r0 = {SIZE};", 2),
        *(row(register, 800, MODEL_ROW) for register in ("r31", "r30", "r12", "r18")),
        *(row(register, 50, ITEM_ROW) for register in ("r29", "r19")),
        *(
            row(register, 100, WORD_ROW)
            for register in ("r28", "r27", "r26", "r25", "r20", "r21", "r22", "r23")
        ),
        *loop_end("r12"),
        *loop_end("r24"),
    ],
    # The CROP object's update: the square's update (fn_80072B0C), then every model of
    # every tile goes to one of its two model lists.
    0x800738CC: [
        table("r27", "r4 + -3624", MODELS),
        table("r28", "r3 + -22908", FLAGS),
        row("r19", 100, WORD_ROW),
        row("r29", 100, WORD_ROW),
        row("r20", 800, MODEL_ROW),
        *loop_end("r25"),
        *loop_end("r24"),
    ],
    # The CROP object's destructor: frees the models it owns.
    0x80073A60: [
        table("r27", "r5 + -3624", MODELS),
        table("r28", "r4 + 16376", ATTACHMENTS),
        table("r29", "r3 + -22908", FLAGS),
        row("r23", 800, MODEL_ROW),
        row("r24", 800, MODEL_ROW),
        row("r22", 100, WORD_ROW),
        row("r16", 100, WORD_ROW),
        *loop_end("r18"),
        *loop_end("r19"),
    ],
    # The colour of a tile's models (from the player's actions, fn_80064D98).
    0x8007351C: [
        table("r0", "r3 + -3624", MODELS),
        table("r6", "r4 + -3624", MODELS),
        row_offset("r5", "r5", 800, MODEL_ROW, times=4),
    ],
    # The models of an item of kind 6, from its list of parts.
    0x8003B444: [
        row_offset("r7", "r22", 800, MODEL_ROW),
        table("r5", "r5 + -3624", MODELS),
        table("r0", "r4 + 16376", ATTACHMENTS),
        table("r0", "r4 + -22908", FLAGS),
        row_offset("r4", "r22", 100, WORD_ROW),
    ],
    # The models of an item of the other kinds.
    0x8007252C: [
        row_offset("r4", "r26", 800, MODEL_ROW),
        table("r3", "r3 + -3624", MODELS),
        row_offset("r6", "r26", 100, WORD_ROW),
        table("r0", "r5 + -22908", FLAGS),
    ],
    # What the tile's models show for the tile's state.
    0x800727B8: [
        table("r0", "r5 + -3624", MODELS, times=2),
        table("r6", "r4 + -3624", MODELS, times=2),
        table("r3", "r3 + -3624", MODELS),
        row_offset("r5", "r5", 800, MODEL_ROW, times=8),
        row_offset("r4", "r27", 800, MODEL_ROW),
    ],
    # The square's update: which tiles it covers, and the models of the tiles that came in
    # or changed.
    0x80072B0C: [
        table("r14", "r4 + -3624", MODELS),
        ("r0 = (r4 + 20);", "r0 = (r4 + hmmm_crop_grid_inner_end);", 1),
        ("r0 = (r5 + 20);", "r0 = (r5 + hmmm_crop_grid_inner_end);", 1),
        table("r27", "r26 + -25536", ITEMS),
        *tile_slot("r4", "r4_rot_3", "r22", "r3"),
        *tile_slot("r0", "r0_rot_3", "r23", "r0"),
        row_offset("r3", "r31", 50, ITEM_ROW),
        row_offset("r20", "r31", 100, WORD_ROW),
        row_offset("r15", "r31", 100, WORD_ROW),
        table("r15", "r26 + -24284", KINDS),
        row_offset("r3", "r31", 800, MODEL_ROW, times=2),
        table("r0", "r14 + 20000", ATTACHMENTS),
        table("r28", "r26 + -19284", FLAGS),
        table("r19", "r26 + -19284", FLAGS),
        table("r4", "r26 + -19284", FLAGS, times=2),
        table("r24", "r26 + -21784", EFFECTS),
        table("r0", "r26 + -21784", EFFECTS),
        table("r4", "r26 + -16784", HEIGHTS, times=2),
        table("r3", "r26 + -16784", HEIGHTS),
        *loop_end("r16"),
        *loop_end("r0"),
    ],
}

# The beginnings of the two forms of a loop comparison (loop_end).
OPTIONAL_PREFIXES = ("SetCRResident(cr, xer, 0, static_cast<uint32_t>(", "if ((static_cast<uint32_t>(")

FUNCTION_HEADER = 'extern "C" void func_{:08X}(CpuContext* MKW_RESTRICT ctx)'


def closing_brace(lines: list[str], open_brace: int) -> int:
    """The line that closes the brace opened on line `open_brace`."""
    depth = 0
    for i in range(open_brace, len(lines)):
        depth += lines[i].count("{") - lines[i].count("}")
        if depth == 0:
            return i
    raise SystemExit("error: a translated function does not end")


def is_loop_comparison(statement: str) -> bool:
    return statement.startswith(OPTIONAL_PREFIXES) and "static_cast<uint32_t>(25)" in statement


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
        if not found and is_loop_comparison(old):
            continue
        if len(found) != times:
            raise SystemExit(
                f"error: func_{address:08X} has {len(found)} times, not {times}, the statement:\n    {old}"
            )
        for i in found:
            indent = lines[i][: len(lines[i]) - len(lines[i].lstrip())]
            lines[i] = indent + new
    # Every loop of the function must have been found in one of its two forms.
    left = [
        i
        for i in body
        if "static_cast<uint32_t>(25)" in lines[i] and lines[i].strip().startswith(OPTIONAL_PREFIXES)
    ]
    if left:
        raise SystemExit(f"error: func_{address:08X} still compares with 25:\n    {lines[left[0]].strip()}")
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
        # A copy of the function elsewhere would keep the game's square.
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
        f"crop grid: {changed} of {len(CHANGES)} functions changed"
        if changed
        else "crop grid: already changed"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
