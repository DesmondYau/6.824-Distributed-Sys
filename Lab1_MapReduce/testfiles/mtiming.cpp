#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <csignal>

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
        int xpid;
        std::string pat = "mr-worker-" + phase + "-";
        if (name.rfind(pat, 0) == 0) {
            // extract PID after prefix
            try {
                xpid = std::stoi(name.substr(pat.size()));
                // check if process is alive
                if (kill(xpid, 0) == 0) {
                    ret += 1;
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

// Map function
extern "C" std::vector<KeyValue> Map(const std::string &filename, const std::string &contents) {
    auto t0 = std::chrono::system_clock::now();
    double ts = static_cast<double>(std::chrono::system_clock::to_time_t(t0))
                + (static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t0.time_since_epoch()).count() % 1000000000) / 1e9);
    pid_t pid = getpid();

    int n = nparallel("map");

    std::vector<KeyValue> kva;
    kva.push_back({"times-" + std::to_string(pid), std::to_string(ts)});
    kva.push_back({"parallel-" + std::to_string(pid), std::to_string(n)});
    return kva;
}

// Reduce function
extern "C" std::string Reduce(const std::string &key, const std::vector<std::string> &values) {
    std::vector<std::string> vv = values;
    std::sort(vv.begin(), vv.end());

    std::string result;
    for (std::size_t i = 0; i < vv.size(); i++) {
        if (i > 0) result += " ";
        result += vv[i];
    }
    return result;
}
