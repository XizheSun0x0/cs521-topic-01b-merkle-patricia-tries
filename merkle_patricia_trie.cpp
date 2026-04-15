#include <array>
#include <cassert>
#include <cstdint>
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
    static inline uint64_t rotl64(uint64_t x, int y)
    {
        return (x << y) | (x >> (64 - y));
    }
    static void keccakf(uint64_t st[25])
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
            static const int piln[24] = {10, 7, 11, 17, 18, 3, 5, 16,
                                         8, 21, 24, 4, 15, 23, 19, 13,
                                         12, 2, 20, 14, 22, 9, 6, 1};
            static const int rotc[24] = {1, 3, 6, 10, 15, 21, 28, 36,
                                         45, 55, 2, 14, 27, 41, 56, 8,
                                         25, 43, 62, 18, 39, 61, 20, 44};
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
    static std::vector<uint8_t> sha3_256(const std::vector<uint8_t> &input)
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
// Recursive Length Prefix encoder
namespace rlp
{
    typedef std::vector<uint8_t> Bytes;
    static Bytes encode_length(size_t len, uint8_t offset)
    {
        if (len < 56)
        {
            Bytes r(1, static_cast<uint8_t>(len + offset));
            return r;
        }
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
    static Bytes encode_string(const Bytes &s)
    {
        if (s.size() == 1 && s[0] < 0x80)
            return s;
        Bytes out = encode_length(s.size(), 0x80);
        out.insert(out.end(), s.begin(), s.end());
        return out;
    }
    static Bytes encode_string(const std::string &s)
    {
        Bytes b(s.begin(), s.end());
        return encode_string(b);
    }
    static Bytes encode_list(const std::vector<Bytes> &items)
    {
        Bytes payload;
        for (size_t i = 0; i < items.size(); i++)
            payload.insert(payload.end(), items[i].begin(), items[i].end());
        Bytes out = encode_length(payload.size(), 0xC0);
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }
}
/**
 * Helper functions for pringing and converting data to hex and nibbles.
 */
static std::string hex(const Bytes &data)
{
    std::ostringstream ss;
    for (size_t i = 0; i < data.size(); i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
    return ss.str();
}
static std::string hex_short(const Bytes &data, int n = 8)
{
    std::string h = hex(data);
    if ((int)h.size() <= n * 2)
        return h;
    return h.substr(0, n) + "..." + h.substr(h.size() - n);
}
static Nibbles to_nibbles(const Bytes &key)
{
    Nibbles nibbles;
    nibbles.reserve(key.size() * 2);
    for (size_t i = 0; i < key.size(); i++)
    {
        nibbles.push_back((key[i] >> 4) & 0x0F);
        nibbles.push_back(key[i] & 0x0F);
    }
    return nibbles;
}
// Trie node definitions
enum NodeType { NODE_NULL, NODE_LEAF, NODE_EXTENSION, NODE_BRANCH };

struct TrieNode;
typedef std::shared_ptr<TrieNode> NodePtr;
// A node in the Merkle Patricia Trie
struct TrieNode {
    NodeType type;
    Nibbles partial;
    Bytes value;
    NodePtr children[16];
    Bytes branch_value;
    NodePtr next;
    mutable Hash cached_hash;  // empty vector = not computed

    TrieNode() : type(NODE_NULL), next() {
        for (int i = 0; i < 16; i++) children[i] = NodePtr();
    }

    void invalidate() { cached_hash.clear(); }

    static NodePtr make_null() {
        return std::make_shared<TrieNode>();
    }
    static NodePtr make_leaf(Nibbles p, Bytes v) {
        NodePtr n = std::make_shared<TrieNode>();
        n->type = NODE_LEAF;
        n->partial = p;
        n->value = v;
        return n;
    }
    // For extension nodes, 'partial' holds the shared key segment, and 'next' points to the child node.
    static NodePtr make_extension(Nibbles p, NodePtr child) {
        NodePtr n = std::make_shared<TrieNode>();
        n->type = NODE_EXTENSION;
        n->partial = p;
        n->next = child;
        return n;
    }
    // For branch nodes, 'partial' and 'value' are not used. 'children' holds the 16 possible branches.
    static NodePtr make_branch() {
        NodePtr n = std::make_shared<TrieNode>();
        n->type = NODE_BRANCH;
        return n;
    }
};
// Hex-Prefix encoding for leaf and extension nodes
static Bytes hex_prefix_encode(const Nibbles& nibbles, bool is_leaf) {
    int flag = is_leaf ? 2 : 0;
    bool odd = (nibbles.size() % 2) != 0;
    Bytes result;
    if (odd) {
        result.push_back(static_cast<uint8_t>((flag + 1) << 4 | nibbles[0]));
        for (size_t i = 1; i < nibbles.size(); i += 2)
            result.push_back(static_cast<uint8_t>((nibbles[i] << 4) | nibbles[i + 1]));
    } else {
        result.push_back(static_cast<uint8_t>(flag << 4));
        for (size_t i = 0; i < nibbles.size(); i += 2)
            result.push_back(static_cast<uint8_t>((nibbles[i] << 4) | nibbles[i + 1]));
    }
    return result;
}
// Compute the length of the shared prefix between two nibble sequences
static size_t shared_prefix_len(const Nibbles& a, const Nibbles& b) {
    size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i]) return i;
    return n;
}
// Main Merkle Patricia Trie class
class MerklePatriciaTrie {
    public:
    MerklePatriciaTrie() : root_(TrieNode::make_null()) {}
    void put(const Bytes& key, const Bytes& value) {
        Nibbles nibbles = to_nibbles(key);
        root_ = insert(root_, nibbles, 0, value);
    }

    void put(const std::string& key, const std::string& value) {
        put(Bytes(key.begin(), key.end()), Bytes(value.begin(), value.end()));
    }
private:
    NodePtr root_;
    // Compute the hash of a node, using caching to avoid redundant calculations
    Hash hash_node(const NodePtr& node) const {
        if (!node || node->type == NODE_NULL) {
            Bytes empty_rlp(1, 0x80);
            return keccak::sha3_256(empty_rlp);
        }
        if (!node->cached_hash.empty()) return node->cached_hash;

        Bytes encoded = rlp_encode_node(node);
        Hash h = keccak::sha3_256(encoded);
        node->cached_hash = h;
        return h;
    }
    // Get the reference for a child node, which is either the hash or the RLP encoding if it's small
    Bytes rlp_encode_node(const NodePtr& node) const {
        if (!node || node->type == NODE_NULL) return Bytes(1, 0x80);

        switch (node->type) {
            case NODE_LEAF: {
                Bytes hp = hex_prefix_encode(node->partial, true);
                std::vector<Bytes> items;
                items.push_back(rlp::encode_string(hp));
                items.push_back(rlp::encode_string(node->value));
                return rlp::encode_list(items);
            }
            case NODE_EXTENSION: {
                Bytes hp = hex_prefix_encode(node->partial, false);
                Bytes child_ref = node_reference(node->next);
                std::vector<Bytes> items;
                items.push_back(rlp::encode_string(hp));
                items.push_back(child_ref);
                return rlp::encode_list(items);
            }
            case NODE_BRANCH: {
                std::vector<Bytes> items;
                for (int i = 0; i < 16; i++) {
                    if (node->children[i])
                        items.push_back(node_reference(node->children[i]));
                    else
                        items.push_back(rlp::encode_string(Bytes()));
                }
                items.push_back(rlp::encode_string(node->branch_value));
                return rlp::encode_list(items);
            }
            default:
                return Bytes(1, 0x80);
        }
    }
    Bytes node_reference(const NodePtr& node) const {
        if (!node || node->type == NODE_NULL)
            return rlp::encode_string(Bytes());
        Bytes encoded = rlp_encode_node(node);
        if (encoded.size() < 32) return encoded;
        return rlp::encode_string(hash_node(node));
    }
    // Recursive insertion function
    NodePtr insert(NodePtr node, const Nibbles& nibbles,
                   size_t depth, const Bytes& value) {
        if (!node || node->type == NODE_NULL) {
            Nibbles remaining(nibbles.begin() + depth, nibbles.end());
            return TrieNode::make_leaf(remaining, value);
        }
        node->invalidate();

        switch (node->type) {
            case NODE_LEAF:      return insert_at_leaf(node, nibbles, depth, value);
            case NODE_EXTENSION: return insert_at_extension(node, nibbles, depth, value);
            case NODE_BRANCH:    return insert_at_branch(node, nibbles, depth, value);
            default:             return node;
        }
    }

    NodePtr insert_at_leaf(NodePtr leaf, const Nibbles& nibbles,
                           size_t depth, const Bytes& value) {
        Nibbles remaining(nibbles.begin() + depth, nibbles.end());
        size_t shared = shared_prefix_len(leaf->partial, remaining);

        if (shared == leaf->partial.size() && shared == remaining.size()) {
            leaf->value = value;
            return leaf;
        }

        NodePtr branch = TrieNode::make_branch();

        if (shared > 0) {
            Nibbles prefix(leaf->partial.begin(), leaf->partial.begin() + shared);
            Nibbles leaf_rest(leaf->partial.begin() + shared, leaf->partial.end());
            Nibbles new_rest(remaining.begin() + shared, remaining.end());

            if (leaf_rest.empty()) {
                branch->branch_value = leaf->value;
            } else {
                uint8_t idx = leaf_rest[0];
                Nibbles lr(leaf_rest.begin() + 1, leaf_rest.end());
                branch->children[idx] = TrieNode::make_leaf(lr, leaf->value);
            }
            if (new_rest.empty()) {
                branch->branch_value = value;
            } else {
                uint8_t idx = new_rest[0];
                Nibbles nr(new_rest.begin() + 1, new_rest.end());
                branch->children[idx] = TrieNode::make_leaf(nr, value);
            }
            return TrieNode::make_extension(prefix, branch);
        }

        if (leaf->partial.empty()) {
            branch->branch_value = leaf->value;
        } else {
            uint8_t idx = leaf->partial[0];
            Nibbles lr(leaf->partial.begin() + 1, leaf->partial.end());
            branch->children[idx] = TrieNode::make_leaf(lr, leaf->value);
        }
        if (remaining.empty()) {
            branch->branch_value = value;
        } else {
            uint8_t idx = remaining[0];
            Nibbles nr(remaining.begin() + 1, remaining.end());
            branch->children[idx] = TrieNode::make_leaf(nr, value);
        }
        return branch;
    }

    NodePtr insert_at_extension(NodePtr ext, const Nibbles& nibbles,
                                size_t depth, const Bytes& value) {
        Nibbles remaining(nibbles.begin() + depth, nibbles.end());
        size_t shared = shared_prefix_len(ext->partial, remaining);

        if (shared == ext->partial.size()) {
            ext->next = insert(ext->next, nibbles, depth + shared, value);
            return ext;
        }

        NodePtr branch = TrieNode::make_branch();

        if (shared + 1 < ext->partial.size()) {
            Nibbles ext_rest(ext->partial.begin() + shared + 1, ext->partial.end());
            branch->children[ext->partial[shared]] =
                TrieNode::make_extension(ext_rest, ext->next);
        } else {
            branch->children[ext->partial[shared]] = ext->next;
        }

        if (shared < remaining.size()) {
            uint8_t idx = remaining[shared];
            Nibbles nr(remaining.begin() + shared + 1, remaining.end());
            branch->children[idx] = TrieNode::make_leaf(nr, value);
        } else {
            branch->branch_value = value;
        }

        if (shared > 0) {
            Nibbles prefix(ext->partial.begin(), ext->partial.begin() + shared);
            return TrieNode::make_extension(prefix, branch);
        }
        return branch;
    }

    NodePtr insert_at_branch(NodePtr branch, const Nibbles& nibbles,
                             size_t depth, const Bytes& value) {
        if (depth == nibbles.size()) {
            branch->branch_value = value;
            return branch;
        }
        uint8_t idx = nibbles[depth];
        branch->children[idx] = insert(branch->children[idx], nibbles, depth + 1, value);
        return branch;
    }
};
// test code
int main() {
    std::cout << "Testing MPT Insertion" << std::endl;
    MerklePatriciaTrie mpt;
    
    // Test basic leaf insertion
    mpt.put("dog", "puppy");
    
    // Test branching and extension creation
    mpt.put("do", "verb");
    mpt.put("horse", "stallion");
    
    std::cout << "Insertion executed without crashing." << std::endl;
    return 0;
}
// int main() {
//     auto leaf = TrieNode::make_leaf({0x01, 0x02}, {'v', 'a', 'l'});
//     std::cout << "Node created. Type: " << leaf->type << std::endl;
//     // Verify Hex-Prefix
//     Bytes hp = hex_prefix_encode({0x0f}, true); // Odd length leaf
//     std::cout << "Hex-Prefix (Leaf, Odd): 0x" << hex(hp) << " (Expected: 3f)" << std::endl;
//     return 0;
// }
// int main()
// {
//     std::cout << "--- Testing Keccak-256 ---" << std::endl;
//     std::string test_str = "hello";
//     Bytes test_bytes(test_str.begin(), test_str.end());
//     Hash h = keccak::sha3_256(test_bytes);
//     std::cout << "sha3(\"hello\"): " << hex(h) << std::endl;
//     // Expected for Keccak-256("hello"):
//     // 1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8
//     std::cout << "\n--- Testing RLP Encoding ---" << std::endl;
//     std::string rlp_test = "dog";
//     Bytes rlp_encoded = rlp::encode_string(rlp_test);
//     std::cout << "RLP(\"dog\"): 0x" << hex(rlp_encoded) << std::endl;
//     // Expected for RLP("dog"): 83646f67 (0x80 + length 3, then 'd','o','g')
//     std::cout << "\n--- Testing Nibble Conversion ---" << std::endl;
//     Bytes key = {0xCA, 0xFE};
//     Nibbles n = to_nibbles(key);
//     std::cout << "Nibbles of 0xCAFE: ";
//     for (auto nib : n)
//         std::cout << std::hex << (int)nib << " ";
//     std::cout << std::dec << std::endl;
//     return 0;
// }