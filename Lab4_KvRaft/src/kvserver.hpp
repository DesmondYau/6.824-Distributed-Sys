#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include "rpc/labrpc.hpp"
#include "raft.hpp"   // for raft::Raft and raft::Persister

// ==================== RPC Argument / Reply structs ====================
// (must be outside the class so the RPC system can marshal them)

struct GetArgs {
    std::string key;
};

struct GetReply {
    std::string value;
    bool wrongLeader;
};

struct PutAppendArgs {
    std::string key;
    std::string value;
    std::string op;   // "Put" or "Append"
};

struct PutAppendReply {
    bool wrongLeader;
};

// The command that gets stored in Raft's log
struct Op {
    // YOU WILL HAVE TO MODIFY THIS STRUCT
    // (add client ID, sequence number, etc. for deduplication)
    std::string key;
    std::string value;
    std::string op;   // "Put" or "Append"
};

class KVServer {
public:
    // Factory used by Config (equivalent to Go's StartKVServer)
    KVServer(labrpc::Server* srv, int me, raft::Persister* persister, int maxraftstate);

    void Get(const GetArgs& args, GetReply& reply);
    void PutAppend(const PutAppendArgs& args, PutAppendReply& reply);

    void Kill();
    bool isKilled() const;

private:
    // ================================================================
    // YOU WILL HAVE TO MODIFY THIS STRUCT
    // ================================================================
    int me;
    raft::Raft* rf;                    // or std::shared_ptr<raft::Raft>
    int maxraftstate;
    std::atomic<bool> dead{false};

    // add your own fields here (map for the KV store, deduplication table, etc.)
};

// Factory function (called by config)
KVServer* StartKVServer(labrpc::Server* srv, int me, raft::Persister* persister, int maxraftstate);