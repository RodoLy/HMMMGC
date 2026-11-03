"""Just enough PowerPC (Gekko) decoding for static analysis.

Decodificación de PowerPC (Gekko) suficiente para análisis estático.

The analysis only needs to recognise a handful of instructions: branches
(to find functions and where they end), lis/addi pairs (to learn which
addresses the code points at) and loads/stores. Decoding those by hand is
faster and more predictable than asking capstone for every word. Capstone is
still used to print readable listings.

El análisis solo necesita reconocer unas cuantas instrucciones: saltos (para
encontrar funciones y dónde terminan), pares lis/addi (para saber a qué
direcciones apunta el código) y loads/stores. Decodificarlas a mano es más
rápido y predecible que pedirle a capstone cada palabra. Capstone se sigue
usando para imprimir listados legibles.

Field names (rD, rA, rB, SIMM, UIMM, BO, LK, AA) follow the IBM PowerPC
750CL User's Manual, chapter 12, so they can be looked up directly.
"""

from dataclasses import dataclass

import capstone

# Primary opcodes live in the top 6 bits of every instruction word.
OPCODE_COMPARE_LOGICAL_IMMEDIATE = 10  # cmplwi
OPCODE_ADD_IMMEDIATE = 14  # addi, and li when rA is 0
OPCODE_ADD_IMMEDIATE_SHIFTED = 15  # addis, and lis when rA is 0
OPCODE_BRANCH_CONDITIONAL = 16  # bc and its aliases (beq, bne, bdnz, ...)
OPCODE_BRANCH = 18  # b, bl
OPCODE_BRANCH_TO_REGISTER = 19  # bclr (blr), bcctr (bctr), rfi
OPCODE_OR_IMMEDIATE = 24  # ori
OPCODE_EXTENDED = 31  # most register-register instructions

# Logical and rotate instructions write their result to the rA field, not rD.
OPCODES_WRITING_RA = {
    20,  # rlwimi
    21,  # rlwinm (also slwi, srwi, clrlwi)
    23,  # rlwnm
    24,  # ori
    25,  # oris
    26,  # xori
    27,  # xoris
    28,  # andi.
    29,  # andis.
}

# Loads/stores with a 16-bit signed displacement ("D-form").
INTEGER_LOAD_OPCODES = {32, 33, 34, 35, 40, 41, 42, 43, 46}  # lwz lwzu lbz lbzu lhz lhzu lha lhau lmw
INTEGER_STORE_OPCODES = {36, 37, 38, 39, 44, 45, 47}  # stw stwu stb stbu sth sthu stmw
FLOAT_LOAD_STORE_OPCODES = {48, 49, 50, 51, 52, 53, 54, 55}  # lfs lfsu lfd lfdu stfs stfsu stfd stfdu
PAIRED_SINGLE_LOAD_STORE_OPCODES = {56, 57, 60, 61}  # psq_l psq_lu psq_st psq_stu (Gekko only)
UPDATE_FORM_OPCODES = {33, 35, 37, 39, 41, 43, 45, 49, 51, 53, 55, 57, 61}  # the "u" variants write rA back
MEMORY_OPCODES = (
    INTEGER_LOAD_OPCODES | INTEGER_STORE_OPCODES | FLOAT_LOAD_STORE_OPCODES | PAIRED_SINGLE_LOAD_STORE_OPCODES
)

# Extended opcodes (bits 21-30) under OPCODE_BRANCH_TO_REGISTER.
EXTENDED_BRANCH_TO_LINK_REGISTER = 16  # bclr
EXTENDED_BRANCH_TO_COUNT_REGISTER = 528  # bcctr
EXTENDED_RETURN_FROM_INTERRUPT = 50  # rfi

# Extended opcodes under OPCODE_EXTENDED.
EXTENDED_LOAD_WORD_INDEXED = 23  # lwzx
EXTENDED_MOVE_FROM_SPECIAL_REGISTER = 339  # mfspr (mflr, mfctr)
EXTENDED_MOVE_TO_SPECIAL_REGISTER = 467  # mtspr (mtlr, mtctr)

SPECIAL_REGISTER_LINK = 8
SPECIAL_REGISTER_COUNT = 9

# BO value meaning "branch always" (ignore the condition register).
BRANCH_OPTIONS_ALWAYS = 20


def sign_extend_16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


@dataclass(frozen=True)
class Instruction:
    """One 32-bit instruction word and its decoded fields.

    Not every field means something for every instruction; callers check the
    opcode (or one of the is_* helpers) first. Field layout, using the
    manual's bit numbering where bit 0 is the most significant:

        bits 0-5   opcode
        bits 6-10  rD (destination) or rS (source) or BO (branch options)
        bits 11-15 rA or BI
        bits 16-20 rB
        bits 16-31 SIMM / UIMM (immediate)
        bits 21-30 extended opcode (opcodes 19 and 31)
        bit  30    AA (absolute address, branches)
        bit  31    LK (save return address in the link register)
    """

    address: int
    word: int

    @property
    def opcode(self) -> int:
        return self.word >> 26

    @property
    def rd(self) -> int:
        return (self.word >> 21) & 0x1F

    @property
    def ra(self) -> int:
        return (self.word >> 16) & 0x1F

    @property
    def rb(self) -> int:
        return (self.word >> 11) & 0x1F

    @property
    def unsigned_immediate(self) -> int:
        return self.word & 0xFFFF

    @property
    def signed_immediate(self) -> int:
        return sign_extend_16(self.word & 0xFFFF)

    @property
    def extended_opcode(self) -> int:
        return (self.word >> 1) & 0x3FF

    @property
    def saves_return_address(self) -> bool:
        """LK bit: bl, blrl, bctrl."""
        return bool(self.word & 1)

    @property
    def is_absolute(self) -> bool:
        """AA bit: the branch target is an absolute address."""
        return bool(self.word & 2)

    @property
    def special_register(self) -> int:
        """SPR number for mfspr/mtspr. The manual stores it with its two
        5-bit halves swapped, so undo that."""
        raw = (self.word >> 11) & 0x3FF
        return ((raw & 0x1F) << 5) | (raw >> 5)

    # -- branches ----------------------------------------------------------

    def branch_target(self) -> int | None:
        """Destination of a direct branch (b, bl, bc). None for anything else."""
        if self.opcode == OPCODE_BRANCH:
            offset = self.word & 0x03FFFFFC
            if offset & 0x02000000:
                offset -= 0x04000000
        elif self.opcode == OPCODE_BRANCH_CONDITIONAL:
            offset = sign_extend_16(self.word & 0xFFFC)
        else:
            return None
        if self.is_absolute:
            return offset & 0xFFFFFFFF
        return (self.address + offset) & 0xFFFFFFFF

    @property
    def is_call(self) -> bool:
        """bl: direct function call."""
        return self.opcode == OPCODE_BRANCH and self.saves_return_address

    @property
    def is_jump(self) -> bool:
        """b: unconditional jump inside the function, or a tail call."""
        return self.opcode == OPCODE_BRANCH and not self.saves_return_address

    @property
    def is_conditional_branch(self) -> bool:
        return self.opcode == OPCODE_BRANCH_CONDITIONAL and not self.saves_return_address

    def _branches_to_register(self, extended_opcode: int) -> bool:
        return self.opcode == OPCODE_BRANCH_TO_REGISTER and self.extended_opcode == extended_opcode

    @property
    def is_return(self) -> bool:
        """blr: unconditional return."""
        return (
            self._branches_to_register(EXTENDED_BRANCH_TO_LINK_REGISTER)
            and not self.saves_return_address
            and self.rd == BRANCH_OPTIONS_ALWAYS
        )

    @property
    def is_jump_to_count_register(self) -> bool:
        """bctr: a switch jump table, or a tail call through a pointer."""
        return (
            self._branches_to_register(EXTENDED_BRANCH_TO_COUNT_REGISTER)
            and not self.saves_return_address
            and self.rd == BRANCH_OPTIONS_ALWAYS
        )

    @property
    def is_indirect_call(self) -> bool:
        """blrl / bctrl: call through a function pointer."""
        return self.saves_return_address and (
            self._branches_to_register(EXTENDED_BRANCH_TO_LINK_REGISTER)
            or self._branches_to_register(EXTENDED_BRANCH_TO_COUNT_REGISTER)
        )

    @property
    def is_return_from_interrupt(self) -> bool:
        return self._branches_to_register(EXTENDED_RETURN_FROM_INTERRUPT)

    @property
    def ends_straight_line_code(self) -> bool:
        """Execution never falls through to the next instruction."""
        return (
            self.is_return or self.is_jump or self.is_jump_to_count_register or self.is_return_from_interrupt
        )

    # -- common idioms -----------------------------------------------------

    @property
    def is_load_immediate(self) -> bool:
        """li rD, value: rD = value (sign-extended)."""
        return self.opcode == OPCODE_ADD_IMMEDIATE and self.ra == 0

    @property
    def is_load_immediate_shifted(self) -> bool:
        """lis rD, value: rD = value << 16."""
        return self.opcode == OPCODE_ADD_IMMEDIATE_SHIFTED and self.ra == 0

    @property
    def is_add_immediate_to_register(self) -> bool:
        """addi rD, rA, value with a real base register (not li)."""
        return self.opcode == OPCODE_ADD_IMMEDIATE and self.ra != 0

    @property
    def is_or_immediate(self) -> bool:
        return self.opcode == OPCODE_OR_IMMEDIATE

    @property
    def is_memory_access(self) -> bool:
        return self.opcode in MEMORY_OPCODES

    @property
    def writes_rd_from_memory(self) -> bool:
        """Integer loads put their result in rD. Float and paired-single
        loads write floating-point registers, which we do not track."""
        return self.opcode in INTEGER_LOAD_OPCODES

    @property
    def writes_back_base_register(self) -> bool:
        return self.opcode in UPDATE_FORM_OPCODES

    @property
    def writes_ra(self) -> bool:
        if self.opcode in OPCODES_WRITING_RA:
            return True
        # Many opcode-31 logical instructions (or, and, xor, slw, ...) also
        # write rA. Treating every one of them that way only loses
        # information, it never invents any.
        return self.opcode == OPCODE_EXTENDED

    @property
    def is_load_word_indexed(self) -> bool:
        """lwzx rD, rA, rB: how switch statements read their jump table."""
        return self.opcode == OPCODE_EXTENDED and self.extended_opcode == EXTENDED_LOAD_WORD_INDEXED

    @property
    def is_move_to_count_register(self) -> bool:
        """mtctr rS (rS is in the rD field)."""
        return (
            self.opcode == OPCODE_EXTENDED
            and self.extended_opcode == EXTENDED_MOVE_TO_SPECIAL_REGISTER
            and self.special_register == SPECIAL_REGISTER_COUNT
        )

    @property
    def is_compare_logical_immediate(self) -> bool:
        """cmplwi rA, value: the bounds check in front of a switch."""
        return self.opcode == OPCODE_COMPARE_LOGICAL_IMMEDIATE


_capstone = capstone.Cs(
    capstone.CS_ARCH_PPC,
    capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN | capstone.CS_MODE_PS,
)


def format_instruction(word: int, address: int) -> str:
    """Readable text for one instruction, or `.word` if capstone cannot
    decode it (data embedded in code, or an unused encoding)."""
    for decoded in _capstone.disasm(word.to_bytes(4, "big"), address, 1):
        return f"{decoded.mnemonic:<9}{decoded.op_str}".rstrip()
    return f".word    0x{word:08X}"
