#pragma once

#include <vector>
#include <string>
#include "rpc/labrpc.hpp"

class Clerk {
public:
    // Equivalent to Go's MakeClerk
    Clerk(labrpc::Network* net, const std::vector<std::string>& servers);

    std::string Get(const std::string& key);
    void Put(const std::string& key, const std::string& value);
    void Append(const std::string& key, const std::string& value);

    // Used by Config (do not modify)
    void connect(const std::vector<int>& to);
    void disconnect(const std::vector<int>& from);

private:
    labrpc::Network* net_;
    std::vector<std::string> servers_;

    // ================================================================
    // YOU WILL HAVE TO MODIFY THIS STRUCT
    // (add client ID, request sequence number, leader index, mutex, etc.)
    // ================================================================
};

// Random helper from the Go file
int64_t nrand();