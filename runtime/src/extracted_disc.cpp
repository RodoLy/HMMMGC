// See extracted_disc.h.

#include "extracted_disc.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

// Offsets inside the disc header (boot.bin), from YAGCD 13.1.
constexpr size_t DISC_HEADER_SIZE = 0x440;
constexpr size_t DOL_OFFSET_FIELD = 0x420;
constexpr size_t FST_OFFSET_FIELD = 0x424;
constexpr size_t FST_SIZE_FIELD = 0x428;

constexpr uint64_t BOOT_INFO2_OFFSET = 0x440;
constexpr uint64_t APPLOADER_OFFSET = 0x2440;

// One entry of the file system table: 12 bytes, big-endian.
constexpr size_t FST_ENTRY_SIZE = 12;

uint32_t big_endian32(const std::vector<uint8_t>& bytes, size_t offset) {
    return (uint32_t{bytes.at(offset)} << 24) | (uint32_t{bytes.at(offset + 1)} << 16) |
           (uint32_t{bytes.at(offset + 2)} << 8) | uint32_t{bytes.at(offset + 3)};
}

std::vector<uint8_t> read_whole_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("cannot open " + path);
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

uint64_t file_size(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("cannot open " + path);
    }
    return static_cast<uint64_t>(file.tellg());
}

}  // namespace

ExtractedDisc::ExtractedDisc(const std::string& root) : root_(root) {
    const std::string sys = root + "/sys/";
    disc_header_ = read_whole_file(sys + "boot.bin");
    boot_info2_ = read_whole_file(sys + "bi2.bin");
    file_system_table_ = read_whole_file(sys + "fst.bin");
    if (disc_header_.size() != DISC_HEADER_SIZE) {
        throw std::runtime_error("boot.bin is not a disc header");
    }

    add_extent(0, disc_header_.size(), sys + "boot.bin");
    add_extent(BOOT_INFO2_OFFSET, boot_info2_.size(), sys + "bi2.bin");
    add_extent(APPLOADER_OFFSET, file_size(sys + "apploader.img"), sys + "apploader.img");
    add_extent(big_endian32(disc_header_, DOL_OFFSET_FIELD), file_size(sys + "main.dol"), sys + "main.dol");
    const uint32_t fst_offset = big_endian32(disc_header_, FST_OFFSET_FIELD);
    if (big_endian32(disc_header_, FST_SIZE_FIELD) != file_system_table_.size()) {
        throw std::runtime_error("fst.bin does not have the size the disc header gives");
    }
    add_extent(fst_offset, file_system_table_.size(), sys + "fst.bin");
    add_files_of_table(root + "/files");

    std::sort(extents_.begin(), extents_.end(),
              [](const Extent& a, const Extent& b) { return a.disc_offset < b.disc_offset; });
}

void ExtractedDisc::add_extent(uint64_t disc_offset, uint64_t size, std::string path) {
    if (size != 0) {
        extents_.push_back({disc_offset, size, std::move(path)});
    }
}

// The table is a flat list of entries: the first is the root folder, whose third word
// is the number of entries. A folder entry's third word is the index just past its last
// child, which is how the nesting is encoded; a file entry gives its disc offset and size.
// The names are zero-terminated strings after the last entry.
void ExtractedDisc::add_files_of_table(const std::string& files_root) {
    const auto& table = file_system_table_;
    const uint32_t entry_count = big_endian32(table, 8);
    const size_t names_offset = entry_count * FST_ENTRY_SIZE;

    struct OpenFolder {
        std::string path;
        uint32_t end_index;
    };
    std::vector<OpenFolder> folders{{files_root, entry_count}};

    for (uint32_t index = 1; index < entry_count; ++index) {
        while (index >= folders.back().end_index) {
            folders.pop_back();
        }
        const size_t entry = index * FST_ENTRY_SIZE;
        const uint32_t type_and_name = big_endian32(table, entry);
        const bool is_folder = (type_and_name >> 24) != 0;
        const char* name = reinterpret_cast<const char*>(&table.at(names_offset + (type_and_name & 0x00FFFFFFu)));
        const std::string path = folders.back().path + "/" + name;
        if (is_folder) {
            folders.push_back({path, big_endian32(table, entry + 8)});
        } else {
            add_extent(big_endian32(table, entry + 4), big_endian32(table, entry + 8), path);
        }
    }
}

bool ExtractedDisc::read(uint64_t offset, size_t length, uint8_t* destination) const {
    std::memset(destination, 0, length);
    const uint64_t end = offset + length;
    uint64_t end_of_last_file = 0;
    for (const Extent& extent : extents_) {
        const uint64_t extent_end = extent.disc_offset + extent.size;
        end_of_last_file = std::max(end_of_last_file, extent_end);
        if (extent_end <= offset || extent.disc_offset >= end) {
            continue;
        }
        const uint64_t from = std::max(offset, extent.disc_offset);
        const uint64_t to = std::min(end, extent_end);
        std::ifstream file(extent.path, std::ios::binary);
        file.seekg(static_cast<std::streamoff>(from - extent.disc_offset));
        file.read(reinterpret_cast<char*>(destination + (from - offset)), static_cast<std::streamsize>(to - from));
        if (!file) {
            return false;
        }
    }
    return end <= end_of_last_file;
}

std::string ExtractedDisc::file_at(uint64_t offset) const {
    for (const Extent& extent : extents_) {
        if (offset - extent.disc_offset < extent.size) {
            return extent.path.substr(root_.size() + 1);
        }
    }
    return {};
}

uint32_t ExtractedDisc::read_big_endian32(uint64_t offset) const {
    uint8_t bytes[4];
    if (!read(offset, sizeof(bytes), bytes)) {
        throw std::runtime_error("the disc ends before the word it should have at offset " + std::to_string(offset));
    }
    return (uint32_t{bytes[0]} << 24) | (uint32_t{bytes[1]} << 16) | (uint32_t{bytes[2]} << 8) | bytes[3];
}
