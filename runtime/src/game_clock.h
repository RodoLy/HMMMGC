// The game's clock: how fast the console's time runs for the game. The time base (and so
// the video retraces, the alarms, the audio DMA and the game's own clock of the day) follows
// the PC's clock at the chosen speed: 100% is the console's, 300% three times as fast.
//
// El reloj del juego: qué tan rápido corre el tiempo de la consola para el juego. La base
// de tiempo (y con ella los retrazos de video, las alarmas, el DMA de audio y el reloj del
// día del juego) sigue al reloj de la PC a la velocidad elegida.
//
// The speed is always the console's, except while the turbo key is held (aurora_host.h
// polls it once a frame). Everything the game does is counted
// in time-base ticks, so the whole game goes faster together.
#pragma once

#include <chrono>
#include <cstdint>

namespace game_clock {

inline constexpr unsigned NORMAL_SPEED_PERCENT = 100;
inline constexpr unsigned TURBO_SPEED_PERCENT = 300;

// Time-base ticks (40.5 MHz) since the runtime started, at the speeds it has run at.
uint64_t elapsed_ticks();

// How long, on the PC's clock, `ticks` time-base ticks take at the current speed.
std::chrono::nanoseconds real_duration(uint64_t ticks);

// The turbo key is held, or no longer.
void set_turbo(bool held);
// The speed now: the turbo's while it is held, otherwise the console's.
unsigned speed_percent();

}  // namespace game_clock
