// See arm64_register_read.h.

#include "arm64_register_read.h"

#include "arm64_load.h"
#include "guest_memory.h"
#include "hardware/hardware_registers.h"
#include "runtime_log.h"

#include "guest_abi/guest_abi.h"

#include <cstring>
#include <optional>

namespace arm64_register_read {
namespace {

// Register 31 is the stack pointer as a base, and the zero register as an offset.
uint64_t register_value(const Registers& registers, unsigned number, bool number_31_is_stack_pointer) {
    if (number == 31) {
        return number_31_is_stack_pointer ? registers.stack_pointer() : 0;
    }
    return registers.general(number);
}

bool is_inside_guest_space(uint64_t host_address) {
    const auto flat_base = reinterpret_cast<uintptr_t>(guest_memory::flat_base());
    return flat_base != 0 && host_address - flat_base < GuestFlat::kGuestSpaceSize;
}

}  // namespace

bool carry_out(Registers& registers) {
    uint32_t instruction = 0;
    std::memcpy(&instruction, reinterpret_cast<const void*>(registers.pc()), sizeof(instruction));
    const std::optional<arm64_load::Load> load = arm64_load::decode(instruction);
    if (!load) {
        runtime_log::error("memory", "a register read through a pointer uses instruction 0x%08X, which is not handled",
                           instruction);
        return false;
    }
    const uint64_t base = register_value(registers, load->base, true);
    const int64_t offset = load->uses_offset_register
                               ? static_cast<int64_t>(arm64_load::extended_offset(
                                     *load, register_value(registers, load->offset_register, false)))
                               : load->immediate_offset;
    const uint64_t address = load->post_index ? base : base + offset;
    const auto guest_address = static_cast<uint32_t>(address - reinterpret_cast<uintptr_t>(guest_memory::flat_base()));
    if (!is_inside_guest_space(address) || !guest_memory::is_hardware_register(guest_address) ||
        !guest_memory::is_hardware_register(guest_address + load->size - 1)) {
        return false;
    }

    // Bring the registers it covers up to date, then read them as the instruction would.
    const auto* pc = reinterpret_cast<const void*>(registers.pc());
    for (unsigned covered = 0; covered < load->size; covered += 4) {
        hardware_registers::prepare_trapped_read(guest_address + covered, pc);
    }
    uint8_t bytes[16] = {};
    std::memcpy(bytes,
                guest_memory::hardware_register_image() + (guest_address - guest_memory::HARDWARE_REGISTERS_BASE),
                load->size);

    if (load->to_vector_register) {
        uint8_t* vector = registers.vector(load->destination);
        if (vector == nullptr) {
            runtime_log::error("memory", "a register read through a pointer loads V%u, which this system does not show",
                               load->destination);
            return false;
        }
        std::memset(vector, 0, 16);
        std::memcpy(vector, bytes, load->size);
    } else if (load->destination != 31) {
        uint64_t value = 0;
        std::memcpy(&value, bytes, load->size);
        if (load->sign_extend) {
            const unsigned unused_bits = 64 - load->size * 8;
            value = static_cast<uint64_t>(static_cast<int64_t>(value << unused_bits) >> unused_bits);
            if (!load->extend_to_64_bits) {
                value = static_cast<uint32_t>(value);
            }
        }
        registers.set_general(load->destination, value);
    }
    if (load->writes_back) {
        if (load->base == 31) {
            registers.set_stack_pointer(base + offset);
        } else {
            registers.set_general(load->base, base + offset);
        }
    }
    registers.set_pc(registers.pc() + 4);
    return true;
}

}  // namespace arm64_register_read
