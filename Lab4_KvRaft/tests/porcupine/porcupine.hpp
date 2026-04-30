// porcupine/porcupine.hpp
#pragma once

#include "model.hpp"
#include "checker.hpp"
#include <chrono>

namespace porcupine {

// ---------------------------------------------------------------------------
// Public API - exactly matching the Go version
// ---------------------------------------------------------------------------

// Simple check: returns true if linearizable
inline bool CheckOperations(const KvModel& model, const std::vector<Operation>& history) {
    auto [res, _] = CheckOperationsVerbose(model, history, std::chrono::milliseconds(0));
    return res == CheckResult::Ok;
}

// Timeout version (0 = no timeout)
inline CheckResult CheckOperationsTimeout(const KvModel& model,
                                          const std::vector<Operation>& history,
                                          std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
    auto [res, _] = CheckOperationsVerbose(model, history, timeout);
    return res;
}

// Full verbose version (used in tests)
inline std::pair<CheckResult, LinearizationInfo> CheckOperationsVerbose(
        const KvModel& model,
        const std::vector<Operation>& history,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {

    return checker::CheckOperationsVerbose(model, history, timeout);
}

// Event-based versions (for completeness, though we mostly use Operation)
inline bool CheckEvents(const KvModel& model, const std::vector<Event>& history) {
    auto [res, _] = CheckEventsVerbose(model, history, std::chrono::milliseconds(0));
    return res == CheckResult::Ok;
}

inline CheckResult CheckEventsTimeout(const KvModel& model,
                                      const std::vector<Event>& history,
                                      std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
    auto [res, _] = CheckEventsVerbose(model, history, timeout);
    return res;
}

inline std::pair<CheckResult, LinearizationInfo> CheckEventsVerbose(
        const KvModel& model,
        const std::vector<Event>& history,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {

    return checker::CheckEventsVerbose(model, history, timeout);
}

} // namespace porcupine