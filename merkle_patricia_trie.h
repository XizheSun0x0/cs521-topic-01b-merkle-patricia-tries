#ifndef MERKLE_PATRICIA_TRIE_H
#define MERKLE_PATRICIA_TRIE_H

/*
 * Merkle Patricia Trie — Header-Only Library
 * Extracted from merkle_patricia_trie.cpp
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

typedef std::vector<uint8_t> Bytes;
typedef std::vector<uint8_t> Hash;
typedef std::vector<uint8_t> Nibbles;

template <typename T>
class Optional
{
public:
    Optional() : has_(false), val_() {}
    Optional(const T &v) : has_(true), val_(v) {}
    bool has_value() const { return has_; }
    explicit operator bool() const { return has_; }
    const T &value() const { return val_; }
    const T &operator*() const { return val_; }
    static Optional none() { return Optional(); }

private:
    bool has_;
    T val_;
};

namespace keccak
{
    static const uint64_t RC[24] = {
        0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808AULL,
        0x8000000080008000ULL, 0x000000000000808BULL, 0x0000000080000001ULL,
        0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008AULL,
        0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000AULL,
        0x000000008000808BULL, 0x800000000000008BULL, 0x8000000000008089ULL,
        0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
        0x000000000000800AULL, 0x800000008000000AULL, 0x8000000080008081ULL,
        0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};
    static inline uint64_t rotl64(uint64_t x, int y) { return (x << y) | (x >> (64 - y)); }
    static inline void keccakf(uint64_t st[25])
    {
        for (int round = 0; round < 24; ++round)
        {
            uint64_t C[5], D[5];
            for (int i = 0; i < 5; i++)
                C[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
            for (int i = 0; i < 5; i++)
            {
                D[i] = C[(i + 4) % 5] ^ rotl64(C[(i + 1) % 5], 1);
                for (int j = 0; j < 25; j += 5)
                    st[j + i] ^= D[i];
            }
            uint64_t t = st[1], tmp;
            int idx;
            static const int piln[24] = {10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1};
            static const int rotc[24] = {1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44};
            for (int i = 0; i < 24; i++)
            {
                idx = piln[i];
                tmp = st[idx];
                st[idx] = rotl64(t, rotc[i]);
                t = tmp;
            }
            for (int j = 0; j < 25; j += 5)
            {
                uint64_t bc[5];
                for (int i = 0; i < 5; i++)
                    bc[i] = st[j + i];
                for (int i = 0; i < 5; i++)
                    st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
            }
            st[0] ^= RC[round];
        }
    }
    static inline std::vector<uint8_t> sha3_256(const std::vector<uint8_t> &input)
    {
        const size_t rate = 136;
        uint64_t st[25];
        std::memset(st, 0, sizeof(st));
        size_t pt = 0;
        for (size_t b = 0; b < input.size(); b++)
        {
            reinterpret_cast<uint8_t *>(st)[pt++] ^= input[b];
            if (pt >= rate)
            {
                keccakf(st);
                pt = 0;
            }
        }
        reinterpret_cast<uint8_t *>(st)[pt] ^= 0x06;
        reinterpret_cast<uint8_t *>(st)[rate - 1] ^= 0x80;
        keccakf(st);
        std::vector<uint8_t> out(32);
        std::memcpy(out.data(), st, 32);
        return out;
    }
}

namespace rlp
{
    static inline Bytes encode_length(size_t len, uint8_t offset)
    {
        if (len < 56)
            return Bytes(1, static_cast<uint8_t>(len + offset));
        Bytes bl;
        size_t tmp = len;
        while (tmp > 0)
        {
            bl.insert(bl.begin(), static_cast<uint8_t>(tmp & 0xFF));
            tmp >>= 8;
        }
        bl.insert(bl.begin(), static_cast<uint8_t>(offset + 55 + bl.size()));
        return bl;
    }
    static inline Bytes encode_string(const Bytes &s)
    {
        if (s.size() == 1 && s[0] < 0x80)
            return s;
        Bytes out = encode_length(s.size(), 0x80);
        out.insert(out.end(), s.begin(), s.end());
        return out;
    }
    static inline Bytes encode_list(const std::vector<Bytes> &items)
    {
        Bytes payload;
        for (size_t i = 0; i < items.size(); i++)
            payload.insert(payload.end(), items[i].begin(), items[i].end());
        Bytes out = encode_length(payload.size(), 0xC0);
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

    static void decode_string (const Bytes& s, Bytes& result) {
        result.clear();

        // Check whether s is a string
        if (!((s.at(0) >= 0x80 && s.at(0) <= 0xBF) || (s.at(0) < 0x80 && s.size() == 1))) {
            std::cerr << "decode_string error: first argument not a string" << std::endl;

            return;
        }

        if (s.at(0) < 0x80) {
            // Single byte
            result.push_back(s.at(0));
        } else if (s.at(0) <= 0xB7) {
            // Short string
            int str_len = s.at(0) - 0x80;

            if (s.size() - 1 != str_len) {
                std::cerr << "Bad encoded string length" << std::endl;

                return;
            }

            result.assign(s.begin() + 1, s.end());
        } else {
            size_t str_len = 0;

            int len_byte_num = s.at(0) - 0xB7;

            size_t length_byte_idx = 1;
            int counter = 1;
            while (counter <= len_byte_num) {
                str_len = (str_len << 8) | (s.at(length_byte_idx));

                length_byte_idx++;
                counter++;
            }

            if (s.size() - 1 - len_byte_num != str_len) {
                std::cerr << "Bad encoded string length" << std::endl;

                return;
            }

            result.assign(s.begin() + len_byte_num + 1, s.end());
        }
    }

    static void decode_rlp_list(const Bytes & list, std::vector<Bytes>& out) {
        out.clear();

        // First byte should >= 0xC0 so we know that it is a list
        if (!(list.at(0) >= 0xC0 && list.at(0) <= 0xFF)) {
            std::cerr << __func__ << ": first argument is not a list" << std::endl;
            std::cerr << "Get first byte: " << std::hex << (int)list.at(0) << std::endl;

            return;
        }

        // Signaling whether it is a long list or a short list
        uint8_t prefix_byte = list.at(0);

        bool is_short_list = (prefix_byte <= 0xF7);

        size_t list_content_length = 0; 
        // This does NOT include the prefix byte or the length bytes
        // Note: size_t must be 8 bytes
        
        int length_bytes_n;
        // This is only used for long lists

        // The byte index from which the real content starts
        size_t content_start_byte_idx;

        // Index used in all for loops
        int byte_idx;

        // Calculate list length
        if (is_short_list) {
            // Short list
            list_content_length = prefix_byte - 0xC0;
            content_start_byte_idx = 1;
        } else {
            // Long list
            length_bytes_n = prefix_byte - 0xF7;

            for (byte_idx = 1; byte_idx <= length_bytes_n; byte_idx++) {
                list_content_length = (list_content_length << 8) | (list.at(byte_idx));
            }

            content_start_byte_idx = byte_idx;
        }

        // Check the encoded length is correct
        if (is_short_list) {
            // Short list
            if (list.size() - 1 != list_content_length) {
                std::cerr << "Wrong short list length in encoding" << std::endl;
                
                exit(1);
            }
        } else {
            // Long list
            if (list.size() - 1 - length_bytes_n != list_content_length) {
                std::cerr << "Wrong long list length in encoding" << std::endl;

                exit(1);
            }
        }

        // Extract item from list, one by one, and compare it
        // with the provided item
        for (byte_idx = content_start_byte_idx; byte_idx < list.size();) {
            // Extract item

            if (list.at(byte_idx) < 0x80) {
                // 0x00 - 0x7F: Single byte
                out.push_back(Bytes(list.begin() + byte_idx, list.begin() + byte_idx + 1));
                // cur_item.assign(list.begin() + byte_idx, list.begin() + byte_idx + 1);

                byte_idx++;
            } else if (list.at(byte_idx) <= 0xB7) {
                // 0x80 - 0xB7: Short string
                int str_len = list.at(byte_idx) - 0x80;

                out.push_back(Bytes(list.begin() + byte_idx, list.begin() + byte_idx + str_len + 1));
                // cur_item.assign(list.begin() + byte_idx, list.begin() + byte_idx + str_len + 1);

                byte_idx += (str_len + 1);
            } else if (list.at(byte_idx) < 0xC0) {
                // 0xB8 - 0xBF: Long string
                size_t str_len = 0;

                int len_byte_num = list.at(byte_idx) - 0xB7;

                size_t length_byte_idx = byte_idx + 1;
                int counter = 1;
                while (counter <= len_byte_num) {
                    str_len = (str_len << 8) | (list.at(length_byte_idx));

                    length_byte_idx++;
                    counter++;
                }

                out.push_back(Bytes(list.begin() + byte_idx, list.begin() + byte_idx + len_byte_num + 1 + str_len));
                // cur_item.assign(list.begin() + byte_idx, list.begin() + byte_idx + len_byte_num + 1 + str_len);

                byte_idx += (1 + len_byte_num + str_len);
            } else if (list.at(byte_idx) <= 0xF7) {
                // 0xC0 - 0xF7: Short List
                size_t list_member_len = list.at(byte_idx) - 0xC0;

                out.push_back(Bytes(list.begin() + byte_idx, list.begin() + byte_idx + list_member_len + 1));
                // cur_item.assign(list.begin() + byte_idx, list.begin() + byte_idx + list_member_len + 1);

                byte_idx += (list_member_len + 1);
            } else if (list.at(byte_idx) <= 0xFF) {
                // 0xF8 - 0xFF: Long List
                size_t list_member_len = 0;

                int len_byte_num = list.at(byte_idx) - 0xF7;

                size_t length_byte_idx = byte_idx + 1;
                int counter = 1;
                while (counter <= len_byte_num) {
                    list_member_len = (list_member_len << 8) | (list.at(length_byte_idx));

                    length_byte_idx++;
                    counter++;
                }

                out.push_back(Bytes(list.begin() + byte_idx, list.begin() + byte_idx + len_byte_num + 1 + list_member_len));
                // cur_item.assign(list.begin() + byte_idx, list.begin() + byte_idx + len_byte_num + 1 + list_member_len);

                byte_idx += (1 + len_byte_num + list_member_len);
            }
        }
    }
}

static inline std::string to_hex(const Bytes &data)
{
    std::ostringstream ss;
    for (size_t i = 0; i < data.size(); i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    return ss.str();
}
static inline std::string hex_short(const Bytes &data, int n = 8)
{
    std::string h = to_hex(data);
    if ((int)h.size() <= n * 2)
        return h;
    return h.substr(0, n) + "..." + h.substr(h.size() - n);
}
static inline Nibbles to_nibbles(const Bytes &key)
{
    Nibbles n;
    n.reserve(key.size() * 2);
    for (size_t i = 0; i < key.size(); i++)
    {
        n.push_back((key[i] >> 4) & 0x0F);
        n.push_back(key[i] & 0x0F);
    }
    return n;
}
static inline Bytes hex_prefix_encode(const Nibbles &nibbles, bool is_leaf)
{
    int flag = is_leaf ? 2 : 0;
    bool odd = (nibbles.size() % 2) != 0;
    Bytes result;
    if (odd)
    {
        result.push_back(static_cast<uint8_t>((flag + 1) << 4 | nibbles[0]));
        for (size_t i = 1; i < nibbles.size(); i += 2)
            result.push_back(static_cast<uint8_t>((nibbles[i] << 4) | nibbles[i + 1]));
    }
    else
    {
        result.push_back(static_cast<uint8_t>(flag << 4));
        for (size_t i = 0; i < nibbles.size(); i += 2)
            result.push_back(static_cast<uint8_t>((nibbles[i] << 4) | nibbles[i + 1]));
    }
    return result;
}
static inline size_t shared_prefix_len(const Nibbles &a, const Nibbles &b)
{
    size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i])
            return i;
    return n;
}

enum NodeType
{
    NODE_NULL,
    NODE_LEAF,
    NODE_EXTENSION,
    NODE_BRANCH
};
struct TrieNode;
typedef std::shared_ptr<TrieNode> NodePtr;

struct TrieNode
{
    NodeType type;
    Nibbles partial;
    Bytes value;
    NodePtr children[16];
    Bytes branch_value;
    NodePtr next;
    mutable Hash cached_hash;
    int node_id;
    static int next_id;
    TrieNode() : type(NODE_NULL), next(), node_id(next_id++)
    {
        for (int i = 0; i < 16; i++)
            children[i] = NodePtr();
    }
    void invalidate() { cached_hash.clear(); }
    static NodePtr make_null() { return std::make_shared<TrieNode>(); }
    static NodePtr make_leaf(Nibbles p, Bytes v)
    {
        NodePtr n = std::make_shared<TrieNode>();
        n->type = NODE_LEAF;
        n->partial = p;
        n->value = v;
        return n;
    }
    static NodePtr make_extension(Nibbles p, NodePtr c)
    {
        NodePtr n = std::make_shared<TrieNode>();
        n->type = NODE_EXTENSION;
        n->partial = p;
        n->next = c;
        return n;
    }
    static NodePtr make_branch()
    {
        NodePtr n = std::make_shared<TrieNode>();
        n->type = NODE_BRANCH;
        return n;
    }
};

struct ProofItem
{
    Bytes rlp_encoded;
};

class MerklePatriciaTrie
{
public:
    MerklePatriciaTrie() : root_(TrieNode::make_null()) {}
    /** Inserts or updates a key-value pair in the trie. */
    void put(const std::string &key, const std::string &value) { root_ = insert(root_, to_nibbles(Bytes(key.begin(), key.end())), 0, Bytes(value.begin(), value.end())); }
    /** Looks up a key in the trie. */
    Optional<std::string> get(const std::string &key) const
    {
        Optional<Bytes> r = lookup(root_, to_nibbles(Bytes(key.begin(), key.end())), 0);
        if (!r)
            return Optional<std::string>::none();
        return Optional<std::string>(std::string(r.value().begin(), r.value().end()));
    }
    /** Removes a key from the trie. */
    bool remove(const std::string &key)
    {
        bool f = false;
        root_ = erase(root_, to_nibbles(Bytes(key.begin(), key.end())), 0, f);
        return f;
    }
    /** Returns the root hash of the trie. */
    Hash root_hash() const { return hash_node(root_); }
    /** Returns the root hash of the trie as a hexadecimal string. */
    std::string root_hash_hex() const { return to_hex(root_hash()); }
    /** Builds a proof for a key in the trie. */
    std::vector<ProofItem> generate_proof(const std::string &key) const
    {
        std::vector<ProofItem> p;
        build_proof(root_, to_nibbles(Bytes(key.begin(), key.end())), 0, p);
        return p;
    }
    // ── RLP Decoder ──────────────────────────────────────────────────────
    // Decode RLP data starting at data[pos]. Returns the payload bytes and
    // advances pos past the item. Returns false on malformed input.
    static bool rlp_decode_item(const Bytes& data, size_t& pos, Bytes& out) {
        if (pos >= data.size()) return false;
        uint8_t prefix = data[pos];

        if (prefix < 0x80) {
            // Single byte
            out.assign(1, prefix);
            pos += 1;
            return true;
        }
        if (prefix <= 0xb7) {
            // Short string: 0-55 bytes
            size_t len = prefix - 0x80;
            pos += 1;
            if (pos + len > data.size()) return false;
            out.assign(data.begin() + pos, data.begin() + pos + len);
            pos += len;
            return true;
        }
        if (prefix <= 0xbf) {
            // Long string: length-of-length follows
            size_t ll = prefix - 0xb7;
            pos += 1;
            if (pos + ll > data.size()) return false;
            size_t len = 0;
            for (size_t i = 0; i < ll; i++) len = (len << 8) | data[pos + i];
            pos += ll;
            if (pos + len > data.size()) return false;
            out.assign(data.begin() + pos, data.begin() + pos + len);
            pos += len;
            return true;
        }
        if (prefix <= 0xf7) {
            // Short list: payload length in prefix
            size_t len = prefix - 0xc0;
            pos += 1;
            if (pos + len > data.size()) return false;
            out.assign(data.begin() + pos, data.begin() + pos + len);
            pos += len;
            return true;
        }
        // Long list: length-of-length follows
        size_t ll = prefix - 0xf7;
        pos += 1;
        if (pos + ll > data.size()) return false;
        size_t len = 0;
        for (size_t i = 0; i < ll; i++) len = (len << 8) | data[pos + i];
        pos += ll;
        if (pos + len > data.size()) return false;
        out.assign(data.begin() + pos, data.begin() + pos + len);
        pos += len;
        return true;
    }

    // Decode an RLP list into its constituent items (each still RLP-wrapped)
    static bool rlp_decode_list(const Bytes& data, std::vector<Bytes>& items) {
        if (data.empty()) return false;
        uint8_t prefix = data[0];

        // Must be a list prefix
        if (prefix < 0xc0) return false;

        // Extract the list payload
        size_t payload_start, payload_len;
        if (prefix <= 0xf7) {
            payload_len = prefix - 0xc0;
            payload_start = 1;
        } else {
            size_t ll = prefix - 0xf7;
            if (1 + ll > data.size()) return false;
            payload_len = 0;
            for (size_t i = 0; i < ll; i++) payload_len = (payload_len << 8) | data[1 + i];
            payload_start = 1 + ll;
        }

        if (payload_start + payload_len > data.size()) return false;

        // Decode each item within the payload
        // We need the raw RLP bytes of each item (not just the payload)
        // so we can check child references
        size_t pos = payload_start;
        size_t end = payload_start + payload_len;
        while (pos < end) {
            size_t item_start = pos;
            // Skip over this item to find its extent
            uint8_t p = data[pos];
            size_t item_len;
            if (p < 0x80) {
                item_len = 1;
            } else if (p <= 0xb7) {
                item_len = 1 + (p - 0x80);
            } else if (p <= 0xbf) {
                size_t llen = p - 0xb7;
                if (pos + 1 + llen > end) return false;
                size_t slen = 0;
                for (size_t i = 0; i < llen; i++) slen = (slen << 8) | data[pos + 1 + i];
                item_len = 1 + llen + slen;
            } else if (p <= 0xf7) {
                item_len = 1 + (p - 0xc0);
            } else {
                size_t llen = p - 0xf7;
                if (pos + 1 + llen > end) return false;
                size_t slen = 0;
                for (size_t i = 0; i < llen; i++) slen = (slen << 8) | data[pos + 1 + i];
                item_len = 1 + llen + slen;
            }
            if (pos + item_len > end) return false;
            items.push_back(Bytes(data.begin() + item_start, data.begin() + item_start + item_len));
            pos += item_len;
        }
        return true;
    }

    // Decode an RLP string item to its raw bytes
    static bool rlp_decode_string(const Bytes& item, Bytes& out) {
        if (item.empty()) { out.clear(); return true; }
        size_t pos = 0;
        return rlp_decode_item(item, pos, out);
    }

    // ── Hex-Prefix Decoder ──────────────────────────────────────────────
    // Decode hex-prefix (compact) encoded path back to nibbles + leaf flag
    static void hex_prefix_decode(const Bytes& encoded, Nibbles& nibbles, bool& is_leaf) {
        nibbles.clear();
        if (encoded.empty()) { is_leaf = false; return; }
        uint8_t first = encoded[0];
        int flag = (first >> 4) & 0x0F;
        is_leaf = (flag >= 2);
        bool odd = (flag % 2) == 1;

        if (odd) {
            nibbles.push_back(first & 0x0F);
        }
        for (size_t i = 1; i < encoded.size(); i++) {
            nibbles.push_back((encoded[i] >> 4) & 0x0F);
            nibbles.push_back(encoded[i] & 0x0F);
        }
    }
    // ── Child Link Verifier ─────────────────────────────────────────────
    // Check that `reference` (raw RLP bytes of a child slot in the parent)
    // correctly points to `child_encoded` (the next proof node's full RLP).
    // Two cases per the inline-vs-hash rule:
    //   - Inline: reference bytes == child_encoded bytes (child < 32 bytes)
    //   - Hash:   reference is an RLP string containing hash(child_encoded)
    static bool verify_child_link(const Bytes& reference, const Bytes& child_encoded) {
        // Case 1: inline embedding — reference IS the child's encoding
        if (reference == child_encoded) return true;

        // Case 2: hash reference — reference is RLP-encoded 32-byte hash
        Hash child_hash = keccak::sha3_256(child_encoded);
        Bytes expected_ref = rlp::encode_string(child_hash);
        if (reference == expected_ref) return true;

        // Case 3: the reference might be a raw 32-byte hash without RLP wrapper
        // (shouldn't happen per spec, but handle gracefully)
        if (reference == child_hash) return true;

        return false;
    }
    /** Verifies a proof for a key-value pair against a given root hash. */
    static bool verify_proof(const Hash& expected_root, const std::string& key,
                             const std::string& value, const std::vector<ProofItem>& proof) {
        Bytes expected_value(value.begin(), value.end());
        if (proof.empty()) return false;

        // Step 1: root hash check
        Hash computed_root = keccak::sha3_256(proof[0].rlp_encoded);
        if (computed_root != expected_root) return false;

        // Step 2: traverse the proof nodes according to the key nibbles
        Nibbles key_nibbles = to_nibbles(Bytes(key.begin(), key.end()));
        size_t nibble_idx = 0;
        
        // Each proof item should correspond to a node along the path from root to leaf
        for (size_t pi = 0; pi < proof.size(); pi++) {
            const Bytes& encoded = proof[pi].rlp_encoded;

            // Decode this proof node
            std::vector<Bytes> items;
            if (!rlp_decode_list(encoded, items)) {
                return false;
            }

            // Determine node type by item count
            if (items.size() == 17) {
                // ── Branch node: 16 children + value ──
                if (nibble_idx == key_nibbles.size()) {
                    // Key is consumed — value must be in slot 16
                    Bytes branch_val;
                    if (!rlp_decode_string(items[16], branch_val)) return false;
                    return branch_val == expected_value;
                }
                // Follow the child at nibbles[nibble_idx]
                uint8_t slot = key_nibbles[nibble_idx];
                nibble_idx++;

                // Verify link to next proof node
                if (pi + 1 < proof.size()) {
                    if (!verify_child_link(items[slot], proof[pi + 1].rlp_encoded))
                        return false;
                } else {
                    // No more proof nodes but key not consumed — invalid
                    return false;
                }

            } else if (items.size() == 2) {
                // ── Leaf or Extension node ──
                Bytes path_encoded;
                if (!rlp_decode_string(items[0], path_encoded)) return false;
                if (path_encoded.empty()) return false;

                // Decode hex-prefix
                bool is_leaf = false;
                Nibbles path;
                hex_prefix_decode(path_encoded, path, is_leaf);

                // Verify nibble path matches key
                for (size_t i = 0; i < path.size(); i++) {
                    if (nibble_idx >= key_nibbles.size()) return false;
                    if (key_nibbles[nibble_idx] != path[i]) return false;
                    nibble_idx++;
                }

                if (is_leaf) {
                    // Key must be fully consumed
                    if (nibble_idx != key_nibbles.size()) return false;
                    // Extract and compare value
                    Bytes leaf_val;
                    if (!rlp_decode_string(items[1], leaf_val)) return false;
                    return leaf_val == expected_value;
                } else {
                    // Extension — verify link to next proof node
                    if (pi + 1 < proof.size()) {
                        if (!verify_child_link(items[1], proof[pi + 1].rlp_encoded))
                            return false;
                    } else {
                        return false;
                    }
                }
            } else {
                return false; // Malformed node
            }
        }

        return false; // Ran out of proof nodes without hitting a leaf
    }
    // /** Verifies a proof for a key-value pair against a given root hash. */
    // static bool verify_proof(const Hash &er, const std::string &key, const std::string &value, const std::vector<ProofItem> &proof)
    // {
    //     Bytes v(value.begin(), value.end());
    //     if (proof.empty())
    //         return false;
    //     Hash c = keccak::sha3_256(proof[0].rlp_encoded);
    //     if (c != er)
    //         return false;
    //     for (size_t pi = 0; pi + 1 < proof.size(); pi++)
    //     {
    //         const Bytes &pe = proof[pi].rlp_encoded;
    //         const Bytes &ce = proof[pi + 1].rlp_encoded;

    //         std::vector<Bytes> children_list;
    //         rlp::decode_rlp_list(pe, children_list);

    //         bool linked = false;
    //         if (ce.size() >= 32)
    //         {
    //             Hash ch = rlp::encode_string(keccak::sha3_256(ce));
    //             linked = (!(ch.empty())) && (std::find(children_list.begin(), children_list.end(), ch) != children_list.end());
    //         }
    //         if (!linked)
    //             linked = (!(ce.empty())) && (std::find(children_list.begin(), children_list.end(), ce) != children_list.end());
    //         if (!linked)
    //         {
    //             Hash ch = rlp::encode_string(keccak::sha3_256(ce));
    //             linked = (!(ch.empty())) && (std::find(children_list.begin(), children_list.end(), ch) != children_list.end());
    //         }
    //         if (!linked)
    //             return false;
    //     }

    //     std::vector<Bytes> decoded_last_proof_item;
    //     rlp::decode_rlp_list(proof.back().rlp_encoded, decoded_last_proof_item);

    //     // Last item should be a leaf node, so at least
    //     // it has to have two bytes
    //     if (decoded_last_proof_item.size() != 2) {
    //         return false;
    //     }

    //     Bytes decoded_path_component;

    //     rlp::decode_string(decoded_last_proof_item.at(0), decoded_path_component);

    //     if (decoded_path_component.empty()) {
    //         return false;
    //     }

    //     uint8_t highest_nibble = (decoded_path_component.at(0)) >> 4;

    //     // Check whether it is a leaf
    //     if (highest_nibble != 2 && highest_nibble != 3) {
    //         return false;
    //     }

    //     return decoded_last_proof_item.at(1) == rlp::encode_string(v);


    //     // return contains_bytes(proof.back().rlp_encoded, v);
    // }
    /** Resets the trie to its initial state. */
    void reset() {
        root_ = TrieNode::make_null();
        TrieNode::next_id = 0;
    }
    const NodePtr &root_node() const { return root_; }
    std::vector<int> proof_node_ids(const std::string &key) const
    {
        std::vector<int> ids;
        collect_proof_ids(root_, to_nibbles(Bytes(key.begin(), key.end())), 0, ids);
        return ids;
    }
    void print_tree() const
    {
        std::cout << "--- Trie Structure ---\n";
        print_node(root_, 0);
        std::cout << "----------------------\n";
    }

private:
    NodePtr root_;
    Hash hash_node(const NodePtr &node) const
    {
        if (!node || node->type == NODE_NULL)
        {
            Bytes e(1, 0x80);
            return keccak::sha3_256(e);
        }
        if (!node->cached_hash.empty())
            return node->cached_hash;
        Bytes enc = rlp_encode_node(node);
        Hash h = keccak::sha3_256(enc);
        node->cached_hash = h;
        return h;
    }
    Bytes rlp_encode_node(const NodePtr &node) const
    {
        if (!node || node->type == NODE_NULL)
            return Bytes(1, 0x80);
        switch (node->type)
        {
        case NODE_LEAF:
        {
            Bytes hp = hex_prefix_encode(node->partial, true);
            std::vector<Bytes> it;
            it.push_back(rlp::encode_string(hp));
            it.push_back(rlp::encode_string(node->value));
            return rlp::encode_list(it);
        }
        case NODE_EXTENSION:
        {
            Bytes hp = hex_prefix_encode(node->partial, false);
            std::vector<Bytes> it;
            it.push_back(rlp::encode_string(hp));
            it.push_back(node_reference(node->next));
            return rlp::encode_list(it);
        }
        case NODE_BRANCH:
        {
            std::vector<Bytes> it;
            for (int i = 0; i < 16; i++)
            {
                if (node->children[i])
                    it.push_back(node_reference(node->children[i]));
                else
                    it.push_back(rlp::encode_string(Bytes()));
            }
            it.push_back(rlp::encode_string(node->branch_value));
            return rlp::encode_list(it);
        }
        default:
            return Bytes(1, 0x80);
        }
    }
    Bytes node_reference(const NodePtr &node) const
    {
        if (!node || node->type == NODE_NULL)
            return rlp::encode_string(Bytes());
        Bytes enc = rlp_encode_node(node);
        if (enc.size() < 32)
            return enc;
        return rlp::encode_string(hash_node(node));
    }
    /** Inserts a key-value pair into the trie. */
    NodePtr insert(NodePtr node, const Nibbles &nib, size_t d, const Bytes &val)
    {
        if (!node || node->type == NODE_NULL)
            return TrieNode::make_leaf(Nibbles(nib.begin() + d, nib.end()), val);
        node->invalidate();
        switch (node->type)
        {
        case NODE_LEAF:
            return insert_at_leaf(node, nib, d, val);
        case NODE_EXTENSION:
            return insert_at_ext(node, nib, d, val);
        case NODE_BRANCH:
            return insert_at_branch(node, nib, d, val);
        default:
            return node;
        }
    }
    NodePtr insert_at_leaf(NodePtr leaf, const Nibbles &nib, size_t d, const Bytes &val)
    {
        Nibbles rem(nib.begin() + d, nib.end());
        size_t sh = shared_prefix_len(leaf->partial, rem);
        if (sh == leaf->partial.size() && sh == rem.size())
        {
            leaf->value = val;
            return leaf;
        }
        NodePtr br = TrieNode::make_branch();
        if (sh > 0)
        {
            Nibbles pfx(leaf->partial.begin(), leaf->partial.begin() + sh), lr(leaf->partial.begin() + sh, leaf->partial.end()), nr(rem.begin() + sh, rem.end());
            if (lr.empty())
                br->branch_value = leaf->value;
            else
                br->children[lr[0]] = TrieNode::make_leaf(Nibbles(lr.begin() + 1, lr.end()), leaf->value);
            if (nr.empty())
                br->branch_value = val;
            else
                br->children[nr[0]] = TrieNode::make_leaf(Nibbles(nr.begin() + 1, nr.end()), val);
            return TrieNode::make_extension(pfx, br);
        }
        if (leaf->partial.empty())
            br->branch_value = leaf->value;
        else
            br->children[leaf->partial[0]] = TrieNode::make_leaf(Nibbles(leaf->partial.begin() + 1, leaf->partial.end()), leaf->value);
        if (rem.empty())
            br->branch_value = val;
        else
            br->children[rem[0]] = TrieNode::make_leaf(Nibbles(rem.begin() + 1, rem.end()), val);
        return br;
    }
    NodePtr insert_at_ext(NodePtr ext, const Nibbles &nib, size_t d, const Bytes &val)
    {
        Nibbles rem(nib.begin() + d, nib.end());
        size_t sh = shared_prefix_len(ext->partial, rem);
        if (sh == ext->partial.size())
        {
            ext->next = insert(ext->next, nib, d + sh, val);
            return ext;
        }
        NodePtr br = TrieNode::make_branch();
        if (sh + 1 < ext->partial.size())
            br->children[ext->partial[sh]] = TrieNode::make_extension(Nibbles(ext->partial.begin() + sh + 1, ext->partial.end()), ext->next);
        else
            br->children[ext->partial[sh]] = ext->next;
        if (sh < rem.size())
            br->children[rem[sh]] = TrieNode::make_leaf(Nibbles(rem.begin() + sh + 1, rem.end()), val);
        else
            br->branch_value = val;
        if (sh > 0)
            return TrieNode::make_extension(Nibbles(ext->partial.begin(), ext->partial.begin() + sh), br);
        return br;
    }
    NodePtr insert_at_branch(NodePtr br, const Nibbles &nib, size_t d, const Bytes &val)
    {
        if (d == nib.size())
        {
            br->branch_value = val;
            return br;
        }
        br->children[nib[d]] = insert(br->children[nib[d]], nib, d + 1, val);
        return br;
    }
    /** Looks up a key in the trie. */
    Optional<Bytes> lookup(const NodePtr &node, const Nibbles &nib, size_t d) const
    {
        if (!node || node->type == NODE_NULL)
            return Optional<Bytes>::none();
        switch (node->type)
        {
        case NODE_LEAF:
        {
            Nibbles r(nib.begin() + d, nib.end());
            if (r == node->partial)
                return Optional<Bytes>(node->value);
            return Optional<Bytes>::none();
        }
        case NODE_EXTENSION:
        {
            Nibbles r(nib.begin() + d, nib.end());
            if (r.size() < node->partial.size())
                return Optional<Bytes>::none();
            for (size_t i = 0; i < node->partial.size(); i++)
                if (r[i] != node->partial[i])
                    return Optional<Bytes>::none();
            return lookup(node->next, nib, d + node->partial.size());
        }
        case NODE_BRANCH:
        {
            if (d == nib.size())
            {
                if (node->branch_value.empty())
                    return Optional<Bytes>::none();
                return Optional<Bytes>(node->branch_value);
            }
            return lookup(node->children[nib[d]], nib, d + 1);
        }
        default:
            return Optional<Bytes>::none();
        }
    }
    /** Removes a key from the trie. */
    NodePtr erase(NodePtr node, const Nibbles &nib, size_t d, bool &found)
    {
        if (!node || node->type == NODE_NULL)
        {
            found = false;
            return node;
        }
        node->invalidate();
        switch (node->type)
        {
        case NODE_LEAF:
        {
            Nibbles r(nib.begin() + d, nib.end());
            if (r == node->partial)
            {
                found = true;
                return TrieNode::make_null();
            }
            found = false;
            return node;
        }
        case NODE_EXTENSION:
        {
            Nibbles r(nib.begin() + d, nib.end());
            if (r.size() < node->partial.size())
            {
                found = false;
                return node;
            }
            for (size_t i = 0; i < node->partial.size(); i++)
            {
                if (r[i] != node->partial[i])
                {
                    found = false;
                    return node;
                }
            }
            node->next = erase(node->next, nib, d + node->partial.size(), found);
            if (!found)
                return node;
            return compact(node);
        }
        case NODE_BRANCH:
        {
            if (d == nib.size())
            {
                if (node->branch_value.empty())
                {
                    found = false;
                    return node;
                }
                node->branch_value.clear();
                found = true;
                return compact(node);
            }
            uint8_t idx = nib[d];
            node->children[idx] = erase(node->children[idx], nib, d + 1, found);
            if (!found)
                return node;
            return compact(node);
        }
        default:
            found = false;
            return node;
        }
    }
    /** Compacts the trie by removing unnecessary nodes. */
    NodePtr compact(NodePtr node)
    {
        if (!node)
            return node;
        if (node->type == NODE_EXTENSION)
        {
            if (!node->next || node->next->type == NODE_NULL)
                return TrieNode::make_null();
            if (node->next->type == NODE_LEAF)
            {
                Nibbles m = node->partial;
                m.insert(m.end(), node->next->partial.begin(), node->next->partial.end());
                return TrieNode::make_leaf(m, node->next->value);
            }
            if (node->next->type == NODE_EXTENSION)
            {
                Nibbles m = node->partial;
                m.insert(m.end(), node->next->partial.begin(), node->next->partial.end());
                return TrieNode::make_extension(m, node->next->next);
            }
            return node;
        }
        if (node->type == NODE_BRANCH)
        {
            int cc = 0, last = -1;
            for (int i = 0; i < 16; i++)
                if (node->children[i] && node->children[i]->type != NODE_NULL)
                {
                    cc++;
                    last = i;
                }
            bool hv = !node->branch_value.empty();
            if (cc == 0 && !hv)
                return TrieNode::make_null();
            if (cc == 0 && hv)
                return TrieNode::make_leaf(Nibbles(), node->branch_value);
            if (cc == 1 && !hv)
            {
                NodePtr ch = node->children[last];
                Nibbles pfx(1, static_cast<uint8_t>(last));
                if (ch->type == NODE_LEAF)
                {
                    pfx.insert(pfx.end(), ch->partial.begin(), ch->partial.end());
                    return TrieNode::make_leaf(pfx, ch->value);
                }
                if (ch->type == NODE_EXTENSION)
                {
                    pfx.insert(pfx.end(), ch->partial.begin(), ch->partial.end());
                    return TrieNode::make_extension(pfx, ch->next);
                }
                return TrieNode::make_extension(pfx, ch);
            }
            return node;
        }
        return node;
    }
    /** Builds a proof for a key in the trie. */
    void build_proof(const NodePtr &node, const Nibbles &nib, size_t d, std::vector<ProofItem> &proof) const
    {
        if (!node || node->type == NODE_NULL)
            return;
        ProofItem it;
        it.rlp_encoded = rlp_encode_node(node);
        proof.push_back(it);
        switch (node->type)
        {
        case NODE_LEAF:
            break;
        case NODE_EXTENSION:
        {
            size_t pl = node->partial.size();
            if (d + pl <= nib.size())
            {
                bool m = true;
                for (size_t i = 0; i < pl && m; i++)
                    m = (node->partial[i] == nib[d + i]);
                if (m)
                    build_proof(node->next, nib, d + pl, proof);
            }
            break;
        }
        case NODE_BRANCH:
        {
            if (d < nib.size())
            {
                uint8_t idx = nib[d];
                if (node->children[idx])
                    build_proof(node->children[idx], nib, d + 1, proof);
            }
            break;
        }
        default:
            break;
        }
    }
    // /** Collects the IDs of nodes involved in a proof. */
    void collect_proof_ids(const NodePtr &node, const Nibbles &nib, size_t d, std::vector<int> &ids) const {
        if (!node || node->type == NODE_NULL)
            return;
        ids.push_back(node->node_id);
        if (node->type == NODE_LEAF)
            return;
        if (node->type == NODE_EXTENSION)
        {
            size_t pl = node->partial.size();
            if (d + pl <= nib.size())
            {
                bool m = true;
                for (size_t i = 0; i < pl && m; i++)
                    m = (node->partial[i] == nib[d + i]);
                if (m)
                    collect_proof_ids(node->next, nib, d + pl, ids);
            }
        }
        if (node->type == NODE_BRANCH && d < nib.size())
        {
            uint8_t idx = nib[d];
            if (node->children[idx])
                collect_proof_ids(node->children[idx], nib, d + 1, ids);
        }
    }
    /** Checks if a byte sequence contains another byte sequence. */
    static bool contains_bytes(const Bytes &h, const Bytes &n)
    {
        if (n.empty())
            return true;
        if (h.size() < n.size())
            return false;
            // Simple byte-wise search 
        for (size_t i = 0; i <= h.size() - n.size(); i++)
            if (std::memcmp(h.data() + i, n.data(), n.size()) == 0)
                return true;
        return false;
    }
    // /** Prints the structure of a node in the trie. */
    void print_node(const NodePtr &node, int indent) const
    {
        std::string pad(indent * 2, ' ');
        if (!node || node->type == NODE_NULL)
        {
            std::cout << pad << "(null)\n";
            return;
        }
        switch (node->type)
        {
        case NODE_LEAF:
            std::cout << pad << "[Leaf] path=";
            for (size_t i = 0; i < node->partial.size(); i++)
                std::cout << std::hex << (int)node->partial[i];
            std::cout << " value=\"" << std::string(node->value.begin(), node->value.end()) << "\"\n";
            break;
        case NODE_EXTENSION:
            std::cout << pad << "[Ext]  path=";
            for (size_t i = 0; i < node->partial.size(); i++)
                std::cout << std::hex << (int)node->partial[i];
            std::cout << "\n";
            print_node(node->next, indent + 1);
            break;
        case NODE_BRANCH:
            std::cout << pad << "[Branch]";
            if (!node->branch_value.empty())
                std::cout << " value=\"" << std::string(node->branch_value.begin(), node->branch_value.end()) << "\"";
            std::cout << "\n";
            for (int i = 0; i < 16; i++)
            {
                if (node->children[i] && node->children[i]->type != NODE_NULL)
                {
                    std::cout << pad << "  [" << std::hex << i << std::dec << "]:\n";
                    print_node(node->children[i], indent + 2);
                }
            }
            break;
        default:
            break;
        }
    }
};

#ifdef MPT_IMPLEMENTATION
int TrieNode::next_id = 0;
#endif

#endif
