// See game_scene.h.

#include "game_scene.h"

#include "guest_memory.h"

#include <cstdint>

namespace game_scene {
namespace {

// The game's event manager (a pointer, 0 before the first one is made). Its word at +0x94
// is the event running: 0x119 when there is none, 0x10A on the title screen, 0x41 in its
// demo, the event's own number in a cutscene (docs/SCRIPTS.md, "How a script runs").
constexpr uint32_t EVENT_MANAGER_POINTER = 0x80321D88u;
constexpr uint32_t RUNNING_EVENT_OFFSET = 0x94u;
constexpr uint32_t NO_EVENT = 0x119u;

// The area whose map is loaded: 0 the farm, 27 the first house.
constexpr uint32_t CURRENT_AREA = 0x80321D40u;

// The map of each area, 4 bytes per area (read by the map loader, fn_8003DCA8): pieces
// across, pieces deep, and a piece's width and depth. The farm is 7 by 7 pieces of 24 by
// 24; the first house one piece of 8 by 6.
constexpr uint32_t AREA_MAPS = 0x80216830u;
constexpr uint32_t AREA_COUNT = 129;
// Areas 0 to 15 are the fields, which the map loader builds from the pieces in the files
// FLDaapp (area aa, piece pp); the rest are rooms of one piece, or more places outdoors
// made of several.
constexpr uint32_t FIELD_AREA_COUNT = 16;

bool no_event_running() {
    const uint32_t manager = guest_memory::read32(EVENT_MANAGER_POINTER);
    return guest_memory::is_cached_main_memory(manager) &&
           guest_memory::read32(manager + RUNNING_EVENT_OFFSET) == NO_EVENT;
}

bool area_is_outdoors() {
    const uint32_t area = guest_memory::read32(CURRENT_AREA);
    if (area >= AREA_COUNT) {
        return false;
    }
    if (area < FIELD_AREA_COUNT) {
        return true;
    }
    const uint32_t map = AREA_MAPS + area * 4;
    const unsigned pieces = guest_memory::read8(map) * guest_memory::read8(map + 1);
    return pieces > 1;
}

}  // namespace

bool player_walks_outdoors() { return no_event_running() && area_is_outdoors(); }

}  // namespace game_scene
