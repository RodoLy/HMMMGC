#!/usr/bin/env python3
"""Run every kind of instruction the game uses on random operands, to check the recompiler.

Ejecuta con operandos aleatorios cada tipo de instrucción que usa el juego, para
comprobar el recompilador.

Playing the game only exercises the code that the player reaches, so "no
divergences" from the lockstep verifier (docs/VALIDATION.md) says little about
the instruction forms that no playthrough executed. This tool builds a program
out of the game's own instructions. For every form it finds in the game's
functions (`lwz`, `fmadds`, `ps_sum0`, ...) it copies a few different words
exactly as the game has them (the same registers, displacements and masks) and
runs each one on random register contents and random memory. The words are dealt
out among the register patterns the game uses (which operands are the same
register, which are r0) before any pattern is repeated, because a translation
that writes its result before it has read every operand only fails when an
operand is the destination (`lwz r3, 8(r3)`), and few words are like that.

The program is an ordinary DOL. `moderngekko-port build` recompiles it, and run
with STATICRECOMP_LOCKSTEP=1 the verifier compares every case against Dolphin's
interpreter. `report` then says, form by form, how many cases were compared and
how many differed. tools/run_isa_conformance.py does all of that in one go.

How a case is built:

  * main sets the machine up (all 32 general registers, and for the
    floating-point forms all 32 floating-point registers, CR, XER and FPSCR) from a
    random row of a table before each group of cases, then calls the cases one
    after another with `bl`.
  * a case is its own function: a few instructions that point a register at
    scratch memory when the instruction under test is a load or a store, the
    instruction itself, and `blr`.
  * the verifier ends a block at every call and return, so each case is compared
    on its own, with the whole register file, and no case needs code to store its
    results.

Jumps, system calls, traps and the instructions that change the memory map are
left out: they would take the program somewhere else.

The words do not have to come from the game. With `generate --synthetic` the tool
makes them up: for every instruction that the opcode tables name it builds words
with every combination of a few registers (so that the same register often shows
up twice, or r0) and some with random operands, and keeps the ones that are
defined instructions (Capstone is the judge: a word with a reserved bit set is
not one). That reaches forms that no game uses, and it needs no game at all, so
the check can run on a machine that has no disc. A seed gives the same words and
the same machine states every time.

Jugar solo ejercita el código al que llega el jugador, así que «sin divergencias»
del verificador lockstep (docs/VALIDATION.md) dice poco de las formas de
instrucción que ninguna partida ejecutó. Esta herramienta arma un programa con
las instrucciones del propio juego. Por cada forma que encuentra en las
funciones del juego (`lwz`, `fmadds`, `ps_sum0`, ...) copia unas cuantas
palabras distintas tal como las tiene el juego (los mismos registros,
desplazamientos y máscaras) y ejecuta cada una con registros y memoria
aleatorios. Las palabras se reparten primero entre las combinaciones de
registros que usa el juego (qué operandos son el mismo registro y cuáles son
r0) antes de repetir alguna, porque una traducción que escribe el resultado
antes de haber leído todos los operandos solo falla cuando un operando es el
destino (`lwz r3, 8(r3)`), y pocas palabras son así.

El programa es un DOL común. `moderngekko-port build` lo recompila y, con
STATICRECOMP_LOCKSTEP=1, el verificador compara cada caso con el intérprete de
Dolphin. `report` dice después, forma por forma, cuántos casos se compararon y
cuántos difirieron. tools/run_isa_conformance.py hace todo eso de una vez.

Cómo se arma un caso:

  * main prepara la máquina (los 32 registros generales y, en las formas de
    punto flotante, también los 32 de punto flotante, CR, XER y FPSCR) con una
    fila aleatoria de una tabla antes de cada grupo de casos, y llama a los casos
    uno tras otro con `bl`.
  * un caso es su propia función: unas instrucciones que apuntan un registro a
    memoria de trabajo cuando la instrucción es un load o un store, la
    instrucción misma y `blr`.
  * el verificador termina un bloque en cada llamada y retorno, así que cada caso
    se compara solo, con todo el archivo de registros, y ningún caso necesita
    código que guarde sus resultados.

Se dejan fuera los saltos, las llamadas al sistema, las trampas y las
instrucciones que cambian el mapa de memoria: llevarían el programa a otro lado.

Las palabras no tienen que venir del juego. Con `generate --synthetic` la
herramienta las inventa: por cada instrucción que nombran las tablas de códigos
de operación arma palabras con todas las combinaciones de unos pocos registros
(para que el mismo registro aparezca seguido dos veces, o r0) y otras con
operandos aleatorios, y se queda con las que son instrucciones definidas
(Capstone es el juez: una palabra con un bit reservado activo no lo es). Así
llega a formas que ningún juego usa, y no necesita juego alguno, así que la
comprobación puede correr en una máquina sin disco. Una semilla da siempre las
mismas palabras y los mismos estados de máquina.

Usage / Uso:
    python3 tools/isa_conformance.py generate --output root/sys/main.dol --cases cases.tsv
    python3 tools/isa_conformance.py generate --synthetic --output root/sys/main.dol --cases cases.tsv
    python3 tools/isa_conformance.py report cases.tsv lockstep.log --coverage coverage.txt
"""

import argparse
import itertools
import random
import re
import struct
import sys
from collections import defaultdict
from collections.abc import Iterator
from dataclasses import dataclass
from enum import Enum, IntEnum
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from dol import Dol, write_dol  # noqa: E402
from function_finder import INSTRUCTION_SIZE  # noqa: E402
from lockstep_coverage import read_verified_addresses  # noqa: E402
from lockstep_report import parse_log  # noqa: E402
from ppc import Instruction, format_instruction, sign_extend  # noqa: E402
from ppc_encode import (  # noqa: E402
    BLR,
    addic,
    b,
    bl,
    lfd,
    li,
    lis,
    lmw,
    lwz,
    mfmsr,
    mtcrf,
    mtfsf,
    mtmsr,
    mtspr,
    ori,
    psq_l,
    rlwinm,
    stfd,
    stw,
)
from ppc_mnemonics import (  # noqa: E402
    ILLEGAL_FORM,
    LINK_BIT_FORMS,
    OPCODE_PAIRED_SINGLE,
    OVERFLOW_ENABLE_BIT,
    RECORD_BIT_FORMS,
    RECORD_OR_LINK_BIT,
    form_name,
    operation_of,
)
from project import MAIN_DOL_PATH, SYMBOLS_PATH  # noqa: E402
from symbols import SymbolTable  # noqa: E402

# -- Where things go in the test program ------------------------------------------------

CODE_ADDRESS = 0x80003100  # where the code of a DOL normally starts
# The state table has a row of STATE_SIZE bytes for each group of cases, so it grows with the
# program and has to end before SCRATCH_ADDRESS. The 4 MB in between hold about 6,400 rows, that
# is 25,000 cases at the default of four per group. build_program checks that the table fits.
STATE_TABLE_ADDRESS = 0x80400000  # one random machine state per group of cases
SCRATCH_ADDRESS = 0x80800000  # random memory for the loads and stores to work on
SCRATCH_SIZE = 0x10100  # 64 KB, and a margin for lmw/stmw running past the last byte
# Loads and stores get an address of SCRATCH_CENTER plus the game's own displacement,
# a signed 16-bit number, so every address falls inside the first 64 KB.
SCRATCH_CENTER = SCRATCH_ADDRESS + 0x8000
DONE_ADDRESS = 0x80900000  # main writes DONE_VALUE here when every case has run
DONE_VALUE = 0xD0D0CAFE
# Before each group of cases, main writes the number of the group here. A program that hangs
# stays at the last group it started, and the number says which cases to look at. (A case that
# raises an exception is enough to hang it: the processor jumps to the exception vector, where
# there is no code.)
PROGRESS_ADDRESS = DONE_ADDRESS + 4
CONTROL_SECTION_SIZE = 8  # the two words above, which sit side by side

# -- Layout of one machine state in the table ------------------------------------------------

# General register n is the word at offset 4 * n, so a single `lmw r3` loads r3-r31.
# r2 holds the address of the row and is not loaded from it.
STATE_CR = 0x80
STATE_XER = 0x84
STATE_FPSCR = 0x88  # a double whose low word is the FPSCR, which is what mtfsf reads
STATE_FPRS = 0x90
FPR_STRIDE = 16  # per register: two singles for psq_l (both halves), then a double for lfd
STATE_SIZE = STATE_FPRS + 32 * FPR_STRIDE
STATE_POINTER_REGISTER = 2

# -- Special registers and bits ---------------------------------------------------------------

SPR_XER = 1
SPR_HID2 = 920
SPR_GQR0 = 912
MSR_FLOATING_POINT_AVAILABLE = 0x2000
HID2_QUANTIZED_LOADS_AND_PAIRED_SINGLES = 0xA0000000  # LSQE | PSE
GQR_TYPES = (0, 4, 5, 6, 7)  # float, u8, u16, s8, s16; the other values are reserved
# FPSCR: the rounding mode and nothing else. The exception-enable bits stay clear so that no
# floating-point instruction raises an exception, and the status bits start clear so that
# the sticky ones the instruction sets are visible in the comparison.
FPSCR_ROUNDING_MODE_MASK = 0x3
# XER: SO, OV, CA and the byte count of the string instructions.
XER_MASK = 0xE000007F
# FPSCR: the manual numbers its bits from the most significant one, and calls bit 20 reserved.
FPSCR_RESERVED_BIT = 20

# The special-purpose registers a word can read or write without changing how the rest of
# the program runs. LR is read-only here because every case returns through it.
READABLE_SPRS = frozenset({1, 8, 9, 26, 27, 272, 273, 274, 275, 287, *range(SPR_GQR0, SPR_GQR0 + 8)})
WRITABLE_SPRS = frozenset({1, 9, 272, 273, 274, 275})  # XER, CTR, SPRG0-3

# -- What each instruction needs before it can run ---------------------------------------------


class Treatment(Enum):
    REGISTERS = "registers only"  # reads and writes registers, CR, XER and the FPSCR
    DISPLACEMENT = "memory at rA + displacement"
    INDEXED = "memory at rA + rB"
    SPECIAL_REGISTER = "mfspr or mtspr"  # safe for some register numbers only
    SKIPPED = "skipped"


# fmt: off
DISPLACEMENT_OPERATIONS = frozenset({
    "lbz", "lbzu", "lha", "lhau", "lhz", "lhzu", "lwz", "lwzu", "lmw",
    "stb", "stbu", "sth", "sthu", "stw", "stwu", "stmw",
    "lfs", "lfsu", "lfd", "lfdu", "stfs", "stfsu", "stfd", "stfdu",
    "psq_l", "psq_lu", "psq_st", "psq_stu",
})
INDEXED_OPERATIONS = frozenset({
    "lbzx", "lbzux", "lhax", "lhaux", "lhzx", "lhzux", "lwzx", "lwzux", "lhbrx", "lwbrx",
    "stbx", "stbux", "sthx", "sthux", "stwx", "stwux", "sthbrx", "stwbrx",
    "lfsx", "lfsux", "lfdx", "lfdux", "stfsx", "stfsux", "stfdx", "stfdux", "stfiwx",
    "psq_lx", "psq_lux", "psq_stx", "psq_stux",
    "dcbf", "dcbi", "dcbst", "dcbt", "dcbtst", "dcbz", "icbi",
})
# The loads that also update their base register. The manual calls them an invalid form when
# the destination is the base register; for the stores and the floating-point loads the same
# register number names a different register, or the value stored before the update.
INTEGER_LOADS_WITH_UPDATE = frozenset({
    "lbzu", "lhau", "lhzu", "lwzu", "lbzux", "lhaux", "lhzux", "lwzux",
})
# Every indexed access that also updates its base register. The manual calls them an invalid
# form when rA is r0, since there is no register to update, and the address is then r0 + rB with
# r0 read as the register, which can be anywhere in memory and raise an exception.
INDEXED_OPERATIONS_WITH_UPDATE = frozenset({
    "lbzux", "lhaux", "lhzux", "lwzux", "stbux", "sthux", "stwux",
    "lfsux", "lfdux", "stfsux", "stfdux", "psq_lux", "psq_stux",
})
# The accesses with a displacement for which the console raises an alignment exception when the
# address is not a multiple of 4, and Dolphin's interpreter does too (Interpreter_LoadStore.cpp).
# The recompiled code makes the access instead of raising the exception, so the two cannot be
# compared on a misaligned address, and no program that works on a console makes one. (The
# indexed forms are always given an aligned address, and the integer loads and stores do not
# raise it: the processor handles a misaligned one itself.)
ALIGNMENT_CHECKED_OPERATIONS = frozenset({
    "lfs", "lfsu", "lfd", "lfdu", "stfs", "stfsu", "stfd", "stfdu", "lmw", "stmw",
})
REQUIRED_ALIGNMENT = 4  # bytes
# "Add to minus one" (rA + CA - 1) and "subtract from minus one" (~rA + CA - 1). With the carry
# set, the sum is rA + 2**32, which always carries out. Dolphin's interpreter works the carry out
# from Helper_Carry(a, carry - 1), which is false for it (addmex and subfmex in
# Interpreter_Integer.cpp), so it disagrees with the manual, and with the recompiled code, on
# XER[CA] whenever CA is set before the instruction. The game uses neither instruction.
ADD_MINUS_ONE_OPERATIONS = frozenset({"addme", "subfme"})
# The FPSCR bits that record an exception, numbered from the most significant as the manual does:
# overflow, underflow, zero divide, inexact, the invalid-operation bits and the three "software"
# ones. Setting one of them with mtfsb1 also sets FX, the exception summary, in Dolphin's
# interpreter (its source says "This instruction can affect FX"); the recompiled code does not.
# This is a gap of the recompiler, not undefined behavior. The game only sets the NI bit (29).
FPSCR_EXCEPTION_BITS = frozenset({3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 21, 22, 23})
# Control flow, traps, and what changes the memory map or reads the clock. Strings and the
# reservation pair are left out because the game does not use them.
SKIPPED_OPERATIONS = frozenset({
    "b", "bc", "bclr", "bcctr", "rfi", "sc", "tw", "twi",
    "mtmsr", "mtsr", "mtsrin", "mfsrin", "mftb", "tlbie", "tlbsync",
    "dcba", "dcbz_l", "eciwx", "ecowx", "lswi", "lswx", "stswi", "stswx", "lwarx", "stwcx.",
})
# fmt: on


def treatment_of(operation: str) -> Treatment:
    """How a word of this operation (its name without `.`, `o`, `l` or `a`) is prepared."""
    if operation == ILLEGAL_FORM or operation in SKIPPED_OPERATIONS:
        return Treatment.SKIPPED
    if operation in DISPLACEMENT_OPERATIONS:
        return Treatment.DISPLACEMENT
    if operation in INDEXED_OPERATIONS:
        return Treatment.INDEXED
    if operation in ("mfspr", "mtspr"):
        return Treatment.SPECIAL_REGISTER
    return Treatment.REGISTERS


class FloatState(IntEnum):
    """What the floating-point registers must hold for an instruction to be run meaningfully."""

    NONE = 0  # an integer instruction: the floating-point registers are left alone
    SINGLES = 1  # values that a single-precision instruction accepts
    DOUBLES = 2  # any double


# The single-precision instructions, and every paired-single one, are only defined for operands
# that fit in a single: the manual says the result is undefined for any other value, so
# Dolphin's interpreter and the recompiled code are free to disagree about a double that
# carries more bits (a NaN payload, say) and nothing would be wrong.
SINGLE_PRECISION_OPERATIONS = frozenset(
    {"fadds", "fsubs", "fmuls", "fdivs", "fmadds", "fmsubs", "fnmadds", "fnmsubs", "fres"}
)


def float_state_of(operation: str) -> FloatState:
    if not operation.startswith(("f", "ps_", "psq_", "lf", "stf", "mffs", "mtfs", "mcrfs")):
        return FloatState.NONE
    if operation in SINGLE_PRECISION_OPERATIONS or operation.startswith(("ps_", "psq_")):
        return FloatState.SINGLES
    return FloatState.DOUBLES


# -- Reading the fields of a word -------------------------------------------------------------


def field_rd(word: int) -> int:
    return (word >> 21) & 0x1F


def field_ra(word: int) -> int:
    return (word >> 16) & 0x1F


def field_rb(word: int) -> int:
    return (word >> 11) & 0x1F


def spr_number(word: int) -> int:
    """The register number of an mfspr or mtspr; the instruction stores its two halves swapped."""
    return field_ra(word) | (field_rb(word) << 5)


# -- Making a word safe to run ------------------------------------------------------------------


def load_constant(register: int, value: int) -> list[int]:
    return [lis(register, value >> 16), ori(register, register, value & 0xFFFF)]


def clear_reserved_fpscr_bit(float_register: int) -> list[int]:
    """Instructions that clear the FPSCR's reserved bit in the low word of a floating-point register.

    No instruction moves bits between the floating-point and the general registers, so the
    double goes through memory: stored, its low word (the half mtfsf reads) changed in a
    general register, stored back, and the double loaded again. The scratch registers
    r0 and r3 are free to change: the machine state is only compared after the case.
    """
    address_register, word_register = 3, 0
    low_word_offset = 4  # a double is stored with its high word first
    return [
        *load_constant(address_register, SCRATCH_CENTER),
        stfd(float_register, 0, address_register),
        lwz(word_register, low_word_offset, address_register),
        # With the first bit of the mask after the last one, the mask wraps around: it keeps
        # every bit except the one between them.
        rlwinm(word_register, word_register, 0, FPSCR_RESERVED_BIT + 1, FPSCR_RESERVED_BIT - 1),
        stw(word_register, low_word_offset, address_register),
        lfd(float_register, 0, address_register),
    ]


def clear_carry() -> list[int]:
    """The instruction that clears XER[CA] and changes nothing else.

    addic adds an immediate to a register and sets CA to the carry out of the sum. Zero added to
    r0 gives r0 again, and cannot carry. (Unlike addi, addic takes r0 as the register itself.)
    """
    return [addic(0, 0, 0)]


def misalignment_to_cancel(word: int, operation: str) -> int:
    """How far below SCRATCH_CENTER the base register must be for the address to be aligned.

    The address is the base register plus the word's displacement, and the displacement is the
    game's own (or a made-up one), so it is not always a multiple of 4. For the operations that
    raise an alignment exception, the base register is lowered by the remainder, which makes the
    address a multiple of 4; for the game's own words the remainder is zero and nothing changes.
    """
    if operation not in ALIGNMENT_CHECKED_OPERATIONS:
        return 0
    return sign_extend(word & 0xFFFF, 16) % REQUIRED_ALIGNMENT  # Python's remainder is never negative


def setup_before(word: int, rng: random.Random) -> list[int] | None:
    """The instructions to run just before `word` so that it can run safely.

    Returns an empty list when the word needs nothing, and None when it cannot be
    made safe (a jump, or a form that would be invalid with the register we must change).
    """
    operation, _ = operation_of(word)
    treatment = treatment_of(operation)
    base, index = field_ra(word), field_rb(word)
    if treatment is Treatment.REGISTERS:
        if operation == "mtfsf":
            # The register mtfsf copies from is random, and a random value would sometimes
            # carry the reserved bit, which the recompiled code keeps and Dolphin's
            # interpreter clears. The game only writes back what it read with mffs.
            return clear_reserved_fpscr_bit(field_rb(word))
        if operation == "mtfsb1" and field_rd(word) in FPSCR_EXCEPTION_BITS:
            return None  # see FPSCR_EXCEPTION_BITS
        if operation in ADD_MINUS_ONE_OPERATIONS:
            # The reference gets XER[CA] wrong when the carry is set going in, so these words
            # only run with it clear. See ADD_MINUS_ONE_OPERATIONS.
            return clear_carry()
        return []
    if treatment is Treatment.SPECIAL_REGISTER:
        allowed = READABLE_SPRS if operation == "mfspr" else WRITABLE_SPRS
        if spr_number(word) not in allowed:
            return None
        if operation == "mtspr" and spr_number(word) == SPR_XER:
            # Only SO, OV, CA and the byte count of XER are defined. A write of the other bits
            # is kept by the recompiled code and cleared by Dolphin's interpreter (the manual
            # calls them reserved). The game only writes back what it read with mfxer.
            return load_constant(field_rd(word), rng.getrandbits(32) & XER_MASK)
        return []
    if treatment is Treatment.DISPLACEMENT:
        if base == 0:  # rA = r0 would mean "no base register" and address the bottom of memory
            return None
        if operation in INTEGER_LOADS_WITH_UPDATE and base == field_rd(word):
            return None  # an invalid form: the loaded value and the new address want the same register
        if operation == "lmw" and base >= field_rd(word):  # the base register would be overwritten
            return None
        return load_constant(base, SCRATCH_CENTER - misalignment_to_cancel(word, operation))
    if treatment is Treatment.INDEXED:
        if base == 0:
            if operation in INDEXED_OPERATIONS_WITH_UPDATE:
                return None  # an invalid form, whose address would be r0 + rB
            return load_constant(index, SCRATCH_CENTER)  # the address is rB alone
        if base == index or (operation in INTEGER_LOADS_WITH_UPDATE and base == field_rd(word)):
            return None
        # A multiple of 8 keeps doubles and cache blocks aligned.
        return [*load_constant(base, SCRATCH_CENTER), li(index, rng.randrange(-0x20, 0x20) * 8)]
    return None


# -- The words the game has ------------------------------------------------------------------


NO_GAME_ADDRESS = 0  # the game address of a word that was not taken from the game


@dataclass(frozen=True)
class Candidate:
    word: int
    game_address: int  # the first place in the game where this exact word appears


def collect_candidates(dol: Dol, symbols: SymbolTable) -> dict[str, list[Candidate]]:
    """Every distinct instruction word in the game's functions, grouped by form name."""
    by_form: dict[str, dict[int, int]] = defaultdict(dict)
    for function in symbols:
        if function.kind != "function" or function.size == 0 or not dol.is_mapped(function.address):
            continue
        for address in range(function.address, function.address + function.size, INSTRUCTION_SIZE):
            word = dol.u32(address)
            form = form_name(word)
            if form != ILLEGAL_FORM:
                by_form[form].setdefault(word, address)
    return {
        form: [Candidate(word, address) for word, address in sorted(words.items())]
        for form, words in by_form.items()
    }


# -- Words that need no game -------------------------------------------------------------------

# The primary opcodes that carry a 10-bit extended opcode (bits 10 to 1 counting from the least
# significant bit). Opcodes 59 and 63 use only the low five bits of it for the opcode and the
# rest for a register operand, and paired singles (opcode 4) mix several widths, so enumerating
# the whole field reaches every form of the five.
OPCODES_WITH_EXTENDED_FIELD = (4, 19, 31, 59, 63)
PRIMARY_OPCODE_COUNT = 64
PRIMARY_OPCODE_SHIFT = 26
EXTENDED_FIELD_SHIFT = 1
EXTENDED_FIELD_VALUES = 1 << 10
# The bits a word can vary in apart from its opcode: all 26 below the primary opcode, or, when
# there is an extended opcode, the operands (bits 25 to 11) and the record or link bit (bit 0).
FREE_BITS_WITHOUT_EXTENDED_FIELD = 0x03FF_FFFF
FREE_BITS_WITH_EXTENDED_FIELD = 0x03FF_F801
OPERAND_FIELD_SHIFTS = (21, 16, 11)  # rD or rS, rA, rB
REGISTER_FIELD_MASK = 0x1F
# The registers that synthetic words draw their operands from. There are few, r0 among them, so
# the operands often name the same register or r0, which is where a translation goes wrong.
SYNTHETIC_REGISTERS = (0, 3, 4, 5)
CHANCE_OF_CHOSEN_REGISTER = 0.7  # the rest of the time the field keeps its random bits
RANDOM_FILLS_PER_ENCODING = 48
SPR_FIELD_MASK = 0x3FF << 11


def encode_spr(word: int, spr: int) -> int:
    """`word` with its SPR field set to `spr`, stored with the two halves swapped (see spr_number)."""
    return (word & ~SPR_FIELD_MASK) | ((spr & 0x1F) << 16) | ((spr >> 5) << 11)


# Capstone prints the instructions and rejects most of the words whose reserved bits are set, which
# makes it the judge of whether a made-up word is an instruction. It is not a perfect judge, and
# these tables say where it needs help.
OVERFLOW_ENABLE_WORD_BIT = OVERFLOW_ENABLE_BIT << EXTENDED_FIELD_SHIFT  # the "o" of addo, subfo, ...
FORMS_WHOSE_RECORD_BIT_CAPSTONE_REJECTS = ("mtfsb0", "mtfsb1")
FORMS_CAPSTONE_DOES_NOT_DECODE = ("fcmpo", "mcrxr")
# The bits that the manual requires to be zero in a form, by form, and that Capstone does not
# check. For the two forms it does not decode at all that is every reserved bit: fcmpo has two
# after the field of the condition register, and mcrxr has nothing but that field. For the rest,
# Capstone follows later versions of the architecture than the 750CL manual, in which these bits
# mean something, and reads such a word as an instruction that the Gekko does not have; the
# recompiler leaves the word to the interpreter, so a case made of it compares nothing.
# fmt: off
COMPARE_RESERVED_BITS = 0x0060_0000  # the "L" that makes a compare 64 bits wide, and the bit before it
CACHE_RESERVED_BITS = 0x03E0_0000  # the hint of dcbt and dcbtst, the flush level of dcbf, ...
NO_OPERAND_BITS = 0x03FF_F800  # everything between the opcode and the extended opcode
AFTER_DESTINATION_BITS = 0x001F_F800  # the ten bits between rD (or frD) and the extended opcode
SEGMENT_REGISTER_RESERVED_BITS = 0x0010_F800  # the bit before the number and the field after it
SEGMENT_REGISTER_INDEXED_RESERVED_BITS = 0x001F_0000  # where the other forms keep rA
RESERVED_BITS_OF_FORMS = {
    "fcmpo": 0x0060_0001, "mcrxr": 0x007F_F801,
    "cmp": COMPARE_RESERVED_BITS, "cmpl": COMPARE_RESERVED_BITS,
    "cmpi": COMPARE_RESERVED_BITS, "cmpli": COMPARE_RESERVED_BITS,
    "dcbf": CACHE_RESERVED_BITS, "dcbi": CACHE_RESERVED_BITS, "dcbst": CACHE_RESERVED_BITS,
    "dcbt": CACHE_RESERVED_BITS, "dcbtst": CACHE_RESERVED_BITS, "dcbz": CACHE_RESERVED_BITS,
    "icbi": CACHE_RESERVED_BITS,
    "eieio": NO_OPERAND_BITS, "sync": NO_OPERAND_BITS, "isync": NO_OPERAND_BITS,
    "mtfsf": 0x0201_0000,  # the "L" and "W" bits that a later version added around the mask
    "mtfsfi": 0x007F_0800,  # the "W" bit that a later version added before the immediate, and the rest
    "mfcr": AFTER_DESTINATION_BITS,  # a later version reads a set bit 11 as mfocrf, which reads one field
    "mtcrf": 0x0010_0800,  # the bit before the mask (mtocrf, in a later version) and the one after it
    "mffs": AFTER_DESTINATION_BITS,  # a later version tells mffsl, mffscdrn, ... apart by these bits
    "mfsr": SEGMENT_REGISTER_RESERVED_BITS, "mtsr": SEGMENT_REGISTER_RESERVED_BITS,
    "mfsrin": SEGMENT_REGISTER_INDEXED_RESERVED_BITS, "mtsrin": SEGMENT_REGISTER_INDEXED_RESERVED_BITS,
}
# fmt: on


def capstone_view(word: int) -> int:
    """The word with the bits that Capstone does not model cleared, so that it can read the rest.

    That is the overflow-enable bit of the `addo` family, and the record bit of `mtfsb0.` and
    `mtfsb1.`. Capstone prints `.word` for a word that has them.
    """
    operation, overflow_enabled = operation_of(word)
    if overflow_enabled:
        word &= ~OVERFLOW_ENABLE_WORD_BIT
    if operation in FORMS_WHOSE_RECORD_BIT_CAPSTONE_REJECTS:
        word &= ~RECORD_OR_LINK_BIT
    return word


def reserved_bits(word: int, operation: str) -> int:
    """The bits that must be zero for `word` to be a defined `operation`, and that Capstone leaves alone.

    Bit 0 is one of them in the forms that have an extended opcode and no use for it: where it is
    not the record bit (`add.`), the link bit (`bl`) or part of the name (`stwcx.`) it is reserved.
    """
    bits = RESERVED_BITS_OF_FORMS.get(operation, 0)
    uses_bit_0 = operation in RECORD_BIT_FORMS or operation in LINK_BIT_FORMS or operation.endswith(".")
    if word >> PRIMARY_OPCODE_SHIFT in OPCODES_WITH_EXTENDED_FIELD and not uses_bit_0:
        bits |= RECORD_OR_LINK_BIT
    return bits


def is_instruction(word: int) -> bool:
    """Whether the word is a defined instruction: a form of the tables, with its reserved bits clear."""
    operation, _ = operation_of(word)
    if operation == ILLEGAL_FORM or word & reserved_bits(word, operation):
        return False
    if operation in FORMS_CAPSTONE_DOES_NOT_DECODE:
        return True
    listing = format_instruction(Instruction(0, capstone_view(word)))
    if listing.startswith(".word"):
        return False
    # The paired singles share their opcode with AltiVec, and a paired-single word with a reserved
    # bit set is often an AltiVec instruction, which the Gekko does not have. Capstone says which
    # one it read, and the name must be the form's own (these instructions have no aliases).
    return word >> PRIMARY_OPCODE_SHIFT != OPCODE_PAIRED_SINGLE or listing.startswith(operation)


def named_encodings() -> Iterator[tuple[int, int]]:
    """The opcode bits of every instruction the tables name, with the bits left free for the operands.

    An opcode that is a whole instruction (`lwz`) gives one encoding. One that has an extended
    opcode (`add`, `fmadds`) gives one for each value of the extended field that names a form.
    """
    for primary in range(PRIMARY_OPCODE_COUNT):
        if primary in OPCODES_WITH_EXTENDED_FIELD:
            for extended in range(EXTENDED_FIELD_VALUES):
                encoding = (primary << PRIMARY_OPCODE_SHIFT) | (extended << EXTENDED_FIELD_SHIFT)
                if operation_of(encoding)[0] != ILLEGAL_FORM:
                    yield encoding, FREE_BITS_WITH_EXTENDED_FIELD
        elif operation_of(primary << PRIMARY_OPCODE_SHIFT)[0] != ILLEGAL_FORM:
            yield primary << PRIMARY_OPCODE_SHIFT, FREE_BITS_WITHOUT_EXTENDED_FIELD


def register_combinations(encoding: int, free_bits: int) -> Iterator[int]:
    """`encoding` with every way of filling its three operand fields from SYNTHETIC_REGISTERS.

    All the other bits stay zero, which is the value that the reserved bits of most forms must
    have (`sync`, `mcrxr`, `fcmpo` are valid only when they are). Each combination comes once
    for every value of the two lowest bits that the encoding leaves free: bit 0 is the record or
    link bit, and bit 1 is the absolute-address bit of the branches.
    """
    low_bit_values = [value for value in range(4) if value & ~free_bits == 0]
    for registers in itertools.product(SYNTHETIC_REGISTERS, repeat=len(OPERAND_FIELD_SHIFTS)):
        word = encoding
        for shift, register in zip(OPERAND_FIELD_SHIFTS, registers, strict=True):
            word |= register << shift
        for low_bits in low_bit_values:
            yield word | low_bits


def synthetic_word(encoding: int, free_bits: int, rng: random.Random) -> int:
    """A word with the opcode bits of `encoding` and random operands."""
    word = encoding | (rng.getrandbits(32) & free_bits)
    for shift in OPERAND_FIELD_SHIFTS:
        if rng.random() < CHANCE_OF_CHOSEN_REGISTER:
            word = (word & ~(REGISTER_FIELD_MASK << shift)) | (rng.choice(SYNTHETIC_REGISTERS) << shift)
    operation, _ = operation_of(word)
    if operation in ("mfspr", "mtspr"):
        # A random register number is almost never one that is safe to read or write.
        allowed = READABLE_SPRS if operation == "mfspr" else WRITABLE_SPRS
        word = encode_spr(word, rng.choice(sorted(allowed)))
    return word


def synthetic_candidates(
    rng: random.Random, random_fills_per_encoding: int = RANDOM_FILLS_PER_ENCODING
) -> dict[str, list[Candidate]]:
    """Words of every form the opcode tables name, built without the game, grouped by form name.

    Each encoding gets every combination of a few registers, which puts the same register in
    several operands, and a number of fills with random operands. A word counts only if it is
    an instruction (see is_instruction): that rejects the words with a reserved bit set, which
    no compiler emits and the manual does not define.
    """
    by_form: dict[str, set[int]] = defaultdict(set)
    for encoding, free_bits in named_encodings():
        words = list(register_combinations(encoding, free_bits))
        words += [synthetic_word(encoding, free_bits, rng) for _ in range(random_fills_per_encoding)]
        for word in words:
            form = form_name(word)
            if form != ILLEGAL_FORM and is_instruction(word):
                by_form[form].add(word)
    return {
        form: [Candidate(word, NO_GAME_ADDRESS) for word in sorted(words)] for form, words in by_form.items()
    }


def synthetic_candidates_for_seed(seed: int) -> dict[str, list[Candidate]]:
    """The made-up words of one run: the same words every time for the same seed."""
    return synthetic_candidates(random.Random(f"synthetic-words-{seed}"))


# The register operands of a disassembled instruction: r3, f31. An r0 that stands for "zero"
# (the base of a load, the source of `addi`) is printed as a plain 0 and does not match.
REGISTER_OPERAND = re.compile(r"\b[rf]\d+\b")
R0_OPERAND = -1


def register_pattern(word: int) -> tuple[int, ...]:
    """Which register operands of an instruction name the same register, and which name r0.

    Each operand becomes the position of the first operand that names the same register, or
    R0_OPERAND for r0, which some forms read as zero: `add r3, r27, r28` gives (0, 1, 2),
    `add r3, r3, r4` gives (0, 0, 2) and `add r3, r0, r4` gives (0, -1, 2).
    """
    first_position: dict[str, int] = {}
    pattern = []
    listing = format_instruction(Instruction(0, capstone_view(word)))
    for position, register in enumerate(REGISTER_OPERAND.findall(listing)):
        pattern.append(R0_OPERAND if register == "r0" else first_position.setdefault(register, position))
    return tuple(pattern)


def deal_by_register_pattern(candidates: list[Candidate], rng: random.Random) -> list[Candidate]:
    """The candidates in an order that tries every register pattern before repeating one.

    A translation that writes its result before it has read every source goes wrong only when
    a source is the destination (`lwz r3, 8(r3)`), and the game has such words in small numbers
    (one `fmadd` has all four operands the same register). Picking words at random would
    mostly miss them, so the words are grouped by pattern and dealt out one group at a time.
    """
    groups: dict[tuple[int, ...], list[Candidate]] = defaultdict(list)
    for candidate in sorted(candidates, key=lambda candidate: candidate.word):
        groups[register_pattern(candidate.word)].append(candidate)
    ordered_groups = [groups[pattern] for pattern in sorted(groups)]
    rng.shuffle(ordered_groups)  # when a form has more patterns than words, which ones is left to chance
    for group in ordered_groups:
        rng.shuffle(group)
    longest_group = max(len(group) for group in ordered_groups)
    return [group[turn] for turn in range(longest_group) for group in ordered_groups if turn < len(group)]


# -- Random machine states ----------------------------------------------------------------------

INTERESTING_WORDS = (
    0, 1, 2, 3, 4, 7, 8, 0x1F, 0x20, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x10000,
    0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFE, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA,
)  # fmt: skip
INTERESTING_SINGLES = (
    0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x3F000000, 0x40000000, 0x40490FDB, 0x7F7FFFFF,
    0x00800000, 0x007FFFFF, 0x00000001, 0x7F800000, 0xFF800000, 0x7FC00000, 0x7FA00000, 0x4B000000,
    0x4F000000, 0xCF000000,
)  # fmt: skip
INTERESTING_DOUBLES = (
    0x0000000000000000, 0x8000000000000000, 0x3FF0000000000000, 0xBFF0000000000000, 0x3FE0000000000000,
    0x4000000000000000, 0x7FEFFFFFFFFFFFFF, 0x0010000000000000, 0x000FFFFFFFFFFFFF, 0x0000000000000001,
    0x7FF0000000000000, 0xFFF0000000000000, 0x7FF8000000000000, 0x7FF0000000000001, 0x41DFFFFFFFC00000,
    0x41E0000000000000, 0xC1E0000000000000, 0x4330000000000000, 0x43E0000000000000, 0x47EFFFFFE0000000,
    0x3810000000000000, 0x36A0000000000000,
)  # fmt: skip


def random_word(rng: random.Random) -> int:
    """A 32-bit value, often one of the edge cases where arithmetic goes wrong."""
    roll = rng.random()
    if roll < 0.40:
        return rng.choice(INTERESTING_WORDS)
    if roll < 0.55:
        return rng.getrandbits(8)
    if roll < 0.70:
        return -rng.getrandbits(8) & 0xFFFFFFFF
    return rng.getrandbits(32)


def random_single_bits(rng: random.Random) -> int:
    return rng.choice(INTERESTING_SINGLES) if rng.random() < 0.4 else rng.getrandbits(32)


def random_double_bits(rng: random.Random) -> int:
    """The bits of a double: an edge case, a number of moderate size, or anything at all."""
    roll = rng.random()
    if roll < 0.35:
        return rng.choice(INTERESTING_DOUBLES)
    if roll < 0.85:
        exponent = 1023 + rng.randrange(-40, 41)
        return (rng.getrandbits(1) << 63) | (exponent << 52) | rng.getrandbits(52)
    return rng.getrandbits(64)


def random_gqr(rng: random.Random) -> int:
    """A graphics quantization register: load and store type and scale, all valid."""
    load_type, store_type = rng.choice(GQR_TYPES), rng.choice(GQR_TYPES)
    return (rng.getrandbits(6) << 24) | (load_type << 16) | (rng.getrandbits(6) << 8) | store_type


def random_state(rng: random.Random) -> bytes:
    """One row of the state table, STATE_SIZE bytes."""
    row = bytearray(STATE_SIZE)
    for register in range(32):
        struct.pack_into(">I", row, 4 * register, random_word(rng))
    struct.pack_into(">I", row, STATE_CR, rng.getrandbits(32))
    struct.pack_into(">I", row, STATE_XER, rng.getrandbits(32) & XER_MASK)
    struct.pack_into(">Q", row, STATE_FPSCR, rng.getrandbits(2) & FPSCR_ROUNDING_MODE_MASK)
    for register in range(32):
        offset = STATE_FPRS + FPR_STRIDE * register
        struct.pack_into(">II", row, offset, random_single_bits(rng), random_single_bits(rng))
        struct.pack_into(">Q", row, offset + 8, random_double_bits(rng))
    return bytes(row)


def state_setup_code(row_address: int, float_state: FloatState) -> list[int]:
    """Instructions that load a state-table row into the machine."""
    pointer = STATE_POINTER_REGISTER
    code = load_constant(pointer, row_address)
    code += [lwz(0, STATE_CR, pointer), mtcrf(0xFF, 0), lwz(0, STATE_XER, pointer), mtspr(SPR_XER, 0)]
    if float_state is not FloatState.NONE:
        code += [lfd(0, STATE_FPSCR, pointer), mtfsf(0xFF, 0)]
        for register in range(32):
            offset = STATE_FPRS + FPR_STRIDE * register
            # psq_l with GQR0 (float, no scaling) fills both halves with two singles.
            code.append(psq_l(register, offset, pointer))
            if float_state is FloatState.DOUBLES:
                code.append(lfd(register, offset + 8, pointer))  # replace the first half with a double
    # The general registers last: r0 and r1 were scratch above, and lmw also fills r3-r31.
    code += [lwz(0, 0, pointer), lwz(1, 4, pointer), lmw(3, 12, pointer)]
    return code


def progress_code(group_number: int) -> list[int]:
    """Instructions that write `group_number` to PROGRESS_ADDRESS.

    They run before the group's machine state is loaded, which overwrites the two registers
    they use.
    """
    address_register, number_register = 4, 0
    return [
        *load_constant(number_register, group_number),
        lis(address_register, PROGRESS_ADDRESS >> 16),
        stw(number_register, PROGRESS_ADDRESS & 0xFFFF, address_register),
    ]


# -- The program ------------------------------------------------------------------------------


@dataclass(frozen=True)
class Case:
    """One instruction of the game, run once on a random machine state."""

    address: int  # where the case's function starts (its setup, then the instruction, then blr)
    word_address: int  # where the instruction under test is in the test program
    form: str
    word: int
    game_address: int  # where the game has this word, to read it in context

    @property
    def text(self) -> str:
        return " ".join(format_instruction(Instruction(self.word_address, self.word)).split())


@dataclass(frozen=True)
class CaseDraft:
    """A case before the program has an address for it."""

    form: str
    word: int
    game_address: int
    setup: list[int]
    float_state: FloatState


@dataclass
class Program:
    text_words: list[int]
    data_sections: dict[int, tuple[int, bytes]]
    cases: list[Case]
    skipped_forms: dict[str, str]  # form -> why no case was made
    group_first_cases: list[int]  # for each group, the position in `cases` of its first case

    def cases_of_group(self, group_number: int) -> list[Case]:
        """The cases that main runs after it writes `group_number` to PROGRESS_ADDRESS."""
        if not 0 <= group_number < len(self.group_first_cases):
            return []
        end_positions = [*self.group_first_cases[1:], len(self.cases)]
        return self.cases[self.group_first_cases[group_number] : end_positions[group_number]]

    def write(self, path: Path) -> Path:
        return write_dol(
            path, text={0: (CODE_ADDRESS, self.text_words)}, data=self.data_sections, entry=CODE_ADDRESS
        )


@dataclass(frozen=True)
class Section:
    """A stretch of memory that the test program fills when it is loaded."""

    name: str
    address: int
    size: int

    @property
    def end(self) -> int:
        return self.address + self.size


class SectionsOverlapError(ValueError):
    """Two parts of the test program would be loaded on top of each other."""


def check_that_sections_do_not_overlap(sections: list[Section]) -> None:
    """Raise SectionsOverlapError if two of `sections` share a byte of memory.

    The loader copies the sections one after the other, so the one copied last overwrites the
    end of the one before. Nothing fails when that happens: a row of the state table that the
    scratch memory covered just holds other numbers, and the cases that use it start from a
    machine state nobody chose.
    """
    in_memory_order = sorted(sections, key=lambda section: section.address)
    for section, following in zip(in_memory_order, in_memory_order[1:], strict=False):
        if section.end > following.address:
            raise SectionsOverlapError(
                f"{section.name} (0x{section.address:08X} to 0x{section.end:08X}) runs into "
                f"{following.name} (which starts at 0x{following.address:08X}): "
                "use fewer --words-per-form or a larger --group-size"
            )


def choose_drafts(
    candidates: dict[str, list[Candidate]], words_per_form: int, rng: random.Random
) -> tuple[list[CaseDraft], dict[str, str]]:
    """Up to `words_per_form` runnable words for each form, and the reason for the forms with none."""
    drafts = []
    skipped = {}
    for form in sorted(candidates):
        operation, _ = operation_of(candidates[form][0].word)
        if treatment_of(operation) is Treatment.SKIPPED:
            skipped[form] = "control flow, trap or system instruction"
            continue
        chosen = 0
        for candidate in deal_by_register_pattern(candidates[form], rng):
            setup = setup_before(candidate.word, rng)
            if setup is None:
                continue
            drafts.append(
                CaseDraft(form, candidate.word, candidate.game_address, setup, float_state_of(operation))
            )
            chosen += 1
            if chosen == words_per_form:
                break
        if chosen == 0:
            skipped[form] = "no word of this form can be run safely"
    return drafts, skipped


def build_program(
    candidates: dict[str, list[Candidate]], seed: int, words_per_form: int, group_size: int
) -> Program:
    """The whole test program for the words in `candidates`."""
    rng = random.Random(seed)
    drafts, skipped = choose_drafts(candidates, words_per_form, rng)
    # The cases of a group share one machine state, so a group holds cases that want the same
    # kind of floating-point registers (and the integer ones skip loading them at all).
    drafts.sort(key=lambda draft: (draft.float_state, draft.form))
    groups = []
    for float_state in FloatState:
        members = [draft for draft in drafts if draft.float_state is float_state]
        groups += [members[start : start + group_size] for start in range(0, len(members), group_size)]

    # main: a prologue, then for each group a note of its number, a state set-up and one `bl`
    # per case, then the end.
    prologue = [mfmsr(0), ori(0, 0, MSR_FLOATING_POINT_AVAILABLE), mtmsr(0)]
    prologue += [lis(0, HID2_QUANTIZED_LOADS_AND_PAIRED_SINGLES >> 16), mtspr(SPR_HID2, 0)]
    prologue += [li(0, 0), mtspr(SPR_GQR0, 0)]  # float, unscaled: state_setup_code relies on it
    for gqr in range(1, 8):
        prologue += [*load_constant(0, random_gqr(rng)), mtspr(SPR_GQR0 + gqr, 0)]
    state_rows = [random_state(rng) for _ in groups]
    setups = [
        state_setup_code(STATE_TABLE_ADDRESS + STATE_SIZE * index, group[0].float_state)
        for index, group in enumerate(groups)
    ]
    # The end of main: tell the outside world every case has run, then loop forever on one branch.
    epilogue = [*load_constant(3, DONE_VALUE), lis(4, DONE_ADDRESS >> 16), stw(3, 0, 4)]
    progress_length = len(progress_code(0))  # the same for every group number
    main_length = (
        len(prologue)
        + (progress_length * len(groups))
        + sum(len(s) for s in setups)
        + len(drafts)
        + len(epilogue)
        + 1
    )

    # Lay the cases out after main, one right after the other. They must touch: DolRecomp
    # charges cycles to a block only when its first instruction follows an instruction that
    # ends a block, and the verifier skips a block that charges none. Padding between cases
    # would be taken for data in the middle of one function and the cases would be skipped.
    next_address = CODE_ADDRESS + INSTRUCTION_SIZE * main_length
    cases = []
    case_words: list[list[int]] = []
    for draft in (draft for group in groups for draft in group):
        words = [*draft.setup, draft.word, BLR]
        cases.append(
            Case(
                address=next_address,
                word_address=next_address + INSTRUCTION_SIZE * len(draft.setup),
                form=draft.form,
                word=draft.word,
                game_address=draft.game_address,
            )
        )
        case_words.append(words)
        next_address += INSTRUCTION_SIZE * len(words)

    main = list(prologue)
    case_index = 0
    group_first_cases = []
    for group_number, (group, setup) in enumerate(zip(groups, setups, strict=True)):
        group_first_cases.append(case_index)
        main += progress_code(group_number)
        main += setup
        for _ in group:
            main.append(bl(CODE_ADDRESS + INSTRUCTION_SIZE * len(main), cases[case_index].address))
            case_index += 1
    main += epilogue
    end_address = CODE_ADDRESS + INSTRUCTION_SIZE * len(main)
    main.append(b(end_address, end_address))  # a branch to itself: the program is finished
    assert len(main) == main_length, "the cases were laid out for a main of a different length"

    text_words = list(main)
    for words in case_words:
        text_words += words

    state_table = b"".join(state_rows)
    scratch = bytes(rng.getrandbits(8) for _ in range(SCRATCH_SIZE))
    control_words = bytes(CONTROL_SECTION_SIZE)
    check_that_sections_do_not_overlap(
        [
            Section("the code", CODE_ADDRESS, INSTRUCTION_SIZE * len(text_words)),
            Section("the state table", STATE_TABLE_ADDRESS, len(state_table)),
            Section("the scratch memory", SCRATCH_ADDRESS, len(scratch)),
            Section("the control words", DONE_ADDRESS, len(control_words)),
        ]
    )
    data_sections = {
        0: (STATE_TABLE_ADDRESS, state_table),
        1: (SCRATCH_ADDRESS, scratch),
        2: (DONE_ADDRESS, control_words),
    }
    return Program(text_words, data_sections, cases, skipped, group_first_cases)


# -- The list of cases, to find them again in the verifier's log ---------------------------------

CASES_HEADER = "# case_address\tword_address\tform\tword\tgame_address\tinstruction"


def format_cases(cases: list[Case]) -> str:
    lines = [CASES_HEADER]
    for case in cases:
        lines.append(
            f"0x{case.address:08X}\t0x{case.word_address:08X}\t{case.form}\t0x{case.word:08X}"
            f"\t0x{case.game_address:08X}\t{case.text}"
        )
    return "\n".join(lines) + "\n"


def parse_cases(text: str) -> list[Case]:
    cases = []
    for line in text.splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        address, word_address, form, word, game_address, *_ = line.split("\t")
        cases.append(
            Case(int(address, 16), int(word_address, 16), form, int(word, 16), int(game_address, 16))
        )
    return cases


# -- Report ------------------------------------------------------------------------------------


@dataclass
class FormResult:
    cases: int = 0
    compared: int = 0  # the verifier compared the case and found it equal
    diverged: int = 0
    not_compared: int = 0  # the verifier skipped it: the module runs the instruction through the interpreter

    def add(self, other: "FormResult") -> None:
        self.cases += other.cases
        self.compared += other.compared
        self.diverged += other.diverged
        self.not_compared += other.not_compared


FORM_TABLE_ROW = "  {form:<12} {cases:>6} {equal:>6} {diverged:>9} {not_compared:>13}"


def classify(
    cases: list[Case], verified_addresses: set[int], diverged_entries: set[int]
) -> dict[str, FormResult]:
    results: dict[str, FormResult] = defaultdict(FormResult)
    for case in cases:
        result = results[case.form]
        result.cases += 1
        if case.address in diverged_entries:
            result.diverged += 1
        elif case.word_address in verified_addresses:
            result.compared += 1
        else:
            result.not_compared += 1
    return results


def format_report(
    results: dict[str, FormResult],
    divergence_lines: list[str],
    summary: dict[str, int] | None,
) -> str:
    total = FormResult()
    for result in results.values():
        total.add(result)
    lines = [
        f"Cases: {total.cases:,} of {len(results)} forms",
        f"  compared and equal:  {total.compared:,}",
        f"  diverged:            {total.diverged:,}",
        f"  not compared:        {total.not_compared:,}",
    ]
    if summary:
        lines.append(f"Verifier summary: {' '.join(f'{name}={value}' for name, value in summary.items())}")
    lines.append("")
    if divergence_lines:
        lines.append("Divergences:")
        lines += [f"  {line}" for line in divergence_lines]
        lines.append("")
    problem_forms = sorted(name for name, result in results.items() if result.compared < result.cases)
    if problem_forms:
        lines.append("Forms with cases that were not compared and equal:")
        lines.append(
            FORM_TABLE_ROW.format(
                form="form", cases="cases", equal="equal", diverged="diverged", not_compared="not compared"
            )
        )
        for name in problem_forms:
            result = results[name]
            lines.append(
                FORM_TABLE_ROW.format(
                    form=name,
                    cases=result.cases,
                    equal=result.compared,
                    diverged=result.diverged,
                    not_compared=result.not_compared,
                )
            )
    else:
        lines.append("Every case of every form was compared and equal.")
    return "\n".join(lines) + "\n"


# -- Command line --------------------------------------------------------------------------------


def command_generate(arguments: argparse.Namespace) -> int:
    if arguments.synthetic:
        candidates = synthetic_candidates_for_seed(arguments.seed)
    else:
        candidates = collect_candidates(Dol(arguments.dol), SymbolTable.read(arguments.symbols))
    try:
        program = build_program(candidates, arguments.seed, arguments.words_per_form, arguments.group_size)
    except SectionsOverlapError as overlap:
        print(f"error: {overlap}", file=sys.stderr)
        return 1
    program.write(arguments.output)
    arguments.cases.write_text(format_cases(program.cases))
    forms = {case.form for case in program.cases}
    print(
        f"{len(program.cases):,} cases of {len(forms)} forms; "
        f"{len(program.text_words):,} instructions written to {arguments.output}"
    )
    for form, reason in program.skipped_forms.items():
        print(f"  not tested: {form} ({reason})")
    return 0


@dataclass
class Evaluation:
    """What one run of the test program showed, case by case and form by form."""

    results: dict[str, FormResult]
    divergence_lines: list[str]
    summary: dict[str, int] | None  # the verifier's own counters, if the run closed cleanly
    diverged_entries: set[int]  # where the diverging blocks start

    @property
    def report(self) -> str:
        return format_report(self.results, self.divergence_lines, self.summary)


def evaluate(cases: list[Case], log_text: str, coverage_text: str = "") -> Evaluation:
    """Match the verifier's log and coverage file against the cases that the program holds."""
    run = parse_log(log_text)
    verified_addresses, _ = read_verified_addresses([coverage_text])
    diverged_entries = {divergence.entry_address for divergence in run.divergences}
    case_at_entry = {case.address: case for case in cases}
    divergence_lines = []
    for divergence in run.divergences:
        case = case_at_entry.get(divergence.entry_address)
        where = f"{case.form}: {case.text}" if case else "not a case (setup code?)"
        divergence_lines.append(
            f"entry=0x{divergence.entry_address:08X}  {where}  [{divergence.fields_text}]"
        )
    return Evaluation(
        classify(cases, verified_addresses, diverged_entries), divergence_lines, run.summary, diverged_entries
    )


def command_report(arguments: argparse.Namespace) -> int:
    coverage_text = arguments.coverage.read_text() if arguments.coverage else ""
    evaluation = evaluate(
        parse_cases(arguments.cases.read_text()), arguments.log.read_text(errors="replace"), coverage_text
    )
    print(evaluation.report, end="")
    return 1 if evaluation.diverged_entries else 0


def existing_file(text: str) -> Path:
    """An argparse type: a path that must name a file, so a typo is reported before any work."""
    path = Path(text)
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"{path} not found")
    return path


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(dest="command", required=True)

    generate = commands.add_parser("generate", help="write the test program")
    generate.add_argument(
        "--synthetic",
        action="store_true",
        help="use made-up words of every form, not the game's; needs no game (--dol, --symbols unused)",
    )
    generate.add_argument(
        "--dol", type=existing_file, default=MAIN_DOL_PATH, help=f"the game's DOL (default: {MAIN_DOL_PATH})"
    )
    generate.add_argument(
        "--symbols", type=existing_file, default=SYMBOLS_PATH, help=f"default: {SYMBOLS_PATH}"
    )
    generate.add_argument("--output", type=Path, required=True, help="where to write the test program's DOL")
    generate.add_argument("--cases", type=Path, required=True, help="where to write the list of cases")
    generate.add_argument("--seed", type=int, default=1, help="seed of the random choices (default: 1)")
    generate.add_argument(
        "--words-per-form", type=int, default=12, help="different words tried per form (default: 12)"
    )
    generate.add_argument(
        "--group-size", type=int, default=4, help="cases that share one machine state (default: 4)"
    )
    generate.set_defaults(handler=command_generate)

    report = commands.add_parser("report", help="say what the verifier found in a run of the test program")
    report.add_argument("cases", type=existing_file, help="the list written by `generate`")
    report.add_argument("log", type=existing_file, help="the output of the run, with STATICRECOMP_LOCKSTEP=1")
    report.add_argument(
        "--coverage", type=existing_file, help="the file written by STATICRECOMP_LOCKSTEP_COVERAGE_FILE"
    )
    report.set_defaults(handler=command_report)

    arguments = parser.parse_args(argv)
    return arguments.handler(arguments)


if __name__ == "__main__":
    sys.exit(main())
