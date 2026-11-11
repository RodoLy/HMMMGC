// The game's memory card functions (CARD), replaced by aurora's, which keep the card as a
// folder of .gci files, as Dolphin does. Addresses from config/G4AEE9/symbols.txt.
//
// Las funciones de memory card del juego (CARD), reemplazadas por las de aurora, que
// guardan la tarjeta como una carpeta de archivos .gci, como Dolphin.
//
// On the console the CARD library talks to the card in slot A over the EXI bus, block by
// block. aurora gives the same functions on top of a folder: the card in slot A is
// <player folder>/USA/Card A (player_folder.h), and each file of the card is a .gci file
// in it, in the format of Dolphin's GCI folders. The game only uses the functions that
// wait for their result, so no callback of the game is ever called. The structures it
// passes (CARDFileInfo, CARDStat) are in its memory and byte order, so they are copied
// field by field.

#include "graphics/aurora_host.h"
#include "guest_memory.h"
#include "native_functions.h"

#include <dolphin/card.h>

#include <array>
#include <cstdint>
#include <string>

namespace {

// Where the boot leaves the disc's ID (console_boot.cpp): the game code, then the maker.
// The CARD library names the game's files with them.
constexpr uint32_t DISC_ID_ADDRESS = 0x80000000u;
constexpr uint32_t GAME_CODE_LENGTH = 4;
constexpr uint32_t MAKER_CODE_LENGTH = 2;

// CARDFileInfo as the game lays it out (0x14 bytes, big-endian).
constexpr uint32_t FILE_INFO_CHANNEL = 0x00;      // s32
constexpr uint32_t FILE_INFO_FILE_NUMBER = 0x04;  // s32
constexpr uint32_t FILE_INFO_OFFSET = 0x08;       // s32: where the next read or write starts
constexpr uint32_t FILE_INFO_LENGTH = 0x0C;       // s32
constexpr uint32_t FILE_INFO_FIRST_BLOCK = 0x10;  // u16 (iBlock)

// CARDStat as the game lays it out (0x6C bytes, big-endian); aurora's has the same
// offsets in host byte order.
constexpr uint32_t STATUS_FILE_NAME = 0x00;           // char[32]
constexpr uint32_t STATUS_LENGTH = 0x20;              // u32
constexpr uint32_t STATUS_TIME = 0x24;                // u32: seconds since 2000-01-01
constexpr uint32_t STATUS_GAME_NAME = 0x28;           // u8[4]
constexpr uint32_t STATUS_COMPANY = 0x2C;             // u8[2]
constexpr uint32_t STATUS_BANNER_FORMAT = 0x2E;       // u8
constexpr uint32_t STATUS_ICON_ADDRESS = 0x30;        // u32: offset of the icon in the file
constexpr uint32_t STATUS_ICON_FORMAT = 0x34;         // u16
constexpr uint32_t STATUS_ICON_SPEED = 0x36;          // u16
constexpr uint32_t STATUS_COMMENT_ADDRESS = 0x38;     // u32: offset of the comment in the file
constexpr uint32_t STATUS_OFFSET_BANNER = 0x3C;       // u32
constexpr uint32_t STATUS_OFFSET_BANNER_TLUT = 0x40;  // u32
constexpr uint32_t STATUS_OFFSET_ICON = 0x44;         // u32[8]
constexpr uint32_t STATUS_OFFSET_ICON_TLUT = 0x64;    // u32
constexpr uint32_t STATUS_OFFSET_DATA = 0x68;         // u32

// ---- Copies between the game's memory and the host ----

std::string read_string(uint32_t guest_address, uint32_t capacity) {
    std::string text;
    for (uint32_t i = 0; i < capacity; ++i) {
        const char character = static_cast<char>(guest_memory::read8(guest_address + i));
        if (character == '\0') {
            break;
        }
        text.push_back(character);
    }
    return text;
}

void read_bytes(uint32_t guest_address, uint8_t* host, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        host[i] = guest_memory::read8(guest_address + i);
    }
}

void write_bytes(uint32_t guest_address, const uint8_t* host, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        guest_memory::write8(guest_address + i, host[i]);
    }
}

// A result (an s32) that the game passes by address; it may pass none (0).
void write_result(uint32_t guest_address, int32_t value) {
    if (guest_address != 0) {
        guest_memory::write32(guest_address, static_cast<uint32_t>(value));
    }
}

CARDFileInfo read_file_info(uint32_t guest_address) {
    CARDFileInfo file_info{};
    file_info.chan = static_cast<s32>(guest_memory::read32(guest_address + FILE_INFO_CHANNEL));
    file_info.fileNo = static_cast<s32>(guest_memory::read32(guest_address + FILE_INFO_FILE_NUMBER));
    file_info.offset = static_cast<s32>(guest_memory::read32(guest_address + FILE_INFO_OFFSET));
    file_info.length = static_cast<s32>(guest_memory::read32(guest_address + FILE_INFO_LENGTH));
    file_info.iBlock = guest_memory::read16(guest_address + FILE_INFO_FIRST_BLOCK);
    return file_info;
}

void write_file_info(uint32_t guest_address, const CARDFileInfo& file_info) {
    guest_memory::write32(guest_address + FILE_INFO_CHANNEL, static_cast<uint32_t>(file_info.chan));
    guest_memory::write32(guest_address + FILE_INFO_FILE_NUMBER, static_cast<uint32_t>(file_info.fileNo));
    guest_memory::write32(guest_address + FILE_INFO_OFFSET, static_cast<uint32_t>(file_info.offset));
    guest_memory::write32(guest_address + FILE_INFO_LENGTH, static_cast<uint32_t>(file_info.length));
    guest_memory::write16(guest_address + FILE_INFO_FIRST_BLOCK, file_info.iBlock);
}

CARDStat read_status(uint32_t guest_address) {
    CARDStat status{};
    read_bytes(guest_address + STATUS_FILE_NAME, reinterpret_cast<uint8_t*>(status.fileName), CARD_FILENAME_MAX);
    status.length = guest_memory::read32(guest_address + STATUS_LENGTH);
    status.time = guest_memory::read32(guest_address + STATUS_TIME);
    read_bytes(guest_address + STATUS_GAME_NAME, status.gameName, sizeof(status.gameName));
    read_bytes(guest_address + STATUS_COMPANY, status.company, sizeof(status.company));
    status.bannerFormat = guest_memory::read8(guest_address + STATUS_BANNER_FORMAT);
    status.iconAddr = guest_memory::read32(guest_address + STATUS_ICON_ADDRESS);
    status.iconFormat = guest_memory::read16(guest_address + STATUS_ICON_FORMAT);
    status.iconSpeed = guest_memory::read16(guest_address + STATUS_ICON_SPEED);
    status.commentAddr = guest_memory::read32(guest_address + STATUS_COMMENT_ADDRESS);
    status.offsetBanner = guest_memory::read32(guest_address + STATUS_OFFSET_BANNER);
    status.offsetBannerTlut = guest_memory::read32(guest_address + STATUS_OFFSET_BANNER_TLUT);
    for (uint32_t icon = 0; icon < CARD_ICON_MAX; ++icon) {
        status.offsetIcon[icon] = guest_memory::read32(guest_address + STATUS_OFFSET_ICON + icon * 4u);
    }
    status.offsetIconTlut = guest_memory::read32(guest_address + STATUS_OFFSET_ICON_TLUT);
    status.offsetData = guest_memory::read32(guest_address + STATUS_OFFSET_DATA);
    return status;
}

void write_status(uint32_t guest_address, const CARDStat& status) {
    write_bytes(guest_address + STATUS_FILE_NAME, reinterpret_cast<const uint8_t*>(status.fileName), CARD_FILENAME_MAX);
    guest_memory::write32(guest_address + STATUS_LENGTH, status.length);
    guest_memory::write32(guest_address + STATUS_TIME, status.time);
    write_bytes(guest_address + STATUS_GAME_NAME, status.gameName, sizeof(status.gameName));
    write_bytes(guest_address + STATUS_COMPANY, status.company, sizeof(status.company));
    guest_memory::write8(guest_address + STATUS_BANNER_FORMAT, status.bannerFormat);
    guest_memory::write32(guest_address + STATUS_ICON_ADDRESS, status.iconAddr);
    guest_memory::write16(guest_address + STATUS_ICON_FORMAT, status.iconFormat);
    guest_memory::write16(guest_address + STATUS_ICON_SPEED, status.iconSpeed);
    guest_memory::write32(guest_address + STATUS_COMMENT_ADDRESS, status.commentAddr);
    guest_memory::write32(guest_address + STATUS_OFFSET_BANNER, status.offsetBanner);
    guest_memory::write32(guest_address + STATUS_OFFSET_BANNER_TLUT, status.offsetBannerTlut);
    for (uint32_t icon = 0; icon < CARD_ICON_MAX; ++icon) {
        guest_memory::write32(guest_address + STATUS_OFFSET_ICON + icon * 4u, status.offsetIcon[icon]);
    }
    guest_memory::write32(guest_address + STATUS_OFFSET_ICON_TLUT, status.offsetIconTlut);
    guest_memory::write32(guest_address + STATUS_OFFSET_DATA, status.offsetData);
}

// ---- The natives ----

// void CARDInit(void). aurora's takes the game and maker codes that name the game's files
// (the SDK reads them from the disc's ID), and opens the card from the player's folder,
// which aurora learns when it starts: if the game gets here before GXInit, aurora starts
// now.
void CARDInitNative() {
    aurora_host::initialize();
    std::array<char, GAME_CODE_LENGTH + 1> game_code{};
    std::array<char, MAKER_CODE_LENGTH + 1> maker_code{};
    read_bytes(DISC_ID_ADDRESS, reinterpret_cast<uint8_t*>(game_code.data()), GAME_CODE_LENGTH);
    read_bytes(DISC_ID_ADDRESS + GAME_CODE_LENGTH, reinterpret_cast<uint8_t*>(maker_code.data()), MAKER_CODE_LENGTH);
    CARDSetLoadType(CARD_GCIFOLDER);
    CARDInit(game_code.data(), maker_code.data());
}
PPC_NATIVE_OVERRIDE_VOID(80189398, CARDInitNative, (), ());

// s32 CARDProbeEx(s32 channel, s32* memory_size, s32* sector_size): whether a card is in
// the slot, with its size in megabits and its sector size in bytes.
int32_t CARDProbeExNative(int32_t channel, uint32_t memory_size_address, uint32_t sector_size_address) {
    s32 memory_size = 0;
    s32 sector_size = 0;
    const s32 result = CARDProbeEx(channel, &memory_size, &sector_size);
    write_result(memory_size_address, memory_size);
    write_result(sector_size_address, sector_size);
    return result;
}
PPC_NATIVE_OVERRIDE(8018C380, CARDProbeExNative, int32_t,
                    (int32_t channel, uint32_t memory_size_address, uint32_t sector_size_address),
                    (channel, memory_size_address, sector_size_address));

// s32 CARDMount(s32 channel, void* work_area, CARDCallback detach_callback): the work area
// is memory for the SDK's own copy of the card's tables, which aurora does not need; the
// callback would be called if the card were taken out, which cannot happen.
int32_t CARDMountNative(int32_t channel, uint32_t work_area_address, uint32_t detach_callback_address) {
    return CARDMount(channel, nullptr, nullptr);
}
PPC_NATIVE_OVERRIDE(8018CC28, CARDMountNative, int32_t,
                    (int32_t channel, uint32_t work_area_address, uint32_t detach_callback_address),
                    (channel, work_area_address, detach_callback_address));

// s32 CARDUnmount(s32 channel). aurora keeps the card open while the program runs (its
// CARDUnmount is not written yet and answers that there is no card), so unmounting
// succeeds without doing anything.
int32_t CARDUnmountNative(int32_t channel) { return CARD_RESULT_READY; }
PPC_NATIVE_OVERRIDE(8018CD0C, CARDUnmountNative, int32_t, (int32_t channel), (channel));

// s32 CARDCheck(s32 channel): whether the card's file system is consistent.
int32_t CARDCheckNative(int32_t channel) { return CARDCheck(channel); }
PPC_NATIVE_OVERRIDE(8018C260, CARDCheckNative, int32_t, (int32_t channel), (channel));

// s32 CARDFormat(s32 channel): erases the whole card.
int32_t CARDFormatNative(int32_t channel) { return CARDFormat(channel); }
PPC_NATIVE_OVERRIDE(8018D554, CARDFormatNative, int32_t, (int32_t channel), (channel));

// s32 CARDFreeBlocks(s32 channel, s32* free_bytes, s32* free_files)
int32_t CARDFreeBlocksNative(int32_t channel, uint32_t free_bytes_address, uint32_t free_files_address) {
    s32 free_bytes = 0;
    s32 free_files = 0;
    const s32 result = CARDFreeBlocks(channel, &free_bytes, &free_files);
    write_result(free_bytes_address, free_bytes);
    write_result(free_files_address, free_files);
    return result;
}
PPC_NATIVE_OVERRIDE(801895A0, CARDFreeBlocksNative, int32_t,
                    (int32_t channel, uint32_t free_bytes_address, uint32_t free_files_address),
                    (channel, free_bytes_address, free_files_address));

// s32 CARDOpen(s32 channel, const char* file_name, CARDFileInfo* file_info)
int32_t CARDOpenNative(int32_t channel, uint32_t file_name_address, uint32_t file_info_address) {
    const std::string file_name = read_string(file_name_address, CARD_FILENAME_MAX);
    CARDFileInfo file_info = read_file_info(file_info_address);
    const s32 result = CARDOpen(channel, file_name.c_str(), &file_info);
    if (result == CARD_RESULT_READY) {
        write_file_info(file_info_address, file_info);
    }
    return result;
}
PPC_NATIVE_OVERRIDE(8018DA1C, CARDOpenNative, int32_t,
                    (int32_t channel, uint32_t file_name_address, uint32_t file_info_address),
                    (channel, file_name_address, file_info_address));

// s32 CARDClose(CARDFileInfo* file_info)
int32_t CARDCloseNative(uint32_t file_info_address) {
    CARDFileInfo file_info = read_file_info(file_info_address);
    return CARDClose(&file_info);
}
PPC_NATIVE_OVERRIDE(8018DB38, CARDCloseNative, int32_t, (uint32_t file_info_address), (file_info_address));

// s32 CARDCreate(s32 channel, const char* file_name, u32 size, CARDFileInfo* file_info):
// a new file of `size` bytes (whole 8 KiB blocks), left open in `file_info`.
int32_t CARDCreateNative(int32_t channel, uint32_t file_name_address, uint32_t size, uint32_t file_info_address) {
    const std::string file_name = read_string(file_name_address, CARD_FILENAME_MAX);
    CARDFileInfo file_info = read_file_info(file_info_address);
    const s32 result = CARDCreate(channel, file_name.c_str(), size, &file_info);
    if (result == CARD_RESULT_READY) {
        write_file_info(file_info_address, file_info);
    }
    return result;
}
PPC_NATIVE_OVERRIDE(8018DEE4, CARDCreateNative, int32_t,
                    (int32_t channel, uint32_t file_name_address, uint32_t size, uint32_t file_info_address),
                    (channel, file_name_address, size, file_info_address));

// s32 CARDRead(CARDFileInfo* file_info, void* buffer, s32 length, s32 offset): the bytes
// go straight into the game's buffer, as they are on the card.
int32_t CARDReadNative(uint32_t file_info_address, uint32_t buffer_address, int32_t length, int32_t offset) {
    uint8_t* buffer = guest_memory::host_pointer(buffer_address, static_cast<size_t>(length));
    if (buffer == nullptr) {
        return CARD_RESULT_FATAL_ERROR;
    }
    const CARDFileInfo file_info = read_file_info(file_info_address);
    return CARDRead(&file_info, buffer, length, offset);
}
PPC_NATIVE_OVERRIDE(8018E358, CARDReadNative, int32_t,
                    (uint32_t file_info_address, uint32_t buffer_address, int32_t length, int32_t offset),
                    (file_info_address, buffer_address, length, offset));

// s32 CARDWrite(CARDFileInfo* file_info, const void* buffer, s32 length, s32 offset)
int32_t CARDWriteNative(uint32_t file_info_address, uint32_t buffer_address, int32_t length, int32_t offset) {
    const uint8_t* buffer = guest_memory::host_pointer(buffer_address, static_cast<size_t>(length));
    if (buffer == nullptr) {
        return CARD_RESULT_FATAL_ERROR;
    }
    const CARDFileInfo file_info = read_file_info(file_info_address);
    return CARDWrite(&file_info, buffer, length, offset);
}
PPC_NATIVE_OVERRIDE(8018E6D4, CARDWriteNative, int32_t,
                    (uint32_t file_info_address, uint32_t buffer_address, int32_t length, int32_t offset),
                    (file_info_address, buffer_address, length, offset));

// s32 CARDDelete(s32 channel, const char* file_name)
int32_t CARDDeleteNative(int32_t channel, uint32_t file_name_address) {
    const std::string file_name = read_string(file_name_address, CARD_FILENAME_MAX);
    return CARDDelete(channel, file_name.c_str());
}
PPC_NATIVE_OVERRIDE(8018E8D0, CARDDeleteNative, int32_t, (int32_t channel, uint32_t file_name_address),
                    (channel, file_name_address));

// s32 CARDGetStatus(s32 channel, s32 file_number, CARDStat* status): the file's name,
// size, banner, icons and comment.
int32_t CARDGetStatusNative(int32_t channel, int32_t file_number, uint32_t status_address) {
    CARDStat status{};
    const s32 result = CARDGetStatus(channel, file_number, &status);
    if (result == CARD_RESULT_READY) {
        write_status(status_address, status);
    }
    return result;
}
PPC_NATIVE_OVERRIDE(8018EB10, CARDGetStatusNative, int32_t,
                    (int32_t channel, int32_t file_number, uint32_t status_address),
                    (channel, file_number, status_address));

// s32 CARDSetStatus(s32 channel, s32 file_number, CARDStat* status). Not named in
// symbols.txt yet: right after CARDSetStatusAsync, it calls it with __CARDSyncCallback and
// then waits in __CARDSync, as every synchronous CARD function does.
int32_t CARDSetStatusNative(int32_t channel, int32_t file_number, uint32_t status_address) {
    const CARDStat status = read_status(status_address);
    return CARDSetStatus(channel, file_number, &status);
}
PPC_NATIVE_OVERRIDE(8018ED98, CARDSetStatusNative, int32_t,
                    (int32_t channel, int32_t file_number, uint32_t status_address),
                    (channel, file_number, status_address));

}  // namespace
