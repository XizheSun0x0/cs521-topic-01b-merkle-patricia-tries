# cs521-topic-01b-merkle-patricia-tries
This repo contains Merkle Patricia Tires Implementation by Asher and Wilson.


## Project Structure

```
project/
├── merkle_patricia_trie.h    # Header-only MPT library (shared by both targets)
├── merkle_patricia_trie.cpp  # Original standalone reference (not compiled)
├── localtest.cpp             # Terminal test runner — 5 demo scenarios
├── benchmark.cpp             # benchmark this implementation
├── server.cpp                # HTTP API server for the Vue frontend
├── CMakeLists.txt            # CMake build
├── Makefile                  # Make build (alternative)
├── README.md
└── frontend/                 # Vue 3 project 
    └── ...
```

## Build & Run

### Prerequisites

- C++14 compiler (Apple Clang, GCC, or MSVC)
- Node.js 18+ (for the Vue frontend)

### Build the C++ targets

To test code, run in project dir:
```bash
#test in terminal
make localtest; ./localtest; make clean
#demostrate with graphic interface on VUE
make server; ./server; make clean
#run benchmark
make benchmark; ./benchmark; make clean
```

**Option B — CMake:**
```bash
#test in terminal
mkdir build && cd build
cmake ..
make
./localtest
#demonstrate in VUE
mkdir build && cd build
cmake ..
make
./server
#run benchmark
mkdir build && cd build
cmake ..
make
./benchmark
```
### Run the server + frontend

**Terminal 1 — Start the C++ server:**
```bash
./server          # default port 8080
```

**Terminal 2 — Start the Vue frontend:**
```bash
cd frontend/mpt-display
npm install
npm run dev       # Vite dev server, default http://localhost:5173
```