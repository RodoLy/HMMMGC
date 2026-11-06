// The remaining symbols of the guest ABI: the graphics gather pipe, and a few hooks that
// WiiCompiled's headers declare for Mario Kart Wii and that mean nothing for this game.
//
// Los símbolos que faltan de la ABI: el "gather pipe" de gráficos, y unos ganchos que los
// encabezados de WiiCompiled declaran para Mario Kart Wii y que no significan nada aquí.

#include "fault_handler.h"
#include "hardware/hardware_registers.h"
#include "runtime_log.h"

#include "guest_abi/guest_abi.h"
#include "isa/ppc_isa_context.h"

#include <cstring>
#include <string_view>

// ---- The write-gather pipe (0xCC008000) ----
//
// The game sends graphics commands by storing them, one value at a time, to a single
// address; the hardware gathers them into the command FIFO in main memory. The
// translator turns those stores into these calls, in program order and big-endian.

namespace {
void send_to_gather_pipe(const uint8_t* bytes, uint32_t size) { hardware_registers::write_to_gather_pipe(bytes, size); }
}  // namespace

extern "C" void GX_HLE_FIFO_Write8(uint8_t value) { send_to_gather_pipe(&value, 1); }

extern "C" void GX_HLE_FIFO_Write16(uint16_t value) {
    const uint8_t bytes[2] = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    send_to_gather_pipe(bytes, sizeof(bytes));
}

extern "C" void GX_HLE_FIFO_Write32(uint32_t value) {
    const uint8_t bytes[4] = {static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
                              static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    send_to_gather_pipe(bytes, sizeof(bytes));
}

extern "C" void GX_HLE_FIFO_WriteFloat(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    GX_HLE_FIFO_Write32(bits);
}

// `data` is already in guest (big-endian) byte order.
extern "C" void GX_HLE_FIFO_WriteBurst(const uint8_t* data, uint32_t size_bytes) {
    send_to_gather_pipe(data, size_bytes);
}

// ---- Hooks of WiiCompiled's headers that only Mario Kart Wii uses ----

// The region of the translated game, used by WiiCompiled to move Mario Kart Wii's fixed
// addresses between its versions. 0 means "addresses as they are", which is what a game
// other than Mario Kart Wii needs.
extern "C" const uint8_t kMkwGuestRegion = 0;

// Mario Kart Wii's renderer option hook (dispatch.h calls it for one address only, which
// is no function of this game).
uint32_t FilterScnRendererPathMaskOption(uint32_t path_mask) noexcept { return path_mask; }

// Read by the translated code where the host page size is not known at compile time
// (AArch64 outside Windows). The flat view needs no per-page checks: MEM1 is plain
// memory and the register window is 64 KiB-aligned.
#if !defined(MKW_GUEST_FLAT_FIXED_PAGE_SIZE)
namespace GuestFlat {
bool g_requiresCheckedAccess = false;
}
#endif

// ---- Fatal error hooks of the ISA helpers ----

void ShowRuntimeFatalPopup(std::string_view category, std::string_view details) noexcept {
    runtime_log::error("runtime", "%.*s: %.*s", static_cast<int>(category.size()), category.data(),
                       static_cast<int>(details.size()), details.data());
}

extern "C" void DumpHostStackTraceForRuntimeHelper() { fault_handler::print_guest_location(); }

void MarkFatalErrorReported() {}
