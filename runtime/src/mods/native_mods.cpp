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
        hook.function(&state);
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

}  // namespace

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
    static const ModernGekkoModHostApi HOST_API{MODERNGEKKO_MOD_HOST_ABI_VERSION, nullptr, no_event, no_export};
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
            continue;
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
            mod->on_load(&HOST_API);
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
