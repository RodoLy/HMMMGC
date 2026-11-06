// The registers of the GameCube's CPU (Gekko) that the translated code cannot keep in
// host variables: the special-purpose registers reached with mfspr/mtspr, the time base,
// and the decrementer.
//
// Los registros del CPU del GameCube (Gekko) que el código traducido no puede guardar en
// variables del host: los de propósito especial, la base de tiempo y el decrementador.
#pragma once

#include <cstdint>

struct CpuContext;

namespace cpu_registers {

// The time base counts at a quarter of the bus clock (162 MHz / 4), on the console and here.
inline constexpr uint64_t TIME_BASE_TICKS_PER_SECOND = 40'500'000;

// The time base: ticks since the runtime started, plus whatever the game wrote to it.
uint64_t time_base();

// The decrementer counts down at the time-base rate; when it passes zero the CPU takes
// the decrementer exception, which the OS uses for its alarms. These tell when that is
// due, and take it (once per value written, as on the console).
uint64_t decrementer_deadline();
bool take_decrementer_exception();

// mfspr and mtspr of special-purpose register `number` on the thread running `cpu`.
uint32_t read_special_register(CpuContext* cpu, uint32_t number);
void write_special_register(CpuContext* cpu, uint32_t number, uint32_t value);

}  // namespace cpu_registers
