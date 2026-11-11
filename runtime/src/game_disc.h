// The game disc, read as the drive reads it (bytes at disc offsets), from a disc image
// (.iso or .gcm: the plain copy of the disc that Dolphin and other tools make) or from its
// extracted form (sys/ and files/, as tools/extract_disc.py and Dolphin write it).
//
// El disco del juego, leído como lo lee la unidad (bytes en desplazamientos del disco),
// de una imagen del disco (.iso o .gcm: la copia tal cual del disco que hacen Dolphin y
// otras herramientas) o de su forma extraída (sys/ y files/).
//
// The game finds its files through the file system table (FST) and then asks the drive
// for bytes at disc offsets. An image holds every byte at its own offset. In the extracted
// form the table says at which offset each file starts, so a read of an offset is a read
// of the file that holds it. Compressed images (RVZ, WIA, GCZ, CISO) are not read: Dolphin
// converts them to ISO (right click on the game > Convert File).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class GameDisc {
public:
    // Reads the layout of the disc at `path`: an image file, or the folder of an extracted
    // disc. Throws std::runtime_error when it is neither, or when its system files are
    // missing or malformed.
    explicit GameDisc(const std::string& path);

    // Copies `length` bytes from disc offset `offset` into `destination`; bytes that
    // belong to no file (padding between files) read as zero. Returns false when part
    // of the range is past the end of the disc.
    bool read(uint64_t offset, size_t length, uint8_t* destination) const;

    // The big-endian word at disc offset `offset`. Throws std::runtime_error past the end.
    uint32_t read_big_endian32(uint64_t offset) const;

    // The path on the disc ("sys/main.dol", "files/...") of the file that holds disc
    // offset `offset` (empty when no file does). For logs.
    std::string file_at(uint64_t offset) const;

    const std::vector<uint8_t>& file_system_table() const { return file_system_table_; }
    const std::vector<uint8_t>& boot_info2() const { return boot_info2_; }
    const std::vector<uint8_t>& disc_header() const { return disc_header_; }

private:
    // Bytes of the disc: where they start and how many there are, the file on the host
    // that holds them from its first byte on, and their path on the disc.
    struct Extent {
        uint64_t disc_offset;
        uint64_t size;
        std::string host_path;
        std::string disc_path;
    };

    void read_image(const std::string& image);
    void read_extracted(const std::string& root);
    void check_system_files() const;
    // The files of the table, as (offset, size, path on the disc); `files_root` is where
    // an extracted disc keeps them, empty for an image.
    void add_files_of_table(const std::string& files_root);
    static void add_extent(std::vector<Extent>& extents, Extent extent);

    // What reads come from: the image, or each file of an extracted disc. Sorted by offset.
    std::vector<Extent> sources_;
    // The files of the disc, for file_at. Sorted by offset.
    std::vector<Extent> files_;
    std::vector<uint8_t> disc_header_;
    std::vector<uint8_t> boot_info2_;
    std::vector<uint8_t> file_system_table_;
};
