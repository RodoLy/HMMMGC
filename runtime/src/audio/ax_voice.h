// The voices of AX, the GameCube's sound library, as its DSP program plays them: each
// voice reads samples from the audio memory, converts them to 32 kHz and mixes them into
// the frame's buffers.
//
// Las voces de AX, la biblioteca de sonido del GameCube, como las toca su programa del
// DSP: cada voz lee muestras de la memoria de audio, las convierte a 32 kHz y las mezcla
// en los búferes del cuadro.
//
// Ported from Dolphin's high-level emulation of AX (Source/Core/Core/HW/DSPHLE/UCodes/
// AXStructs.h and AXVoice.h, and the DSP's accelerator in Core/DSP/DSPAccelerator.cpp;
// Copyright 2008 Dolphin Emulator Project, GPL-2.0-or-later), keeping only the GameCube.
//
// The CPU describes each voice with a parameter block in main memory (AXPB): where its
// sample is in the audio memory and in which format (4-bit ADPCM, 8 or 16-bit PCM), the
// ratio of the sample-rate conversion, a volume envelope and one volume for each buffer.
// The blocks form a list. Every 5 ms frame the DSP walks the list: one millisecond at a
// time it applies the updates the CPU queued for that millisecond, reads the samples
// through its accelerator, converts their rate, and adds them to the buffers; then it
// writes the block back, with the new position in the sample.
#pragma once

#include <cstdint>

namespace ax_voice {

// AX mixes at 32 kHz, in frames of 5 ms.
inline constexpr uint32_t SAMPLES_PER_MILLISECOND = 32;
inline constexpr uint32_t MILLISECONDS_PER_FRAME = 5;
inline constexpr uint32_t SAMPLES_PER_FRAME = SAMPLES_PER_MILLISECOND * MILLISECONDS_PER_FRAME;

// The nine buffers of a frame: the main mix and the two auxiliary sends (A and B, which
// the CPU runs its effects on), each with left, right and surround channels.
struct MixBuffers {
    int32_t* main_left;
    int32_t* main_right;
    int32_t* main_surround;
    int32_t* aux_a_left;
    int32_t* aux_a_right;
    int32_t* aux_a_surround;
    int32_t* aux_b_left;
    int32_t* aux_b_right;
    int32_t* aux_b_surround;
};

// What changes between versions of the AX program.
struct ProgramVersion {
    // The parameter block has a low-pass filter (all but the oldest version).
    bool has_low_pass_filter = true;
    // The meaning of the block's mixer_control bits: the oldest version packs them
    // differently.
    bool has_old_mixer_control = false;
};

// Plays one frame of every voice in the list that starts at `first_block`, a guest
// address, adding the samples to `buffers`.
void play_voice_list(uint32_t first_block, const MixBuffers& buffers, const ProgramVersion& version);

}  // namespace ax_voice
