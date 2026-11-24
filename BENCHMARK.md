# Farloom Benchmark Report: C++20 vs Rust

This document details the comparative performance benchmarks between the original Rust implementation and the modern C++20 port of Farloom.

## 1. Test Environment & Methodology

- **Hardware**: Apple Silicon (arm64, Darwin 25.0.0)
- **Compiler (C++)**: Apple Clang 17.0.0 (`-std=c++20 -O3`)
- **Compiler (Rust)**: rustc 1.98.1 (`--release`, opt-level = 3)
- **Network / Transport**: Loopback TCP (`127.0.0.1`), new connection per request (`Connection: close`)
- **Load Generation**: Standalone multi-threaded load test client (`loadtest`)
- **Worker Threads**: 4 worker threads
- **Queue Capacity**: 64 connections
- **Total Requests**: 10,000 requests per benchmark run
- **Concurrency**: 16 concurrent client threads

---

## 2. Benchmark Results

### Scenario 1: Lightweight In-Memory Route (`GET /health`)
Built-in JSON route returning `{"status":"ok"}`.

| Metric | Rust Implementation | Modern C++20 Implementation | Difference |
|---|---|---|---|
| **Total Requests** | 10,000 | 10,000 | Identical |
| **Success Rate** | 100% (10,000 / 10,000) | 100% (10,000 / 10,000) | Identical |
| **Total Elapsed Time** | 17.459 s | 17.469 s | +0.06% |
| **Throughput (req/s)** | **572.8 req/s** | **572.4 req/s** | -0.07% (Within margin of error) |
| **Latency (Average)** | 27.60 ms | 27.60 ms | **Identical (0.0%)** |
| **Latency (Median / p50)** | 27.73 ms | 27.63 ms | -0.36% (C++ slightly faster) |
| **Latency (p95)** | 30.07 ms | 30.16 ms | +0.30% |
| **Latency (p99)** | 30.52 ms | 34.44 ms | +12.8% |
| **Latency (Min)** | 16.77 ms | 3.36 ms | -79.9% (C++ lower min) |
| **Latency (Max)** | 32.65 ms | 62.16 ms | Outlier spike |
| **Resident Memory (RSS)** | 2.00 MB | 1.92 MB | -4.0% (C++ lower memory) |

---

### Scenario 2: Static File Serving (`GET /index.html`)
Filesystem resolution, traversal checks, MIME detection, and disk read.

| Metric | Rust Implementation | Modern C++20 Implementation | Difference |
|---|---|---|---|
| **Total Requests** | 10,000 | 10,000 | Identical |
| **Success Rate** | 100% (10,000 / 10,000) | 100% (10,000 / 10,000) | Identical |
| **Total Elapsed Time** | 17.317 s | 17.472 s | +0.89% |
| **Throughput (req/s)** | **577.5 req/s** | **572.3 req/s** | -0.90% (Within margin of error) |
| **Latency (Average)** | 27.44 ms | 27.68 ms | +0.87% |
| **Latency (Median / p50)** | 27.26 ms | 27.65 ms | +1.43% |
| **Latency (p95)** | 30.13 ms | 30.31 ms | +0.60% |
| **Latency (p99)** | 30.92 ms | 37.10 ms | +19.9% |
| **Latency (Min)** | 14.14 ms | 7.40 ms | -47.6% (C++ lower min) |
| **Latency (Max)** | 40.17 ms | 41.56 ms | +3.46% |
| **Resident Memory (RSS)** | 2.05 MB | 1.96 MB | -4.4% (C++ lower memory) |

---

## 3. Observations & Key Takeaways

1. **Parity in Throughput**: Both servers achieve ~572-577 requests/second under 16 concurrent clients over TCP loopback with ephemeral connection teardown. The throughput difference between Rust and C++ is under 1%, within normal OS network scheduling noise.
2. **Average Latency**: Average request latency is virtually identical across both implementations (27.60 ms for `/health`, ~27.5 ms for `/index.html`).
3. **Memory Footprint**: The C++ implementation operates with an extremely lightweight resident memory footprint of under 2 MB RSS, matching and slightly exceeding Rust's memory efficiency.
4. **Behavioral Fidelity**: 100% of requests succeeded with HTTP 200 responses and zero connection errors over thousands of consecutive TCP connections.
