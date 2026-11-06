// Native replacements of the game's functions.
//
// Reemplazos nativos de funciones del juego.
//
// A few functions of the game's SDK cannot run as translated code, because they depend
// on what the console's hardware does asynchronously (an interrupt arriving, the CPU
// jumping to another thread's saved registers). The runtime provides them natively and
// registers each one at the guest address of the function it replaces.
//
// The translator reads these markers from the runtime's sources (WiiCompiled's
// PPC_NATIVE_OVERRIDE_VOID syntax: address in hex without 0x, symbol, host parameters,
// host arguments): it leaves those functions out of the translation, never inlines them,
// and sends every call to them through the registry, where the native wins.
//
// It also reads the native's body to learn which guest registers the call reads and
// writes, looking for `<parameter>->gpr[N]` with the parameter named in the marker. The
// marker must therefore name the native's CpuContext parameter exactly as its definition
// does; otherwise the translator sees no register access and does not pass the
// arguments. A native that hands its context to another function is treated as reading
// and writing everything, which is always safe.
#pragma once

#include "guest_abi/guest_abi.h"

namespace native_functions {

// Registers `entry` as the function at `address`, above the translation of that address.
struct Registration {
    Registration(uint32_t address, const char* name, void (*entry)(CpuContext*)) {
        TranslatedFunctionInfo info;
        info.address = address;
        info.name = name;
        info.priority = kNativeFunctionPriority;
        // A native keeps the callee-saved floating-point registers like any guest function.
        info.nonvolatileFprWriteMask = 0;
        info.entryPoint = reinterpret_cast<void*>(entry);
        info.rawCpuInvoker = entry;
        info.mustRemainDynamicallyDispatchable = true;
        info.kind = FunctionKind::Native;
        TranslatedFunctionRegistry::Register(info);
    }
};

}  // namespace native_functions

#define PPC_NATIVE_OVERRIDE_VOID(address_hex, symbol, parameters, arguments) \
    static const native_functions::Registration native_registration_##address_hex(0x##address_hex##u, #symbol, symbol)
