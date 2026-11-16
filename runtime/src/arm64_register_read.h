// On an ARM64 host, a read of a hardware register through a pointer (see fault_handler.h) is
// carried out by the fault handler itself: it decodes the load that faulted (arm64_load.h),
// reads the register's current value and writes it to the load's destination register, and
// steps over the instruction. ARM64 has no single-step trap for a program to use, on any of the
// systems the runtime runs on. This is that part, the same on every system; each system's fault
// handler gives it the faulting thread's registers as it keeps them (fault_handler_windows.cpp,
// fault_handler_posix.cpp).
//
// En un procesador ARM64, una lectura de un registro de hardware con un puntero la hace el propio
// manejador de fallos: decodifica la carga que falló, lee el valor del registro, lo escribe en el
// registro de destino de la carga y salta la instrucción. Esta es esa parte, igual en todos los
// sistemas; el manejador de cada sistema le da los registros del hilo como ese sistema los guarda.
#pragma once

#include <cstdint>

namespace arm64_register_read {

// The registers of the thread that faulted, in the context the system gave the handler. A change
// made through it is what the thread sees when the handler returns.
class Registers {
public:
    virtual ~Registers() = default;
    virtual uint64_t pc() const = 0;
    virtual void set_pc(uint64_t value) = 0;
    // X0 to X30.
    virtual uint64_t general(unsigned number) const = 0;
    virtual void set_general(unsigned number, uint64_t value) = 0;
    virtual uint64_t stack_pointer() const = 0;
    virtual void set_stack_pointer(uint64_t value) = 0;
    // The 16 bytes of V<number>, or nullptr when the context does not have them.
    virtual uint8_t* vector(unsigned number) = 0;
};

// Carries out the load at the faulting instruction from the current value of the hardware
// registers it reads, and steps over it. False when the instruction is not a load this knows, or
// does not read hardware registers: the fault is then a real one.
bool carry_out(Registers& registers);

}  // namespace arm64_register_read
