// See arm64_load.h.

#include "arm64_load.h"

namespace arm64_load {
namespace {

uint32_t bits(uint32_t value, unsigned high, unsigned low) { return (value >> low) & ((1u << (high - low + 1)) - 1); }

int64_t sign_extend_field(uint32_t value, unsigned width) {
    const uint32_t sign = 1u << (width - 1);
    return static_cast<int64_t>(static_cast<int32_t>((value ^ sign) - sign));
}

}  // namespace

std::optional<Load> decode(uint32_t instruction) {
    // The "load/store register" group: bits 29-27 are 111.
    if (bits(instruction, 29, 27) != 0b111) {
        return std::nullopt;
    }
    const uint32_t size_field = bits(instruction, 31, 30);
    const bool vector = bits(instruction, 26, 26) != 0;
    const uint32_t opc = bits(instruction, 23, 22);

    Load load{};
    load.destination = bits(instruction, 4, 0);
    load.base = bits(instruction, 9, 5);
    load.to_vector_register = vector;
    unsigned scale = size_field;
    if (vector) {
        if (opc == 0b01) {
            load.size = 1u << size_field;  // B, H, S, D
        } else if (opc == 0b11 && size_field == 0) {
            load.size = 16;  // Q
            scale = 4;
        } else {
            return std::nullopt;  // a store
        }
    } else {
        load.size = 1u << size_field;
        switch (opc) {
            case 0b01:
                break;  // LDR, LDRB, LDRH: zero-extended
            case 0b10:  // LDRSB, LDRSH, LDRSW to an X register (with size 3: a prefetch)
                if (size_field == 0b11) {
                    return std::nullopt;
                }
                load.sign_extend = true;
                load.extend_to_64_bits = true;
                break;
            case 0b11:  // LDRSB, LDRSH to a W register
                if (size_field >= 0b10) {
                    return std::nullopt;
                }
                load.sign_extend = true;
                break;
            default:
                return std::nullopt;  // a store
        }
    }

    const uint32_t form = bits(instruction, 25, 24);
    if (form == 0b01) {  // unsigned, scaled 12-bit offset
        load.immediate_offset = static_cast<int64_t>(bits(instruction, 21, 10)) << scale;
        return load;
    }
    if (form != 0b00) {
        return std::nullopt;
    }
    if (bits(instruction, 21, 21) == 0) {  // signed 9-bit offset, not scaled
        load.immediate_offset = sign_extend_field(bits(instruction, 20, 12), 9);
        switch (bits(instruction, 11, 10)) {
            case 0b00:  // LDUR
            case 0b10:  // LDTR (unprivileged): the same access in user mode
                return load;
            case 0b01:
                load.post_index = true;
                load.writes_back = true;
                return load;
            case 0b11:
                load.writes_back = true;
                return load;
        }
    }
    if (bits(instruction, 11, 10) == 0b10) {  // register offset
        load.uses_offset_register = true;
        load.offset_register = bits(instruction, 20, 16);
        load.offset_extend = bits(instruction, 15, 13);
        load.offset_shift = bits(instruction, 12, 12) != 0 ? scale : 0;
        if ((load.offset_extend & 0b010) == 0) {
            return std::nullopt;  // reserved extends
        }
        return load;
    }
    return std::nullopt;  // atomics and other forms
}

uint64_t extended_offset(const Load& load, uint64_t value) {
    uint64_t extended = value;
    switch (load.offset_extend) {
        case 0b010:  // UXTW
            extended = static_cast<uint32_t>(value);
            break;
        case 0b110:  // SXTW
            extended = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(value)));
            break;
        default:  // LSL (UXTX) and SXTX: the whole register
            break;
    }
    return extended << load.offset_shift;
}

}  // namespace arm64_load
