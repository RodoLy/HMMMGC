// See disc_art.h.

#include "disc_art.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <span>
#include <string_view>
#include <utility>

namespace disc_art {
namespace {

constexpr uint32_t GAMECUBE_MAGIC = 0xC2339F3D;  // at 0x1C of every GameCube disc
constexpr std::string_view GAME = "G4AE";        // Harvest Moon: Magical Melody (the USA's is G4AEE9)
constexpr uint32_t TPL_MAGIC = 0x0020AF30;       // a texture palette file
constexpr uint32_t RGB5A3 = 5;                   // the logo's texel format
constexpr std::array<std::string_view, 4> LOGO = {"graphic", "Menu", "english", "title_e.tpl"};
constexpr size_t LARGEST_TABLE = 8u << 20;
constexpr size_t LARGEST_TEXTURE = 4u << 20;

uint32_t be32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 |
           static_cast<uint32_t>(p[2]) << 8 | p[3];
}
uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>(p[0] << 8 | p[1]); }

std::optional<std::vector<uint8_t>> read_at(std::ifstream& disc, uint64_t offset, size_t size) {
    std::vector<uint8_t> bytes(size);
    disc.seekg(static_cast<std::streamoff>(offset));
    if (!disc.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))) {
        return std::nullopt;
    }
    return bytes;
}

// Where a file is on the disc and how big, from the file system table: entries of 12 bytes, the
// first the root; a directory's entries follow it, up to the index its last word gives.
std::optional<std::pair<uint32_t, uint32_t>> find_file(const std::vector<uint8_t>& table,
                                                       std::span<const std::string_view> path) {
    if (table.size() < 12) {
        return std::nullopt;
    }
    const uint32_t count = be32(&table[8]);
    if (count == 0 || count > table.size() / 12) {
        return std::nullopt;
    }
    const size_t names = static_cast<size_t>(count) * 12;
    const auto name_of = [&](uint32_t entry) {
        const size_t at = names + (be32(&table[entry * 12]) & 0xFFFFFF);
        if (at >= table.size()) {
            return std::string_view();
        }
        const auto* start = reinterpret_cast<const char*>(&table[at]);
        return std::string_view(start, strnlen(start, table.size() - at));
    };
    uint32_t entry = 1;
    uint32_t end = count;  // the entries of the directory looked in are [entry, end)
    for (size_t part = 0; part < path.size(); ++part) {
        bool found = false;
        while (entry < end) {
            const bool directory = table[entry * 12] != 0;
            const uint32_t next = directory ? be32(&table[entry * 12 + 8]) : entry + 1;
            if (name_of(entry) == path[part]) {
                const bool last = part + 1 == path.size();
                if (directory == last) {
                    return std::nullopt;  // a folder where the file should be, or the other way round
                }
                if (last) {
                    return std::pair(be32(&table[entry * 12 + 4]), be32(&table[entry * 12 + 8]));
                }
                end = std::min(next, count);
                ++entry;
                found = true;
                break;
            }
            if (next <= entry) {
                return std::nullopt;  // a broken table
            }
            entry = next;
        }
        if (!found) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

// An RGB5A3 texel: opaque (top bit set) with 5 bits a color, or 3 bits of alpha and 4 a color.
void put_texel(uint8_t* out, uint16_t value) {
    if ((value & 0x8000) != 0) {
        const auto five = [](uint32_t bits) { return static_cast<uint8_t>(bits << 3 | bits >> 2); };
        out[0] = five((value >> 10) & 31);
        out[1] = five((value >> 5) & 31);
        out[2] = five(value & 31);
        out[3] = 255;
    } else {
        const uint32_t alpha = (value >> 12) & 7;
        out[0] = static_cast<uint8_t>(((value >> 8) & 15) * 17);
        out[1] = static_cast<uint8_t>(((value >> 4) & 15) * 17);
        out[2] = static_cast<uint8_t>((value & 15) * 17);
        out[3] = static_cast<uint8_t>(alpha << 5 | alpha << 2 | alpha >> 1);
    }
}

// The first picture of a texture palette file, when it is RGB5A3 (texels in tiles of 4x4).
std::optional<Picture> decode(const std::vector<uint8_t>& tpl) {
    if (tpl.size() < 12 || be32(&tpl[0]) != TPL_MAGIC || be32(&tpl[4]) == 0) {
        return std::nullopt;
    }
    const uint32_t table = be32(&tpl[8]);
    if (static_cast<size_t>(table) + 8 > tpl.size()) {
        return std::nullopt;
    }
    const uint32_t header = be32(&tpl[table]);
    if (static_cast<size_t>(header) + 12 > tpl.size()) {
        return std::nullopt;
    }
    const int height = be16(&tpl[header]);
    const int width = be16(&tpl[header + 2]);
    const uint32_t format = be32(&tpl[header + 4]);
    size_t at = be32(&tpl[header + 8]);
    const size_t tiles = static_cast<size_t>((width + 3) / 4) * static_cast<size_t>((height + 3) / 4);
    if (format != RGB5A3 || width == 0 || height == 0 || at + tiles * 32 > tpl.size()) {
        return std::nullopt;
    }
    Picture picture{width, height, std::vector<uint8_t>(static_cast<size_t>(width) * height * 4)};
    for (int tile_y = 0; tile_y < height; tile_y += 4) {
        for (int tile_x = 0; tile_x < width; tile_x += 4) {
            for (int y = tile_y; y < tile_y + 4; ++y) {
                for (int x = tile_x; x < tile_x + 4; ++x, at += 2) {
                    if (x < width && y < height) {
                        put_texel(&picture.rgba[(static_cast<size_t>(y) * width + x) * 4], be16(&tpl[at]));
                    }
                }
            }
        }
    }
    return picture;
}

// The disc's header, when it is a GameCube disc.
std::optional<std::vector<uint8_t>> disc_header(std::ifstream& disc) {
    std::optional<std::vector<uint8_t>> header = read_at(disc, 0, 0x440);
    if (!header || be32(&(*header)[0x1C]) != GAMECUBE_MAGIC) {
        return std::nullopt;
    }
    return header;
}

}  // namespace

std::optional<std::string> game_code(const std::filesystem::path& disc_image) {
    std::ifstream disc(disc_image, std::ios::binary);
    const std::optional<std::vector<uint8_t>> header = disc_header(disc);
    if (!header) {
        return std::nullopt;
    }
    std::string code(header->begin(), header->begin() + 6);
    const bool readable =
        std::ranges::all_of(code, [](char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); });
    return readable ? std::optional(code) : std::nullopt;
}

std::optional<Picture> title_logo(const std::filesystem::path& disc_image) {
    std::ifstream disc(disc_image, std::ios::binary);
    const std::optional<std::vector<uint8_t>> header = disc_header(disc);
    if (!header || std::string_view(reinterpret_cast<const char*>(header->data()), GAME.size()) != GAME) {
        return std::nullopt;
    }
    const uint32_t table_offset = be32(&(*header)[0x424]);
    const uint32_t table_size = be32(&(*header)[0x428]);
    if (table_size == 0 || table_size > LARGEST_TABLE) {
        return std::nullopt;
    }
    const std::optional<std::vector<uint8_t>> table = read_at(disc, table_offset, table_size);
    if (!table) {
        return std::nullopt;
    }
    const auto file = find_file(*table, LOGO);
    if (!file || file->second == 0 || file->second > LARGEST_TEXTURE) {
        return std::nullopt;
    }
    const std::optional<std::vector<uint8_t>> tpl = read_at(disc, file->first, file->second);
    return tpl ? decode(*tpl) : std::nullopt;
}

Picture resized(const Picture& picture, int width, int height) {
    Picture result{width, height, std::vector<uint8_t>(static_cast<size_t>(width) * height * 4)};
    const float step_x = static_cast<float>(picture.width) / static_cast<float>(width);
    const float step_y = static_cast<float>(picture.height) / static_cast<float>(height);
    for (int y = 0; y < height; ++y) {
        const float top = static_cast<float>(y) * step_y;
        const float bottom = top + step_y;
        for (int x = 0; x < width; ++x) {
            const float left = static_cast<float>(x) * step_x;
            const float right = left + step_x;
            // The colors weighed by how much of each source pixel is covered and by its alpha.
            float sum[4] = {0.f, 0.f, 0.f, 0.f};
            float covered = 0.f;
            const int last_y = std::min(picture.height, static_cast<int>(std::ceil(bottom)));
            const int last_x = std::min(picture.width, static_cast<int>(std::ceil(right)));
            for (int sy = static_cast<int>(top); sy < last_y; ++sy) {
                const float share_y = std::min(bottom, sy + 1.f) - std::max(top, static_cast<float>(sy));
                for (int sx = static_cast<int>(left); sx < last_x; ++sx) {
                    const float share = share_y * (std::min(right, sx + 1.f) - std::max(left, static_cast<float>(sx)));
                    const uint8_t* source = &picture.rgba[(static_cast<size_t>(sy) * picture.width + sx) * 4];
                    const float alpha = source[3] / 255.f;
                    for (int c = 0; c < 3; ++c) {
                        sum[c] += source[c] * alpha * share;
                    }
                    sum[3] += alpha * share;
                    covered += share;
                }
            }
            uint8_t* out = &result.rgba[(static_cast<size_t>(y) * width + x) * 4];
            const float alpha = covered > 0.f ? sum[3] / covered : 0.f;
            for (int c = 0; c < 3; ++c) {
                out[c] = sum[3] > 0.f ? static_cast<uint8_t>(std::clamp(sum[c] / sum[3], 0.f, 255.f) + 0.5f) : 0;
            }
            out[3] = static_cast<uint8_t>(alpha * 255.f + 0.5f);
        }
    }
    return result;
}

}  // namespace disc_art
