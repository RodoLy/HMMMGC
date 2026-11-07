// See gx_draw.h. Display list commands as YAGCD (section 5.11, "GX FIFO commands") lists
// them.

#include "graphics/gx_draw.h"

#include "graphics/frame_interpolation.h"
#include "graphics/vertex_layout.h"
#include "guest_memory.h"
#include "runtime_log.h"

#include <dolphin/gx.h>

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

// Hands a whole primitive (a draw command and its vertices, big-endian as the GPU reads
// them) to aurora's command FIFO. Defined in aurora (lib/dolphin/gx/GXVert.cpp) without
// being in its public headers, as WiiCompiled uses it.
namespace aurora::gx::fifo {
bool submit_raw_draw(GXPrimitive primitive, GXVtxFmt vertex_format, const uint8_t* vertices, uint16_t vertex_count,
                     uint32_t vertex_bytes);
}

namespace gx_draw {
namespace {

// Commands of a display list (the first byte of each one).
constexpr uint8_t COMMAND_NOP = 0x00;
constexpr uint8_t COMMAND_LOAD_CP_REGISTER = 0x08;   // + register (1 byte) + value (4)
constexpr uint8_t COMMAND_LOAD_XF_REGISTERS = 0x10;  // + (count - 1) and address (4) + count values (4 each)
constexpr uint8_t COMMAND_LOAD_INDEXED_A = 0x20;     // + index, length and address (4): position matrices
constexpr uint8_t COMMAND_LOAD_INDEXED_D = 0x38;     // ... normal matrices, texture matrices, lights
constexpr uint8_t COMMAND_CALL_DISPLAY_LIST = 0x40;  // + address (4) + size (4)
constexpr uint8_t COMMAND_INVALIDATE_VERTEX_CACHE = 0x48;
constexpr uint8_t COMMAND_LOAD_BP_REGISTER = 0x61;  // + register and value (4)
constexpr uint8_t FIRST_DRAW_COMMAND = 0x80;        // primitive in the high bits, vertex format in the low 3
constexpr uint8_t LAST_DRAW_COMMAND = 0xBF;
constexpr uint8_t VERTEX_FORMAT_MASK = 0x07;
constexpr uint8_t PRIMITIVE_MASK = 0xF8;

// aurora takes array strides of one byte.
constexpr uint32_t LARGEST_ARRAY_STRIDE = 0xFF;

// A primitive whose vertices are still arriving.
struct OpenPrimitive {
    bool open = false;
    uint32_t primitive = 0;
    uint32_t vertex_format = 0;
    uint32_t vertex_count = 0;
    // The size of its vertices in the current vertex layout. Only a primitive drawn right
    // away has one: in a display list the layout is the one of the moment the list runs.
    uint32_t expected_bytes = 0;
    std::vector<uint8_t> vertices;
};
OpenPrimitive g_primitive;

// A primitive inside a display list that aurora recorded, kept to find the arrays that
// its indices use each time the list runs.
struct RecordedPrimitive {
    uint32_t vertex_format = 0;
    uint32_t vertex_count = 0;
    std::vector<uint8_t> vertices;
};

// The display list being recorded (between GXBeginDisplayList and GXEndDisplayList).
struct Recording {
    bool active = false;
    uint32_t guest_address = 0;
    std::vector<RecordedPrimitive> primitives;
};
Recording g_recording;

// A display list that aurora recorded, with what its bytes were when the recording ended.
// The game frees lists and reuses their memory (for a model loaded after a cutscene, say),
// so a list is only taken for the recorded one while its bytes are still the same.
struct RecordedList {
    uint32_t size = 0;
    uint64_t fingerprint = 0;
    std::vector<RecordedPrimitive> primitives;
};

// Every display list that aurora recorded, by the guest address of the list. A list that
// is not here (or no longer matches) comes from the game's files.
std::unordered_map<uint32_t, RecordedList> g_recorded_display_lists;

// FNV-1a: enough to tell whether the bytes of a list changed.
uint64_t fingerprint(const uint8_t* bytes, uint32_t size) {
    uint64_t hash = 0xCBF29CE484222325ull;
    for (uint32_t i = 0; i < size; ++i) {
        hash = (hash ^ bytes[i]) * 0x100000001B3ull;
    }
    return hash;
}

uint32_t read_big_endian16(const uint8_t* bytes) { return (uint32_t{bytes[0]} << 8) | bytes[1]; }

uint32_t read_big_endian32(const uint8_t* bytes) {
    return (uint32_t{bytes[0]} << 24) | (uint32_t{bytes[1]} << 16) | (uint32_t{bytes[2]} << 8) | bytes[3];
}

// How far into each array the draws read: the end of the last element they use.
using ArraySpans = std::array<uint32_t, vertex_layout::ARRAY_COUNT>;

void add_vertex_spans(uint32_t vertex_format, const vertex_layout::HighestIndices& highest, ArraySpans& spans) {
    for (uint32_t attribute = 0; attribute < vertex_layout::ARRAY_COUNT; ++attribute) {
        if (highest[attribute] < 0) {
            continue;
        }
        const uint32_t end = static_cast<uint32_t>(highest[attribute]) * vertex_layout::array_stride(attribute) +
                             vertex_layout::array_element_size(vertex_format, attribute);
        spans[attribute] = std::max(spans[attribute], end);
    }
}

// Tells aurora where each used array is and how much of it the draws read.
void publish_arrays(const ArraySpans& spans) {
    for (uint32_t attribute = 0; attribute < vertex_layout::ARRAY_COUNT; ++attribute) {
        if (spans[attribute] == 0) {
            continue;
        }
        const uint32_t address = vertex_layout::array_address(attribute);
        const uint32_t stride = vertex_layout::array_stride(attribute);
        const uint8_t* data = guest_memory::host_pointer(address, spans[attribute]);
        if (data == nullptr || stride > LARGEST_ARRAY_STRIDE) {
            runtime_log::warning("gx", "array %u at 0x%08X (stride %u, %u bytes) cannot be given to aurora", attribute,
                                 address, stride, spans[attribute]);
            continue;
        }
        GXSetArray(static_cast<GXAttr>(attribute), data, spans[attribute], static_cast<u8>(stride), false);
    }
}

// Walks a display list from the game's files for the arrays its draws and its indexed
// matrix loads read. Returns false when a command is not understood.
bool find_display_list_spans(const uint8_t* list, uint32_t size, ArraySpans& spans) {
    uint32_t offset = 0;
    while (offset < size) {
        const uint8_t command = list[offset++];
        if (command == COMMAND_NOP || command == COMMAND_INVALIDATE_VERTEX_CACHE) {
            continue;
        }
        if (command >= FIRST_DRAW_COMMAND && command <= LAST_DRAW_COMMAND) {
            if (offset + 2 > size) {
                return false;
            }
            const uint32_t vertex_format = command & VERTEX_FORMAT_MASK;
            const uint32_t vertex_count = read_big_endian16(list + offset);
            const uint32_t bytes = vertex_count * vertex_layout::vertex_size(vertex_format);
            offset += 2;
            if (offset + bytes > size) {
                return false;
            }
            vertex_layout::HighestIndices highest;
            highest.fill(-1);
            vertex_layout::find_highest_indices(vertex_format, list + offset, vertex_count, highest);
            add_vertex_spans(vertex_format, highest, spans);
            offset += bytes;
            continue;
        }
        switch (command & PRIMITIVE_MASK) {
            case COMMAND_LOAD_CP_REGISTER:
                offset += 5;
                continue;
            case COMMAND_LOAD_XF_REGISTERS: {
                if (offset + 4 > size) {
                    return false;
                }
                const uint32_t count = (read_big_endian32(list + offset) >> 16) + 1;
                offset += 4 + 4 * count;
                continue;
            }
            case COMMAND_CALL_DISPLAY_LIST:
                offset += 8;
                continue;
            default:
                break;
        }
        if (command >= COMMAND_LOAD_INDEXED_A && command <= COMMAND_LOAD_INDEXED_D && (command & 0x07) == 0) {
            if (offset + 4 > size) {
                return false;
            }
            // index (16 bits), length - 1 in words (4 bits), XF address (12 bits)
            const uint32_t value = read_big_endian32(list + offset);
            const uint32_t index = value >> 16;
            const uint32_t length = (((value >> 12) & 0x0Fu) + 1u) * 4u;
            const uint32_t array = vertex_layout::POSITION_MATRIX_ARRAY + (command - COMMAND_LOAD_INDEXED_A) / 8u;
            spans[array] = std::max(spans[array], index * vertex_layout::array_stride(array) + length);
            offset += 4;
            continue;
        }
        if (command == COMMAND_LOAD_BP_REGISTER) {
            offset += 4;
            continue;
        }
        runtime_log::warning("gx", "display list command 0x%02X at offset %u is not understood", command, offset - 1);
        return false;
    }
    return true;
}

// The arrays that the primitives of a recorded display list read, in the vertex layout
// that the list runs with. A primitive whose vertices do not fit that layout is left out
// (aurora reports the list as it reads it).
void add_recorded_spans(const std::vector<RecordedPrimitive>& primitives, ArraySpans& spans) {
    for (const RecordedPrimitive& primitive : primitives) {
        const uint32_t bytes = primitive.vertex_count * vertex_layout::vertex_size(primitive.vertex_format);
        if (bytes != primitive.vertices.size()) {
            continue;
        }
        vertex_layout::HighestIndices highest;
        highest.fill(-1);
        vertex_layout::find_highest_indices(primitive.vertex_format, primitive.vertices.data(), primitive.vertex_count,
                                            highest);
        add_vertex_spans(primitive.vertex_format, highest, spans);
    }
}

// Hands bytes that are not vertices of a primitive drawn right away to aurora's command
// FIFO (or to the display list it is recording), as they are.
void write_commands(const uint8_t* bytes, size_t size) {
    if (frame_interpolation::is_recording()) {
        frame_interpolation::record([commands = std::vector<uint8_t>(bytes, bytes + size)] {
            for (const uint8_t byte : commands) {
                GXCmd1u8(byte);
            }
        });
        return;
    }
    for (size_t i = 0; i < size; ++i) {
        GXCmd1u8(bytes[i]);
    }
}

// Hands a primitive drawn right away to aurora, with the arrays its indices use.
void draw_primitive(const OpenPrimitive& primitive) {
    vertex_layout::HighestIndices highest;
    highest.fill(-1);
    vertex_layout::find_highest_indices(primitive.vertex_format, primitive.vertices.data(), primitive.vertex_count,
                                        highest);
    ArraySpans spans{};
    add_vertex_spans(primitive.vertex_format, highest, spans);
    publish_arrays(spans);
    aurora::gx::fifo::submit_raw_draw(static_cast<GXPrimitive>(primitive.primitive),
                                      static_cast<GXVtxFmt>(primitive.vertex_format), primitive.vertices.data(),
                                      static_cast<uint16_t>(primitive.vertex_count), primitive.expected_bytes);
}

// Hands a display list to aurora, with the arrays its draws use.
void draw_display_list(uint32_t guest_address, uint32_t size) {
    const uint8_t* list = guest_memory::host_pointer(guest_address, size);
    ArraySpans spans{};
    const auto recorded = g_recorded_display_lists.find(guest_address);
    if (recorded != g_recorded_display_lists.end() &&
        (recorded->second.size != size || recorded->second.fingerprint != fingerprint(list, size))) {
        g_recorded_display_lists.erase(recorded);  // its memory holds another list now
        find_display_list_spans(list, size, spans);
    } else if (recorded != g_recorded_display_lists.end()) {
        add_recorded_spans(recorded->second.primitives, spans);
    } else {
        find_display_list_spans(list, size, spans);
    }
    publish_arrays(spans);
    GXCallDisplayList(list, size);
}

}  // namespace

void begin_primitive(uint32_t primitive, uint32_t vertex_format, uint32_t vertex_count) {
    finish_primitive();
    g_primitive.open = true;
    g_primitive.primitive = primitive;
    g_primitive.vertex_format = vertex_format & VERTEX_FORMAT_MASK;
    g_primitive.vertex_count = vertex_count;
    g_primitive.vertices.clear();
    if (g_recording.active) {
        // In a display list the vertices are only bytes: the GPU reads them with the
        // vertex layout of the moment the list runs, which the game may not have set yet
        // (it records the lists of its models when it loads them). aurora writes the draw
        // command, after any state the game changed before it; the vertices follow.
        g_primitive.expected_bytes = 0;
        GXBegin(static_cast<GXPrimitive>(primitive), static_cast<GXVtxFmt>(g_primitive.vertex_format),
                static_cast<u16>(vertex_count));
        return;
    }
    g_primitive.expected_bytes = vertex_count * vertex_layout::vertex_size(g_primitive.vertex_format);
}

void write_to_pipe(const uint8_t* bytes, uint32_t size) {
    if (!g_primitive.open) {
        // Not vertex data: a command that the game writes by itself. aurora's command
        // processor reads it like the GPU would.
        write_commands(bytes, size);
        return;
    }
    g_primitive.vertices.insert(g_primitive.vertices.end(), bytes, bytes + size);
    if (!g_recording.active && g_primitive.expected_bytes != 0 &&
        g_primitive.vertices.size() >= g_primitive.expected_bytes) {
        finish_primitive();
    }
}

void finish_primitive() {
    if (!g_primitive.open) {
        return;
    }
    g_primitive.open = false;
    if (g_recording.active) {
        write_commands(g_primitive.vertices.data(), g_primitive.vertices.size());
        GXEnd();
        g_recording.primitives.push_back(
            {g_primitive.vertex_format, g_primitive.vertex_count, std::move(g_primitive.vertices)});
        g_primitive.vertices = {};
        return;
    }
    if (g_primitive.expected_bytes == 0 || g_primitive.vertices.size() != g_primitive.expected_bytes) {
        runtime_log::warning("gx", "a primitive of %u vertices got %zu bytes instead of %u; it is not drawn",
                             g_primitive.vertex_count, g_primitive.vertices.size(), g_primitive.expected_bytes);
        return;
    }
    if (frame_interpolation::is_recording()) {
        frame_interpolation::mark_draw(0, g_primitive.vertex_count);
        frame_interpolation::record([primitive = g_primitive] { draw_primitive(primitive); });
        return;
    }
    draw_primitive(g_primitive);
}

void begin_display_list(uint32_t guest_address, uint32_t size) {
    finish_primitive();
    g_recording.active = true;
    g_recording.guest_address = guest_address;
    g_recording.primitives.clear();
    GXBeginDisplayList(guest_memory::host_pointer(guest_address, size), size);
}

uint32_t end_display_list() {
    finish_primitive();
    const uint32_t size = GXEndDisplayList();
    g_recording.active = false;
    const uint8_t* list = guest_memory::host_pointer(g_recording.guest_address, size);
    if (list != nullptr) {
        g_recorded_display_lists[g_recording.guest_address] = {size, fingerprint(list, size),
                                                               std::move(g_recording.primitives)};
    }
    g_recording.primitives = {};
    return size;
}

void call_display_list(uint32_t guest_address, uint32_t size) {
    finish_primitive();
    const uint8_t* list = guest_memory::host_pointer(guest_address, size);
    if (list == nullptr) {
        runtime_log::warning("gx", "display list at 0x%08X (%u bytes) is not in memory", guest_address, size);
        return;
    }
    if (frame_interpolation::is_recording()) {
        // The list and the arrays it reads stay in the game's memory until the frame is
        // drawn: the game draws from them again in the next frame.
        frame_interpolation::mark_draw(guest_address, size);
        frame_interpolation::record([=] { draw_display_list(guest_address, size); });
        return;
    }
    draw_display_list(guest_address, size);
}

bool is_building_display_list() { return g_recording.active; }

}  // namespace gx_draw
