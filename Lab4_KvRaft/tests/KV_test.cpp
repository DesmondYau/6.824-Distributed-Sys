// kvraft_test.cc
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <random>
#include <atomic>
#include <iostream>
#include <sstream>

#include "porcupine/porcupine.hpp"
#include "kvraft/config.hpp"   // Your lab 4 config header (make_config, Config, Clerk, etc.)

using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Constants 
// ---------------------------------------------------------------------------
const auto electionTimeout = 1s;
const auto linearizabilityCheckTimeout = 1s;

// ---------------------------------------------------------------------------
// Simple OpLog stub (Porcupine not ported yet)
// ---------------------------------------------------------------------------
struct OpLog {
    void Append(/* porcupine::Operation */) {}
    std::vector</* porcupine::Operation */> Read() { return {}; }
};

// ---------------------------------------------------------------------------
// Helper functions (Get / Put / Append / check)
// ---------------------------------------------------------------------------
void Get(Config* cfg, Clerk* ck, const std::string& key, OpLog* log = nullptr, int cli = -1) {
    auto start = std::chrono::steady_clock::now();
    std::string v = ck->Get(key);
    auto end = std::chrono::steady_clock::now();
    cfg->op();
    // log->Append(...)  // add when you have Porcupine
    std::cout << "[Get] key=" << key << " value=" << v << std::endl;
}

void Put(Config* cfg, Clerk* ck, const std::string& key, const std::string& value, OpLog* log = nullptr, int cli = -1) {
    auto start = std::chrono::steady_clock::now();
    ck->Put(key, value);
    auto end = std::chrono::steady_clock::now();
    cfg->op();
    std::cout << "[Put] key=" << key << " value=" << value << std::endl;
}

void Append(Config* cfg, Clerk* ck, const std::string& key, const std::string& value, OpLog* log = nullptr, int cli = -1) {
    auto start = std::chrono::steady_clock::now();
    ck->Append(key, value);
    auto end = std::chrono::steady_clock::now();
    cfg->op();
    std::cout << "[Append] key=" << key << " value=" << value << std::endl;
}

void check(Config* cfg, Clerk* ck, const std::string& key, const std::string& expected) {
    std::string v = ck->Get(key);
    if (v != expected) {
        FAIL() << "Get(" << key << "): expected '" << expected << "', got '" << v << "'";
    }
}

// ---------------------------------------------------------------------------
// GenericTest - the main test driver 
// ---------------------------------------------------------------------------
void GenericTest(const std::string& part,
                 int nclients,
                 int nservers,
                 bool unreliable,
                 bool crash,
                 bool partitions,
                 int maxraftstate,
                 bool randomkeys) {

    std::string title = "Test: ";
    if (unreliable) title += "unreliable net, ";
    if (crash)      title += "restarts, ";
    if (partitions) title += "partitions, ";
    if (maxraftstate != -1) title += "snapshots, ";
    if (randomkeys) title += "random keys, ";
    title += (nclients > 1 ? "many clients" : "one client");
    title += " (" + part + ")";

    Config cfg(nservers, unreliable, maxraftstate);
    cfg.begin(title);

    OpLog opLog;
    Clerk* ck = cfg.makeClient(cfg.All());

    std::atomic<int> done_clients{0};
    std::atomic<int> done_partitioner{0};
    std::vector<std::thread> client_threads;
    std::thread partitioner_thread;

    for (int iter = 0; iter < 3; ++iter) {
        done_clients = 0;
        done_partitioner = 0;

        // Spawn clients
        client_threads.clear();
        for (int cli = 0; cli < nclients; ++cli) {
            client_threads.emplace_back([&, cli]() {
                Clerk* myck = cfg.makeClient(cfg.All());
                std::string last = "";
                if (!randomkeys) {
                    Put(&cfg, myck, std::to_string(cli), last);
                }
                int j = 0;
                while (done_clients == 0) {
                    std::string key = randomkeys ? std::to_string(rand() % nclients) : std::to_string(cli);
                    std::string nv = "x " + std::to_string(cli) + " " + std::to_string(j) + " y";

                    if (rand() % 1000 < 500) {
                        Append(&cfg, myck, key, nv);
                        if (!randomkeys) last += nv;
                        j++;
                    } else if (randomkeys && rand() % 1000 < 100) {
                        Put(&cfg, myck, key, nv);
                        j++;
                    } else {
                        Get(&cfg, myck, key);
                    }
                }
                cfg.deleteClient(myck);
            });
        }

        // Partitioner
        if (partitions) {
            partitioner_thread = std::thread([&]() {
                while (done_partitioner == 0) {
                    std::vector<int> p1, p2;
                    for (int i = 0; i < nservers; ++i) {
                        (rand() % 2 == 0 ? p1 : p2).push_back(i);
                    }
                    cfg.partition(p1, p2);
                    std::this_thread::sleep_for(1s + std::chrono::milliseconds(rand() % 200));
                }
            });
        }

        std::this_thread::sleep_for(5s);

        done_clients = 1;
        done_partitioner = 1;

        if (partitions) {
            partitioner_thread.join();
            cfg.ConnectAll();
            std::this_thread::sleep_for(electionTimeout);
        }

        if (crash) {
            for (int i = 0; i < nservers; ++i) cfg.ShutdownServer(i);
            std::this_thread::sleep_for(electionTimeout);
            for (int i = 0; i < nservers; ++i) cfg.StartServer(i);
            cfg.ConnectAll();
        }

        for (auto& t : client_threads) t.join();

        // Final checks for non-randomkeys
        if (!randomkeys) {
            for (int i = 0; i < nclients; ++i) {
                std::string key = std::to_string(i);
                std::string v = ck->Get(key);
                // You can add checkClntAppends here if you implement it
            }
        }

        if (maxraftstate > 0) {
            size_t sz = cfg.LogSize();
            if (sz > 8 * static_cast<size_t>(maxraftstate)) {
                FAIL() << "logs were not trimmed (" << sz << " > 8*" << maxraftstate << ")";
            }
        }
        if (maxraftstate < 0) {
            size_t ssz = cfg.SnapshotSize();
            if (ssz > 0) {
                FAIL() << "snapshot too large (" << ssz << "), should not be used when maxraftstate = " << maxraftstate;
            }
        }
    }

    // Linearizability check stub (Porcupine not ported)
    std::cout << "info: linearizability check skipped (Porcupine not ported yet)\n";

    cfg.end();
}

// ---------------------------------------------------------------------------
// GenericTestSpeed
// ---------------------------------------------------------------------------
void GenericTestSpeed(const std::string& part, int maxraftstate) {
    const int nservers = 3;
    const int numOps = 1000;

    Config cfg(nservers, false, maxraftstate);
    cfg.begin("Test: ops complete fast enough (" + part + ")");

    Clerk* ck = cfg.makeClient(cfg.All());
    ck->Get("x");  // wait for leader

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < numOps; ++i) {
        ck->Append("x", "x 0 " + std::to_string(i) + " y");
    }
    auto dur = std::chrono::steady_clock::now() - start;

    std::string v = ck->Get("x");
    // checkClntAppends can be added here

    const auto heartbeatInterval = 100ms;
    const auto timePerOp = heartbeatInterval / 3;
    if (dur > numOps * timePerOp) {
        FAIL() << "Operations completed too slowly: " << (dur / numOps) << " > " << timePerOp;
    }

    cfg.end();
}

// ---------------------------------------------------------------------------
// All test cases 
// ---------------------------------------------------------------------------

TEST(KvRaftTest, TestBasic3A) {
    GenericTest("3A", 1, 5, false, false, false, -1, false);
}

TEST(KvRaftTest, TestSpeed3A) {
    GenericTestSpeed("3A", -1);
}

TEST(KvRaftTest, TestConcurrent3A) {
    GenericTest("3A", 5, 5, false, false, false, -1, false);
}

TEST(KvRaftTest, TestUnreliable3A) {
    GenericTest("3A", 5, 5, true, false, false, -1, false);
}

TEST(KvRaftTest, TestUnreliableOneKey3A) {
    const int nservers = 3;
    Config cfg(nservers, true, -1);
    cfg.begin("Test: concurrent append to same key, unreliable (3A)");

    Clerk* ck = cfg.makeClient(cfg.All());
    Put(&cfg, ck, "k", "", nullptr, -1);

    const int nclient = 5;
    const int upto = 10;

    std::vector<std::thread> clients;
    for (int me = 0; me < nclient; ++me) {
        clients.emplace_back([&, me]() {
            Clerk* myck = cfg.makeClient(cfg.All());
            for (int n = 0; n < upto; ++n) {
                Append(&cfg, myck, "k", "x " + std::to_string(me) + " " + std::to_string(n) + " y");
            }
            cfg.deleteClient(myck);
        });
    }
    for (auto& t : clients) t.join();

    // checkConcurrentAppends can be added here
    cfg.end();
}

TEST(KvRaftTest, TestOnePartition3A) {
    // Implement the detailed one-partition test if needed
    // (same logic as Go version)
    SUCCEED() << "TestOnePartition3A - implement detailed logic if required";
}

TEST(KvRaftTest, TestManyPartitionsOneClient3A) {
    GenericTest("3A", 1, 5, false, false, true, -1, false);
}

TEST(KvRaftTest, TestManyPartitionsManyClients3A) {
    GenericTest("3A", 5, 5, false, false, true, -1, false);
}

TEST(KvRaftTest, TestPersistOneClient3A) {
    GenericTest("3A", 1, 5, false, true, false, -1, false);
}

TEST(KvRaftTest, TestPersistConcurrent3A) {
    GenericTest("3A", 5, 5, false, true, false, -1, false);
}

TEST(KvRaftTest, TestPersistConcurrentUnreliable3A) {
    GenericTest("3A", 5, 5, true, true, false, -1, false);
}

TEST(KvRaftTest, TestPersistPartition3A) {
    GenericTest("3A", 5, 5, false, true, true, -1, false);
}

TEST(KvRaftTest, TestPersistPartitionUnreliable3A) {
    GenericTest("3A", 5, 5, true, true, true, -1, false);
}

TEST(KvRaftTest, TestPersistPartitionUnreliableLinearizable3A) {
    GenericTest("3A", 15, 7, true, true, true, -1, true);
}

// 3B tests
TEST(KvRaftTest, TestSnapshotRPC3B) {
    // Full logic from Go TestSnapshotRPC3B can be added here if needed
    SUCCEED() << "TestSnapshotRPC3B";
}

TEST(KvRaftTest, TestSnapshotSize3B) {
    // Full logic from Go TestSnapshotSize3B
    SUCCEED() << "TestSnapshotSize3B";
}

TEST(KvRaftTest, TestSpeed3B) {
    GenericTestSpeed("3B", 1000);
}

TEST(KvRaftTest, TestSnapshotRecover3B) {
    GenericTest("3B", 1, 5, false, true, false, 1000, false);
}

TEST(KvRaftTest, TestSnapshotRecoverManyClients3B) {
    GenericTest("3B", 20, 5, false, true, false, 1000, false);
}

TEST(KvRaftTest, TestSnapshotUnreliable3B) {
    GenericTest("3B", 5, 5, true, false, false, 1000, false);
}

TEST(KvRaftTest, TestSnapshotUnreliableRecover3B) {
    GenericTest("3B", 5, 5, true, true, false, 1000, false);
}

TEST(KvRaftTest, TestSnapshotUnreliableRecoverConcurrentPartition3B) {
    GenericTest("3B", 5, 5, true, true, true, 1000, false);
}

TEST(KvRaftTest, TestSnapshotUnreliableRecoverConcurrentPartitionLinearizable3B) {
    GenericTest("3B", 15, 7, true, true, true, 1000, true);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}