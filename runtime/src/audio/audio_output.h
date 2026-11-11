// The PC's sound output: what the console's audio DMA sends to its digital-to-analog
// converter, played through SDL on the default playback device.
//
// La salida de sonido de la PC: lo que el DMA de audio de la consola manda a su
// convertidor digital-analógico, tocado con SDL en el dispositivo de reproducción por
// defecto.
//
// The device keeps a margin of sound waiting (the game makes it in bursts), and plays a
// little faster or slower to hold that margin as the PC's clock and the sound card's
// drift apart. Without a playback device the sound is dropped and the game goes on.
// Where there is no sound card, SDL's "disk" driver writes what the device would play to
// a file (SDL_AUDIO_DRIVER=disk, SDL_AUDIO_DISK_OUTPUT_FILE=<file>): raw 16-bit stereo at
// the device's rate, which the log gives.
#pragma once

#include <cstdint>

namespace audio_output {

// Plays `frame_count` stereo frames as the audio DMA reads them from the game's memory:
// 16-bit samples, big-endian, the right channel first. `sample_rate` is the DSP's (32 or
// 48 kHz). The device opens with the first call, once aurora has started SDL.
void play(const uint8_t* frames, uint32_t frame_count, uint32_t sample_rate);

// How loud the sound plays: 0 is silent, 1 the game's own. It changes the sound already waiting
// too, so it can change while the game runs.
void set_volume(float volume);

void shutdown();

}  // namespace audio_output
