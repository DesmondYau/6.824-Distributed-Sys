// config.cc
#include "config.hpp"
#include <random>
#include <thread>
#include <iomanip>

static std::string randstring(int n) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::string s(n, '0');
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, sizeof(charset)-2);
    for (int i = 0; i < n; ++i) {
        s[i] = charset[dist(gen)];
    }
    return s;
}

Config::Config(int n_, bool unreliable, int maxraftstate_)
    : n(n_), maxraftstate(maxraftstate_), start(std::chrono::steady_clock::now())
{
    net = labrpc::MakeNetwork();
    kvservers.resize(n);
    saved.resize(n);
    endnames.resize(n);
    for (int i = 0; i < n; ++i) {
        StartServer(i);
    }
    ConnectAll();
    net->Reliable(!unreliable);
}

Config::~Config() {
    cleanup();
}

void Config::begin(const std::string& description) {
    std::cout << description << " ..." << std::endl;
    t0 = std::chrono::steady_clock::now();
    rpcs0 = rpcTotal();
    ops = 0;
}

void Config::end() {
    checkTimeout();
    if (!t->failed()) {
        auto duration = std::chrono::steady_clock::now() - t0;
        double secs = std::chrono::duration<double>(duration).count();
        int nrpc = rpcTotal() - rpcs0;
        int nops = ops.load();
        std::cout << " ... Passed -- "
                  << std::fixed << std::setprecision(1) << secs << "s "
                  << n << " " << nrpc << " " << nops << std::endl;
    }
}

void Config::op() {
    ops.fetch_add(1, std::memory_order_relaxed);
}

void Config::cleanup() {
    std::lock_guard<std::mutex> lock(mu);
    for (auto* srv : kvservers) {
        if (srv) srv->Kill();
    }
    if (net) net->Cleanup();
    checkTimeout();
}

size_t Config::LogSize() {
    size_t maxsz = 0;
    for (const auto& p : saved) {
        if (p) maxsz = std::max(maxsz, p->RaftStateSize());
    }
    return maxsz;
}

size_t Config::SnapshotSize() {
    size_t maxsz = 0;
    for (const auto& p : saved) {
        if (p) maxsz = std::max(maxsz, p->SnapshotSize());
    }
    return maxsz;
}

int Config::rpcTotal() {
    return net ? net->GetTotalCount() : 0;
}

void Config::checkTimeout() {
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(120)) {
        std::cerr << "test took longer than 120 seconds" << std::endl;
    }
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------
void Config::connectUnlocked(int i, const std::vector<int>& to) {
    for (int j : to) {
        std::string endname = endnames[i][j];
        net->Enable(endname, true);
    }
    for (int j : to) {
        std::string endname = endnames[j][i];
        net->Enable(endname, true);
    }
}

void Config::disconnectUnlocked(int i, const std::vector<int>& from) {
    for (int j : from) {
        if (!endnames[i].empty()) {
            std::string endname = endnames[i][j];
            net->Enable(endname, false);
        }
    }
    for (int j : from) {
        if (!endnames[j].empty()) {
            std::string endname = endnames[j][i];
            net->Enable(endname, false);
        }
    }
}

void Config::ConnectAll() {
    std::lock_guard<std::mutex> lock(mu);
    for (int i = 0; i < n; ++i) {
        connectUnlocked(i, All());
    }
}

void Config::partition(const std::vector<int>& p1, const std::vector<int>& p2) {
    std::lock_guard<std::mutex> lock(mu);
    for (int i : p1) {
        disconnectUnlocked(i, p2);
        connectUnlocked(i, p1);
    }
    for (int i : p2) {
        disconnectUnlocked(i, p1);
        connectUnlocked(i, p2);
    }
}

// ---------------------------------------------------------------------------
// Server management
// ---------------------------------------------------------------------------
void Config::ShutdownServer(int i) {
    std::lock_guard<std::mutex> lock(mu);
    disconnectUnlocked(i, All());
    net->DeleteServer(i);

    if (kvservers[i]) {
        KVServer* kv = kvservers[i];
        kvservers[i] = nullptr;
        mu.unlock();
        kv->Kill();
        mu.lock();
    }
}

void Config::StartServer(int i) {
    std::lock_guard<std::mutex> lock(mu);

    // fresh set of ClientEnd names
    endnames[i].resize(n);
    for (int j = 0; j < n; ++j) {
        endnames[i][j] = randstring(20);
    }

    // fresh set of ClientEnds
    std::vector<labrpc::ClientEnd*> ends(n);
    for (int j = 0; j < n; ++j) {
        ends[j] = net->MakeEnd(endnames[i][j]);
        net->Connect(endnames[i][j], j);
    }

    // fresh persister
    if (saved[i] == nullptr) {
        saved[i] = std::make_unique<raft::Persister>();
    } else {
        saved[i] = saved[i]->Copy();
    }

    kvservers[i] = StartKVServer(ends, i, saved[i].get(), maxraftstate);

    // register services
    auto kvsvc = labrpc::MakeService(kvservers[i]);
    auto rfsvc = labrpc::MakeService(kvservers[i]->rf);
    auto srv = labrpc::MakeServer();
    srv->AddService(kvsvc);
    srv->AddService(rfsvc);
    net->AddServer(i, srv);
}

// ---------------------------------------------------------------------------
// Client management
// ---------------------------------------------------------------------------
Clerk* Config::makeClient(const std::vector<int>& to) {
    std::lock_guard<std::mutex> lock(mu);

    std::vector<std::string> endnames_local(n);
    std::vector<labrpc::ClientEnd*> ends(n);

    for (int j = 0; j < n; ++j) {
        endnames_local[j] = randstring(20);
        ends[j] = net->MakeEnd(endnames_local[j]);
        net->Connect(endnames_local[j], j);
    }

    Clerk* ck = MakeClerk(ends);   // assume this function exists
    clerks[ck] = endnames_local;
    nextClientId++;

    ConnectClientUnlocked(ck, to);
    return ck;
}

void Config::deleteClient(Clerk* ck) {
    std::lock_guard<std::mutex> lock(mu);
    clerks.erase(ck);
}

void Config::ConnectClientUnlocked(Clerk* ck, const std::vector<int>& to) {
    auto it = clerks.find(ck);
    if (it == clerks.end()) return;
    const auto& names = it->second;
    for (int j : to) {
        net->Enable(names[j], true);
    }
}

void Config::ConnectClient(Clerk* ck, const std::vector<int>& to) {
    std::lock_guard<std::mutex> lock(mu);
    ConnectClientUnlocked(ck, to);
}

void Config::DisconnectClientUnlocked(Clerk* ck, const std::vector<int>& from) {
    auto it = clerks.find(ck);
    if (it == clerks.end()) return;
    const auto& names = it->second;
    for (int j : from) {
        net->Enable(names[j], false);
    }
}

void Config::DisconnectClient(Clerk* ck, const std::vector<int>& from) {
    std::lock_guard<std::mutex> lock(mu);
    DisconnectClientUnlocked(ck, from);
}

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------
std::vector<int> Config::All() {
    std::vector<int> all(n);
    for (int i = 0; i < n; ++i) all[i] = i;
    return all;
}

std::pair<bool, int> Config::Leader() {
    std::lock_guard<std::mutex> lock(mu);
    for (int i = 0; i < n; ++i) {
        if (kvservers[i]) {
            bool isLeader = kvservers[i]->rf->IsLeader();  // assume your Raft has IsLeader()
            if (isLeader) return {true, i};
        }
    }
    return {false, -1};
}

std::pair<std::vector<int>, std::vector<int>> Config::make_partition() {
    auto [isLeader, leader] = Leader();
    std::vector<int> p1, p2;
    p1.reserve(n/2 + 1);
    p2.reserve(n/2);

    int j = 0;
    for (int i = 0; i < n; ++i) {
        if (i != leader) {
            if (j < static_cast<int>(p1.size())) {
                p1.push_back(i);
            } else {
                p2.push_back(i);
            }
            ++j;
        }
    }
    p2.push_back(leader);
    return {p1, p2};
}

// Factory
Config* make_config(testing::Test* t, int n, bool unreliable, int maxraftstate) {
    return new Config(n, unreliable, maxraftstate);
}