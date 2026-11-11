// Decodes the ARM64 instructions that load one register from memory (LDR, LDUR, LDRB,
// LDRH, LDRSB, LDRSH, LDRSW, and the loads of SIMD and floating-point registers), in
// their immediate, unscaled, pre-index, post-index and register-offset forms. Used to
// carry out by hand a load that faulted (fault_handler_windows.cpp); the encodings are
// those of the Arm Architecture Reference Manual, "Loads and stores".
//
// Decodifica las instrucciones de ARM64 que cargan un registro desde memoria, para hacer
// a mano una carga que falló.
#pragma once

#include <cstdint>
#include <optional>

namespace arm64_load {

struct Load {
    unsigned destination;       // Rt (31: the zero register, for general registers)
    bool to_vector_register;    // a SIMD/floating-point register (V0-V31)
    unsigned size;              // bytes read: 1, 2, 4, 8 or 16
    bool sign_extend;           // LDRSB, LDRSH, LDRSW
    bool extend_to_64_bits;     // the signed result fills the whole X register
    unsigned base;              // Rn (31: the stack pointer)
    int64_t immediate_offset;   // added to the base (before or after the access, see below)
    bool uses_offset_register;  // the offset is register Rm, extended and shifted
    unsigned offset_register;   // Rm (31: the zero register)
    unsigned offset_extend;     // the "option" field: 2 UXTW, 3 LSL, 6 SXTW, 7 SXTX
    unsigned offset_shift;      // left shift of the extended Rm
    bool post_index;            // the access uses the base alone; base += offset afterwards
    bool writes_back;           // pre- or post-index: the base register is updated
};

// The load encoded in `instruction`, or nothing for any other instruction (stores,
// pairs, prefetches, atomics...).
std::optional<Load> decode(uint32_t instruction);

// The value of the extended and shifted offset register, given the register's value.
uint64_t extended_offset(const Load& load, uint64_t offset_register_value);

}  // namespace arm64_load
