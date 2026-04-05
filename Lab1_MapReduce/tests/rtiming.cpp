#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <dirent.h>
#include <unistd.h>
#include <csignal>

// KeyValue struct
struct KeyValue {
    std::string key;
    std::string value;
};

// helper: check how many workers are alive in this phase
int nparallel(const std::string &phase) {
    pid_t pid = getpid();
    std::string myfilename = "mr-worker-" + phase + "-" + std::to_string(pid);

    // create marker file
    std::ofstream out(myfilename);
    out << "x";
    out.close();

    // scan directory for other worker marker files
    DIR *dir = opendir(".");
    if (!dir) {
        std::cerr << "Cannot open current directory" << std::endl;
        return 0;
    }

    int ret = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        std::string pat = "mr-worker-" + phase + "-";
        if (name.rfind(pat, 0) == 0) {
            try {
                int xpid = std::stoi(name.substr(pat.size()));
                if (kill(xpid, 0) == 0) {
                    ret += 1; // process exists and is alive
                }
            } catch (...) {
                // ignore parse errors
            }
        }
    }
    closedir(dir);

    // sleep briefly to overlap with other workers
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // remove marker file
    std::remove(myfilename.c_str());

    return ret;
}

// Map function: emits fixed keys
extern "C" std::vector<KeyValue> Map(const std::string &filename, const std::string &contents) {
    std::vector<KeyValue> kva;
    kva.push_back({"a", "1"});
    kva.push_back({"b", "1"});
    kva.push_back({"c", "1"});
    kva.push_back({"d", "1"});
    kva.push_back({"e", "1"});
    kva.push_back({"f", "1"});
    kva.push_back({"g", "1"});
    kva.push_back({"h", "1"});
    kva.push_back({"i", "1"});
    kva.push_back({"j", "1"});
    return kva;
}

// Reduce function: reports parallelism
extern "C" std::string Reduce(const std::string &key, const std::vector<std::string> &values) {
    int n = nparallel("reduce");
    return std::to_string(n);
}
