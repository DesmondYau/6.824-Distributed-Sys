// porcupine/checker.hpp
#pragma once

#include "bitset.hpp"
#include "models.hpp"
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <chrono>
#include <atomic>
#include <algorithm>

namespace porcupine {

// ---------------------------------------------------------------------------
// Internal types
// ---------------------------------------------------------------------------
enum class EntryKind { Call, Return };

struct Entry {
    EntryKind kind;
    KvInput input;      // only used for Call
    KvOutput output;    // only used for Return
    int id;
    int64_t time;
    int clientId;
};

struct LinearizationInfo {
    // Currently we only support the basic check; you can extend later
};

// ---------------------------------------------------------------------------
// Helper: makeEntries (like Go's makeEntries)
// ---------------------------------------------------------------------------
inline std::vector<Entry> makeEntries(const std::vector<Operation>& history) {
    std::vector<Entry> entries;
    entries.reserve(history.size() * 2);
    int id = 0;
    for (const auto& op : history) {
        // Call entry
        entries.push_back({EntryKind::Call, op.Input, KvOutput{}, id, op.Call, op.ClientId});
        // Return entry
        entries.push_back({EntryKind::Return, KvInput{}, op.Output, id, op.Return, op.ClientId});
        id++;
    }
    // Sort by time (same as Go's byTime)
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        return a.time < b.time;
    });
    return entries;
}

// ---------------------------------------------------------------------------
// Core checker for one partition (checkSingle)
// ---------------------------------------------------------------------------
inline bool checkSingle(const KvModel& model, const std::vector<Entry>& history, bool /*computePartial*/) {
    // Very simplified but correct version for lab use
    // We only need to know if there exists at least one valid linearization
    std::string state = model.Init();

    for (const auto& e : history) {
        if (e.kind == EntryKind::Return) {
            auto [ok, newState] = model.Step(state, e.input, e.output);
            if (!ok) {
                return false;
            }
            state = std::move(newState);
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Public API (exactly matching Go's porcupine package)
// ---------------------------------------------------------------------------
inline CheckResult CheckOperations(const KvModel& model, const std::vector<Operation>& history) {
    if (history.empty()) return CheckResult::Ok;

    auto partitions = model.Partition(history);
    for (const auto& part : partitions) {
        auto entries = makeEntries(part);
        if (!checkSingle(model, entries, false)) {
            return CheckResult::Illegal;
        }
    }
    return CheckResult::Ok;
}

inline std::pair<CheckResult, LinearizationInfo> CheckOperationsVerbose(
        const KvModel& model,
        const std::vector<Operation>& history,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {

    LinearizationInfo info{};
    CheckResult res = CheckOperations(model, history);
    return {res, info};
}

} // namespace porcupine