// See audio_memory.h.

#include "hardware/audio_memory.h"

#include <vector>

namespace audio_memory {

uint8_t* data() {
    static std::vector<uint8_t> memory(SIZE);
    return memory.data();
}

}  // namespace audio_memory
