"""Build tiny .lsb scripts by hand so the parser can be tested without game data.

Arma scripts .lsb mínimos a mano para probar el lector sin datos del juego.

The encoders follow the format described in docs/SCRIPTS.md and are written
apart from tools/lsb.py on purpose: test_lsb.py also feeds the parser byte
strings copied from real scripts, so a mistake that the encoder and the
decoder share cannot hide.
"""

import struct

HEADER_SIZE = 0x14
GLOBAL_SLOT_SIZE = 8
NO_ENTRY = 0xFFFFFFFF
# An int slot holding 0, which is how every global of the real scripts starts
# out: s16 tag 1 (int), s16 flags 0, u32 value 0.
ZERO_INT_SLOT = struct.pack(">hhI", 1, 0, 0)

VARINT_PAYLOAD_BITS = 7
VARINT_PAYLOAD_MASK = 0x7F
VARINT_CONTINUATION_BIT = 0x80

# Opcode numbers, as the interpreter's jump table at 0x80219D0C orders them.
OP_NOP = 0x00
OP_ENTER = 0x01
OP_PUSH_INT = 0x02
OP_PUSH_FLOAT = 0x03
OP_LOCAL_GET = 0x06
OP_CALL_BUILTIN_VALUE = 0x0B
OP_CALL_BUILTIN = 0x0C
OP_JUMP = 0x11
OP_JUMP_IF_ZERO = 0x12
OP_CALL = 0x14
OP_RETURN = 0x15
OP_RETURN_VOID = 0x16
OP_ADD = 0x1B
OP_SUBTRACT = 0x1C
OP_LINE = 0x33
FIRST_UNASSIGNED_OPCODE = 0x34


def encode_varint(value: int) -> bytes:
    """Fewest bytes whose 7-bit groups hold `value` as a signed number.

    The top bit of every byte except the last says "more follows"; the
    remaining 7n bits are one two's-complement number."""
    length = 1
    while not -(1 << (VARINT_PAYLOAD_BITS * length - 1)) <= value < (1 << (VARINT_PAYLOAD_BITS * length - 1)):
        length += 1
    field = value & ((1 << (VARINT_PAYLOAD_BITS * length)) - 1)
    groups = [
        (field >> (VARINT_PAYLOAD_BITS * position)) & VARINT_PAYLOAD_MASK
        for position in reversed(range(length))
    ]
    return bytes([group | VARINT_CONTINUATION_BIT for group in groups[:-1]] + [groups[-1]])


def instruction(opcode: int, operand: int | None = None) -> bytes:
    """An opcode followed by its integer operand, when it has one."""
    return bytes([opcode]) + (b"" if operand is None else encode_varint(operand))


def push_float(value: float) -> bytes:
    """PUSH_FLOAT carries the raw big-endian bytes of a single-precision float."""
    return bytes([OP_PUSH_FLOAT]) + struct.pack(">f", value)


def build_script(
    code: bytes,
    main: int | None = 0,
    init: int | None = None,
    argument: int | None = None,
    global_count: int = 0,
    string_table: bytes = b"",
) -> bytes:
    """A whole .lsb file: header, global slots, code, then the string table."""
    code_start = HEADER_SIZE + GLOBAL_SLOT_SIZE * global_count
    string_table_offset = code_start + len(code)
    header = struct.pack(
        ">5I",
        NO_ENTRY if init is None else init,
        NO_ENTRY if main is None else main,
        NO_ENTRY if argument is None else argument,
        string_table_offset,
        global_count,
    )
    return header + ZERO_INT_SLOT * global_count + code + string_table
