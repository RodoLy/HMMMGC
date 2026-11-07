"""Checks on the native runtime's sources (runtime/src) that need no compiler.

Comprobaciones sobre las fuentes del runtime nativo que no necesitan compilador.

The translator learns which functions of the game are replaced by natives, and which guest
registers each native reads and writes, from the PPC_NATIVE_OVERRIDE markers in the
runtime's sources. A native that takes the whole register file is read for
`<parameter>->gpr[N]`, with the parameter named in the marker: if the marker names another
parameter than the definition, the translator sees no access at all and stops passing the
call's arguments. A native with C++ parameters is described by the marker's parameter
list, which the compiler checks against the definition (native_functions.h). These tests
keep the markers in step with the definitions, every native at the start of a function of
the game, and each native named after the function it replaces.

El traductor sabe qué funciones del juego reemplazan los nativos, y qué registros lee y
escribe cada uno, por los marcadores PPC_NATIVE_OVERRIDE de las fuentes del runtime. Estas
pruebas mantienen los marcadores iguales a las definiciones, cada nativo al inicio de una
función del juego y con el nombre de la función que reemplaza.
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from project import REPOSITORY_ROOT, SYMBOLS_PATH  # noqa: E402
from symbols import SymbolTable  # noqa: E402

RUNTIME_SOURCES = REPOSITORY_ROOT / "runtime" / "src"

# PPC_NATIVE_OVERRIDE_VOID(8017108C, OSLoadContextNative, (CpuContext * cpu), (cpu));
# PPC_NATIVE_OVERRIDE(8018F234, GXInitNative, uint32_t, (uint32_t a, uint32_t b), (a, b));
MARKER = re.compile(
    r"PPC_NATIVE_OVERRIDE(?P<void>_VOID)?\(\s*(?P<address>[0-9A-Fa-f]{8})\s*,\s*(?P<symbol>\w+)\s*,"
    r"(?:\s*(?P<result>\w+)\s*,)?"
    r"\s*\((?P<parameters>[^)]*)\)\s*,\s*\((?P<arguments>[^)]*)\)\s*\)\s*;"
)
REGISTER_FILE_PARAMETER = re.compile(r"^CpuContext\s*\*\s*(?P<name>\w+)$")
# Functions that symbols.txt does not name yet, with the SDK name their native goes by.
# OSLoadContext: it restores a context's registers, GQRs and SRR0/SRR1 and returns with
# rfi; the OS's interrupt and exception handlers end with it.
# CARDSetStatus: right after CARDSetStatusAsync, it calls it with the SDK's synchronous
# callback and then waits in __CARDSync, as every synchronous CARD function does.
NOT_YET_NAMED = {0x8017108C: "OSLoadContext", 0x8018ED98: "CARDSetStatus"}
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


def parameter_names(parameters: str) -> list[str]:
    """The names in a C++ parameter list: `uint32_t a, float b` -> ["a", "b"]."""
    return [parameter.split()[-1].lstrip("*") for parameter in parameters.split(",") if parameter.strip()]


def test_the_runtime_replaces_the_functions_it_needs():
    assert {match["symbol"] for _, match in natives()} >= EXPECTED_NATIVES


def test_a_marker_has_a_result_type_only_when_it_is_not_void():
    for _, match in natives():
        assert (match["result"] is None) == (match["void"] is not None), match[0]


def test_every_register_file_marker_names_the_context_parameter_of_its_definition():
    for path, match in natives():
        register_file = REGISTER_FILE_PARAMETER.match(match["parameters"].strip())
        if not register_file:
            continue
        definition = re.search(
            rf"void\s+{match['symbol']}\s*\(\s*CpuContext\s*\*\s*(\w+)\s*\)\s*\{{",
            path.read_text(encoding="utf-8"),
        )
        assert definition, f"{match['symbol']} is not defined in {path.name}"
        assert definition[1] == register_file["name"] == match["arguments"].strip(), match[0]


def test_every_marker_passes_its_parameters_in_order():
    for _, match in natives():
        arguments = [argument.strip() for argument in match["arguments"].split(",") if argument.strip()]
        assert arguments == parameter_names(match["parameters"]), match[0]


def test_every_native_replaces_a_function_of_the_game_from_its_start_and_is_named_after_it():
    table = SymbolTable.read(SYMBOLS_PATH)
    for _, match in natives():
        address = int(match["address"], 16)
        symbol = table.by_address.get(address)
        assert symbol is not None and symbol.kind == "function", match[0]
        name = NOT_YET_NAMED.get(address, symbol.name)
        assert match["symbol"] == name + "Native", match[0]


def test_no_address_has_two_natives():
    addresses = [match["address"].upper() for _, match in natives()]
    assert len(addresses) == len(set(addresses))
