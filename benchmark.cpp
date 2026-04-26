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


struct ScaleResult {
    int n;
    double insert_us, lookup_us, delete_us;
    double root_hash_us, proof_gen_us, proof_verify_us;
    double avg_proof_depth;
    double avg_proof_bytes;
};

static ScaleResult bench_at_scale(int n) {
    ScaleResult res;
    res.n = n;
    Timer t;

    // Pre-generate keys/values
    std::vector<std::string> keys(n), vals(n);
    for (int i = 0; i < n; i++) {
        keys[i] = make_key(i);
        vals[i] = make_value(i);
    }

    MerklePatriciaTrie trie;

    // ── INSERT ──
    t.start();
    for (int i = 0; i < n; i++) trie.put(keys[i], vals[i]);
    double insert_total = t.elapsed_us();
    res.insert_us = insert_total / n;

    // ── ROOT HASH (on full trie) ──
    // Invalidate cache by doing a dummy put + remove
    trie.put("__benchmark_dummy__", "x");
    trie.remove("__benchmark_dummy__");

    t.start();
    Hash root = trie.root_hash();
    double root_total = t.elapsed_us();
    res.root_hash_us = root_total;

    // ── LOOKUP (all keys) ──
    t.start();
    for (int i = 0; i < n; i++) {
        Optional<std::string> v = trie.get(keys[i]);
        if (!v) {
            std::cerr << "ERROR: key not found during lookup benchmark\n";
            std::exit(1);
        }
    }
    double lookup_total = t.elapsed_us();
    res.lookup_us = lookup_total / n;

    // ── PROOF GENERATION ──
    // Sample min(n, 500) keys for proof benchmarks
    int sample = n < 500 ? n : 500;
    std::vector<double> proof_depths(sample), proof_sizes(sample);
    t.start();
    for (int i = 0; i < sample; i++) {
        int idx = (i * 7) % n; // pseudo-random sampling
        std::vector<ProofItem> proof = trie.generate_proof(keys[idx]);
        proof_depths[i] = proof.size();
        size_t total_bytes = 0;
        for (size_t j = 0; j < proof.size(); j++) total_bytes += proof[j].rlp_encoded.size();
        proof_sizes[i] = total_bytes;
    }
    double proof_gen_total = t.elapsed_us();
    res.proof_gen_us = proof_gen_total / sample;
    res.avg_proof_depth = std::accumulate(proof_depths.begin(), proof_depths.end(), 0.0) / sample;
    res.avg_proof_bytes = std::accumulate(proof_sizes.begin(), proof_sizes.end(), 0.0) / sample;

    // ── PROOF VERIFICATION ──
    t.start();
    for (int i = 0; i < sample; i++) {
        int idx = (i * 7) % n;
        std::vector<ProofItem> proof = trie.generate_proof(keys[idx]);
        MerklePatriciaTrie::verify_proof(root, keys[idx], vals[idx], proof);
    }
    double proof_verify_total = t.elapsed_us();
    // Subtract generation time to isolate verification
    res.proof_verify_us = (proof_verify_total - proof_gen_total) / sample;
    if (res.proof_verify_us < 0) res.proof_verify_us = proof_verify_total / sample; // fallback

    // ── DELETE (all keys) ──
    t.start();
    for (int i = 0; i < n; i++) trie.remove(keys[i]);
    double delete_total = t.elapsed_us();
    res.delete_us = delete_total / n;

    // Verify trie is empty
    for (int i = 0; i < n; i++) {
        Optional<std::string> v = trie.get(keys[i]);
        if (v) {
            std::cerr << "ERROR: key still present after delete benchmark\n";
            std::exit(1);
        }
    }

    return res;
}

static void bench_keccak() {
    print_header("BENCHMARK: Keccak-256 Raw Throughput");

    int sizes[] = {32, 64, 128, 256, 1024};
    int reps = 10000;
    print_table_header();
    for (int s = 0; s < 5; s++) {
        std::vector<uint8_t> data(sizes[s], 0xAB);
        Timer t;
        t.start();
        for (int i = 0; i < reps; i++) {
            Hash h = keccak::sha3_256(data);
            data[0] = h[0]; // prevent optimization
        }
        double total = t.elapsed_us();
        char label[32];
        snprintf(label, sizeof(label), "Keccak-%dB input", sizes[s]);
        print_row(label, reps, total / 1000.0, total / reps);
    }
}

static void bench_determinism() {
    print_header("BENCHMARK: Insertion Order Determinism");

    int sizes[] = {100, 1000, 5000};
    for (int si = 0; si < 3; si++) {
        int n = sizes[si];
        std::vector<std::string> keys(n), vals(n);
        for (int i = 0; i < n; i++) { keys[i] = make_key(i); vals[i] = make_value(i); }

        // Forward order
        MerklePatriciaTrie tA;
        for (int i = 0; i < n; i++) tA.put(keys[i], vals[i]);

        // Reverse order
        MerklePatriciaTrie tB;
        for (int i = n - 1; i >= 0; i--) tB.put(keys[i], vals[i]);

        // Random order (Fisher-Yates)
        std::vector<int> perm(n);
        for (int i = 0; i < n; i++) perm[i] = i;
        for (int i = n - 1; i > 0; i--) std::swap(perm[i], perm[(i * 2654435761u) % (i + 1)]);
        MerklePatriciaTrie tC;
        for (int i = 0; i < n; i++) tC.put(keys[perm[i]], vals[perm[i]]);

        Hash hA = tA.root_hash(), hB = tB.root_hash(), hC = tC.root_hash();
        bool match = (hA == hB) && (hB == hC);
        std::cout << "    n=" << std::setw(5) << n
                  << "  forward=reverse=shuffled: "
                  << (match ? "PASS (all identical)" : "FAIL (mismatch!)")
                  << "  root=" << to_hex(hA).substr(0, 16) << "...\n";
    }
}

static void bench_proof_negative() {
    print_header("BENCHMARK: Proof Security — Negative Tests");

    MerklePatriciaTrie trie;
    for (int i = 0; i < 1000; i++) trie.put(make_key(i), make_value(i));

    Hash root = trie.root_hash();
    std::string test_key = make_key(42);
    std::string correct_val = make_value(42);
    std::vector<ProofItem> proof = trie.generate_proof(test_key);

    // 1. Correct proof
    bool v1 = MerklePatriciaTrie::verify_proof(root, test_key, correct_val, proof);
    std::cout << "    Correct proof:          " << (v1 ? "PASS" : "FAIL") << "\n";

    // 2. Wrong value
    bool v2 = MerklePatriciaTrie::verify_proof(root, test_key, "WRONG_VALUE", proof);
    std::cout << "    Wrong value:            " << (!v2 ? "PASS (rejected)" : "FAIL (accepted!)") << "\n";

    // 3. Tampered root
    Hash fake_root = root;
    fake_root[0] ^= 0xFF;
    bool v3 = MerklePatriciaTrie::verify_proof(fake_root, test_key, correct_val, proof);
    std::cout << "    Tampered root:          " << (!v3 ? "PASS (rejected)" : "FAIL (accepted!)") << "\n";

    // 4. Empty proof
    bool v4 = MerklePatriciaTrie::verify_proof(root, test_key, correct_val, {});
    std::cout << "    Empty proof:            " << (!v4 ? "PASS (rejected)" : "FAIL (accepted!)") << "\n";

    // 5. Proof for wrong key
    std::string other_key = make_key(99);
    bool v5 = MerklePatriciaTrie::verify_proof(root, other_key, correct_val, proof);
    std::cout << "    Wrong key's proof:      " << (!v5 ? "PASS (rejected)" : "FAIL (accepted!)") << "\n";

    // 6. Stale proof after mutation
    trie.put(make_key(42), "UPDATED");
    Hash new_root = trie.root_hash();
    bool v6 = MerklePatriciaTrie::verify_proof(new_root, test_key, correct_val, proof);
    std::cout << "    Stale proof (post-mut): " << (!v6 ? "PASS (rejected)" : "FAIL (accepted!)") << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << std::fixed;
 
    // ── Keccak raw speed ──
    bench_keccak();
 
    // ── Scaling benchmarks ──
    int scales[] = {100, 500, 1000, 2000, 5000, 10000};
    int num_scales = 6;
 
    print_header("BENCHMARK: Insert / Lookup / Delete Throughput");
    print_table_header();
    std::vector<ScaleResult> results;
    for (int si = 0; si < num_scales; si++) {
        ScaleResult r = bench_at_scale(scales[si]);
        results.push_back(r);
        char label[32];
        snprintf(label, sizeof(label), "Insert (n=%d)", r.n);
        print_row(label, r.n, r.insert_us * r.n / 1000.0, r.insert_us);
        snprintf(label, sizeof(label), "Lookup (n=%d)", r.n);
        print_row(label, r.n, r.lookup_us * r.n / 1000.0, r.lookup_us);
        snprintf(label, sizeof(label), "Delete (n=%d)", r.n);
        print_row(label, r.n, r.delete_us * r.n / 1000.0, r.delete_us);
        std::cout << "\n";
    }
 
    // ── Root hash cost ──
    print_header("BENCHMARK: Root Hash Computation (Full Trie)");
    std::cout << "    " << std::left << std::setw(16) << "Trie Size"
              << std::right << std::setw(14) << "Root Hash"
              << "\n";
    std::cout << "    " << std::string(30, '-') << "\n";
    for (size_t i = 0; i < results.size(); i++) {
        std::cout << "    " << std::left << std::setw(16) << results[i].n
                  << std::right << std::setw(10) << std::setprecision(2) << results[i].root_hash_us << " us\n";
    }
 
    // ── Proof stats ──
    print_header("BENCHMARK: Proof Generation & Verification");
    std::cout << "    " << std::left << std::setw(10) << "Size"
              << std::right << std::setw(12) << "Gen (us)"
              << std::setw(12) << "Verify (us)"
              << std::setw(10) << "Depth"
              << std::setw(14) << "Proof Bytes"
              << "\n";
    std::cout << "    " << std::string(58, '-') << "\n";
    for (size_t i = 0; i < results.size(); i++) {
        std::cout << "    " << std::left << std::setw(10) << results[i].n
                  << std::right << std::setw(12) << std::setprecision(2) << results[i].proof_gen_us
                  << std::setw(12) << std::setprecision(2) << results[i].proof_verify_us
                  << std::setw(10) << std::setprecision(1) << results[i].avg_proof_depth
                  << std::setw(14) << std::setprecision(1) << results[i].avg_proof_bytes
                  << "\n";
    }
 
    // ── Scaling summary ──
    print_header("BENCHMARK: Scaling Summary (us per operation)");
    std::cout << "    " << std::left << std::setw(8) << "n"
              << std::right << std::setw(10) << "Insert"
              << std::setw(10) << "Lookup"
              << std::setw(10) << "Delete"
              << std::setw(12) << "RootHash"
              << std::setw(10) << "ProofGen"
              << std::setw(8) << "Depth"
              << "\n";
    std::cout << "    " << std::string(68, '-') << "\n";
    for (size_t i = 0; i < results.size(); i++) {
        const ScaleResult& r = results[i];
        std::cout << "    " << std::left << std::setw(8) << r.n
                  << std::right
                  << std::setw(10) << std::setprecision(2) << r.insert_us
                  << std::setw(10) << std::setprecision(2) << r.lookup_us
                  << std::setw(10) << std::setprecision(2) << r.delete_us
                  << std::setw(12) << std::setprecision(2) << r.root_hash_us
                  << std::setw(10) << std::setprecision(2) << r.proof_gen_us
                  << std::setw(8) << std::setprecision(1) << r.avg_proof_depth
                  << "\n";
    }
 
    // ── Determinism ──
    bench_determinism();
 
    // ── Security ──
    bench_proof_negative();
 
    // ── Summary ──
    print_header("BENCHMARK COMPLETE");
    std::cout << "  Measured across " << num_scales << " scales from "
              << scales[0] << " to " << scales[num_scales-1] << " entries.\n\n"
              << "  What to look for:\n"
              << "    - Insert/Lookup/Delete should scale ~O(key_length), not O(n)\n"
              << "    - Proof depth should grow logarithmically (~4-8 for 10K entries)\n"
              << "    - Proof bytes should stay small (<1KB) regardless of trie size\n"
              << "    - Root hash is O(n) on first compute, cached on subsequent\n"
              << "    - All negative proof tests must PASS (reject invalid proofs)\n"
              << "    - Determinism must hold across insertion orders\n\n";
 
    return 0;
}