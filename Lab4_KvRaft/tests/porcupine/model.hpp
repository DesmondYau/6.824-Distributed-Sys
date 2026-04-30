// porcupine/model.hpp
#pragma once

#include "../models.hpp"   // your KvInput / KvOutput / KvModel
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace porcupine {

// ==============================================
// Types matching Go exactly
// ==============================================

struct Operation {
    int ClientId = 0;           // optional, unless you want visualization
    KvInput Input;
    int64_t Call = 0;           // invocation time (nanoseconds)
    KvOutput Output;
    int64_t Return = 0;         // response time (nanoseconds)
};

enum class EventKind : bool {
    CallEvent   = false,
    ReturnEvent = true
};

struct Event {
    int ClientId = 0;
    EventKind Kind;
    KvInput Input;      // used for Call
    KvOutput Output;    // used for Return
    int Id = 0;
};

// ==============================================
// The Model definition (same as Go)
// ==============================================

struct Model {
    // Partition functions
    std::function<std::vector<std::vector<Operation>>(const std::vector<Operation>&)> Partition;
    std::function<std::vector<std::vector<Event>>(const std::vector<Event>&)> PartitionEvent;

    // Initial state of the system
    std::function<std::string()> Init;

    // Step function: returns (ok, newState)
    std::function<std::pair<bool, std::string>(const std::string& state,
                                               const KvInput& input,
                                               const KvOutput& output)> Step;

    // Equality on states
    std::function<bool(const std::string&, const std::string&)> Equal;

    // For visualization
    std::function<std::string(const KvInput&, const KvOutput&)> DescribeOperation;
    std::function<std::string(const std::string&)> DescribeState;
};

// ==============================================
// Default helpers (exactly as in Go)
// ==============================================

inline std::vector<std::vector<Operation>> NoPartition(const std::vector<Operation>& history) {
    return {history};
}

inline std::vector<std::vector<Event>> NoPartitionEvent(const std::vector<Event>& history) {
    return {history};
}

inline bool ShallowEqual(const std::string& a, const std::string& b) {
    return a == b;
}

inline std::string DefaultDescribeOperation(const KvInput& input, const KvOutput& output) {
    return KvModel::DescribeOperation(input, output);
}

inline std::string DefaultDescribeState(const std::string& state) {
    return state.empty() ? "<empty>" : state;
}

// ==============================================
// CheckResult (same as Go)
// ==============================================

enum class CheckResult {
    Ok,      // linearizable
    Illegal, // not linearizable
    Unknown  // timed out (possible false positive)
};

} // namespace porcupine