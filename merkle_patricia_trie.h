#ifndef MERKLE_PATRICIA_TRIE_H
#define MERKLE_PATRICIA_TRIE_H

/*
 * Merkle Patricia Trie — Header-Only Library
 * Extracted from merkle_patricia_trie.cpp
 */

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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
    typedef std::vector<uint8_t> Bytes;
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
}

typedef std::vector<uint8_t> Bytes;
typedef std::vector<uint8_t> Hash;
typedef std::vector<uint8_t> Nibbles;

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
        //TODO: Implement proof generation logic
        //issue #7
    }
    /** Verifies a proof for a key-value pair against a given root hash. */
    static bool verify_proof(const Hash &er, const std::string &key, const std::string &value, const std::vector<ProofItem> &proof)
    {
        //TODO: Implement proof verification logic
        //issue #6
    }
    /** Resets the trie to its initial state. */
    void reset() {
        //TODO: Implement trie reset logic
        //issue #8
    }
    const NodePtr &root_node() const { return root_; }
    std::vector<int> proof_node_ids(const std::string &key) const
    {
        //TODO: Implement logic to collect node IDs involved in a proof
        //issue #9
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
        //TODO: Implement logic to collect node IDs involved in a proof
        //issue #12
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
