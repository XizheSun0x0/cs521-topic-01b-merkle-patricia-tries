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



namespace keccak {



static const uint64_t RC[24] = {

0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808AULL,

0x8000000080008000ULL, 0x000000000000808BULL, 0x0000000080000001ULL,

0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008AULL,

0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000AULL,

0x000000008000808BULL, 0x800000000000008BULL, 0x8000000000008089ULL,

0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,

0x000000000000800AULL, 0x800000008000000AULL, 0x8000000080008081ULL,

0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};



static inline uint64_t rotl64(uint64_t x, int y) {

return (x << y) | (x >> (64 - y));

}



static void keccakf(uint64_t st[25]) {

for (int round = 0; round < 24; ++round) {

uint64_t C[5], D[5];

for (int i = 0; i < 5; i++)

C[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];

for (int i = 0; i < 5; i++) {

D[i] = C[(i + 4) % 5] ^ rotl64(C[(i + 1) % 5], 1);

for (int j = 0; j < 25; j += 5) st[j + i] ^= D[i];

}

uint64_t t = st[1], tmp;

int idx;

static const int piln[24] = {10, 7, 11, 17, 18, 3, 5, 16,

8, 21, 24, 4, 15, 23, 19, 13,

12, 2, 20, 14, 22, 9, 6, 1};

static const int rotc[24] = {1, 3, 6, 10, 15, 21, 28, 36,

45, 55, 2, 14, 27, 41, 56, 8,

25, 43, 62, 18, 39, 61, 20, 44};

for (int i = 0; i < 24; i++) {

idx = piln[i];

tmp = st[idx];

st[idx] = rotl64(t, rotc[i]);

t = tmp;

}

for (int j = 0; j < 25; j += 5) {

uint64_t bc[5];

for (int i = 0; i < 5; i++) bc[i] = st[j + i];

for (int i = 0; i < 5; i++)

st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];

}

st[0] ^= RC[round];

}

}



static std::vector<uint8_t> sha3_256(const std::vector<uint8_t>& input) {

const size_t rate = 136;

uint64_t st[25];

std::memset(st, 0, sizeof(st));

size_t pt = 0;



for (size_t b = 0; b < input.size(); b++) {

reinterpret_cast<uint8_t*>(st)[pt++] ^= input[b];

if (pt >= rate) { keccakf(st); pt = 0; }

}

reinterpret_cast<uint8_t*>(st)[pt] ^= 0x06;

reinterpret_cast<uint8_t*>(st)[rate - 1] ^= 0x80;

keccakf(st);



std::vector<uint8_t> out(32);

std::memcpy(out.data(), st, 32);

return out;

}



}

// Recursive Length Prefix encoder

namespace rlp {



typedef std::vector<uint8_t> Bytes;



static Bytes encode_length(size_t len, uint8_t offset) {

if (len < 56) {

Bytes r(1, static_cast<uint8_t>(len + offset));

return r;

}

Bytes bl;

size_t tmp = len;

while (tmp > 0) {

bl.insert(bl.begin(), static_cast<uint8_t>(tmp & 0xFF));

tmp >>= 8;

}

bl.insert(bl.begin(), static_cast<uint8_t>(offset + 55 + bl.size()));

return bl;

}



static Bytes encode_string(const Bytes& s) {

if (s.size() == 1 && s[0] < 0x80) return s;

Bytes out = encode_length(s.size(), 0x80);

out.insert(out.end(), s.begin(), s.end());

return out;

}



static Bytes encode_string(const std::string& s) {

Bytes b(s.begin(), s.end());

return encode_string(b);

}



static Bytes encode_list(const std::vector<Bytes>& items) {

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

static std::string hex(const Bytes& data) {

std::ostringstream ss;

for (size_t i = 0; i < data.size(); i++)

ss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];

return ss.str();

}



static std::string hex_short(const Bytes& data, int n = 8) {

std::string h = hex(data);

if ((int)h.size() <= n * 2) return h;

return h.substr(0, n) + "..." + h.substr(h.size() - n);

}



static Nibbles to_nibbles(const Bytes& key) {

Nibbles nibbles;

nibbles.reserve(key.size() * 2);

for (size_t i = 0; i < key.size(); i++) {

nibbles.push_back((key[i] >> 4) & 0x0F);

nibbles.push_back(key[i] & 0x0F);

}

return nibbles;

}



//test code

int main() {

std::cout << "--- Testing Keccak-256 ---" << std::endl;

std::string test_str = "hello";

Bytes test_bytes(test_str.begin(), test_str.end());

Hash h = keccak::sha3_256(test_bytes);

std::cout << "sha3(\"hello\"): " << hex(h) << std::endl;


// Expected for Keccak-256("hello"):

// 1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8


std::cout << "\n--- Testing RLP Encoding ---" << std::endl;

std::string rlp_test = "dog";

Bytes rlp_encoded = rlp::encode_string(rlp_test);

std::cout << "RLP(\"dog\"): 0x" << hex(rlp_encoded) << std::endl;


// Expected for RLP("dog"): 83646f67 (0x80 + length 3, then 'd','o','g')



std::cout << "\n--- Testing Nibble Conversion ---" << std::endl;

Bytes key = {0xCA, 0xFE};

Nibbles n = to_nibbles(key);

std::cout << "Nibbles of 0xCAFE: ";

for(auto nib : n) std::cout << std::hex << (int)nib << " ";

std::cout << std::dec << std::endl;



return 0;

}