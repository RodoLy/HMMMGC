// The game disc, read from its extracted form (sys/ and files/, as tools/extract_disc.py
// and Dolphin write it) as if it were the disc image.
//
// El disco del juego, leído de su forma extraída como si fuera la imagen del disco.
//
// The game finds its files through the file system table (FST) and then asks the drive
// for bytes at disc offsets. The table says at which offset each file starts, so a read
// of an offset is a read of the file that holds it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class ExtractedDisc {
public:
    // Reads the layout of the disc under `root`. Throws std::runtime_error when the
    // system files are missing or malformed.
    explicit ExtractedDisc(const std::string& root);

    // Copies `length` bytes from disc offset `offset` into `destination`; bytes that
    // belong to no file (padding between files) read as zero. Returns false when part
    // of the range is past the end of every file.
    bool read(uint64_t offset, size_t length, uint8_t* destination) const;

    // The big-endian word at disc offset `offset`. Throws std::runtime_error past the end.
    uint32_t read_big_endian32(uint64_t offset) const;

    // The path, relative to the disc root, of the file that holds disc offset `offset`
    // (empty when no file does). For logs.
    std::string file_at(uint64_t offset) const;

    const std::vector<uint8_t>& file_system_table() const { return file_system_table_; }
    const std::vector<uint8_t>& boot_info2() const { return boot_info2_; }
    const std::vector<uint8_t>& disc_header() const { return disc_header_; }

private:
    struct Extent {
        uint64_t disc_offset;
        uint64_t size;
        std::string path;  // file on the host that holds these bytes
    };

    void add_extent(uint64_t disc_offset, uint64_t size, std::string path);
    void add_files_of_table(const std::string& files_root);

    std::string root_;
    std::vector<Extent> extents_;  // sorted by disc offset
    std::vector<uint8_t> disc_header_;
    std::vector<uint8_t> boot_info2_;
    std::vector<uint8_t> file_system_table_;
};
