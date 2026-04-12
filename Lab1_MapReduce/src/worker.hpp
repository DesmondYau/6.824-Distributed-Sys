#pragma once
#include "../include/json.hpp"

struct KeyValuePair
{
    std::string key_;
    std::string value_;
};

using MapFunc = std::vector<KeyValuePair>(*)(const std::string& filename, const std::string& contents);
using ReduceFunc = std::string(*)(const std::string& key, const std::vector<std::string> values);