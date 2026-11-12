// See game_disc.h.

#include "game_disc.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

// Offsets inside the disc header (boot.bin), from YAGCD 13.1.
constexpr size_t DISC_HEADER_SIZE = 0x440;
constexpr size_t DISC_MAGIC_FIELD = 0x1C;
constexpr uint32_t GAMECUBE_DISC_MAGIC = 0xC2339F3Du;
constexpr size_t DOL_OFFSET_FIELD = 0x420;
constexpr size_t FST_OFFSET_FIELD = 0x424;
constexpr size_t FST_SIZE_FIELD = 0x428;

constexpr uint64_t BOOT_INFO2_OFFSET = 0x440;
constexpr size_t BOOT_INFO2_SIZE = 0x2000;
constexpr uint64_t APPLOADER_OFFSET = 0x2440;
// The apploader's header: after the date, the size of its code and then the size of its
// trailer, which follow the header.
constexpr size_t APPLOADER_HEADER_SIZE = 0x20;
constexpr uint64_t APPLOADER_SIZES_FIELD = APPLOADER_OFFSET + 0x14;

// A DOL's header: the file offsets of its 7 text and 11 data sections, then their
// addresses, then their sizes.
constexpr size_t DOL_SECTION_COUNT = 18;
constexpr size_t DOL_SIZES_FIELD = 0x90;
constexpr size_t DOL_HEADER_SIZE = 0x100;

// One entry of the file system table: 12 bytes, big-endian.
constexpr size_t FST_ENTRY_SIZE = 12;

// The first bytes of the compressed disc images that Dolphin also writes, to say what
// to do with one.
struct CompressedFormat {
    std::array<uint8_t, 4> magic;
    const char* name;
};
constexpr std::array<CompressedFormat, 5> COMPRESSED_FORMATS = {{
    {{'R', 'V', 'Z', 0x01}, "RVZ"},
    {{'W', 'I', 'A', 0x01}, "WIA"},
    {{'C', 'I', 'S', 'O'}, "CISO"},
    {{0x01, 0xC0, 0x0B, 0xB1}, "GCZ"},
    {{'W', 'B', 'F', 'S'}, "WBFS"},
}};

uint32_t big_endian32(const uint8_t* bytes) {
    return (uint32_t{bytes[0]} << 24) | (uint32_t{bytes[1]} << 16) | (uint32_t{bytes[2]} << 8) | uint32_t{bytes[3]};
}

uint32_t big_endian32(const std::vector<uint8_t>& bytes, size_t offset) {
    if (offset + 4 > bytes.size()) {
        throw std::out_of_range("a word past the end of the data");
    }
    return big_endian32(bytes.data() + offset);
}

std::vector<uint8_t> read_whole_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot open " + path);
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::vector<uint8_t> read_file_part(const std::string& path, uint64_t offset, size_t length) {
    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> bytes(length);
    file.seekg(static_cast<std::streamoff>(offset));
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length));
    if (!file) {
        throw std::runtime_error(path + " ends before offset " + std::to_string(offset + length));
    }
    return bytes;
}

uint64_t file_size(const std::string& path) {
    std::error_code error;
    const uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        throw std::runtime_error("cannot open " + path);
    }
    return size;
}

// How many bytes of the file a DOL takes, from its header: up to the end of its last section.
uint64_t dol_size(const std::vector<uint8_t>& header) {
    uint64_t end = DOL_HEADER_SIZE;
    for (size_t section = 0; section < DOL_SECTION_COUNT; ++section) {
        const uint32_t offset = big_endian32(header, section * 4);
        const uint32_t size = big_endian32(header, DOL_SIZES_FIELD + section * 4);
        if (size != 0) {
            end = std::max<uint64_t>(end, uint64_t{offset} + size);
        }
    }
    return end;
}

}  // namespace

GameDisc::GameDisc(const std::string& path) {
    std::error_code error;
    if (std::filesystem::is_directory(path, error)) {
        read_extracted(path);
    } else if (std::filesystem::is_regular_file(path, error)) {
        read_image(path);
    } else {
        throw std::runtime_error("there is no disc image or extracted disc at " + path);
    }
    const auto by_offset = [](const Extent& a, const Extent& b) { return a.disc_offset < b.disc_offset; };
    std::sort(sources_.begin(), sources_.end(), by_offset);
    std::sort(files_.begin(), files_.end(), by_offset);
}

void GameDisc::read_image(const std::string& image) {
    const uint64_t size = file_size(image);
    if (size < APPLOADER_OFFSET) {
        throw std::runtime_error(image + " is too small to be a GameCube disc image");
    }
    disc_header_ = read_file_part(image, 0, DISC_HEADER_SIZE);
    if (big_endian32(disc_header_, DISC_MAGIC_FIELD) != GAMECUBE_DISC_MAGIC) {
        for (const CompressedFormat& format : COMPRESSED_FORMATS) {
            if (std::equal(format.magic.begin(), format.magic.end(), disc_header_.begin())) {
                throw std::runtime_error(image + " is a compressed disc image (" + format.name +
                                         "): convert it to ISO with Dolphin (right click on the game > Convert File)");
            }
        }
        throw std::runtime_error(image + " is not a GameCube disc image");
    }
    boot_info2_ = read_file_part(image, BOOT_INFO2_OFFSET, BOOT_INFO2_SIZE);
    const uint32_t fst_offset = big_endian32(disc_header_, FST_OFFSET_FIELD);
    const uint32_t fst_size = big_endian32(disc_header_, FST_SIZE_FIELD);
    if (uint64_t{fst_offset} + fst_size > size) {
        throw std::runtime_error(image + " ends before its file system table (a cut copy of the disc?)");
    }
    file_system_table_ = read_file_part(image, fst_offset, fst_size);
    check_system_files();

    add_extent(sources_, {0, size, image, ""});
    const std::vector<uint8_t> apploader = read_file_part(image, APPLOADER_SIZES_FIELD, 8);
    const uint64_t apploader_size = APPLOADER_HEADER_SIZE + uint64_t{big_endian32(apploader, 0)} + big_endian32(apploader, 4);
    const uint32_t dol_offset = big_endian32(disc_header_, DOL_OFFSET_FIELD);
    add_extent(files_, {0, DISC_HEADER_SIZE, image, "sys/boot.bin"});
    add_extent(files_, {BOOT_INFO2_OFFSET, BOOT_INFO2_SIZE, image, "sys/bi2.bin"});
    add_extent(files_, {APPLOADER_OFFSET, apploader_size, image, "sys/apploader.img"});
    add_extent(files_, {dol_offset, dol_size(read_file_part(image, dol_offset, DOL_HEADER_SIZE)), image, "sys/main.dol"});
    add_extent(files_, {fst_offset, fst_size, image, "sys/fst.bin"});
    add_files_of_table("");
}

void GameDisc::read_extracted(const std::string& root) {
    const std::string sys = root + "/sys/";
    disc_header_ = read_whole_file(sys + "boot.bin");
    boot_info2_ = read_whole_file(sys + "bi2.bin");
    file_system_table_ = read_whole_file(sys + "fst.bin");
    if (disc_header_.size() != DISC_HEADER_SIZE) {
        throw std::runtime_error("boot.bin is not a disc header");
    }
    if (big_endian32(disc_header_, FST_SIZE_FIELD) != file_system_table_.size()) {
        throw std::runtime_error("fst.bin does not have the size the disc header gives");
    }
    check_system_files();

    const auto add_system_file = [&](uint64_t disc_offset, uint64_t size, const char* name) {
        add_extent(files_, {disc_offset, size, sys + name, std::string("sys/") + name});
    };
    add_system_file(0, disc_header_.size(), "boot.bin");
    add_system_file(BOOT_INFO2_OFFSET, boot_info2_.size(), "bi2.bin");
    add_system_file(APPLOADER_OFFSET, file_size(sys + "apploader.img"), "apploader.img");
    add_system_file(big_endian32(disc_header_, DOL_OFFSET_FIELD), file_size(sys + "main.dol"), "main.dol");
    add_system_file(big_endian32(disc_header_, FST_OFFSET_FIELD), file_system_table_.size(), "fst.bin");
    add_files_of_table(root + "/files");
    // Each file holds its own bytes.
    sources_ = files_;
}

void GameDisc::check_system_files() const {
    if (file_system_table_.size() < FST_ENTRY_SIZE ||
        uint64_t{big_endian32(file_system_table_, 8)} * FST_ENTRY_SIZE > file_system_table_.size()) {
        throw std::runtime_error("the disc's file system table is malformed");
    }
}

void GameDisc::add_extent(std::vector<Extent>& extents, Extent extent) {
    if (extent.size != 0) {
        extents.push_back(std::move(extent));
    }
}

// The table is a flat list of entries: the first is the root folder, whose third word
// is the number of entries. A folder entry's third word is the index just past its last
// child, which is how the nesting is encoded; a file entry gives its disc offset and size.
// The names are zero-terminated strings after the last entry.
void GameDisc::add_files_of_table(const std::string& files_root) {
    const auto& table = file_system_table_;
    const uint32_t entry_count = big_endian32(table, 8);
    const size_t names_offset = entry_count * FST_ENTRY_SIZE;

    struct OpenFolder {
        std::string path;  // on the disc
        uint32_t end_index;
    };
    std::vector<OpenFolder> folders{{"files", entry_count}};

    for (uint32_t index = 1; index < entry_count; ++index) {
        while (index >= folders.back().end_index) {
            folders.pop_back();
        }
        const size_t entry = index * FST_ENTRY_SIZE;
        const uint32_t type_and_name = big_endian32(table, entry);
        const bool is_folder = (type_and_name >> 24) != 0;
        const size_t name_offset = names_offset + (type_and_name & 0x00FFFFFFu);
        if (name_offset >= table.size()) {
            throw std::runtime_error("the disc's file system table is malformed");
        }
        const char* name = reinterpret_cast<const char*>(&table[name_offset]);
        const std::string path = folders.back().path + "/" + std::string(name, strnlen(name, table.size() - name_offset));
        if (is_folder) {
            folders.push_back({path, big_endian32(table, entry + 8)});
        } else {
            // "files/x" on the disc is <root>/x in an extracted disc.
            const std::string host_path = files_root.empty() ? "" : files_root + path.substr(std::strlen("files"));
            add_extent(files_, {big_endian32(table, entry + 4), big_endian32(table, entry + 8), host_path, path});
        }
    }
}

bool GameDisc::read(uint64_t offset, size_t length, uint8_t* destination) const {
    std::memset(destination, 0, length);
    const uint64_t end = offset + length;
    uint64_t end_of_last_source = 0;
    for (const Extent& extent : sources_) {
        const uint64_t extent_end = extent.disc_offset + extent.size;
        end_of_last_source = std::max(end_of_last_source, extent_end);
        if (extent_end <= offset || extent.disc_offset >= end) {
            continue;
        }
        const uint64_t from = std::max(offset, extent.disc_offset);
        const uint64_t to = std::min(end, extent_end);
        std::ifstream file(extent.host_path, std::ios::binary);
        file.seekg(static_cast<std::streamoff>(from - extent.disc_offset));
        file.read(reinterpret_cast<char*>(destination + (from - offset)), static_cast<std::streamsize>(to - from));
        if (!file) {
            return false;
        }
    }
    return end <= end_of_last_source;
}

bool GameDisc::has_file(const std::string& disc_path) const {
    const auto same = [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    };
    return std::ranges::any_of(files_, [&](const Extent& extent) {
        return std::ranges::equal(extent.disc_path, disc_path, same);
    });
}

std::string GameDisc::file_at(uint64_t offset) const {
    for (const Extent& extent : files_) {
        if (offset - extent.disc_offset < extent.size) {
            return extent.disc_path;
        }
    }
    return {};
}

uint32_t GameDisc::read_big_endian32(uint64_t offset) const {
    uint8_t bytes[4];
    if (!read(offset, sizeof(bytes), bytes)) {
        throw std::runtime_error("the disc ends before the word it should have at offset " + std::to_string(offset));
    }
    return big_endian32(bytes);
}
