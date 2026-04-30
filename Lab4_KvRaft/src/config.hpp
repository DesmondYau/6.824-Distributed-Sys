// config.hpp
#pragma once

#include <vector>
#include <map>
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <iostream>

#include "rpc/labrpc.hpp"
#include "raft.hpp"      // for Persister
#include "kvraft.hpp"    // for KVServer and Clerk

class Config {
public:
    Config(int n, bool unreliable, int maxraftstate);
    ~Config();

    void begin(const std::string& description);
    void end();
    void op();                          // called on every client operation

    void ShutdownServer(int i);
    void StartServer(int i);
    void ConnectAll();
    void partition(const std::vector<int>& p1, const std::vector<int>& p2);

    Clerk* makeClient(const std::vector<int>& to);
    void deleteClient(Clerk* ck);
    void ConnectClient(Clerk* ck, const std::vector<int>& to);
    void DisconnectClient(Clerk* ck, const std::vector<int>& from);

    std::vector<int> All();

    size_t LogSize();
    size_t SnapshotSize();
    int rpcTotal();

    void cleanup();

    // Test helpers
    std::pair<bool, int> Leader();           // (isLeader, leaderId)
    std::pair<std::vector<int>, std::vector<int>> make_partition();

private:
    std::mutex mu;
    testing::Test* t = nullptr;              // for gtest failure reporting
    labrpc::Network* net = nullptr;
    int n = 0;
    std::vector<KVServer*> kvservers;
    std::vector<std::unique_ptr<raft::Persister>> saved;
    std::vector<std::vector<std::string>> endnames;   // endnames[i][j] = name of client end from i to j
    std::map<Clerk*, std::vector<std::string>> clerks;
    int nextClientId = 0;
    int maxraftstate = -1;
    std::chrono::steady_clock::time_point start;

    // begin/end statistics
    std::chrono::steady_clock::time_point t0;
    int rpcs0 = 0;
    std::atomic<int32_t> ops{0};

    void checkTimeout();
    void connectUnlocked(int i, const std::vector<int>& to);
    void disconnectUnlocked(int i, const std::vector<int>& from);
    void ConnectClientUnlocked(Clerk* ck, const std::vector<int>& to);
    void DisconnectClientUnlocked(Clerk* ck, const std::vector<int>& from);
};

// Factory function (equivalent to Go's make_config)
Config* make_config(testing::Test* t, int n, bool unreliable, int maxraftstate);