// The GameCube's memory-mapped hardware registers (0xCC000000-0xCC00FFFF): every access
// of the game to them comes here and goes to the model of its device, in the files next
// to this one.
//
// Los registros de hardware del GameCube: cada acceso del juego a ellos llega aquí y va al
// modelo de su dispositivo, en los archivos de al lado.
//
// The first read and the first write of each register are logged with the function of
// the game that made them, which turns a run into the list of hardware the runtime has to
// provide. A register without a model keeps the last value written to it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class ExtractedDisc;

namespace hardware_registers {

// `disc` is what the DVD interface reads from; it must outlive the runtime.
void initialize(const ExtractedDisc& disc);

// Accesses of `width` bytes (1, 2 or 4) made by translated code. `host_caller` is a host
// code address inside the translated function that made it, to name it in the log.
uint32_t read(uint32_t guest_address, unsigned width, const void* host_caller = nullptr);
void write(uint32_t guest_address, uint32_t value, unsigned width, const void* host_caller = nullptr);

// A read through a pointer is about to load the register at `guest_address` straight from
// the register image (see fault_handler.h): brings the image up to date first.
void prepare_trapped_read(uint32_t guest_address, const void* host_caller);

// Graphics commands the game stored to the write-gather pipe, in guest byte order.
void write_to_gather_pipe(const uint8_t* bytes, size_t size);

// Interrupt causes pending in the processor interface, and those the game has unmasked.
uint32_t pending_interrupts();
uint32_t enabled_interrupts();

// Brings the devices that run on their own clock (the video beam, the audio sample
// counter) up to the current time base, raising the interrupts that came due; and tells
// when the next one is due.
void advance_time();
uint64_t next_event_time();

// One line about what the devices have done so far, for the watchdog.
std::string describe_activity();

}  // namespace hardware_registers
