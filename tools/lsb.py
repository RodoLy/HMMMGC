#!/usr/bin/env python3
"""Read and disassemble the .lsb event scripts of Harvest Moon: Magical Melody.

Lee y desensambla los scripts de eventos .lsb de Harvest Moon: Magical Melody.

A .lsb file is the bytecode of a small stack machine, the game's "lsrun"
interpreter (lsrun.cpp, around 0x80051784 in main.dol). The layout below was
worked out from that interpreter and checked against all 291 scripts on the
disc (`--check` repeats the check on your copy); docs/SCRIPTS.md has the
evidence for every claim.

Un archivo .lsb es el bytecode de una pequeña máquina de pila, el intérprete
"lsrun" del juego (lsrun.cpp, cerca de 0x80051784 en main.dol). La estructura
de abajo se dedujo de ese intérprete y se comprobó con los 291 scripts del
disco (`--check` repite la comprobación con tu copia); docs/SCRIPTS.md trae
la evidencia de cada afirmación.

File layout (every integer is big-endian) / Estructura (todo en big-endian):

    0x00  u32  init entry       code offset, or 0xFFFFFFFF when absent
    0x04  u32  main entry       where an event starts running
    0x08  u32  argument entry   entry that receives one integer argument
    0x0C  u32  string table     file offset; equals the file size when empty
    0x10  u32  global count     number of 8-byte global-variable slots
    0x14  global slots, then the code up to the string table

Entries and the operands of jumps and calls are measured from the start of
the code (entries) or from the instruction itself (jumps and calls), never
from the start of the file; the listing uses code offsets throughout.

Las entradas y los operandos de saltos y llamadas se miden desde el inicio
del código (entradas) o desde la propia instrucción (saltos y llamadas),
nunca desde el inicio del archivo; el listado usa offsets de código.

Usage / Uso:
    python3 tools/lsb.py Eve_rm01                    # by name, from orig/G4AEE9/files/script
    python3 tools/lsb.py path/to/Eve_rm01.lsb        # by path
    python3 tools/lsb.py Eve_rm01 --start 0xC9F      # from a code offset (the main entry here)
    python3 tools/lsb.py Eve_rm01 --file-offsets     # print file offsets, handy for patches
    python3 tools/lsb.py --check                     # validate every script of the disc
    python3 tools/lsb.py --builtins                  # native function behind each built-in index
"""

import argparse
import struct
import sys
from collections import Counter
from dataclasses import dataclass
from enum import IntEnum
from functools import cached_property
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from dol import Dol  # noqa: E402
from project import MAIN_DOL_PATH, SCRIPT_DIRECTORY, SYMBOLS_PATH  # noqa: E402
from symbols import SymbolTable  # noqa: E402

SCRIPT_EXTENSION = ".lsb"

# Header: five big-endian u32 values, then one slot per global variable.
HEADER_FORMAT = ">5I"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
# A global slot has the layout of an interpreter stack slot: s16 type tag,
# s16 flags, u32 value (lsrun.cpp reads and writes them 8 bytes at a time).
GLOBAL_SLOT_SIZE = 8
# "No such entry point": every script on the disc leaves the init entry unset.
NO_ENTRY = 0xFFFFFFFF

# Operands: whole numbers are signed variable-length integers, big-endian
# groups of 7 bits where the top bit of every byte but the last is set. The
# interpreter repeats this decoding loop in every handler (see fn_800522EC);
# the result is sign-extended from 7 bits per byte read.
VARINT_CONTINUATION_BIT = 0x80
VARINT_PAYLOAD_MASK = 0x7F
VARINT_PAYLOAD_BITS = 7
# PUSH_FLOAT carries four raw bytes: an IEEE 754 single, big-endian.
FLOAT_OPERAND_SIZE = 4
FLOAT_OPERAND_FORMAT = ">f"
# Most digits a single-precision float can need so that it reads back unchanged.
FLOAT_DIGITS_TO_ROUND_TRIP = 9
# Whole floats below this are listed as plain integers ("10", not "1e+01").
FLOAT_PLAIN_INTEGER_LIMIT = 1_000_000_000

# How the interpreter reaches the native built-ins (fn_800522EC, the handlers
# of CALL_BUILTIN and CALL_BUILTIN_VALUE): the operand is checked against a
# count kept at 0x80322E54 (230 on this disc) and then picks a function
# pointer from a table at 0x80219EAC. An empty (null) slot makes the call a
# no-op.
BUILTIN_TABLE_ADDRESS = 0x80219EAC
BUILTIN_COUNT_ADDRESS = 0x80322E54
POINTER_SIZE = 4

# Width of the raw-bytes column: the longest instruction on the disc is five
# bytes (PUSH_FLOAT: the opcode and four bytes), written as hex pairs.
BYTES_COLUMN_WIDTH = 3 * 5 - 1


class Opcode(IntEnum):
    """Every opcode the interpreter's jump table (0x80219D0C, 0x34 entries) accepts.

    Operand notation: `n` is a variable-length integer. Names come from what
    each handler does (see docs/SCRIPTS.md); the four opcodes that no script
    on the disc uses keep a neutral name because their handlers were not
    analysed in depth. Local variable numbers count from the first slot after
    the call-frame record: 0, 1, ... are the function's locals, -1 is the
    frame record itself and -2, -3, ... are the arguments the caller pushed.
    """

    NOP = 0x00
    ENTER = 0x01  # n: reserve n empty slots on the stack for local variables 0 to n-1
    PUSH_INT = 0x02  # n: push an integer
    PUSH_FLOAT = 0x03  # 4 raw bytes: push a float
    PUSH_STRING = 0x04  # n: push a pointer to the string at offset n of the string table
    LOCAL_REF = 0x05  # n: push a reference to local variable n
    LOCAL_GET = 0x06  # n: push the value of local variable n
    LOCAL_SET = 0x07  # n: pop a value into local variable n
    GLOBAL_REF = 0x08  # n: push a reference to global variable n
    GLOBAL_GET = 0x09  # n: push the value of global variable n
    GLOBAL_SET = 0x0A  # n: pop a value into global variable n
    CALL_BUILTIN_VALUE = 0x0B  # n: call native built-in n, then push its result
    CALL_BUILTIN = 0x0C  # n: call native built-in n, ignore its result
    UNKNOWN_0D = 0x0D  # n: handler fn_800540CC(thread, 2); never emitted on the disc
    UNKNOWN_0E = 0x0E  # n: handler fn_800540CC(thread, 0); never emitted on the disc
    UNKNOWN_0F = 0x0F  # n: handler fn_80054AFC (pushes a type-7 slot); never emitted on the disc
    UNKNOWN_10 = 0x10  # n: handler fn_80053F5C (allocates a 0x5C-byte object); never emitted on the disc
    JUMP = 0x11  # n: continue at this instruction's address + n
    JUMP_IF_ZERO = 0x12  # n: pop an integer; continue at address + n when it is zero
    CALL_VALUE = 0x13  # n: call the code at address + n, then push its result when it returns
    CALL = 0x14  # n: call the code at address + n, drop its result
    RETURN = 0x15  # n: return, releasing n + 1 slots (n arguments and locals, and the call-frame record)
    RETURN_VOID = 0x16  # the same as RETURN 0 (fn_80053D20 calls the return code with 0)
    NEW_ARRAY = 0x17  # allocates an array and pushes it (type-4 slot); never emitted on the disc
    ARRAY_GET = 0x18  # reads an element of an array (type 4) or element reference (type 8); never emitted
    ARRAY_SET = 0x19  # writes one; never emitted on the disc
    PUSH_RESULT = 0x1A  # push the result register (what the last built-in or return left there)
    # Binary operators pop two slots and compute TOP op SECOND, so a compiler
    # that wants `a - b` pushes b first and a second.
    ADD = 0x1B
    SUBTRACT = 0x1C
    MULTIPLY = 0x1D
    DIVIDE = 0x1E
    MODULO = 0x1F
    SHIFT_LEFT = 0x20
    SHIFT_RIGHT = 0x21  # arithmetic (sign-preserving)
    BIT_AND = 0x22
    BIT_OR = 0x23
    BIT_XOR = 0x24
    EQUAL = 0x25
    NOT_EQUAL = 0x26
    LESS = 0x27
    LESS_OR_EQUAL = 0x28
    GREATER = 0x29
    GREATER_OR_EQUAL = 0x2A
    LOGICAL_AND = 0x2B  # both operands non-zero (not short-circuit: both are already evaluated)
    LOGICAL_OR = 0x2C
    NEGATE = 0x2D
    BIT_NOT = 0x2E
    LOGICAL_NOT = 0x2F
    PRINT = 0x30  # pop a value and print it through OSReport; also yields
    PRINT_LINE = 0x31  # same, followed by a newline
    NOP_32 = 0x32  # the jump table points it at the same code as NOP
    LINE = 0x33  # n: stores n in the thread (+0x28), apparently a source line number; never emitted


BINARY_OPERATORS = frozenset(Opcode(code) for code in range(Opcode.ADD, Opcode.LOGICAL_OR + 1))
UNARY_OPERATORS = frozenset({Opcode.NEGATE, Opcode.BIT_NOT, Opcode.LOGICAL_NOT})
OPCODES_WITHOUT_OPERAND = (
    frozenset(
        {
            Opcode.NOP,
            Opcode.RETURN_VOID,
            Opcode.NEW_ARRAY,
            Opcode.ARRAY_GET,
            Opcode.ARRAY_SET,
            Opcode.PUSH_RESULT,
            Opcode.PRINT,
            Opcode.PRINT_LINE,
            Opcode.NOP_32,
        }
    )
    | BINARY_OPERATORS
    | UNARY_OPERATORS
)
# Their operand is an offset from the opcode byte itself (the handlers start
# from `pc - 1`, the address of the opcode), so code can move without edits.
RELATIVE_BRANCH_OPCODES = frozenset({Opcode.JUMP, Opcode.JUMP_IF_ZERO, Opcode.CALL_VALUE, Opcode.CALL})
BUILTIN_CALL_OPCODES = frozenset({Opcode.CALL_BUILTIN_VALUE, Opcode.CALL_BUILTIN})

# Names of built-ins whose behaviour was read from their native code. Every
# other index keeps no name on purpose: docs/SCRIPTS.md lists the evidence
# behind each of these and what is known about the rest.
BUILTIN_NAMES: dict[int, str] = {
    6: "WAIT",  # fn_800631A0: stores its argument in the thread's wait timer and yields
    48: "YIELD",  # fn_80062EC8: only sets the thread's yield flag
    51: "SHOW_MESSAGE",  # fn_800629D0: hands its argument to the message window, fn_80084FCC
    55: "GET_GAME_VALUE",  # fn_80060D94: 192-way switch over save-data counters
    56: "SET_FLAG",  # fn_80060D14: sets a bit of the main flag set (save data +4)
    57: "CLEAR_FLAG",  # fn_80060C94: clears it
    58: "TEST_FLAG",  # fn_80060BE8: tests it and returns the result
    62: "WAIT_MESSAGE",  # fn_800627C0: retries itself while the message window is busy
}


class ScriptError(ValueError):
    """The bytes are not a well-formed .lsb script. / Los bytes no son un .lsb válido."""


def read_varint(data: bytes, position: int) -> tuple[int, int]:
    """Decode one signed variable-length integer; return (value, position after it)."""
    start = position
    value = 0
    length = 0
    while True:
        if position >= len(data):
            raise ScriptError(f"the integer at 0x{start:X} is cut off by the end of the data")
        byte = data[position]
        position += 1
        length += 1
        value = (value << VARINT_PAYLOAD_BITS) | (byte & VARINT_PAYLOAD_MASK)
        if not byte & VARINT_CONTINUATION_BIT:
            break
    significant_bits = VARINT_PAYLOAD_BITS * length
    if value >> (significant_bits - 1):
        value -= 1 << significant_bits
    return value, position


def round_to_single_precision(value: float) -> float:
    return struct.unpack(FLOAT_OPERAND_FORMAT, struct.pack(FLOAT_OPERAND_FORMAT, value))[0]


def format_float(value: float) -> str:
    """Shortest text that reads back as the same single-precision float."""
    for digits in range(1, FLOAT_DIGITS_TO_ROUND_TRIP + 1):
        text = f"{value:.{digits}g}"
        if round_to_single_precision(float(text)) == value:
            break
    parsed = float(text)
    if parsed.is_integer() and abs(parsed) < FLOAT_PLAIN_INTEGER_LIMIT:
        return str(int(parsed))
    return text


@dataclass(frozen=True)
class Header:
    """The five header words. Entry points are code offsets, None when absent."""

    init_entry: int | None
    main_entry: int | None
    argument_entry: int | None
    string_table_offset: int
    global_count: int

    @property
    def code_offset(self) -> int:
        """File offset where the code starts: right after the global slots."""
        return HEADER_SIZE + GLOBAL_SLOT_SIZE * self.global_count

    @property
    def entries(self) -> list[tuple[str, int]]:
        """The entry points that exist, as (name, code offset)."""
        named = (
            ("init", self.init_entry),
            ("main", self.main_entry),
            ("argument", self.argument_entry),
        )
        return [(name, offset) for name, offset in named if offset is not None]


@dataclass(frozen=True)
class Instruction:
    offset: int  # code offset of the opcode byte
    opcode: Opcode
    operand: int | float | None
    encoded: bytes  # the opcode byte and its operand, as stored

    @property
    def size(self) -> int:
        return len(self.encoded)

    @property
    def end(self) -> int:
        return self.offset + self.size

    # decode_instruction gives every opcode below an integer operand.

    @property
    def branch_target(self) -> int | None:
        """Code offset a jump or call lands on, or None for other instructions."""
        if self.opcode in RELATIVE_BRANCH_OPCODES:
            return self.offset + int(self.operand)
        return None

    @property
    def builtin_index(self) -> int | None:
        """Which native built-in a call instruction names, or None for other instructions."""
        if self.opcode in BUILTIN_CALL_OPCODES:
            return int(self.operand)
        return None


@dataclass(frozen=True)
class Script:
    header: Header
    global_slots: tuple[bytes, ...]
    instructions: tuple[Instruction, ...]
    string_table: bytes
    file_size: int

    @property
    def code_offset(self) -> int:
        return self.header.code_offset

    @property
    def code_size(self) -> int:
        return self.header.string_table_offset - self.code_offset

    def file_offset(self, code_offset: int) -> int:
        return self.code_offset + code_offset

    @cached_property
    def instruction_offsets(self) -> frozenset[int]:
        return frozenset(instruction.offset for instruction in self.instructions)

    @cached_property
    def call_targets(self) -> frozenset[int]:
        """Where CALL and CALL_VALUE land: the starts of the script's functions."""
        return frozenset(
            instruction.branch_target
            for instruction in self.instructions
            if instruction.opcode in (Opcode.CALL, Opcode.CALL_VALUE)
            and instruction.branch_target is not None
        )

    @cached_property
    def jump_targets(self) -> frozenset[int]:
        return frozenset(
            instruction.branch_target
            for instruction in self.instructions
            if instruction.opcode in (Opcode.JUMP, Opcode.JUMP_IF_ZERO)
            and instruction.branch_target is not None
        )

    def problems(self) -> list[str]:
        """Every entry point, jump and call that does not land on an instruction."""
        found = []
        for name, offset in self.header.entries:
            if offset not in self.instruction_offsets:
                found.append(f"{name} entry 0x{offset:X} is not the start of an instruction")
        for instruction in self.instructions:
            target = instruction.branch_target
            if target is not None and target not in self.instruction_offsets:
                found.append(
                    f"{instruction.opcode.name} at 0x{instruction.offset:X} lands on 0x{target:X},"
                    " which is not the start of an instruction"
                )
        return found


def decode_instruction(code: bytes, position: int) -> Instruction:
    """Decode the instruction at `position` of the code bytes."""
    opcode_byte = code[position]
    try:
        opcode = Opcode(opcode_byte)
    except ValueError:
        raise ScriptError(f"unknown opcode 0x{opcode_byte:02X} at code offset 0x{position:X}") from None

    cursor = position + 1
    operand: int | float | None
    if opcode in OPCODES_WITHOUT_OPERAND:
        operand = None
    elif opcode is Opcode.PUSH_FLOAT:
        if cursor + FLOAT_OPERAND_SIZE > len(code):
            raise ScriptError(
                f"the float operand at code offset 0x{cursor:X} is cut off by the end of the code"
            )
        operand = struct.unpack_from(FLOAT_OPERAND_FORMAT, code, cursor)[0]
        cursor += FLOAT_OPERAND_SIZE
    else:
        operand, cursor = read_varint(code, cursor)
    return Instruction(position, opcode, operand, bytes(code[position:cursor]))


def parse_script(data: bytes) -> Script:
    """Split a .lsb file into header, global slots, instructions and string table."""
    if len(data) < HEADER_SIZE:
        raise ScriptError(f"{len(data)} bytes is shorter than the {HEADER_SIZE}-byte header")
    init_entry, main_entry, argument_entry, string_table_offset, global_count = struct.unpack_from(
        HEADER_FORMAT, data
    )

    def entry_or_none(value: int) -> int | None:
        return None if value == NO_ENTRY else value

    header = Header(
        init_entry=entry_or_none(init_entry),
        main_entry=entry_or_none(main_entry),
        argument_entry=entry_or_none(argument_entry),
        string_table_offset=string_table_offset,
        global_count=global_count,
    )
    code_start = header.code_offset
    if not code_start <= string_table_offset <= len(data):
        raise ScriptError(
            f"string table offset 0x{string_table_offset:X} is outside the file"
            f" (code starts at 0x{code_start:X}, file ends at 0x{len(data):X})"
        )

    global_slots = tuple(
        data[HEADER_SIZE + GLOBAL_SLOT_SIZE * index : HEADER_SIZE + GLOBAL_SLOT_SIZE * (index + 1)]
        for index in range(global_count)
    )
    code = data[code_start:string_table_offset]
    instructions = []
    position = 0
    while position < len(code):
        instruction = decode_instruction(code, position)
        instructions.append(instruction)
        position = instruction.end
    return Script(
        header=header,
        global_slots=global_slots,
        instructions=tuple(instructions),
        string_table=data[string_table_offset:],
        file_size=len(data),
    )


def load_script(path: Path) -> Script:
    return parse_script(path.read_bytes())


def resolve_script_path(argument: str, directory: Path) -> Path:
    """A path to a file, or a bare script name such as Eve_rm01 looked up in `directory`."""
    path = Path(argument)
    if path.is_file():
        return path
    named = directory / (argument if argument.endswith(SCRIPT_EXTENSION) else argument + SCRIPT_EXTENSION)
    if named.is_file():
        return named
    raise FileNotFoundError(f"no script {argument!r}: not a file, and {named} does not exist")


def format_operand(instruction: Instruction) -> str:
    opcode = instruction.opcode
    operand = instruction.operand
    if operand is None:
        return ""
    if opcode is Opcode.PUSH_FLOAT:
        assert isinstance(operand, float)
        return format_float(operand)
    if opcode in (Opcode.CALL, Opcode.CALL_VALUE):
        return f"sub_{instruction.branch_target:04X}"
    if opcode in (Opcode.JUMP, Opcode.JUMP_IF_ZERO):
        return f".L{instruction.branch_target:04X}"
    return str(operand)


def format_instruction(instruction: Instruction) -> str:
    """Mnemonic, operand and, for built-ins with a known name, a trailing comment."""
    text = f"{instruction.opcode.name} {format_operand(instruction)}".rstrip()
    index = instruction.builtin_index
    builtin_name = BUILTIN_NAMES.get(index) if index is not None else None
    return f"{text}  ; {builtin_name}" if builtin_name else text


def disassemble(
    script: Script,
    start: int | None = None,
    end: int | None = None,
    use_file_offsets: bool = False,
) -> list[str]:
    """Listing lines for the instructions between two code offsets (default: all)."""
    entry_labels: dict[int, list[str]] = {}
    for name, offset in script.header.entries:
        entry_labels.setdefault(offset, []).append(f"{name}:")

    lines = []
    for instruction in script.instructions:
        if start is not None and instruction.offset < start:
            continue
        if end is not None and instruction.offset >= end:
            break
        labels = list(entry_labels.get(instruction.offset, []))
        if instruction.offset in script.call_targets:
            labels.append(f"sub_{instruction.offset:04X}:")
        if labels:
            lines.append("")  # a blank line sets a function apart from the code above it
            lines.extend(labels)
        if instruction.offset in script.jump_targets:
            lines.append(f".L{instruction.offset:04X}:")
        shown_offset = script.file_offset(instruction.offset) if use_file_offsets else instruction.offset
        encoded = " ".join(f"{byte:02X}" for byte in instruction.encoded)
        lines.append(
            f"  {shown_offset:05X}  {encoded:<{BYTES_COLUMN_WIDTH}}  {format_instruction(instruction)}"
        )
    return lines


def describe_header(path: Path, script: Script) -> list[str]:
    header = script.header
    entries = ", ".join(f"{name} 0x{offset:X}" for name, offset in header.entries) or "none"
    string_table = f"{len(script.string_table)} bytes" if script.string_table else "empty"
    return [
        f"; {path.name}: {script.file_size} bytes, {header.global_count} global slot(s),"
        f" code at file offsets 0x{script.code_offset:X}-0x{header.string_table_offset:X}"
        f" ({len(script.instructions)} instructions), string table {string_table}",
        f"; entry points (code offsets): {entries}",
    ]


def check_directory(directory: Path) -> int:
    """Parse every script in a directory and report the ones that do not hold up."""
    paths = sorted(directory.glob(f"*{SCRIPT_EXTENSION}"))
    if not paths:
        print(f"no {SCRIPT_EXTENSION} files in {directory}", file=sys.stderr)
        return 1
    failures = 0
    for path in paths:
        try:
            problems = load_script(path).problems()
        except ScriptError as error:
            problems = [str(error)]
        for problem in problems:
            failures += 1
            print(f"{path.name}: {problem}")
    print(f"{len(paths)} scripts checked, {failures} problem(s)")
    return 1 if failures else 0


def count_builtin_usage(directory: Path) -> dict[int, tuple[int, int]]:
    """For each built-in index the scripts call: (call sites, scripts that call it)."""
    call_sites: Counter[int] = Counter()
    scripts_calling: Counter[int] = Counter()
    for path in sorted(directory.glob(f"*{SCRIPT_EXTENSION}")):
        indices = [
            instruction.builtin_index
            for instruction in load_script(path).instructions
            if instruction.builtin_index is not None
        ]
        call_sites.update(indices)
        scripts_calling.update(set(indices))
    return {index: (call_sites[index], scripts_calling[index]) for index in call_sites}


def describe_builtins(dol: Dol, symbols: SymbolTable, script_directory: Path) -> list[str]:
    """One line per filled slot of the native built-in table, with how much the scripts use it."""
    count = dol.u32(BUILTIN_COUNT_ADDRESS)
    usage = count_builtin_usage(script_directory) if script_directory.is_dir() else {}
    lines = [f"; {count} slots at 0x{BUILTIN_TABLE_ADDRESS:08X}; empty slots are left out"]
    for index in range(count):
        function_address = dol.u32(BUILTIN_TABLE_ADDRESS + POINTER_SIZE * index)
        if function_address == 0:
            continue
        symbol = symbols.by_address.get(function_address)
        function_name = symbol.name if symbol else "?"
        call_sites, scripts = usage.get(index, (0, 0))
        lines.append(
            f"{index:4}  0x{function_address:08X}  {function_name:<14}  {BUILTIN_NAMES.get(index, ''):<15}"
            f"  {call_sites:6} calls in {scripts:3} scripts"
        )
    return lines


def parse_offset(text: str) -> int:
    return int(text, 0)


def main() -> int:
    parser = argparse.ArgumentParser(description="Disassemble a Harvest Moon: Magical Melody .lsb script.")
    parser.add_argument("script", nargs="?", help="script name (Eve_rm01) or path to a .lsb file")
    parser.add_argument("--start", type=parse_offset, help="first code offset to list (decimal or 0x hex)")
    parser.add_argument("--end", type=parse_offset, help="code offset to stop before")
    parser.add_argument(
        "--file-offsets", action="store_true", help="show file offsets instead of code offsets"
    )
    parser.add_argument(
        "--check", action="store_true", help="validate every script in the directory instead of listing one"
    )
    parser.add_argument(
        "--builtins",
        action="store_true",
        help="list the native function behind every built-in index, and how much the scripts call it",
    )
    parser.add_argument(
        "--directory", type=Path, default=SCRIPT_DIRECTORY, help=f"default: {SCRIPT_DIRECTORY}"
    )
    parser.add_argument("--dol", type=Path, default=MAIN_DOL_PATH, help=f"default: {MAIN_DOL_PATH}")
    parser.add_argument("--symbols", type=Path, default=SYMBOLS_PATH, help=f"default: {SYMBOLS_PATH}")
    arguments = parser.parse_args()

    if arguments.check:
        return check_directory(arguments.directory)
    if arguments.builtins:
        if not arguments.dol.is_file():
            parser.error(f"{arguments.dol} not found; extract your disc first (tools/extract_disc.py)")
        symbols = SymbolTable.read(arguments.symbols)
        print("\n".join(describe_builtins(Dol(arguments.dol), symbols, arguments.directory)))
        return 0
    if arguments.script is None:
        parser.error("give a script name or path, or use --check or --builtins")
    try:
        path = resolve_script_path(arguments.script, arguments.directory)
        script = load_script(path)
    except (FileNotFoundError, ScriptError) as error:
        parser.error(str(error))

    print("\n".join(describe_header(path, script)))
    print("\n".join(disassemble(script, arguments.start, arguments.end, arguments.file_offsets)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
