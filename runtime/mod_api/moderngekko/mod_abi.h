/*
 * The native runtime's stand-in for ModernGekko's mod ABI (moderngekko/mod_abi.h), so
 * that the mods in mods/ compile unchanged into the native runtime. It declares the same
 * names with the same meaning, but only the parts the native runtime supports: patches
 * and hooks, the CPU registers, and the guest memory. Exports, imports and events are
 * accepted in a descriptor and ignored (no mod of this game uses them).
 *
 * El sustituto del runtime nativo para la ABI de mods de ModernGekko, para que los mods
 * de mods/ compilen sin cambios dentro del runtime nativo. Declara los mismos nombres
 * con el mismo significado, pero solo lo que el runtime nativo soporta.
 *
 * A mod here is not a shared library: it is compiled into the runtime, and its
 * moderngekko_get_mod() is renamed per mod by the build (see runtime/CMakeLists.txt).
 * runtime/src/mods/native_mods.h runs its patches and hooks.
 */

#ifndef MODERNGEKKO_MOD_ABI_H
#define MODERNGEKKO_MOD_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODERNGEKKO_MOD_ABI_VERSION 1u
#define MODERNGEKKO_MOD_HOST_ABI_VERSION 1u
/* The CPU state below is the native runtime's own; a descriptor states this version. */
#define MODERNGEKKO_CPU_ABI_VERSION 1u

/* Mods are linked into the runtime: nothing is exported from a library. */
#define MODERNGEKKO_MOD_EXPORT

typedef struct CPUState CPUState;

typedef uint64_t (*PPCExternalRead)(CPUState* cpu, uint32_t address, uint8_t size);
typedef void (*PPCExternalWrite)(CPUState* cpu, uint32_t address, uint64_t value, uint8_t size);

/*
 * The game's CPU as a mod sees it. The runtime fills it from the game's registers before
 * a patch or hook runs, and copies the registers back after it.
 */
struct CPUState {
    uint32_t gpr[32];
    double fpr[32];
    uint32_t pc; /* the address the patch or hook is at; a patch sets it to lr to return */
    uint32_t lr;
    uint32_t ctr;
    uint32_t cr;
    uint32_t xer;
    PPCExternalRead external_read;   /* guest memory, big-endian values of 1, 2, 4 or 8 bytes */
    PPCExternalWrite external_write;
    void* external_user_data;
};

typedef void (*ModernGekkoModFunction)(CPUState* state);

typedef enum ModernGekkoModPatchFlags { MODERNGEKKO_MOD_PATCH_NONE = 0, MODERNGEKKO_MOD_PATCH_FORCE = 1 } ModernGekkoModPatchFlags;

typedef enum ModernGekkoModHookKind { MODERNGEKKO_MOD_HOOK_ENTRY = 0, MODERNGEKKO_MOD_HOOK_RETURN = 1 } ModernGekkoModHookKind;

typedef struct ModernGekkoModDependency {
    const char* id;
    const char* minimum_version;
    uint32_t optional;
} ModernGekkoModDependency;

typedef struct ModernGekkoModPatch {
    uint32_t address;
    ModernGekkoModFunction function;
    uint32_t flags;
} ModernGekkoModPatch;

typedef struct ModernGekkoModHook {
    uint32_t address;
    ModernGekkoModFunction function;
    uint32_t kind;
} ModernGekkoModHook;

typedef struct ModernGekkoModExportEntry {
    const char* name;
    ModernGekkoModFunction function;
} ModernGekkoModExportEntry;

typedef struct ModernGekkoModImportEntry {
    const char* dependency_id;
    const char* name;
    ModernGekkoModFunction* slot;
} ModernGekkoModImportEntry;

typedef struct ModernGekkoModEvent {
    const char* name;
} ModernGekkoModEvent;

typedef struct ModernGekkoModCallback {
    const char* dependency_id;
    const char* event_name;
    ModernGekkoModFunction function;
} ModernGekkoModCallback;

typedef struct ModernGekkoModHostApi {
    uint32_t abi_version;
    void* user_data;
    int (*trigger_event)(void* user_data, const char* provider_id, const char* event_name, CPUState* state);
    ModernGekkoModFunction (*find_export)(void* user_data, const char* provider_id, const char* export_name);
} ModernGekkoModHostApi;

typedef struct ModernGekkoModDesc {
    uint32_t abi_version;
    uint32_t cpu_abi_version;
    uint32_t cpu_state_size;
    char game_id[8];
    const char* id;
    const char* version;
    const char* display_name;
    const ModernGekkoModDependency* dependencies;
    uint32_t num_dependencies;
    const ModernGekkoModPatch* patches;
    uint32_t num_patches;
    const ModernGekkoModHook* hooks;
    uint32_t num_hooks;
    const ModernGekkoModExportEntry* exports;
    uint32_t num_exports;
    const ModernGekkoModImportEntry* imports;
    uint32_t num_imports;
    const ModernGekkoModEvent* events;
    uint32_t num_events;
    const ModernGekkoModCallback* callbacks;
    uint32_t num_callbacks;
    void (*on_load)(const ModernGekkoModHostApi* api);
    void (*on_unload)(void);
} ModernGekkoModDesc;

typedef const ModernGekkoModDesc* (*ModernGekkoGetModFn)(void);

#define RECOMP_PATCH(address, function) {(address), (function), MODERNGEKKO_MOD_PATCH_NONE}
#define RECOMP_FORCE_PATCH(address, function) {(address), (function), MODERNGEKKO_MOD_PATCH_FORCE}
#define RECOMP_HOOK(address, function) {(address), (function), MODERNGEKKO_MOD_HOOK_ENTRY}
#define RECOMP_HOOK_RETURN(address, function) {(address), (function), MODERNGEKKO_MOD_HOOK_RETURN}
#define RECOMP_EXPORT(name, function) {(name), (function)}
#define RECOMP_IMPORT(mod, name, slot) {(mod), (name), (slot)}
#define RECOMP_DECLARE_EVENT(name) {(name)}
#define RECOMP_CALLBACK(mod, event, function) {(mod), (event), (function)}

static inline uint32_t moderngekko_mod_arg_u32(const CPUState* state, uint32_t index) {
    return index < 8u ? state->gpr[3u + index] : 0u;
}

static inline void moderngekko_mod_return_u32(CPUState* state, uint32_t value) {
    state->gpr[3] = value;
    state->pc = state->lr;
}

static inline uint64_t moderngekko_mod_read(CPUState* state, uint32_t address, uint8_t size) {
    return state->external_read ? state->external_read(state, address, size) : 0u;
}

static inline void moderngekko_mod_write(CPUState* state, uint32_t address, uint64_t value, uint8_t size) {
    if (state->external_write) {
        state->external_write(state, address, value, size);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* MODERNGEKKO_MOD_ABI_H */
