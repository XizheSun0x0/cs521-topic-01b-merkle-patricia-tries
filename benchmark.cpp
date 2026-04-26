#define MPT_IMPLEMENTATION
#include "merkle_patricia_trie.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>

// ─────────────────────────────────────────────────────────────────────────────
//  Timer
// ─────────────────────────────────────────────────────────────────────────────
struct Timer {
    std::chrono::high_resolution_clock::time_point start_;
    void start() { start_ = std::chrono::high_resolution_clock::now(); }
    double elapsed_us() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
    }
    double elapsed_ms() const { return elapsed_us() / 1000.0; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Key/value generators
// ─────────────────────────────────────────────────────────────────────────────
static std::string make_key(int i) {
    // Simulate realistic keys: hex-encoded "addresses"
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%08x", i * 2654435761u); // Knuth multiplicative hash for spread
    return std::string(buf);
}

static std::string make_value(int i) {
    char buf[64];
    snprintf(buf, sizeof(buf), "{nonce:%d,balance:%d}", i % 100, i * 7);
    return std::string(buf);
}




// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << std::fixed;
    std::cout << "Benchmarking Merkle Patricia Trie with 100,000 entries...\n";
    const int num_entries = 100000; 
    std::vector<std::string> keys(num_entries), values(num_entries);
    for (int i = 0; i < num_entries; i++) {
        keys[i] = make_key(i);
        values[i] = make_value(i);
    }  
    //verify Timer, make_key, make_value
    {
        Timer t;
        t.start();
        for (int i = 0; i < 1000; i++) {
            make_key(i);
            make_value(i);
        }
        std::cout << "Key/value generation: " << t.elapsed_ms() << " ms\n";
    } 
    return 0;
}