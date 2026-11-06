"""Checks on the native runtime's sources (runtime/src) that need no compiler.

Comprobaciones sobre las fuentes del runtime nativo que no necesitan compilador.

The translator learns which guest registers a native replacement reads and writes by
reading its body for `<parameter>->gpr[N]`, taking the parameter's name from the
PPC_NATIVE_OVERRIDE_VOID marker. If the marker names another parameter than the
definition, the translator sees no access at all and stops passing the call's arguments
(the runtime then reads stale registers). These tests keep the two in step, and every
native at the start of a function of the game.

El traductor sabe qué registros lee y escribe un reemplazo nativo leyendo su cuerpo, con
el nombre del parámetro que dice el marcador. Si el marcador nombra otro parámetro, el
traductor no ve ningún acceso y deja de pasar los argumentos: estas pruebas mantienen los
dos iguales.
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from project import REPOSITORY_ROOT, SYMBOLS_PATH  # noqa: E402
from symbols import SymbolTable  # noqa: E402

RUNTIME_SOURCES = REPOSITORY_ROOT / "runtime" / "src"

# PPC_NATIVE_OVERRIDE_VOID(8017108C, OSLoadContextNative, (CpuContext * cpu), (cpu));
MARKER = re.compile(
    r"PPC_NATIVE_OVERRIDE_VOID\(\s*(?P<address>[0-9A-Fa-f]{8})\s*,\s*(?P<symbol>\w+)\s*,"
    r"\s*\(\s*CpuContext\s*\*\s*(?P<parameter>\w+)\s*\)\s*,\s*\(\s*(?P<argument>\w+)\s*\)\s*\)\s*;"
)
# The natives this runtime cannot do without (see runtime/src/os/guest_os.h).
EXPECTED_NATIVES = {
    "OSDisableInterruptsNative",
    "OSEnableInterruptsNative",
    "OSRestoreInterruptsNative",
    "SelectThreadNative",
    "OSLoadContextNative",
}


def natives() -> list[tuple[Path, re.Match]]:
    found = []
    for path in sorted(RUNTIME_SOURCES.rglob("*.cpp")):
        found.extend((path, match) for match in MARKER.finditer(path.read_text(encoding="utf-8")))
    return found


def test_the_runtime_replaces_the_functions_it_needs():
    assert {match["symbol"] for _, match in natives()} >= EXPECTED_NATIVES


def test_every_marker_names_the_context_parameter_of_its_definition():
    for path, match in natives():
        definition = re.search(
            rf"void\s+{match['symbol']}\s*\(\s*CpuContext\s*\*\s*(\w+)\s*\)\s*\{{",
            path.read_text(encoding="utf-8"),
        )
        assert definition, f"{match['symbol']} is not defined in {path.name}"
        assert definition[1] == match["parameter"] == match["argument"], match[0]


def test_every_native_replaces_a_function_of_the_game_from_its_start():
    table = SymbolTable.read(SYMBOLS_PATH)
    for _, match in natives():
        symbol = table.by_address.get(int(match["address"], 16))
        assert symbol is not None and symbol.kind == "function", match[0]


def test_no_address_has_two_natives():
    addresses = [match["address"].upper() for _, match in natives()]
    assert len(addresses) == len(set(addresses))
