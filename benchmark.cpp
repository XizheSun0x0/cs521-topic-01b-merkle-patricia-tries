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
//  Pretty printing
// ─────────────────────────────────────────────────────────────────────────────
static void print_header(const std::string& title) {
    std::cout << "\n" << std::string(78, '=') << "\n  " << title << "\n" << std::string(78, '=') << "\n\n";
}

static void print_row(const std::string& label, int n, double total_ms, double per_op_us) {
    double ops_sec = (per_op_us > 0) ? 1000000.0 / per_op_us : 0;
    std::cout << "    " << std::left << std::setw(24) << label
              << std::right << std::setw(7) << n << " ops"
              << std::setw(12) << std::fixed << std::setprecision(2) << total_ms << " ms"
              << std::setw(10) << std::setprecision(2) << per_op_us << " us/op"
              << std::setw(12) << std::setprecision(0) << ops_sec << " ops/s"
              << "\n";
}

static void print_table_header() {
    std::cout << "    " << std::left << std::setw(24) << "Operation"
              << std::right << std::setw(10) << "Count"
              << std::setw(12) << "Total"
              << std::setw(10) << "Per-op"
              << std::setw(12) << "Throughput"
              << "\n";
    std::cout << "    " << std::string(68, '-') << "\n";
}


// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << std::fixed;
    //test all function in this benchmark
    print_header("Merkle Patricia Trie Benchmark");
    const int N = 100000;
    std::vector<std::string> keys(N), values(N);
    for (int i = 0; i < N; i++) {
        keys[i] = make_key(i);
        values[i] = make_value(i);
    }  
    MerklePatriciaTrie trie;    
    print_table_header();
    print_row("Insert", N, [&]() {
        Timer t;
        t.start();
        for (int i = 0; i < N; i++) {
            trie.put(keys[i], values[i]);
        }
        return t.elapsed_ms();
    }(), 0); // per-op will be calculated later
    return 0;
}