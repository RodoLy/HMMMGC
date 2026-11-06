// See guest_symbols.h.

#include "guest_symbols.h"

#include <dlfcn.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <vector>

namespace guest_symbols {
namespace {

struct FunctionSymbol {
    uint32_t address;
    uint32_t size;
    std::string name;
};

std::vector<FunctionSymbol> g_functions;  // sorted by address

}  // namespace

bool load(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    // One line per symbol, in the format of decomp-toolkit:
    //   OSInit = .text:0x8016EF2C; // type:function size:0x4E0
    const std::regex function_line(R"(^(\S+) = \.\w+:0x([0-9A-Fa-f]{8}); // type:function size:0x([0-9A-Fa-f]+))");
    std::string line;
    std::smatch match;
    while (std::getline(file, line)) {
        if (std::regex_search(line, match, function_line)) {
            g_functions.push_back({static_cast<uint32_t>(std::stoul(match[2].str(), nullptr, 16)),
                                   static_cast<uint32_t>(std::stoul(match[3].str(), nullptr, 16)), match[1].str()});
        }
    }
    std::sort(g_functions.begin(), g_functions.end(),
              [](const FunctionSymbol& a, const FunctionSymbol& b) { return a.address < b.address; });
    return !g_functions.empty();
}

std::string describe(uint32_t guest_address) {
    char text[96];
    const auto after =
        std::upper_bound(g_functions.begin(), g_functions.end(), guest_address,
                         [](uint32_t address, const FunctionSymbol& symbol) { return address < symbol.address; });
    if (after != g_functions.begin()) {
        const FunctionSymbol& function = *(after - 1);
        const uint32_t offset = guest_address - function.address;
        if (offset < function.size) {
            if (offset == 0) {
                return function.name;
            }
            std::snprintf(text, sizeof(text), "%s+0x%X", function.name.c_str(), offset);
            return text;
        }
    }
    std::snprintf(text, sizeof(text), "0x%08X", guest_address);
    return text;
}

std::string describe_host_code(const void* host_address) {
    Dl_info info{};
    if (host_address == nullptr || dladdr(host_address, &info) == 0 || info.dli_sname == nullptr) {
        return "(unknown code)";
    }
    // func_8016EF2C, and its specialised copies such as func_8016EF2C_statefree.
    constexpr char TRANSLATED_PREFIX[] = "func_";
    if (std::strncmp(info.dli_sname, TRANSLATED_PREFIX, sizeof(TRANSLATED_PREFIX) - 1) == 0) {
        const uint32_t guest_address =
            static_cast<uint32_t>(std::strtoul(info.dli_sname + sizeof(TRANSLATED_PREFIX) - 1, nullptr, 16));
        return describe(guest_address);
    }
    return info.dli_sname;
}

}  // namespace guest_symbols
