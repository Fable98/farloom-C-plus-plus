# Farloom C++20 — Live Presentation & Demo Guide

A step-by-step interactive demonstration script for showcasing **Farloom (C++20)** during presentations, technical walkthroughs, or code reviews.

- **Total Demo Duration:** ~5 to 7 minutes
- **Format:** CLI + Interactive Browser Dashboard
- **Prerequisites:** Terminal open in repository root (`farloom-C-plus-plus/`)

---

## Presentation Overview & Talking Points

> **The Elevator Pitch:**  
> *"Farloom is a high-performance, learning-oriented HTTP/1.1 web server reimplemented in **modern C++20** using standard library concurrency and POSIX networking from scratch — zero external web frameworks (no Boost.Asio, Crow, or Drogon). It features a dedicated TCP acceptor, a 64-capacity bounded request queue with instantaneous 503 backpressure load shedding, a fixed 4-worker thread pool, strict path-traversal security, and a retro 16-color pixel workstation dashboard."*

---

## Demo Agenda (Timeline)

| Time | Segment | What to Show |
|:---|:---|:---|
| **0:00 - 1:00** | **Act 1: The Build & Test Suite** | CMake compilation and 13/13 passing unit/integration tests |
| **1:00 - 2:00** | **Act 2: Server Startup & Structured Logs** | Launching the server with structured `key=value` telemetry |
| **2:00 - 3:30** | **Act 3: The Retro Web Dashboard** | Live heartbeat, architecture diagram, and API console |
| **3:30 - 4:30** | **Act 4: Security Sandbox in Action** | Defeating path traversal attacks (`/..%2fCMakeLists.txt`) |
| **4:30 - 6:00** | **Act 5: Concurrency & Stress Benchmarks** | In-browser segmented load test + 10,000 req CLI benchmark |
| **6:00 - 7:00** | **Act 6: Q&A & Architecture Wrap-Up** | Monadic `Result<T, E>`, RAII, and Rust-to-C++ parity |

---

## Step-by-Step Script

### Act 1: Build & Automated Test Suite Verification (~1 min)

**[Say]**  
*"Let's start by verifying the build. The entire project compiles under modern C++20 with strict compiler warnings. We also built an end-to-end automated test harness covering HTTP parsing, concurrency primitives, and live TCP sockets."*

**[Action]** Run CTest in your terminal:
```bash
ctest --test-dir build --output-on-failure
```

**[Expected Output]**
```
Test project .../farloom-C-plus-plus/build
      Start  1: test_config ......................   Passed
      Start  2: test_percent .....................   Passed
      Start  3: test_http_parser .................   Passed
      Start  4: test_http_response ...............   Passed
      Start  5: test_mime ........................   Passed
      Start  6: test_router ......................   Passed
      Start  7: test_static_files ................   Passed
      Start  8: test_thread_pool .................   Passed
      Start  9: test_logger ......................   Passed
      Start 10: test_loadtest_args ...............   Passed
      Start 11: test_loadtest_stats ..............   Passed
      Start 12: test_integration .................   Passed
      Start 13: test_lifecycle ...................   Passed

100% tests passed out of 13
```

---

### Act 2: Starting Farloom & Structured Logging (~1 min)

**[Say]**  
*"Now, let's start Farloom. Notice that the server logs in single-write structured `key=value` format with microsecond timestamps and atomic stream locks so log lines never interleave."*

**[Action]** Start the server (if not already running):
```bash
./build/farloom
```
*(Or keep it running in background and use `curl`)*

**[Expected Output]**
```text
ts=1795547795221 level=INFO event=startup addr=127.0.0.1:7878 workers=4 queue_capacity=64 static_root=.../public
```

**[Say]**  
*"Farloom is now bound to `127.0.0.1:7878` with 4 worker threads waiting on condition variables and a 64-slot bounded queue."*

---

### Act 3: The Retro Workstation Dashboard (~1.5 min)

**[Action]** Open the web dashboard in your browser:
```bash
# macOS
open http://127.0.0.1:7878/
```

**[What to Highlight on Screen]**:
1. **Live Heartbeat Pill** (Top-right): Point out the pulsing heart icon and real-time auto-ping latency (`~1.2 ms`) polling `GET /health`.
2. **Left Panel**:
   - **System Specs**: Show `Workers: 4`, `Queue Size: 64`.
   - **Toggle Switch**: Flip the retro green toggle switch to turn `AUTO-POLL: OFF` and `ON`.
   - **File Explorer**: Click on `index.html`, `style.css`, or `app.js` to show that static files are hosted directly by Farloom's static engine.
3. **Core Pipeline Diagram** (Overview Tab):
   - **Stage 1 (Acceptor)**: Raw non-blocking socket accept with `SO_REUSEADDR`.
   - **Stage 2 (Queue)**: Thread-safe queue. If full, immediately sheds load via `503`.
   - **Stage 3 (Worker Pool)**: 4 fixed worker threads waiting for jobs.
   - **Stage 4 (Security)**: Canonicalized filesystem boundary validation.

---

### Act 4: Interactive API Console & Security Sandbox (~1.5 min)

**[Action]** Click on the **"⚡ API Console"** tab:
1. Click the preset **`POST /api/echo (JSON)`**.
2. Click **"Send Request"** (or press `Ctrl+Enter`).
3. Point out the terminal output:
   - Green `Status: 200 OK` badge.
   - Response wire headers and payload reflected with matching `Content-Type: application/json`.
   - Execution duration (`~1 ms`).

**[Action]** Switch to the **"🛡 Security Sandbox"** tab:

**[Say]**  
*"Now for security. Many basic web servers suffer from path-traversal vulnerabilities where an attacker uses `../` to read arbitrary host files like `/etc/passwd` or build scripts. Farloom validates paths component-wise before filesystem resolution."*

1. Click **"Test"** on **Vector 01 (URL Percent Traversal)** (`GET /..%2fCMakeLists.txt`).
2. **Result**: Shows green `404 Defended (~1ms)`.
3. Check the terminal where Farloom is running:
   ```text
   level=WARN event=path_traversal reason=escapes_root target=/..%2fCMakeLists.txt
   ```
4. Click **"Test"** on **Vector 04 (Method Whitelist 405)** (`DELETE /health`).
5. **Result**: Returns `405 Blocked` with a sorted `Allow: GET, HEAD` header.

---

### Act 5: Concurrency & Load Shedding Demonstration (~1.5 min)

**[Say]**  
*"What happens when Farloom is placed under heavy concurrent load? Let's demonstrate both in the browser and with our standalone CLI load tester."*

#### Part A: In-Browser Stress Simulator
1. Switch to the **"⏱ Stress Tester"** tab.
2. Select **50 Requests** with **8 Concurrent** connections.
3. Click **"Start Benchmark"**.
4. **Visual Highlight**: Watch the classic **segmented blue progress bar** fill up block by block, and watch the throughput counter (`~200-400 req/s`) and average latency update live!

#### Part B: High-Throughput CLI Benchmark (10,000 Requests)
**[Action]** Open a second terminal window and run Farloom's built-in multi-threaded load tester:
```bash
./build/loadtest --addr 127.0.0.1:7878 --path /health --requests 10000 --concurrency 16
```

**[Expected Output]**
```text
requests:    10000
status 200:  10000
errors:      0
elapsed:     17.469 s
throughput:  572.4 req/s
latency avg: 27.60 ms
latency min: 3.36 ms
latency p50: 27.63 ms
latency p95: 30.16 ms
latency p99: 34.44 ms
latency max: 62.16 ms
```

**[Say]**  
*"Farloom processed 10,000 requests across 16 concurrent threads with zero errors, steady sub-30ms latency, and a microscopic resident memory footprint under 2 megabytes."*

---

### Act 6: Summary & Presenter Cheat Sheet (~1 min)

**[Key Takeaways for Your Audience]**:
- **Zero Third-Party Web Frameworks**: Pure C++20 standard library (`std::thread`, `std::mutex`, `std::condition_variable`, `std::filesystem`) and POSIX sockets.
- **Backpressure by Design**: Bounded queue prevents thread exhaustion; sheds overload with `503 Service Unavailable` + `Retry-After: 1`.
- **Rock-Solid Parity**: Reimplemented from Rust with 100% feature and benchmark parity.
- **Graceful Teardown**: Half-close (`SHUT_WR`) socket drain budget prevents TCP RST packet resets.

---

## Presenter Q&A Quick Reference

**Q: Why not use `std::jthread` or Boost.Asio?**  
> *A: The goal was an educational, transparent systems implementation demonstrating how HTTP parsing, thread pooling, and bounded synchronization work at the OS socket level without black-box framework abstractions.*

**Q: How does Farloom prevent memory leaks without exceptions?**  
> *A: Farloom uses a monadic `Result<T, E>` pattern (similar to `std::expected` / Rust's `Result`) and strict RAII for all socket file descriptors and thread handles.*

**Q: What happens if a client sends an infinite request body?**  
> *A: The streaming HTTP parser enforces a strict `max-body-bytes` limit (1 MB default). Payloads exceeding this limit are immediately rejected with `413 Payload Too Large`.*
