#!/usr/bin/env python3
"""Mark in the translated game the places where the mods act, for the native runtime.

Marca en el juego traducido los lugares donde actúan los mods, para el runtime nativo.

In ModernGekko a mod's patches and hooks are applied while the game runs. In the native
runtime the translated functions call each other directly, so there is nothing to change
while the game runs: the calls are written into the translated C++ instead, once, after
the translator and before the compiler. This reads which addresses the mods patch or hook
(from their C descriptors, through the C preprocessor, so that every name and constant is
resolved exactly as when the mod is compiled) and adds to the translated code:

- at the entry of a patched or hooked function: a call of hmmm_mod_hook_entry, which
  returns from the function when a mod's patch has returned;
- before each return of a function with return hooks: a call of hmmm_mod_hook_return;
- at a hooked instruction inside a function: the function's registers are stored in the
  CPU context, hmmm_mod_hook_at runs the hooks, and the registers are read back.

En ModernGekko los patches y hooks de un mod se aplican mientras corre el juego. En el
runtime nativo las funciones traducidas se llaman directamente, así que las llamadas se
escriben en el C++ traducido, una vez, entre el traductor y el compilador.

A hooked function must stay a function of its own: it is refused when the translator
inlined it somewhere or made a state-free copy of it, since those calls would skip the
hook. Running the tool again does nothing new: every insertion carries a marker.

Usage / Uso:
    python3 tools/insert_mod_hooks.py --mod jamie_marriage --mod player_speed
"""

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parent.parent

# One entry of a mod's patch or hook table, as the stand-in mod ABI header
# (runtime/mod_api/moderngekko/mod_abi.h) expands RECOMP_PATCH and RECOMP_HOOK.
TABLE_ENTRY = re.compile(
    r"\{\s*\(\s*(0[xX][0-9A-Fa-f]+)[uU]?\s*\)\s*,\s*\(\s*\w+\s*\)\s*,\s*"
    r"(MODERNGEKKO_MOD_(?:PATCH_NONE|PATCH_FORCE|HOOK_ENTRY|HOOK_RETURN))\s*\}"
)

MARKER = "// mod hook point"
DECLARATIONS = (
    f"{MARKER}: the calls of runtime/src/mods/native_mods.h\n"
    'extern "C" bool hmmm_mod_hook_entry(uint32_t address, CpuContext* cpu);\n'
    'extern "C" void hmmm_mod_hook_return(uint32_t address, CpuContext* cpu);\n'
    'extern "C" void hmmm_mod_hook_at(uint32_t address, CpuContext* cpu);\n'
)

# Locals that hold the guest registers inside a translated function.
GPR_LOCAL = re.compile(r"^    uint32_t r(\d+) = ctx->gpr\[\d+\];$")
FPR_LOCAL = re.compile(r"^    PPC_FPR f(\d+) = ctx->fpr\[\d+\];$")
SPECIAL_LOCALS = ("cr", "ctr", "xer")


@dataclass(frozen=True)
class HookPoint:
    address: int
    on_return: bool  # a return hook; otherwise an entry hook, a patch or a hook inside


def hook_points_of(mod_id: str, compiler: str, symbol_header: Path) -> set[HookPoint]:
    """The addresses that one mod patches or hooks."""
    source = REPOSITORY / "mods" / mod_id / "mod.c"
    preprocessed = subprocess.run(
        [
            compiler,
            "-E",
            "-P",
            str(source),
            f"-I{REPOSITORY / 'runtime' / 'mod_api'}",
            f"-I{REPOSITORY / 'mods' / 'include'}",
            f"-I{symbol_header.parent}",
        ],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    points = set()
    for address, kind in TABLE_ENTRY.findall(preprocessed):
        points.add(HookPoint(int(address, 16), kind == "MODERNGEKKO_MOD_HOOK_RETURN"))
    if not points:
        raise SystemExit(f"error: no patches or hooks found in {source}")
    return points


class Shards:
    """The translated functions, in the shards that the build compiles."""

    def __init__(self, translation: Path):
        self.paths = sorted((translation / "build_shards" / "base_common").glob("*.cpp"))
        self.texts = {path: path.read_text(encoding="utf-8") for path in self.paths}
        self.changed: set[Path] = set()

    def find(self, pattern: str) -> list[Path]:
        return [path for path, text in self.texts.items() if pattern in text]

    def function_bounds(self, lines: list[str], address: int) -> tuple[int, int]:
        """Indices of the opening and closing braces of func_<address>."""
        header = f'extern "C" void func_{address:08X}(CpuContext* MKW_RESTRICT ctx)'
        start = lines.index(header)
        if lines[start + 1] != "{":
            raise SystemExit(f"error: unexpected layout of func_{address:08X}")
        return start + 1, closing_brace(lines, start + 1)


def closing_brace(lines: list[str], open_brace: int) -> int:
    """The line that closes the brace opened on line `open_brace`. The blocks of the labels
    inside a function also start at the first column, so the braces are counted."""
    depth = 0
    for i in range(open_brace, len(lines)):
        depth += lines[i].count("{") - lines[i].count("}")
        if depth == 0:
            return i
    raise SystemExit("error: a translated function does not end")


def check_stays_a_function(shards: Shards, address: int) -> None:
    name = f"{address:08X}"
    if shards.find(f"inline leaf 0x{name}"):
        raise SystemExit(f"error: the translator inlined 0x{name}, so a hook there would not run")
    if shards.find(f"func_{name}_statefree"):
        raise SystemExit(f"error: the translator made a state-free copy of 0x{name}, which a hook would miss")


def ensure_declarations(lines: list[str]) -> None:
    if any(line.startswith(MARKER) for line in lines):
        return
    first_include_end = max(i for i, line in enumerate(lines[:20]) if line.startswith("#include"))
    lines[first_include_end + 1 : first_include_end + 1] = DECLARATIONS.rstrip("\n").split("\n")


def insert_entry(lines: list[str], open_brace: int, address: int) -> bool:
    marker = f"    {MARKER} entry 0x{address:08X}"
    if lines[open_brace + 1] == marker:
        return False
    lines[open_brace + 1 : open_brace + 1] = [
        marker,
        f"    if (hmmm_mod_hook_entry(0x{address:08X}u, ctx)) return;",
    ]
    return True


def insert_returns(lines: list[str], open_brace: int, close_brace: int, address: int) -> bool:
    call = f"hmmm_mod_hook_return(0x{address:08X}u, ctx); return; }} {MARKER}"
    changed = False
    for i in range(open_brace, close_brace):
        stripped = lines[i].strip()
        if stripped == "return;":
            indent = lines[i][: len(lines[i]) - len(lines[i].lstrip())]
            lines[i] = f"{indent}{{ {call}"
            changed = True
    return changed


def register_locals(lines: list[str], open_brace: int, close_brace: int) -> list[tuple[str, str]]:
    """(local, CPU context field) of each register the function keeps in a local."""
    pairs = []
    for line in lines[open_brace:close_brace]:
        if match := GPR_LOCAL.match(line):
            pairs.append((f"r{match[1]}", f"ctx->gpr[{match[1]}]"))
        elif match := FPR_LOCAL.match(line):
            pairs.append((f"f{match[1]}", f"ctx->fpr[{match[1]}]"))
        elif any(line == f"    uint32_t {name} = ctx->{name};" for name in SPECIAL_LOCALS):
            name = line.split()[1]
            pairs.append((name, f"ctx->{name}"))
    return pairs


def insert_inside(lines: list[str], label: int, locals_: list[tuple[str, str]], address: int) -> bool:
    marker = f"    {MARKER} at 0x{address:08X}"
    if lines[label + 2] == marker:
        return False
    if lines[label + 1] != "{":
        raise SystemExit(f"error: unexpected layout at loc_{address:08X}")
    block = [marker]
    block += [f"    {field} = {local};" for local, field in locals_]
    block.append(f"    hmmm_mod_hook_at(0x{address:08X}u, ctx);")
    block += [f"    {local} = {field};" for local, field in locals_]
    lines[label + 2 : label + 2] = block
    return True


def insert(shards: Shards, point: HookPoint) -> bool:
    name = f"{point.address:08X}"
    function_files = shards.find(f'extern "C" void func_{name}(CpuContext* MKW_RESTRICT ctx)\n{{')
    if function_files:
        check_stays_a_function(shards, point.address)
        path = function_files[0]
        lines = shards.texts[path].split("\n")
        ensure_declarations(lines)
        open_brace, close_brace = shards.function_bounds(lines, point.address)
        if point.on_return:
            changed = insert_returns(lines, open_brace, close_brace, point.address)
        else:
            changed = insert_entry(lines, open_brace, point.address)
    else:
        if point.on_return:
            raise SystemExit(f"error: a return hook at 0x{name}, which does not start a function")
        label_files = shards.find(f"\nloc_{name}:\n")
        if len(label_files) != 1:
            raise SystemExit(f"error: no single translated instruction at 0x{name}")
        path = label_files[0]
        lines = shards.texts[path].split("\n")
        ensure_declarations(lines)
        label = lines.index(f"loc_{name}:")
        function_header = re.compile(r'^extern "C" void func_[0-9A-F]{8}\(CpuContext\* MKW_RESTRICT ctx\)$')
        header = max(i for i in range(label) if function_header.match(lines[i]))
        open_brace = header + 1
        close_brace = closing_brace(lines, open_brace)
        changed = insert_inside(lines, label, register_locals(lines, open_brace, close_brace), point.address)
    shards.texts[path] = "\n".join(lines)
    if changed:
        shards.changed.add(path)
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    default_translation = REPOSITORY / "build" / "native" / "translation" / "game"
    parser.add_argument("--translation", type=Path, default=default_translation)
    parser.add_argument("--mod", action="append", default=[], help="a mod of mods/ (repeat for several)")
    parser.add_argument("--compiler", default="clang", help="a C compiler, for its preprocessor")
    parser.add_argument("--symbol-header", type=Path, default=REPOSITORY / "build" / "G4AEE9_symbols.h")
    arguments = parser.parse_args()

    shards = Shards(arguments.translation)
    if not shards.paths:
        raise SystemExit(f"error: no translated shards in {arguments.translation}")
    points: set[HookPoint] = set()
    for mod_id in arguments.mod:
        points |= hook_points_of(mod_id, arguments.compiler, arguments.symbol_header)
    added = sum(insert(shards, point) for point in sorted(points, key=lambda p: (p.address, p.on_return)))
    for path in shards.changed:
        path.write_text(shards.texts[path], encoding="utf-8")
    print(f"{len(points)} hook points for {', '.join(arguments.mod) or 'no mods'}; {added} added")
    return 0


if __name__ == "__main__":
    sys.exit(main())
