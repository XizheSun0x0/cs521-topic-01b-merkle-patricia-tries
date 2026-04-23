# cs521-topic-01b-merkle-patricia-tries
This repo contains Merkle Patricia Tires Implementation by Asher and Wilson.

To test code, run:
```bash
make run; make clean
```

Or run with CMake
```bash
cmake --build build;./build/test_app;rm -rf build         
```

---

## Server API Reference

Base URL: `http://localhost:8080`

All endpoints return JSON. CORS headers are included (`Access-Control-Allow-Origin: *`).

---

### `GET /root`

Returns the current Keccak-256 state root hash of the trie.

**Response:**
```json
{
  "root": "9a74d99ff587fcf8722954a139497a8f74cfcb4de171d2fa422046557559b2ee"
}
```

---

### `GET /entries`

Returns all stored key-value pairs.

**Response:**
```json
{
  "entries": [
    { "key": "dog", "value": "puppy" },
    { "key": "horse", "value": "stallion" }
  ]
}
```

---

### `GET /tree`

Returns the full trie structure as a recursive JSON tree. This is the primary endpoint for visualization.

**Response:**
```json
{
  "tree": {
    "id": 10,
    "type": "EXT",
    "partial": "6",
    "child": {
      "id": 7,
      "type": "BRANCH",
      "children": [
        null,
        {
          "nibble": 1,
          "node": {
            "id": 5,
            "type": "LEAF",
            "partial": "a3f",
            "value": "puppy"
          }
        },
        null,
        ...
      ],
      "value": "verb"
    }
  }
}
```

**Node types and their JSON shapes:**

| type | fields |
|------|--------|
| `"LEAF"` | `id`, `type`, `partial` (hex nibble string), `value` |
| `"EXT"` | `id`, `type`, `partial` (hex nibble string), `child` (single node) |
| `"BRANCH"` | `id`, `type`, `children` (array of 16 slots), optional `value` |

Each `children` slot is either `null` or `{ "nibble": <0-15>, "node": <node> }`.

If `tree` is `null`, the trie is empty.
---

### `POST /put`

Insert or update a key-value pair.

**Request:**
```json
{ "key": "dog", "value": "puppy" }
```

**Response:**
```json
{ "ok": true, "root": "cacf756906847a884a459b572322069bc6f455a84bda07843486e7975f65fbab" }
```

---

### `POST /get`

Lookup a key. If found, also returns the IDs of nodes along the proof path (for highlighting in the tree visualization).

**Request:**
```json
{ "key": "dog" }
```

**Response (found):**
```json
{
  "found": true,
  "value": "puppy",
  "proof_ids": [10, 7, 8, 4, 5, 1]
}
```

**Response (not found):**
```json
{ "found": false }
```

---

### `POST /delete`

Delete a key from the trie.

**Request:**
```json
{ "key": "dog" }
```

**Response:**
```json
{
  "deleted": true,
  "root": "ab12...new_root_hash"
}
```

---

### `POST /proof`

Generate a full Merkle proof for a key, verify it against the current root, and return the proof details.

**Request:**
```json
{ "key": "dog" }
```

**Response (found):**
```json
{
  "found": true,
  "value": "puppy",
  "verified": true,
  "proof_length": 6,
  "proof_ids": [10, 7, 8, 4, 5, 1],
  "nodes": [
    { "index": 0, "hash": "9a74d99f...full_64_char_hex", "size": 35 },
    { "index": 1, "hash": "dfcafc78...full_64_char_hex", "size": 98 },
    ...
  ]
}
```

**Response (not found):**
```json
{ "found": false }
```

`proof_ids` is the list of trie node IDs that form the path from root to the target leaf — use these to highlight nodes in the tree visualization.

`nodes` is the actual Merkle proof: each entry is an RLP-encoded trie node with its Keccak-256 hash and byte size. The first node's hash equals the state root.

---

### `POST /verify`

Verify a proof against a specific root hash (useful for demonstrating that old proofs fail against new roots).

**Request:**
```json
{
  "key": "dog",
  "value": "puppy",
  "root": "9a74d99ff587fcf8722954a139497a8f74cfcb4de171d2fa422046557559b2ee"
}
```

**Response:**
```json
{ "verified": true }
```

---

### `POST /demo`

Reset the trie and load 6 sample entries: `dog→puppy`, `doge→coin`, `do→verb`, `horse→stallion`, `apple→fruit`, `ant→insect`.

**Response:**
```json
{ "ok": true, "root": "9a74d99f...", "count": 6 }
```

---

### `POST /reset`

Clear the trie completely.

**Response:**
```json
{ "ok": true, "root": "bc36789e..." }
```

---

## Architecture

```
┌─────────────────────┐        fetch()        ┌─────────────────────┐
│                     │  ◄──────────────────►  │                     │
│   Vue 3 Frontend    │    JSON over HTTP      │   C++ Backend       │
│                     │                        │                     │
│  • Tree SVG render  │   GET  /root           │  • Keccak-256       │
│  • Proof highlight  │   GET  /tree           │  • RLP encoding     │
│  • Entry management │   GET  /entries        │  • MPT engine       │
│  • Pan/zoom canvas  │   POST /put,get,del    │  • Proof gen/verify │
│                     │   POST /proof,verify   │  • POSIX HTTP       │
└─────────────────────┘   POST /demo,reset     └─────────────────────┘
```