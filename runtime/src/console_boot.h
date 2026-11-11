// What the console has done before the game's first instruction runs: the boot ROM
// (IPL) and the disc's apploader fill the bottom of memory with facts about the console
// and the disc, load the file system table and the debug settings, and set the CPU up.
//
// Lo que hizo la consola antes de la primera instrucción del juego: el ROM de arranque y
// el "apploader" del disco llenan el inicio de la memoria con datos de la consola y del
// disco, cargan la tabla de archivos y preparan el CPU.
#pragma once

#include <cstdint>

class GameDisc;
struct CpuContext;

namespace console_boot {

// Writes the low-memory globals, the FST and bi2 the way they are after a boot on the
// console (the values were read from a boot of this game under Dolphin).
void prepare_memory(const GameDisc& disc);

// The CPU state the game's entry point starts with.
void prepare_cpu(CpuContext& cpu);

// The game's entry point, from the header of main.dol.
uint32_t entry_point(const GameDisc& disc);

}  // namespace console_boot
