// The game's mods (mods/), compiled into the native runtime instead of loaded by
// ModernGekko. Each mod is the same C code; its descriptor says which functions of the
// game it patches (replaces) or hooks (runs code at), and this module runs them there.
//
// Los mods del juego, compilados dentro del runtime nativo en vez de que ModernGekko los
// cargue. Cada mod es el mismo código en C; su descriptor dice qué funciones del juego
// parcha o engancha, y este módulo los corre ahí.
//
// The translated code does not call through a table that could be changed while the game
// runs, so the places where mods act are marked in it when the runtime is built:
// tools/insert_mod_hooks.py adds a call to one of the hmmm_mod_* functions below at the
// entry of a hooked function, before each of its returns, or at a hooked instruction
// inside it. The same list of mods drives that tool and the build (runtime/CMakeLists.txt).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct CpuContext;

namespace native_mods {

// The ids of the mods compiled in.
std::vector<std::string> compiled_in();

// Reads the descriptors of the mods compiled in, checks that they are for this game, and
// runs their on_load. With `enabled`, only the mods it names are turned on (the player's
// choice, player_options.h); without it, all of them. Call once, before the game starts.
void load(const std::vector<std::string>* enabled);

// Whether a mod can be turned on and off while the game runs: the ones that only act at each
// frame of the game (the two of the character's speed). A mod that changes what the game has
// already loaded (jamie_marriage) waits for the next start.
bool switches_while_running(const std::string& id);
// Turns one of those mods on or off, from now.
void set_running(const std::string& id, bool on);
// Makes a mod read its settings again (its on_load), after the environment variable that holds
// one of them changed.
void read_settings_again(const std::string& id);

}  // namespace native_mods

// The calls that tools/insert_mod_hooks.py puts in the translated code. `address` is the
// guest address of the function or instruction, `cpu` the game's registers.
extern "C" {
// At the entry of a function: runs its entry hooks, then its patch if a mod has one.
// True when the patch returned, so the game's function must not run.
bool hmmm_mod_hook_entry(uint32_t address, CpuContext* cpu);
// Before the function returns to its caller: runs its return hooks.
void hmmm_mod_hook_return(uint32_t address, CpuContext* cpu);
// At an instruction inside a function: runs the hooks placed there.
void hmmm_mod_hook_at(uint32_t address, CpuContext* cpu);
}
