#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>

struct KeyValue {
    std::string key;
    std::string value;
};

// simulate crash or delay
void maybeCrash() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 999);
    int rr = dist(gen);

    if (rr < 330) {
        // crash
        std::cerr << "Worker crashed!" << std::endl;
        exit(1);
    } else if (rr < 660) {
        // delay
        std::uniform_int_distribution<> delayDist(0, 10000); // up to 10s
        int ms = delayDist(gen);
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

// Map function: takes filename and contents
extern "C" std::vector<KeyValue> Map(const std::string &filename, const std::string &contents) {
    maybeCrash();

    std::vector<KeyValue> kva;
    kva.push_back({"a", filename});
    kva.push_back({"b", std::to_string(filename.size())});
    kva.push_back({"c", std::to_string(contents.size())});
    kva.push_back({"d", "xyzzy"});
    return kva;
}

// Reduce function: takes key and list of values
extern "C" std::string Reduce(const std::string &key, const std::vector<std::string> &values) {
    maybeCrash();

    std::vector<std::string> vv = values;
    std::sort(vv.begin(), vv.end());
    
    std::string result;
    for (size_t i = 0; i < vv.size(); i++) {
        if (i > 0) result += " ";
        result += vv[i];
    }
    return result;
}