// models.hpp
#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdint>

struct KvInput {
    uint8_t Op;        // 0 = Get, 1 = Put, 2 = Append
    std::string Key;
    std::string Value;
};

struct KvOutput {
    std::string Value;
};

struct KvModel {
    // Partition operations by key (same logic as the Go version)
    static std::vector<std::vector<KvInput>> Partition(const std::vector<KvInput>& history) {
        std::map<std::string, std::vector<KvInput>> m;
        for (const auto& op : history) {
            m[op.Key].push_back(op);
        }

        std::vector<std::string> keys;
        for (const auto& p : m) {
            keys.push_back(p.first);
        }
        std::sort(keys.begin(), keys.end());

        std::vector<std::vector<KvInput>> result;
        for (const auto& k : keys) {
            result.push_back(m[k]);
        }
        return result;
    }

    // Initial state for a single key
    static std::string Init() {
        return "";
    }

    // State machine step function (same as Go)
    static std::pair<bool, std::string> Step(const std::string& state,
                                             const KvInput& input,
                                             const KvOutput& output) {
        if (input.Op == 0) {           // Get
            return {output.Value == state, state};
        } else if (input.Op == 1) {    // Put
            return {true, input.Value};
        } else {                       // Append
            return {true, state + input.Value};
        }
    }

    // Nice description for debugging / visualization
    static std::string DescribeOperation(const KvInput& input, const KvOutput& output) {
        if (input.Op == 0) return "get('" + input.Key + "') -> '" + output.Value + "'";
        if (input.Op == 1) return "put('" + input.Key + "', '" + input.Value + "')";
        if (input.Op == 2) return "append('" + input.Key + "', '" + input.Value + "')";
        return "<invalid op>";
    }
};