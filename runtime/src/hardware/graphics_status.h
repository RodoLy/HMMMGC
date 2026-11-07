// What the CPU sees of the graphics chip: the status of its command processor (CP,
// 0xCC000000), the interrupts of its pixel engine (PE, 0xCC001000), and the write-gather
// pipe (0xCC008000) through which the game sends its commands.
//
// Lo que el CPU ve del chip gráfico: el estado de su procesador de comandos, las
// interrupciones de su "pixel engine", y el "gather pipe" por el que el juego le manda
// sus comandos.
//
// The pipe comes here only until GXInit: from then on aurora is the GPU, and what the game
// stores in the pipe goes to graphics/gx_draw.h (see hardware_registers.h). Until then the
// commands are stored in the FIFO as on the console, and the two that the game could wait
// for (draw done, and the tokens of GXSetDrawSync) are answered as soon as they are sent.
#pragma once

#include <cstddef>
#include <cstdint>

namespace graphics_status {

inline constexpr uint32_t COMMAND_PROCESSOR_BASE = 0xCC000000u;
inline constexpr uint32_t PIXEL_ENGINE_BASE = 0xCC001000u;
inline constexpr uint32_t GATHER_PIPE_BASE = 0xCC008000u;

void refresh_command_processor(uint32_t address);
void write_pixel_engine(uint32_t address, uint16_t value);

// Graphics commands stored to the gather pipe, in guest (big-endian) byte order.
void write_to_gather_pipe(const uint8_t* bytes, size_t size);
uint64_t bytes_sent();

}  // namespace graphics_status
