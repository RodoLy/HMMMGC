// The registry of translated functions: which host function runs each guest address.
// It is the dispatch half of the guest ABI (guest_abi/dispatch.h in WiiCompiled).
//
// El registro de funciones traducidas: qué función del host corre cada dirección del
// juego. Es la mitad de despacho de la ABI.
//
// Direct calls of the game are bound when the translated code is compiled. The registry
// serves the rest: calls through function pointers (bctrl), jumps through tables (bctr),
// and calls into the game made by the runtime. The translator emits one table of records
// per shard of code, registered by static constructors before main() runs, and one
// generated table indexed by address for the indirect jumps.

#include "fault_handler.h"
#include "guest_symbols.h"
#include "runtime_log.h"

#include "guest_abi/guest_abi.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

struct Registry {
    std::vector<TranslatedFunctionInfo> functions;  // sorted by address once finalized
    std::vector<RawDispatchRecord> raw_records;     // same order as `functions`
};

// Built on first use: the static constructors that fill it run in no defined order.
Registry& get_registry() {
    static Registry registry;
    return registry;
}

template <typename Entry>
const Entry* find_by_address(const std::vector<Entry>& entries, uint32_t address) {
    const auto found = std::lower_bound(entries.begin(), entries.end(), address,
                                        [](const Entry& entry, uint32_t wanted) { return entry.address < wanted; });
    return found != entries.end() && found->address == address ? &*found : nullptr;
}

}  // namespace

void RegisterBulkTranslatedFunctions(const BulkTranslatedFunctionRecord* records, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const BulkTranslatedFunctionRecord& record = records[i];
        TranslatedFunctionInfo info;
        info.address = record.address;
        info.name = record.name != nullptr ? record.name : "";
        info.moduleId = record.moduleId;
        info.priority = record.priority;
        // A function that keeps the callee-saved FPRs needs no guard around its calls.
        info.nonvolatileFprWriteMask =
            record.preservesNonvolatileFprs ? 0u : (record.nonvolatileFprWriteMask & kPpcAllNonvolatileFprMask);
        info.entryPoint = reinterpret_cast<void*>(record.entry);
        info.rawCpuInvoker = record.entry;
        info.mustRemainDynamicallyDispatchable = record.mustRemainDynamicallyDispatchable;
        info.kind = record.kind;
        TranslatedFunctionRegistry::Register(info);
    }
}

void TranslatedFunctionRegistry::Register(TranslatedFunctionInfo info) { get_registry().functions.push_back(info); }

// Sorts the registry and lets the inline lookups of dispatch.h cache their answers.
// When two registrations share an address, the one with the higher priority wins (a
// native replacement over the translation of the same function).
void TranslatedFunctionRegistry::Finalize() {
    Registry& registry = get_registry();
    auto& functions = registry.functions;
    std::stable_sort(functions.begin(), functions.end(), [](const auto& a, const auto& b) {
        return a.address != b.address ? a.address < b.address : a.priority > b.priority;
    });
    functions.erase(std::unique(functions.begin(), functions.end(),
                                [](const auto& a, const auto& b) { return a.address == b.address; }),
                    functions.end());
    registry.raw_records.clear();
    for (const TranslatedFunctionInfo& info : functions) {
        registry.raw_records.push_back({info.address, info.rawCpuInvoker, info.nonvolatileFprWriteMask,
                                        ShouldPreserveNonvolatileGprsForRawCpuCall(&info)});
    }
    lookupPublished_.store(true, std::memory_order_release);
    runtime_log::info("registry", "%zu translated functions", functions.size());
}

const TranslatedFunctionInfo* TranslatedFunctionRegistry::FindByAddressPtrSlow(uint32_t address) {
    return find_by_address(get_registry().functions, address);
}

const RawDispatchRecord* TranslatedFunctionRegistry::FindRawByAddressPtrSlow(uint32_t address) {
    return find_by_address(get_registry().raw_records, address);
}

void RegisterStaticIndirectDispatchTable(const StaticIndirectDispatchTable* table) {
    g_publishedStaticIndirectDispatchTable.store(table, std::memory_order_release);
}

// A call or jump to an address that has no translated function: the game computed a
// wrong pointer, or the translator missed a function. Either way nothing can run there.
void ReportMissingIndirectJumpTarget(uint32_t target, CpuContext* cpu) {
    RuntimeCrash::FatalMissingGuestTarget(target, cpu);
}

namespace RuntimeCrash {
void FatalMissingGuestTarget(uint32_t target, CpuContext* cpu) noexcept {
    runtime_log::error("dispatch", "no translated function at %s (called from %s)",
                       guest_symbols::describe(target).c_str(),
                       cpu != nullptr ? guest_symbols::describe(cpu->lr).c_str() : "?");
    fault_handler::report_fatal("call of a missing guest function");
}
}  // namespace RuntimeCrash
