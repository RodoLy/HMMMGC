// See native_mods.h.

#include "mods/native_mods.h"

#include "guest_memory.h"
#include "runtime_log.h"

#include "guest_abi/guest_abi.h"
#include "moderngekko/mod_abi.h"

// COMPILED_IN_MODS, written by runtime/CMakeLists.txt: each mod's moderngekko_get_mod(),
// renamed after the mod, and a null pointer at the end.
#include "native_mod_list.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace native_mods {
namespace {

constexpr char GAME_ID[] = "G4AEE9";

struct NamedFunction {
    const char* mod_id;
    ModernGekkoModFunction function;
};

struct HooksAtAddress {
    std::vector<NamedFunction> on_entry;  // entry hooks, and hooks at an instruction
    std::vector<NamedFunction> on_return;
    NamedFunction patch{nullptr, nullptr};
    bool patch_is_forced = false;
    bool reached = false;  // the game has been here: logged once, to show that the mods run
};

std::unordered_map<uint32_t, HooksAtAddress> g_hooks;

// The mods that can be turned on and off while the game runs (native_mods.h): their hooks are
// always in place, and are skipped while the mod is off.
constexpr const char* SWITCHED_WHILE_RUNNING[] = {"player_speed", "carry_speed"};
std::vector<std::string> g_switched_off;

bool is_switched_off(const char* mod_id) {
    return std::find(g_switched_off.begin(), g_switched_off.end(), mod_id) != g_switched_off.end();
}

// Mods that a save can depend on. Once the player married Jamie with jamie_marriage on, the
// save needs it (without it Jamie does not live in the house nor talk as the spouse, and the
// game would go on as if the ending had never come); once the player is engaged or married to a
// partner of the player's own sex with marry_anyone on, the save needs that one (without it the
// wedding would never come, and the pregnancy scenes would start). Turned off by the player, such
// a mod is loaded all the same, and its hooks run only while the loaded save needs it; with any
// other save it does nothing, as if it were off.
//
// Mods de los que puede depender una partida: casado con Jamie, o comprometido o casado con una
// pareja del mismo sexo, la partida sigue usando el mod aunque el jugador lo haya desactivado; con
// cualquier otra partida no hace nada.
constexpr const char* JAMIE_MARRIAGE = "jamie_marriage";
constexpr const char* MARRY_ANYONE = "marry_anyone";
// Where the game keeps its save data (a pointer, 0 until a game is loaded), and in it the
// player's character (0 the boy, 1 the girl) and the numbers of the spouse's and the fiance's
// characters: Jamie is character 2, the boy's partners 3 to 12 and the girl's 13 to 22
// (mods/jamie_marriage/mod.c, mods/marry_anyone/mod.c).
constexpr uint32_t SAVE_DATA_POINTER = 0x80321F48u;
constexpr uint32_t PLAYER_CHARACTER_OFFSET = 0x227Cu;
constexpr uint32_t SPOUSE_OFFSET = 0x22B8u;
constexpr uint32_t FIANCE_OFFSET = 0x2330u;
constexpr uint32_t JAMIE_CHARACTER_NUMBER = 2u;
constexpr uint32_t FIRST_PARTNER_OF_THE_BOY = 3u;
constexpr uint32_t FIRST_PARTNER_OF_THE_GIRL = 13u;
constexpr uint32_t LAST_PARTNER = 22u;

struct KeptForSaves {
    const char* mod_id;
    bool (*save_needs_it)(uint32_t save_data);
    const char* needed;      // what the log says when a save needs the mod
    const char* not_needed;  // and when it does not
    bool kept = false;       // the player turned it off, and it was loaded for the saves
    bool was_needed = false;
};

bool married_to_jamie(uint32_t save_data) {
    return guest_memory::read32(save_data + SPOUSE_OFFSET) == JAMIE_CHARACTER_NUMBER;
}

bool is_of_the_players_sex(uint32_t save_data, uint32_t partner) {
    if (partner < FIRST_PARTNER_OF_THE_BOY || partner > LAST_PARTNER) {
        return false;
    }
    const bool partner_is_for_the_boy = partner < FIRST_PARTNER_OF_THE_GIRL;
    const bool player_is_the_boy = guest_memory::read32(save_data + PLAYER_CHARACTER_OFFSET) == 0;
    return partner_is_for_the_boy != player_is_the_boy;
}

bool engaged_or_married_to_the_same_sex(uint32_t save_data) {
    return is_of_the_players_sex(save_data, guest_memory::read32(save_data + SPOUSE_OFFSET)) ||
           is_of_the_players_sex(save_data, guest_memory::read32(save_data + FIANCE_OFFSET));
}

KeptForSaves g_kept_for_saves[] = {
    {JAMIE_MARRIAGE, married_to_jamie, "this save is married to Jamie: jamie_marriage runs for it",
     "this save is not married to Jamie: jamie_marriage stays off"},
    {MARRY_ANYONE, engaged_or_married_to_the_same_sex,
     "this save is engaged or married to a partner of the player's sex: marry_anyone runs for it",
     "this save is not engaged or married to a partner of the player's sex: marry_anyone stays off"},
};

KeptForSaves* kept_for_saves(const char* mod_id) {
    for (KeptForSaves& kept : g_kept_for_saves) {
        if (std::strcmp(kept.mod_id, mod_id) == 0) {
            return &kept;
        }
    }
    return nullptr;
}

// Whether a hook of the mod runs now.
bool hook_runs(const char* mod_id) {
    if (!g_switched_off.empty() && is_switched_off(mod_id)) {
        return false;
    }
    KeptForSaves* kept = kept_for_saves(mod_id);
    if (kept != nullptr && kept->kept) {
        const uint32_t save_data = guest_memory::read32(SAVE_DATA_POINTER);
        const bool needed = guest_memory::is_cached_main_memory(save_data) && kept->save_needs_it(save_data);
        if (needed != kept->was_needed) {
            kept->was_needed = needed;
            runtime_log::info("mods", "%s", needed ? kept->needed : kept->not_needed);
        }
        return needed;
    }
    return true;
}

const ModernGekkoModHostApi& host_api();

// ---- The mod's view of the game: CPUState ----

uint64_t read_guest(CPUState*, uint32_t address, uint8_t size) {
    switch (size) {
        case 1:
            return guest_memory::read8(address);
        case 2:
            return guest_memory::read16(address);
        case 4:
            return guest_memory::read32(address);
        case 8:
            return (uint64_t{guest_memory::read32(address)} << 32) | guest_memory::read32(address + 4);
        default:
            return 0;
    }
}

void write_guest(CPUState*, uint32_t address, uint64_t value, uint8_t size) {
    switch (size) {
        case 1:
            guest_memory::write8(address, static_cast<uint8_t>(value));
            return;
        case 2:
            guest_memory::write16(address, static_cast<uint16_t>(value));
            return;
        case 4:
            guest_memory::write32(address, static_cast<uint32_t>(value));
            return;
        case 8:
            guest_memory::write64(address, value);
            return;
        default:
            return;
    }
}

CPUState state_from(const CpuContext& cpu, uint32_t address) {
    CPUState state{};
    std::memcpy(state.gpr, cpu.gpr, sizeof(state.gpr));
    for (int i = 0; i < 32; ++i) {
        state.fpr[i] = cpu.fpr[i].d;
    }
    state.pc = address;
    state.lr = cpu.lr;
    state.ctr = cpu.ctr;
    state.cr = cpu.cr;
    state.xer = cpu.xer;
    state.external_read = read_guest;
    state.external_write = write_guest;
    return state;
}

// Copies back what the mod changed. A floating-point register is only written when the
// mod changed it, so that the paired-single half the mod cannot see stays as it was.
void copy_back(const CPUState& state, CpuContext& cpu) {
    std::memcpy(cpu.gpr, state.gpr, sizeof(state.gpr));
    for (int i = 0; i < 32; ++i) {
        if (std::memcmp(&state.fpr[i], &cpu.fpr[i].d, sizeof(double)) != 0) {
            cpu.fpr[i].d = state.fpr[i];
        }
    }
    cpu.lr = state.lr;
    cpu.ctr = state.ctr;
    cpu.cr = state.cr;
    cpu.xer = state.xer;
}

void log_first_visit(HooksAtAddress& hooks, uint32_t address) {
    if (!hooks.reached) {
        hooks.reached = true;
        runtime_log::info("mods", "the game reached 0x%08X, where mods act", address);
    }
}

void run(const std::vector<NamedFunction>& functions, uint32_t address, CpuContext& cpu) {
    if (functions.empty()) {
        return;
    }
    CPUState state = state_from(cpu, address);
    for (const NamedFunction& hook : functions) {
        if (hook_runs(hook.mod_id)) {
            hook.function(&state);
        }
    }
    copy_back(state, cpu);
}

// ---- Loading ----

void add_patch(const char* mod_id, const ModernGekkoModPatch& patch) {
    HooksAtAddress& hooks = g_hooks[patch.address];
    const bool forced = (patch.flags & MODERNGEKKO_MOD_PATCH_FORCE) != 0;
    if (hooks.patch.function != nullptr && (!forced || hooks.patch_is_forced)) {
        // As in ModernGekko: the second patch of a function is refused, unless forced.
        runtime_log::warning("mods", "%s: 0x%08X is already patched by %s; its patch is not used", mod_id,
                             patch.address, hooks.patch.mod_id);
        return;
    }
    hooks.patch = {mod_id, patch.function};
    hooks.patch_is_forced = forced;
}

void add_hook(const char* mod_id, const ModernGekkoModHook& hook) {
    HooksAtAddress& hooks = g_hooks[hook.address];
    auto& list = hook.kind == MODERNGEKKO_MOD_HOOK_RETURN ? hooks.on_return : hooks.on_entry;
    list.push_back({mod_id, hook.function});
}

int no_event(void*, const char*, const char*, CPUState*) { return 0; }

ModernGekkoModFunction no_export(void*, const char*, const char*) { return nullptr; }

const ModernGekkoModHostApi& host_api() {
    static const ModernGekkoModHostApi HOST_API{MODERNGEKKO_MOD_HOST_ABI_VERSION, nullptr, no_event, no_export};
    return HOST_API;
}

const ModernGekkoModDesc* find_mod(const std::string& id) {
    for (const ModernGekkoGetModFn* get_mod = COMPILED_IN_MODS; *get_mod != nullptr; ++get_mod) {
        const ModernGekkoModDesc* mod = (*get_mod)();
        if (mod != nullptr && mod->id != nullptr && id == mod->id) {
            return mod;
        }
    }
    return nullptr;
}

}  // namespace

bool switches_while_running(const std::string& id) {
    return std::find(std::begin(SWITCHED_WHILE_RUNNING), std::end(SWITCHED_WHILE_RUNNING), id) !=
           std::end(SWITCHED_WHILE_RUNNING);
}

void set_running(const std::string& id, bool on) {
    if (!switches_while_running(id)) {
        return;
    }
    std::erase(g_switched_off, id);
    if (!on) {
        g_switched_off.push_back(id);
    }
    runtime_log::info("mods", "%s is turned %s while the game runs", id.c_str(), on ? "on" : "off");
}

void read_settings_again(const std::string& id) {
    if (const ModernGekkoModDesc* mod = find_mod(id); mod != nullptr && mod->on_load != nullptr) {
        mod->on_load(&host_api());
    }
}

std::vector<std::string> compiled_in() {
    std::vector<std::string> ids;
    for (const ModernGekkoGetModFn* get_mod = COMPILED_IN_MODS; *get_mod != nullptr; ++get_mod) {
        const ModernGekkoModDesc* mod = (*get_mod)();
        if (mod != nullptr && mod->id != nullptr) {
            ids.emplace_back(mod->id);
        }
    }
    return ids;
}

void load(const std::vector<std::string>* enabled) {
    std::vector<std::string> compiled_in;
    for (const ModernGekkoGetModFn* get_mod = COMPILED_IN_MODS; *get_mod != nullptr; ++get_mod) {
        const ModernGekkoModDesc* mod = (*get_mod)();
        if (mod != nullptr && mod->id != nullptr) {
            compiled_in.emplace_back(mod->id);
        }
        if (mod == nullptr || mod->abi_version != MODERNGEKKO_MOD_ABI_VERSION ||
            mod->cpu_state_size != sizeof(CPUState)) {
            runtime_log::warning("mods", "a mod was built for another mod ABI and is not used");
            continue;
        }
        if (enabled != nullptr && std::find(enabled->begin(), enabled->end(), mod->id) == enabled->end()) {
            runtime_log::info("mods", "%s is turned off", mod->id);
            if (KeptForSaves* kept = kept_for_saves(mod->id); kept != nullptr) {
                // Loaded all the same, for a save that needs it (hook_runs).
                kept->kept = true;
            } else if (!switches_while_running(mod->id)) {
                continue;
            } else {
                // Loaded all the same, so that the player can turn it on in the game's menu.
                g_switched_off.emplace_back(mod->id);
            }
        }
        if (std::strncmp(mod->game_id, GAME_ID, sizeof(mod->game_id)) != 0) {
            runtime_log::warning("mods", "%s is for game %.8s, not %s; it is not used", mod->id, mod->game_id, GAME_ID);
            continue;
        }
        for (uint32_t i = 0; i < mod->num_patches; ++i) {
            add_patch(mod->id, mod->patches[i]);
        }
        for (uint32_t i = 0; i < mod->num_hooks; ++i) {
            add_hook(mod->id, mod->hooks[i]);
        }
        if (mod->num_exports + mod->num_imports + mod->num_events + mod->num_callbacks != 0) {
            runtime_log::warning("mods", "%s uses exports, imports or events, which the native runtime ignores",
                                 mod->id);
        }
        if (mod->on_load != nullptr) {
            mod->on_load(&host_api());
        }
        runtime_log::info("mods", "%s %s: %u patches, %u hooks", mod->id, mod->version, mod->num_patches,
                          mod->num_hooks);
    }
    for (const std::string& name : enabled != nullptr ? *enabled : std::vector<std::string>{}) {
        if (std::find(compiled_in.begin(), compiled_in.end(), name) == compiled_in.end()) {
            runtime_log::warning("mods", "there is no mod %s in this build", name.c_str());
        }
    }
}

}  // namespace native_mods

using native_mods::g_hooks;

bool hmmm_mod_hook_entry(uint32_t address, CpuContext* cpu) {
    const auto found = g_hooks.find(address);
    if (found == g_hooks.end()) {
        return false;
    }
    native_mods::log_first_visit(found->second, address);
    native_mods::run(found->second.on_entry, address, *cpu);
    if (found->second.patch.function == nullptr) {
        return false;
    }
    CPUState state = native_mods::state_from(*cpu, address);
    found->second.patch.function(&state);
    native_mods::copy_back(state, *cpu);
    // A patch returns to the caller by setting pc to lr (moderngekko_mod_return_u32).
    return state.pc == state.lr;
}

void hmmm_mod_hook_return(uint32_t address, CpuContext* cpu) {
    if (const auto found = g_hooks.find(address); found != g_hooks.end()) {
        native_mods::log_first_visit(found->second, address);
        native_mods::run(found->second.on_return, address, *cpu);
    }
}

void hmmm_mod_hook_at(uint32_t address, CpuContext* cpu) {
    if (const auto found = g_hooks.find(address); found != g_hooks.end()) {
        native_mods::log_first_visit(found->second, address);
        native_mods::run(found->second.on_entry, address, *cpu);
    }
}
