// Names of the game's functions, read from the project's symbol list
// (config/G4AEE9/symbols.txt), so that logs can say "OSInit+0x40" instead of an address.
//
// Nombres de las funciones del juego, leídos de la lista de símbolos del proyecto, para
// que el registro diga "OSInit+0x40" en vez de una dirección.
#pragma once

#include <cstdint>
#include <string>

namespace guest_symbols {

// Reads `path`; returns false (and the runtime simply logs addresses) when it cannot.
bool load(const std::string& path);

// "name+0xOFFSET" for an address inside a known function, "0xADDRESS" otherwise.
std::string describe(uint32_t guest_address);

// The game function whose translation contains the host code address `host_address`
// (translated functions are named func_XXXXXXXX after their guest address), or the host
// symbol when it is runtime code. Needs the program linked with -rdynamic.
std::string describe_host_code(const void* host_address);

}  // namespace guest_symbols
