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
