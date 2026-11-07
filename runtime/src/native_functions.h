// Native replacements of the game's functions.
//
// Reemplazos nativos de funciones del juego.
//
// Some functions of the game's SDK cannot run as translated code, because they depend on
// what the console's hardware does asynchronously (an interrupt arriving, the CPU jumping
// to another thread's saved registers), or because the PC does their job another way (the
// graphics functions, which aurora implements). The runtime provides them natively and
// registers each one at the guest address of the function it replaces.
//
// The translator reads these markers from the runtime's sources (WiiCompiled's syntax:
// address in hex without 0x, symbol, [return type,] host parameters, host arguments): it
// leaves those functions out of the translation, never inlines them, and sends every call
// to them through the registry, where the native wins. There are two kinds of native:
//
//   - One that takes the whole guest register file, `void f(CpuContext* cpu)`, for the
//     functions that have to look at or change more than their arguments (the OS natives).
//     The translator learns which registers it reads and writes by reading its body for
//     `<parameter>->gpr[N]`, with the parameter named in the marker, so the marker must
//     name it exactly as the definition does.
//   - One that takes the game's arguments as C++ parameters, `void f(uint32_t a, float b)`.
//     The arguments come from the registers of the PowerPC calling convention: whole
//     numbers and addresses from r3 to r10 and then from the stack, floating-point numbers
//     from f1 to f8. A return value goes to r3, or to f1 for a floating-point one.
//
// Ver el texto en inglés: los dos tipos de reemplazo y de dónde salen sus argumentos.
#pragma once

#include "guest_abi/guest_abi.h"
#include "guest_memory.h"

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace native_functions {

// Registers `invoke` as the function at `address`, above the translation of that address.
// `entry` is the native itself, kept for the crash reports.
struct Registration {
    Registration(uint32_t address, const char* name, void* entry, void (*invoke)(CpuContext*)) {
        TranslatedFunctionInfo info;
        info.address = address;
        info.name = name;
        info.priority = kNativeFunctionPriority;
        // A native keeps the callee-saved floating-point registers like any guest function.
        info.nonvolatileFprWriteMask = 0;
        info.entryPoint = entry;
        info.rawCpuInvoker = invoke;
        info.mustRemainDynamicallyDispatchable = true;
        info.kind = FunctionKind::Native;
        TranslatedFunctionRegistry::Register(info);
    }
};

namespace detail {

constexpr int FIRST_ARGUMENT_GPR = 3;           // r3
constexpr int ARGUMENT_GPR_COUNT = 8;           // r3 to r10
constexpr int FIRST_ARGUMENT_FPR = 1;           // f1
constexpr uint32_t STACK_ARGUMENTS_OFFSET = 8;  // after the back chain and the saved LR

// Where the next argument comes from, as the PowerPC calling convention hands them out.
struct ArgumentCursor {
    int next_integer = 0;
    int next_floating_point = 0;
};

template <typename T>
T load_argument(const CpuContext* cpu, ArgumentCursor& cursor) {
    if constexpr (std::is_floating_point_v<T>) {
        // A float argument is held in its register as a double, like every scalar value.
        return static_cast<T>(cpu->fpr[FIRST_ARGUMENT_FPR + cursor.next_floating_point++].d);
    } else {
        static_assert(std::is_integral_v<T> || std::is_enum_v<T>, "a native argument is a number or an address");
        const int index = cursor.next_integer++;
        uint32_t value;
        if (index < ARGUMENT_GPR_COUNT) {
            value = cpu->gpr[FIRST_ARGUMENT_GPR + index];
        } else {
            // The ninth whole-number argument and the ones after it are on the caller's stack.
            const uint32_t stack_pointer = cpu->gpr[1];
            value = guest_memory::read32(stack_pointer + STACK_ARGUMENTS_OFFSET +
                                         static_cast<uint32_t>(index - ARGUMENT_GPR_COUNT) * 4u);
        }
        return static_cast<T>(value);
    }
}

}  // namespace detail

// Calls a native from the guest's register file: the register-file kind as it is, the
// other kind with its arguments taken out of the registers and its result put back.
template <auto Native>
struct RawCpuThunk;

template <typename Result, typename... Parameters, Result (*Native)(Parameters...)>
struct RawCpuThunk<Native> {
    static void invoke(CpuContext* cpu) {
        if constexpr (std::is_same_v<std::tuple<Parameters...>, std::tuple<CpuContext*>>) {
            Native(cpu);  // the register-file kind
            return;
        } else {
            invoke_with_arguments(cpu);
        }
    }

    static void invoke_with_arguments(CpuContext* cpu) {
        detail::ArgumentCursor cursor;
        // Brace initialization takes the arguments in order, which is the order of the
        // registers.
        const std::tuple<Parameters...> arguments{detail::load_argument<Parameters>(cpu, cursor)...};
        if constexpr (std::is_void_v<Result>) {
            std::apply(Native, arguments);
        } else if constexpr (std::is_floating_point_v<Result>) {
            cpu->fpr[detail::FIRST_ARGUMENT_FPR].d = static_cast<double>(std::apply(Native, arguments));
        } else {
            cpu->gpr[detail::FIRST_ARGUMENT_GPR] = static_cast<uint32_t>(std::apply(Native, arguments));
        }
    }
};

}  // namespace native_functions

// The translator trusts the parameters written in the marker to know which registers a
// native reads, so each marker is checked against the native's own signature.
#define PPC_NATIVE_REGISTER(address_hex, symbol, result_type, parameters)              \
    static_assert(std::is_same_v<decltype(&symbol), result_type(*) parameters>,        \
                  "the marker of " #symbol " must repeat the native's own signature"); \
    static const native_functions::Registration native_registration_##address_hex(     \
        0x##address_hex##u, #symbol, reinterpret_cast<void*>(&symbol),                 \
        &native_functions::RawCpuThunk<&symbol>::invoke)

#define PPC_NATIVE_OVERRIDE_VOID(address_hex, symbol, parameters, arguments) \
    PPC_NATIVE_REGISTER(address_hex, symbol, void, parameters)

#define PPC_NATIVE_OVERRIDE(address_hex, symbol, result_type, parameters, arguments) \
    PPC_NATIVE_REGISTER(address_hex, symbol, result_type, parameters)
