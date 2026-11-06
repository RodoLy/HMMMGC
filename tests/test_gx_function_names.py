"""The 26 GX functions of the game that were named by reading what their code does.

Las 26 funciones de GX del juego que se nombraron leyendo lo que hace su código.

The signature database (tools/sdk_signatures.py) could not name these: some have the same code shape as a
sibling and differ only in an immediate (GXSetTevColorIn and GXSetTevAlphaIn, GXSetLineWidth and
GXSetPointSize...), others are not in the database at all. A checksum ignores immediates, so it cannot tell
siblings apart; the immediates are exactly what does. Each function was identified from facts in its code
that belong to one function of the Nintendo SDK only:

- the number of a hardware register it writes (an XF register such as 0x1009, a BP register such as 0xEE),
  seen as an immediate constant;
- the offset into the library's state block (GXData) that it reads or writes, seen as the displacement of a
  load or a store; the same offset shows up in the function that uses it. The code finds the block through a
  pointer kept in the small-data area (`lwz r6, -0x4CB0(r2)`: the word at 0x80325AD0);
- the bit field it inserts into that state (rlwimi: shift, first bit, last bit);
- the SDK functions it calls.

The table EVIDENCE holds those facts, one row per function (tools/code_facts.py reads them from the code).
The tests check that the code has them, and that no other function of the GX library does: a row that
fitted two functions would not identify either. The register numbers are Dolphin's
(Source/Core/VideoCommon/BPMemory.h and XFMemory.h).

La base de firmas (tools/sdk_signatures.py) no pudo nombrar estas: algunas tienen la misma forma de código
que una hermana y se distinguen solo por un inmediato (GXSetTevColorIn y GXSetTevAlphaIn, GXSetLineWidth y
GXSetPointSize...), otras no están en la base. Un checksum ignora los inmediatos y no distingue hermanas; los
inmediatos son justo lo que las distingue. Cada función se identificó con datos de su código que pertenecen
a una sola función del SDK: el número de registro de hardware que escribe, el desplazamiento dentro del
bloque de estado de la biblioteca, el campo de bits que inserta y las funciones del SDK que llama. La tabla
EVIDENCE guarda esos datos, una fila por función. Las pruebas comprueban que el código los tiene y que
ninguna otra función de GX los tiene: una fila que sirviera para dos funciones no identificaría a ninguna.
"""

import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from code_facts import CodeFacts, Evidence, read_code_facts  # noqa: E402
from find_library_entry_points import functions_in, stretches_of_library  # noqa: E402

from dol import Dol  # noqa: E402
from function_finder import find_small_data_bases  # noqa: E402
from project import SYMBOLS_PATH  # noqa: E402
from symbols import SymbolTable  # noqa: E402

DOL_PATH = os.environ.get("HM_MM_DOL")
needs_game_files = pytest.mark.skipif(
    not DOL_PATH or not Path(DOL_PATH).is_file(),
    reason="set HM_MM_DOL to your extracted main.dol",
)


# The offsets are into GXData, the state block of the library. The ones that appear in several rows:
#   0x7C    line and point size            0x1D0  pixel engine mode (color/alpha update, blend)
#   0x130   TEV color stage settings       0x1E0  copy source of the display (GXCopyDisp reads it)
#   0x170   TEV alpha stage settings       0x1F0  copy source of a texture (GXCopyTex reads it)
#   0x204   general mode (texgens, chans)  0x5A8  "recording a display list"
#   0x5A9   "a display list saves the state"
# The BP registers (written through the command 0x61): 0x22 point/line size, 0x49 and 0x4A copy source,
# 0xE0 and 0xE1 TEV color registers, 0xEE to 0xF2 fog. The XF registers (written through the command 0x10):
# 0x1009 number of channels, 0x100A to 0x1011 channel colors and controls, 0x103F number of texgens.
EVIDENCE = [
    # The attribute 0x19 (normal, binormal and tangent) is turned into 0x0A (normal); the array's address
    # and stride go to the command processor registers 0xA0+n and 0xB0+n.
    Evidence(0x801912B0, "GXSetArray", 0x44, constants={0x19, 0x0A, 0xA0, 0xB0}),
    Evidence(
        0x80191584,
        "GXSetNumTexGens",
        0x3C,
        constants={0x103F},
        offsets={0x204},
        inserted_fields={(0, 28, 31)},
    ),
    # A switch on the token. Its cases set "display lists save the state" (0x5A9) and "abort waiting for the
    # copy" (0x5AA), and flush the transform unit's vertex count.
    Evidence(0x801915C0, "GXSetMisc", 0x94, offsets={0x5A9, 0x5AA, 0x5AC}),
    Evidence(0x801918D0, "GXPixModeSync", 0x24, constants={0x61}, offsets={0x1DC}),
    Evidence(0x801918F4, "GXTexModeSync", 0x24, constants={0x61, 0x6300}),
    # Swaps the callback that the token interrupt handler calls (see the data-flow test below).
    Evidence(
        0x80191A3C,
        "GXSetDrawSyncCallback",
        0x44,
        offsets={-0x6BE8},
        callees={"OSDisableInterrupts", "OSRestoreInterrupts"},
    ),
    # Width in bits 8-15 and offsets in bits 19-21 of the line/point size register; GXSetLineWidth, which
    # has the same code shape, writes bits 0-7 and 16-18 (Dolphin: linesize, pointsize).
    Evidence(
        0x80191E58,
        "GXSetPointSize",
        0x40,
        constants={0x61},
        offsets={0x7C},
        inserted_fields={(8, 16, 23), (19, 10, 12)},
    ),
    Evidence(0x801920A4, "GXSetDispCopySrc", 0x7C, constants={0x49, 0x4A}, offsets={0x1E0, 0x1E4}),
    Evidence(0x80192120, "GXSetTexCopySrc", 0x7C, constants={0x49, 0x4A}, offsets={0x1F0, 0x1F4}),
    # Three floats stored at the position fields of a light object.
    Evidence(0x80192C40, "GXInitLightPos", 0x10, offsets={0x28, 0x2C, 0x30}),
    Evidence(0x80192CD8, "GXSetChanAmbColor", 0xE8, constants={0x100A}, offsets={0xA8, 0xAC}),
    Evidence(0x80192DC0, "GXSetChanMatColor", 0xE8, constants={0x100C}, offsets={0xB0, 0xB4}),
    Evidence(
        0x80192EA8, "GXSetNumChans", 0x3C, constants={0x1009}, offsets={0x204}, inserted_fields={(4, 25, 27)}
    ),
    Evidence(0x80192EE4, "GXSetChanCtrl", 0xB0, constants={0x100E, 0x1010, 0x1011}),
    # Tile sizes by texture format: 0x40 bytes for format 6 (RGBA8) and 0x16 (Z24X8), 0x20 for the rest,
    # and a switch limited to formats up to 0x3C. It writes nothing to the hardware.
    Evidence(0x80192F94, "GXGetTexBufferSize", 0x15C, constants={0x3C, 0x16, 6, 0x40, 0x20}),
    # Four 4-bit inputs (color) against four 3-bit inputs (alpha), in the two arrays of stage settings.
    Evidence(
        0x8019427C,
        "GXSetTevColorIn",
        0x44,
        offsets={0x130},
        inserted_fields={(12, 16, 19), (8, 20, 23), (4, 24, 27), (0, 28, 31)},
    ),
    Evidence(
        0x801942C0,
        "GXSetTevAlphaIn",
        0x44,
        offsets={0x170},
        inserted_fields={(13, 16, 18), (10, 19, 21), (7, 22, 24), (4, 25, 27)},
    ),
    # Color and alpha operations insert the same fields; only the array they update tells them apart.
    Evidence(
        0x80194304,
        "GXSetTevColorOp",
        0x68,
        offsets={0x130},
        inserted_fields={(18, 13, 13), (19, 12, 12), (22, 8, 9)},
    ),
    Evidence(
        0x8019436C,
        "GXSetTevAlphaOp",
        0x68,
        offsets={0x170},
        inserted_fields={(18, 13, 13), (19, 12, 12), (22, 8, 9)},
    ),
    Evidence(0x801943D4, "GXSetTevColor", 0x60, constants={0x61, 0xE0, 0xE1}),
    # Fog parameters, written to the five fog registers.
    Evidence(0x80194848, "GXSetFog", 0x214, constants={0x61, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2}),
    # The two update flags are single bits of the same register: bit 3 is color, bit 4 is alpha.
    Evidence(0x80194BD4, "GXSetColorUpdate", 0x2C, offsets={0x1D0}, inserted_fields={(3, 28, 28)}),
    Evidence(0x80194C00, "GXSetAlphaUpdate", 0x2C, offsets={0x1D0}, inserted_fields={(4, 27, 27)}),
    # Recording a display list swaps the CPU's FIFO for the list's buffer and, when the list saves the
    # state, keeps 0x5B0 bytes of it. Ending it flushes, restores the FIFO and the state, and returns the
    # size.
    Evidence(
        0x80194E80,
        "GXBeginDisplayList",
        0xCC,
        constants={0x5B0},
        offsets={0x5A8, 0x5A9},
        callees={"GXSetCPUFifo", "__GXSetDirtyState"},
    ),
    Evidence(
        0x80194F4C,
        "GXEndDisplayList",
        0xC4,
        constants={0x5B0},
        offsets={0x5A8, 0x5A9},
        callees={"GXFlush", "GXSetCPUFifo", "OSDisableInterrupts", "OSRestoreInterrupts"},
    ),
    # Keeps the six viewport values in the state, then lets the shared code send them.
    Evidence(
        0x8019553C,
        "GXSetViewportJitter",
        0x58,
        offsets={0x4F4, 0x4F8, 0x4FC, 0x500, 0x504, 0x508},
        callees={"__GXSetViewport"},
    ),
]


def test_the_table_has_26_functions_with_different_names_and_addresses():
    assert len(EVIDENCE) == 26
    assert len({evidence.name for evidence in EVIDENCE}) == 26
    assert len({evidence.address for evidence in EVIDENCE}) == 26


# ---------------------------------------------------------------- the real main.dol


@pytest.fixture(scope="module")
def symbols() -> SymbolTable:
    return SymbolTable.read(SYMBOLS_PATH)


@pytest.fixture(scope="module")
def dol() -> Dol:
    return Dol(Path(DOL_PATH))


@pytest.fixture(scope="module")
def small_data_bases(dol) -> dict[int, int]:
    return find_small_data_bases(dol)


@pytest.fixture(scope="module")
def facts_by_address(dol, symbols, small_data_bases) -> dict[int, CodeFacts]:
    """The facts of every function of the GX library, by address."""
    gx_functions = functions_in(symbols, stretches_of_library(symbols, "GX"))
    return {
        function.address: read_code_facts(dol, function, symbols, small_data_bases)
        for function in gx_functions
    }


@needs_game_files
@pytest.mark.parametrize("evidence", EVIDENCE, ids=lambda evidence: evidence.name)
def test_the_function_has_the_name_the_size_and_the_facts_of_the_table(evidence, symbols, facts_by_address):
    symbol = symbols.by_name(evidence.name)
    assert symbol is not None, f"{evidence.name} is not in symbols.txt"
    assert symbol.address == evidence.address
    assert symbol.size == evidence.size

    missing = evidence.missing_facts(facts_by_address[evidence.address])
    assert not missing, f"not found in the code: {missing}"


@needs_game_files
@pytest.mark.parametrize("evidence", EVIDENCE, ids=lambda evidence: evidence.name)
def test_no_other_function_of_the_gx_library_has_the_same_facts(evidence, facts_by_address):
    fitting = [address for address, facts in facts_by_address.items() if evidence.is_met_by(facts)]
    assert fitting == [evidence.address]


# The data-flow checks: two functions that must use the same piece of state, found in both.


@needs_game_files
def test_the_draw_sync_setter_writes_the_variable_the_token_interrupt_handler_reads(
    symbols, dol, small_data_bases
):
    """The variable at -0x6BE8 off r13 holds the callback of the token (draw sync) interrupt."""
    setter = read_code_facts(dol, symbols.by_name("GXSetDrawSyncCallback"), symbols, small_data_bases)
    handler = read_code_facts(dol, symbols.by_name("GXTokenInterruptHandler"), symbols, small_data_bases)
    assert -0x6BE8 in setter.offsets
    assert -0x6BE8 in handler.offsets


@needs_game_files
def test_each_copy_source_setter_writes_the_state_that_its_copy_function_reads(
    symbols, dol, small_data_bases
):
    disp_copy = read_code_facts(dol, symbols.by_name("GXCopyDisp"), symbols, small_data_bases)
    tex_copy = read_code_facts(dol, symbols.by_name("GXCopyTex"), symbols, small_data_bases)
    assert {0x1E0, 0x1E4} <= disp_copy.offsets
    assert {0x1F0, 0x1F4} <= tex_copy.offsets
    assert not {0x1F0, 0x1F4} & disp_copy.offsets  # each reads only its own
    assert not {0x1E0, 0x1E4} & tex_copy.offsets
