#define MPT_IMPLEMENTATION
#include "merkle_patricia_trie.h"
#include <iostream>
#include <string>

static void divider(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n  " << title << "\n" << std::string(70, '=') << "\n\n";
}
static void sub(const std::string& title) { std::cout << "\n  --- " << title << " ---\n"; }

int main() {
    std::cout << std::dec;

    // ─── DEMO 1: Ethereum-like account state ───
    divider("DEMO 1: Ethereum-Like Account State Storage");
    MerklePatriciaTrie trie;
    struct AE { std::string addr, state; };
    AE accounts[] = {
        {"0xCAFE01", "{nonce:0, balance:1500 ETH}"},
        {"0xCAFE02", "{nonce:3, balance:42 ETH}"},
        {"0xBEEF01", "{nonce:7, balance:100 ETH}"},
        {"0xBEEF02", "{nonce:1, balance:0.5 ETH}"},
        {"0xDEAD01", "{nonce:0, balance:0 ETH}"},
        {"0x1234AB", "{nonce:12, balance:999 ETH}"},
    };
    sub("Inserting 6 accounts");
    for (int i = 0; i < 6; i++) {
        trie.put(accounts[i].addr, accounts[i].state);
        std::cout << "    PUT  " << accounts[i].addr << "  =>  " << accounts[i].state << "\n";
    }
    Hash root1 = trie.root_hash();
    std::cout << "\n  State Root:\n    " << to_hex(root1) << "\n";

    sub("Lookups");
    const char* lk[] = {"0xCAFE01", "0xBEEF02", "0x999999"};
    for (int i = 0; i < 3; i++) {
        Optional<std::string> v = trie.get(std::string(lk[i]));
        std::cout << "    GET  " << lk[i] << "  =>  " << (v ? v.value() : "(NOT FOUND)") << "\n";
    }
    sub("Trie Structure");
    trie.print_tree();

    // ─── DEMO 2: State root sensitivity ───
    divider("DEMO 2: State Root Sensitivity");
    MerklePatriciaTrie t2;
    t2.put("dog","puppy"); t2.put("horse","stallion"); t2.put("do","verb"); t2.put("doge","coin");
    Hash r2a = t2.root_hash();
    std::cout << "  Root after dog/horse/do/doge:\n    " << to_hex(r2a) << "\n";
    t2.put("dog","canine");
    Hash r2b = t2.root_hash();
    std::cout << "  Root after update dog->canine:\n    " << to_hex(r2b) << "\n";
    std::cout << "  Roots differ: " << (r2a != r2b ? "YES" : "NO") << "\n";
    t2.remove("doge");
    Hash r2c = t2.root_hash();
    std::cout << "  Root after delete doge:\n    " << to_hex(r2c) << "\n";
    Optional<std::string> dv = t2.get("doge");
    std::cout << "  Lookup 'doge': " << (dv ? dv.value() : "(NOT FOUND)") << "\n";

    // ─── DEMO 3: Proof-of-Inclusion ───
    divider("DEMO 3: Proof-of-Inclusion");
    MerklePatriciaTrie t3;
    t3.put("alice","1000 ETH"); t3.put("bob","500 ETH"); t3.put("charlie","250 ETH");
    t3.put("alicia","75 ETH"); t3.put("alex","42 ETH");
    Hash root3 = t3.root_hash();
    std::cout << "  Root: " << to_hex(root3) << "\n\n";

    std::vector<ProofItem> pa = t3.generate_proof("alice");
    std::cout << "  Proof for 'alice' (" << pa.size() << " nodes):\n";
    for (size_t i = 0; i < pa.size(); i++) {
        Hash h = keccak::sha3_256(pa[i].rlp_encoded);
        std::cout << "    Node " << i << ": " << hex_short(h) << "  (" << pa[i].rlp_encoded.size() << "B)\n";
    }
    bool v1 = MerklePatriciaTrie::verify_proof(root3, "alice", "1000 ETH", pa);
    bool v2 = MerklePatriciaTrie::verify_proof(root3, "alice", "9999 ETH", pa);
    Hash fake = root3; fake[0] ^= 0xFF;
    bool v3 = MerklePatriciaTrie::verify_proof(fake, "alice", "1000 ETH", pa);
    std::cout << "  Correct value:  " << (v1 ? "PASS" : "FAIL") << "\n";
    std::cout << "  Wrong value:    " << (v2 ? "PASS (bug!)" : "FAIL (correct!)") << "\n";
    std::cout << "  Tampered root:  " << (v3 ? "PASS (bug!)" : "FAIL (correct!)") << "\n";

    // ─── DEMO 4: Proof invalidation ───
    divider("DEMO 4: Proof Invalidation After State Change");
    Hash rb = t3.root_hash();
    std::vector<ProofItem> pb = t3.generate_proof("charlie");
    bool ob = MerklePatriciaTrie::verify_proof(rb, "charlie", "250 ETH", pb);
    std::cout << "  Before delete: " << (ob ? "VALID" : "INVALID") << "\n";
    t3.remove("charlie");
    Hash ra = t3.root_hash();
    bool oa = MerklePatriciaTrie::verify_proof(ra, "charlie", "250 ETH", pb);
    std::cout << "  Root changed:  " << (rb != ra ? "YES" : "NO") << "\n";
    std::cout << "  Old proof vs new root: " << (oa ? "VALID (bug!)" : "INVALID (correct!)") << "\n";

    // ─── DEMO 5: Determinism ───
    divider("DEMO 5: 20-Entry Determinism Check");
    MerklePatriciaTrie tA, tB;
    struct KV { std::string k, v; };
    KV ds[] = {
        {"apple","fruit"},{"banana","fruit"},{"carrot","vegetable"},{"date","fruit"},
        {"eggplant","vegetable"},{"fig","fruit"},{"grape","fruit"},{"habanero","pepper"},
        {"iceberg","lettuce"},{"jalapeno","pepper"},{"kale","vegetable"},{"lemon","citrus"},
        {"mango","fruit"},{"nectarine","fruit"},{"orange","citrus"},{"papaya","fruit"},
        {"quince","fruit"},{"radish","vegetable"},{"spinach","vegetable"},{"tomato","fruit"},
    };
    for (int i = 0; i < 20; i++) tA.put(ds[i].k, ds[i].v);
    for (int i = 19; i >= 0; i--) tB.put(ds[i].k, ds[i].v);
    Hash hA = tA.root_hash(), hB = tB.root_hash();
    std::cout << "  Trie A: " << to_hex(hA) << "\n";
    std::cout << "  Trie B: " << to_hex(hB) << "\n";
    std::cout << "  Match:  " << (hA == hB ? "YES" : "NO") << "\n";

    divider("ALL DEMOS COMPLETE");
    return 0;
}
