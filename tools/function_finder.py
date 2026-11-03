"""Find where every function of a CodeWarrior DOL starts and ends.

Encuentra dónde empieza y termina cada función de un DOL de CodeWarrior.

A static recompiler translates code one function at a time, so it needs the
function boundaries before it can do anything. This module finds them the
same way a person would with a disassembler open:

1. Seeds, addresses known to be function starts:
   - extabindex: CodeWarrior's C++ exception index lists the exact start and
     size of every function that has an exception table. Those are ground
     truth, and they are also used to check the rest of the analysis.
   - targets of `bl` (direct calls),
   - function pointers: code addresses stored in data (vtables, callback
     tables) or built in a register with lis/addi (callbacks passed as
     arguments),
   - the entry point, .ctors and .dtors.
2. Jump tables (C `switch`), found by the lis/addi + lwzx + mtctr + bctr
   pattern. Their entries point *inside* a function, so they must never be
   taken as function starts.
3. Extent: walk forward from each start, remembering the furthest branch
   target inside the function. The function ends at the first blr/b/bctr/rfi
   that lies past every target seen so far.
4. Gaps: code that no function covers starts a new function (functions only
   reached through pointers the analysis could not see). Repeat until stable.

1. Semillas, direcciones que sabemos que inician una función: extabindex
   (tamaños exactos de CodeWarrior), destinos de `bl`, punteros a funciones
   en datos o construidos con lis/addi, entry point, .ctors y .dtors.
2. Tablas de saltos (`switch`), cuyas entradas apuntan *dentro* de una
   función y nunca se toman como inicio de función.
3. Extensión: se avanza desde cada inicio recordando el salto local más
   lejano; la función termina en el primer blr/b/bctr/rfi que está más allá
   de todos ellos.
4. Huecos: código que ninguna función cubre inicia una función nueva. Se
   repite hasta que no cambie nada.

The command-line entry point is tools/analyze.py.
"""

import bisect
import itertools
from collections import defaultdict

from dol import Dol
from ppc import OPCODE_OR_IMMEDIATE, Instruction

# DOL sections as CodeWarrior's makedol writes them. The mapping to real
# section names lives in symbols.py; these are the ones the analysis reads.
EXCEPTION_TABLE_SECTION = "data0"  # extab
EXCEPTION_INDEX_SECTION = "data1"  # extabindex
CONSTRUCTOR_LIST_SECTION = "data2"  # .ctors
DESTRUCTOR_LIST_SECTION = "data3"  # .dtors
EXCEPTION_INDEX_ENTRY_SIZE = 12  # function address, function size, extab entry

# Small-data base registers. The EABI reserves r13 for .sdata/.sbss and r2
# for .sdata2/.sbss2; __init_registers loads them with lis/ori, and code then
# reaches small globals as e.g. `lwz r3, -0x7F00(r13)`.
SMALL_DATA_BASE_REGISTER = 13
SMALL_DATA_2_BASE_REGISTER = 2

# Registers a called function is allowed to overwrite (PowerPC EABI).
CALLER_SAVED_REGISTERS = (0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)

# How far back from a bctr to look for the `cmplwi` that bounds a switch.
SWITCH_BOUNDS_CHECK_LOOKBACK = 16

# Encoding of `nop` (ori r0, r0, 0). Hand-written assembly pads with it.
NOP = 0x60000000
# Shortest run of printable ASCII inside code that is treated as a string.
MIN_STRING_IN_CODE_LENGTH = 8


def find_small_data_bases(dol: Dol) -> dict[int, int]:
    """Values __init_registers puts in r2 and r13, as {register: value}.

    It is the only code in .init that builds those two registers with a
    `lis rN, high` / `ori rN, rN, low` pair, so looking for the pair is enough."""
    init = dol.text_sections()[0]
    bases = {}
    for address in range(init.address, init.end - 4, 4):
        high = Instruction(address, dol.u32(address))
        low = Instruction(address + 4, dol.u32(address + 4))
        for register in (SMALL_DATA_BASE_REGISTER, SMALL_DATA_2_BASE_REGISTER):
            if (
                high.is_load_immediate_shifted
                and high.rd == register
                and low.opcode == OPCODE_OR_IMMEDIATE
                and low.rd == register
                and low.ra == register
            ):
                bases[register] = (high.unsigned_immediate << 16) | low.unsigned_immediate
    return bases


class FunctionFinder:
    """Runs every analysis pass over one DOL. Call run(), then read
    function_sizes, jump_tables, data_in_code and references."""

    def __init__(self, dol: Dol):
        self.dol = dol
        self.sections = {section.name: section for section in dol.sections}
        self.small_data_bases = find_small_data_bases(dol)

        self.instructions: dict[int, Instruction] = {}
        for section in dol.text_sections():
            code = dol.data[section.file_offset : section.file_offset + section.size]
            for offset in range(0, len(code), 4):
                address = section.address + offset
                word = int.from_bytes(code[offset : offset + 4], "big")
                self.instructions[address] = Instruction(address, word)

        self.exception_index_sizes: dict[int, int] = {}  # function start -> size
        self.call_targets: set[int] = set()
        self.function_pointer_candidates: set[int] = set()
        self.jump_table_for_bctr: dict[int, int] = {}  # bctr address -> table address
        self.jump_tables: dict[int, list[int]] = {}  # table address -> case targets
        # target address -> addresses (instructions or data words) that refer to it
        self.references: dict[int, set[int]] = defaultdict(set)
        self.function_sizes: dict[int, int] = {}  # function start -> size
        # Data that lives inside a text section: start -> (size, name).
        self.data_in_code: dict[int, tuple[int, str]] = {}
        self.data_in_code_words: set[int] = set()

    # -- data inside code ----------------------------------------------------

    def find_startup_tables(self) -> None:
        """CodeWarrior's linker appends two tables to the end of .init:

        - _rom_copy_info: (source, destination, size) for every section the
          startup code copies, ending with an all-zero entry.
        - _bss_init_info: (address, size) for every section it clears,
          ending with an all-zero entry.

        __init_data walks both. The first copy entry is always .init itself,
        so the table is found by looking for (init, init, ...)."""
        init = self.sections["text0"]
        for address in range(init.address, init.end - 8, 4):
            if self.dol.u32(address) == init.address and self.dol.u32(address + 4) == init.address:
                copy_table = address
                break
        else:
            return

        entry = copy_table
        while any(self.dol.u32(entry + 4 * word) for word in range(3)):
            entry += 12
        copy_table_end = entry + 12  # include the terminator
        self._mark_data_in_code(copy_table, copy_table_end - copy_table, "_rom_copy_info")

        entry = copy_table_end
        while entry < init.end and (self.dol.u32(entry) or self.dol.u32(entry + 4)):
            entry += 8
        self._mark_data_in_code(copy_table_end, entry + 8 - copy_table_end, "_bss_init_info")

    def rom_copy_entries(self) -> list[tuple[int, int, int]]:
        """(source, destination, size) triples of _rom_copy_info."""
        for start, (size, name) in self.data_in_code.items():
            if name == "_rom_copy_info":
                return [
                    (self.dol.u32(entry), self.dol.u32(entry + 4), self.dol.u32(entry + 8))
                    for entry in range(start, start + size - 12, 12)
                ]
        return []

    def bss_init_entries(self) -> list[tuple[int, int]]:
        """(address, size) pairs of _bss_init_info."""
        for start, (size, name) in self.data_in_code.items():
            if name == "_bss_init_info":
                return [
                    (self.dol.u32(entry), self.dol.u32(entry + 4))
                    for entry in range(start, start + size - 8, 8)
                ]
        return []

    def _string_in_code_length(self, address: int) -> int:
        """Length in bytes (NUL included) of a printable ASCII string starting
        at address, or 0 if there is none."""
        raw = self.dol.read(address, 256)
        end = raw.find(b"\0")
        if end < MIN_STRING_IN_CODE_LENGTH:
            return 0
        text = raw[:end]
        if all(32 <= byte < 127 for byte in text):
            return end + 1
        return 0

    def _mark_data_in_code(self, start: int, size: int, name: str) -> None:
        size = (size + 3) & ~3
        self.data_in_code[start] = (size, name)
        self.data_in_code_words.update(range(start, start + size, 4))

    def _is_filler(self, address: int) -> bool:
        """Words between functions that are not code: zero or nop padding,
        and data that the linker placed inside a text section."""
        return self.instructions[address].word in (0, NOP) or address in self.data_in_code_words

    def _claim_if_string(self, address: int) -> bool:
        """Mark a string sitting in code as data. True if it was one."""
        length = self._string_in_code_length(address)
        if length:
            self._mark_data_in_code(address, length, f"string_{address:08X}")
        return bool(length)

    # -- step 1: seeds -------------------------------------------------------

    def read_exception_index(self) -> None:
        index = self.sections[EXCEPTION_INDEX_SECTION]
        exception_tables = self.sections[EXCEPTION_TABLE_SECTION]
        for entry in range(index.address, index.end, EXCEPTION_INDEX_ENTRY_SIZE):
            function_start = self.dol.u32(entry)
            function_size = self.dol.u32(entry + 4)
            exception_table = self.dol.u32(entry + 8)
            if (
                self.dol.is_code(function_start)
                and function_size
                and exception_tables.contains(exception_table)
            ):
                self.exception_index_sizes[function_start] = function_size

    def collect_call_targets(self) -> None:
        for instruction in self.instructions.values():
            if instruction.is_call:
                target = instruction.branch_target()
                if target in self.instructions:
                    self.call_targets.add(target)

    def _base_register_value(self, register: int, register_values: dict[int, int]) -> int | None:
        if register in self.small_data_bases:
            return self.small_data_bases[register]
        return register_values.get(register)

    def _record_reference(self, from_address: int, target: int) -> None:
        if not self.dol.is_mapped(target):
            return
        self.references[target].add(from_address)
        if self.dol.is_code(target) and target % 4 == 0:
            self.function_pointer_candidates.add(target)

    def follow_register_constants(self, function_starts: set[int]) -> None:
        """Learn which addresses the code builds or reads, and spot switches.

        Two kinds of register knowledge are kept:

        - `register_values`: trusted only inside straight-line code. Forgotten
          after every blr/b/bctr/rfi, and caller-saved registers are forgotten
          after every call. Used for references, so it never guesses.
        - `function_wide_values`: registers set with lis/addi, kept across
          jumps until the next function start (caller-saved ones are still
          dropped at calls). Switches often load their table address before
          a loop and jump back to it, so straight-line knowledge is not
          enough. Only used to find a switch's jump table, and every table
          found that way is checked later (all its cases must land inside
          the function, and their count must match the bounds check).

        Anything not modelled here forgets the registers it might write, so a
        stale value is never reused."""
        register_values: dict[int, int] = {}
        function_wide_values: dict[int, int] = {}
        jump_table_in_register: dict[int, int] = {}
        jump_table_in_count_register: int | None = None

        def set_register(register: int, value: int) -> None:
            register_values[register] = value
            function_wide_values[register] = value

        def forget_register(register: int) -> None:
            register_values.pop(register, None)
            function_wide_values.pop(register, None)

        for address in sorted(self.instructions):
            if address in function_starts:
                register_values.clear()
                function_wide_values.clear()
                jump_table_in_register.clear()
                jump_table_in_count_register = None

            instruction = self.instructions[address]
            base_value = self._base_register_value(instruction.ra, register_values)

            if instruction.is_load_immediate_shifted:
                set_register(instruction.rd, (instruction.unsigned_immediate << 16) & 0xFFFFFFFF)

            elif instruction.is_add_immediate_to_register:
                if base_value is None:
                    forget_register(instruction.rd)
                else:
                    value = (base_value + instruction.signed_immediate) & 0xFFFFFFFF
                    self._record_reference(address, value)
                    set_register(instruction.rd, value)

            elif instruction.is_or_immediate:
                # ori rA, rS, value: source in the rD field, result in rA.
                source_value = register_values.get(instruction.rd)
                if source_value is None:
                    forget_register(instruction.ra)
                else:
                    value = source_value | instruction.unsigned_immediate
                    self._record_reference(address, value)
                    set_register(instruction.ra, value)

            elif instruction.is_memory_access:
                if base_value is not None:
                    self.references[(base_value + instruction.signed_immediate) & 0xFFFFFFFF].add(address)
                if instruction.writes_rd_from_memory:
                    forget_register(instruction.rd)
                if instruction.writes_back_base_register:
                    forget_register(instruction.ra)

            elif instruction.is_load_word_indexed:
                for index_register in (instruction.ra, instruction.rb):
                    table = register_values.get(index_register, function_wide_values.get(index_register))
                    if table is not None and self.dol.section_at(table) and not self.dol.is_code(table):
                        jump_table_in_register[instruction.rd] = table
                forget_register(instruction.rd)

            elif instruction.is_move_to_count_register:
                jump_table_in_count_register = jump_table_in_register.get(instruction.rd)

            elif instruction.is_jump_to_count_register:
                if jump_table_in_count_register is not None:
                    self.jump_table_for_bctr[address] = jump_table_in_count_register

            else:
                forget_register(instruction.rd)
                if instruction.writes_ra:
                    forget_register(instruction.ra)

            if instruction.is_call or instruction.is_indirect_call:
                for register in CALLER_SAVED_REGISTERS:
                    register_values.pop(register, None)
                    function_wide_values.pop(register, None)
                jump_table_in_register.clear()
            if instruction.ends_straight_line_code:
                register_values.clear()
                jump_table_in_register.clear()
                jump_table_in_count_register = None

    def _switch_case_count(self, bctr_address: int) -> int | None:
        """The `cmplwi rN, last_case` in front of a switch gives its size."""
        for distance in range(1, SWITCH_BOUNDS_CHECK_LOOKBACK + 1):
            instruction = self.instructions.get(bctr_address - 4 * distance)
            if instruction is None:
                return None
            if instruction.is_compare_logical_immediate:
                return instruction.unsigned_immediate + 1
        return None

    def _bounds_of_function_around(self, address: int, ordered_starts: list[int]) -> tuple[int, int]:
        """Best known [start, end) of the function containing address: exact
        when extabindex lists it, otherwise up to the next known start."""
        position = bisect.bisect_right(ordered_starts, address) - 1
        start = ordered_starts[position]
        section_end = self.dol.section_at(address).end
        next_start = ordered_starts[position + 1] if position + 1 < len(ordered_starts) else section_end
        if start in self.exception_index_sizes:
            exact_end = start + self.exception_index_sizes[start]
            if address < exact_end:
                return start, exact_end
            # The address lies in a function nobody calls directly, somewhere
            # after this one ends.
            start = exact_end
        return start, min(next_start, section_end)

    def read_jump_tables(self, certain_starts: set[int]) -> None:
        """Read each switch's case targets. A table is only accepted when
        every case lands inside the function that owns the bctr.

        Function bounds here come only from certain starts. Guessed starts
        cannot be used: an undetected table's case labels look exactly like
        function pointers, and would make the table seem to leave its own
        function."""
        ordered_starts = sorted(certain_starts)
        self.jump_tables = {}
        self.rejected_jump_tables: list[int] = []
        for bctr_address, table in self.jump_table_for_bctr.items():
            function_start, function_end = self._bounds_of_function_around(bctr_address, ordered_starts)
            case_count = self._switch_case_count(bctr_address)
            case_targets = []
            entry = table
            while case_count is None or len(case_targets) < case_count:
                if self.dol.section_at(entry) is None:
                    break
                target = self.dol.u32(entry)
                if not function_start <= target < function_end:
                    break
                case_targets.append(target)
                entry += 4
            complete = case_count is None or len(case_targets) == case_count
            if case_targets and complete:
                self.jump_tables[table] = case_targets
            else:
                self.rejected_jump_tables.append(bctr_address)

    def collect_data_pointers(self) -> None:
        """Code addresses stored in data: vtables and callback tables.
        Jump table entries are skipped because they point inside functions."""
        jump_table_entries = {
            table + 4 * case for table, targets in self.jump_tables.items() for case in range(len(targets))
        }
        skipped_sections = {EXCEPTION_TABLE_SECTION, EXCEPTION_INDEX_SECTION}
        for section in self.dol.sections:
            if section.is_text or section.name in skipped_sections:
                continue
            for address in range(section.address, section.end, 4):
                if address in jump_table_entries:
                    continue
                value = self.dol.u32(address)
                if self.dol.is_code(value) and value % 4 == 0:
                    self.function_pointer_candidates.add(value)
                    self.references[value].add(address)

    def _starts_after_previous_function(self, address: int) -> bool:
        """A real function begins right after the previous one ends: after an
        instruction that never falls through, after padding, or at the start
        of a section. Rejects pointers into the middle of a function."""
        previous = self.instructions.get(address - 4)
        return (
            previous is None
            or previous.word in (0, NOP)
            or previous.ends_straight_line_code
            or previous.address in self.data_in_code_words
        )

    def _constructor_and_destructor_functions(self) -> set[int]:
        functions = set()
        for name in (CONSTRUCTOR_LIST_SECTION, DESTRUCTOR_LIST_SECTION):
            section = self.sections[name]
            for address in range(section.address, section.end, 4):
                value = self.dol.u32(address)
                if self.dol.is_code(value):
                    functions.add(value)
        return functions

    # -- step 3: extent ------------------------------------------------------

    def find_function_end(self, start: int, next_function_start: int, function_starts: set[int]) -> int:
        """End address (exclusive) of the function beginning at `start`."""
        furthest_branch_target = start
        address = start
        while address < next_function_start and address in self.instructions:
            instruction = self.instructions[address]
            if address > start and (instruction.word == 0 or address in self.data_in_code_words):
                return address  # alignment padding or data after the function

            target = instruction.branch_target()
            target_is_inside = target is not None and start <= target < next_function_start
            if instruction.is_conditional_branch and target_is_inside:
                furthest_branch_target = max(furthest_branch_target, target)
            elif instruction.is_jump:
                is_tail_call = target in function_starts and target != start
                if target_is_inside and not is_tail_call:
                    furthest_branch_target = max(furthest_branch_target, target)
            elif instruction.is_jump_to_count_register:
                table = self.jump_table_for_bctr.get(address)
                for case in self.jump_tables.get(table, []):
                    if start <= case < next_function_start:
                        furthest_branch_target = max(furthest_branch_target, case)

            if instruction.ends_straight_line_code and address >= furthest_branch_target:
                return address + 4
            address += 4
        return address  # fell through into the next function (e.g. _savegpr_14.._savegpr_31)

    def certain_function_starts(self) -> set[int]:
        """Starts that need no guessing: extabindex, bl targets, the entry
        point, section starts, .ctors and .dtors."""
        starts = set(self.exception_index_sizes) | self.call_targets
        starts.add(self.dol.entry)
        starts |= {section.address for section in self.dol.text_sections()}
        starts |= self._constructor_and_destructor_functions()
        return starts

    def _inside_exception_index_function(self, address: int, ordered_exception_starts: list[int]) -> bool:
        position = bisect.bisect_right(ordered_exception_starts, address) - 1
        if position < 0:
            return False
        start = ordered_exception_starts[position]
        return start < address < start + self.exception_index_sizes[start]

    def find_functions(self) -> None:
        function_starts = self.certain_function_starts()
        # A pointer into code is only a function start if it begins right
        # after another function, and never inside a function whose exact
        # size extabindex gives us.
        ordered_exception_starts = sorted(self.exception_index_sizes)
        for pointer in sorted(self.function_pointer_candidates):
            if pointer in self.data_in_code_words:
                continue
            if (
                self._starts_after_previous_function(pointer)
                and not self._inside_exception_index_function(pointer, ordered_exception_starts)
                and not self._claim_if_string(pointer)
            ):
                function_starts.add(pointer)
        function_starts -= self.data_in_code_words

        section_end_for = {}
        for section in self.dol.text_sections():
            for address in range(section.address, section.end, 4):
                section_end_for[address] = section.end

        while True:
            ordered_starts = sorted(function_starts)
            self.function_sizes = {}
            gap_starts = set()
            found_new_data = False
            for position, start in enumerate(ordered_starts):
                limit = section_end_for[start]
                if position + 1 < len(ordered_starts):
                    limit = min(limit, ordered_starts[position + 1])

                if start in self.exception_index_sizes:
                    end = start + self.exception_index_sizes[start]
                else:
                    end = self.find_function_end(start, limit, function_starts)
                self.function_sizes[start] = end - start

                # Anything between this function's end and the next start
                # that is not filler is a function nobody visibly refers to.
                address = end
                while address < limit and self._is_filler(address):
                    address += 4
                if address < limit:
                    if self._claim_if_string(address):
                        found_new_data = True  # look again past it next round
                    else:
                        gap_starts.add(address)

            if not gap_starts and not found_new_data:
                return
            function_starts |= gap_starts

    def run(self) -> None:
        """All passes, twice. The first round finds most functions; the second
        repeats the register and jump table passes knowing every function
        start, which resets register knowledge at the right places."""
        self.read_exception_index()
        self.find_startup_tables()
        self.collect_call_targets()
        certain_starts = self.certain_function_starts()
        function_starts = certain_starts
        for _ in range(2):
            self.references.clear()
            self.function_pointer_candidates.clear()
            self.jump_table_for_bctr.clear()
            self.follow_register_constants(function_starts)
            self.read_jump_tables(certain_starts)
            self.collect_data_pointers()
            self.find_functions()
            function_starts = set(self.function_sizes)

    def overlapping_functions(self) -> list[tuple[int, int]]:
        ordered_starts = sorted(self.function_sizes)
        return [
            (start, following)
            for start, following in itertools.pairwise(ordered_starts)
            if start + self.function_sizes[start] > following
        ]

    # -- checks --------------------------------------------------------------

    def compare_with_exception_index(self) -> list[tuple[int, int, int]]:
        """Measure every extabindex function with the walker, as if its size
        were unknown, and list disagreements as (start, real size, walked size).
        A walker that agrees here can be trusted on the other functions."""
        function_starts = set(self.function_sizes)
        ordered_starts = sorted(function_starts)
        disagreements = []
        for position, start in enumerate(ordered_starts):
            if start not in self.exception_index_sizes:
                continue
            real_size = self.exception_index_sizes[start]
            limit = ordered_starts[position + 1] if position + 1 < len(ordered_starts) else start + real_size
            walked_end = self.find_function_end(start, max(limit, start + real_size), function_starts)
            walked_size = walked_end - start
            if walked_size != real_size:
                disagreements.append((start, real_size, walked_size))
        return disagreements

    def function_containing(self, address: int, ordered_starts: list[int]) -> int | None:
        position = bisect.bisect_right(ordered_starts, address) - 1
        if position < 0:
            return None
        start = ordered_starts[position]
        return start if address < start + self.function_sizes[start] else None


def functions_by_source_file(dol: Dol, finder: FunctionFinder) -> dict[str, list[int]]:
    """Group functions by the source file name their asserts print.

    CodeWarrior links object files in order, so all functions from one .cpp
    sit next to each other. Functions that mention "Field.cpp" therefore
    mark where Field.cpp lives in .text, which is what a split needs."""
    ordered_starts = sorted(finder.function_sizes)
    functions_per_file: dict[str, set[int]] = defaultdict(set)
    for target, referrers in finder.references.items():
        text = dol.c_string(target, 64)
        if not text:
            continue
        file_name = text.strip()
        if not file_name.endswith((".c", ".cpp")) or " " in file_name or len(file_name) > 40:
            continue
        for referrer in referrers:
            function = finder.function_containing(referrer, ordered_starts)
            if function is not None:
                functions_per_file[file_name].add(function)
    return {file_name: sorted(functions) for file_name, functions in functions_per_file.items()}
