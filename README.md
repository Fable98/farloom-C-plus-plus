# farloom (C++20)

A high-performance, learning-oriented multithreaded HTTP/1.1 web server reimplemented in **modern C++20** using POSIX networking and standard library primitives (no Boost.Asio, Crow, or external HTTP frameworks).

One acceptor thread receives incoming TCP connections and hands them to a fixed-size worker pool through a bounded request queue. When the queue is full, the acceptor immediately sheds load by responding with `503 Service Unavailable` with `Retry-After: 1`.

---

## Features

- **Static file serving** (GET only) with `index.html` directory indexing and automatic MIME type mapping by file extension.
- **Interactive Web Dashboard & Control Plane**:
  - Live heartbeat telemetry with auto-poll toggle and millisecond ping monitoring (`GET /health`).
  - Interactive API console for testing methods (`GET`, `POST`, `PUT`, `DELETE`), custom headers, request bodies, and wire response inspection.
  - In-browser concurrency & stress testing simulator with live segmented progress display, throughput (`req/s`), and average latency calculation.
  - Interactive security sandbox for executing live path-traversal attacks and verifying mitigations.
  - Self-contained zero-dependency retro workstation UI served directly out of `./public`.
- **Strict path-traversal security**:
  - Raw and percent-encoded `..`, backslashes (`\`), NUL bytes (`\0`), drive letters / colons (`:`), and leading `//` are rejected before filesystem resolution.
  - Resolved candidate paths must strictly canonicalize component-wise inside the configured static root.
- **Built-in routes**:
  - `GET /health` returns `{"status":"ok"}` (`application/json`).
  - `POST /api/echo` returns the request payload preserving the client's `Content-Type` (or defaulting to `application/octet-stream`).
- **Standard HTTP/1.1 semantics**:
  - Request body delimitation via `Content-Length`.
  - Rejection of unsupported transfer encodings (e.g. `Transfer-Encoding: chunked` returns 400).
  - Status codes supported: `200 OK`, `400 Bad Request`, `404 Not Found`, `405 Method Not Allowed` (with sorted `Allow` header), `413 Payload Too Large`, `500 Internal Server Error`, `503 Service Unavailable`.
  - Every response carries `Connection: close`.
- **Resilience and socket lifecycle**:
  - Configurable socket read and write timeouts.
  - Half-close drain budget (`close_gracefully` with `SHUT_WR`) preventing TCP RST packet generation.
  - Graceful shutdown draining queued jobs and joining worker threads.
- **Single-write structured logging**:
  - `key=value` log lines to stdout (lifecycle, access, rejection) and stderr (traversal warnings, runtime errors) locked per write to prevent line interleaving.
- **Standalone load testing utility** (`loadtest`):
  - Multi-threaded HTTP benchmark client measuring throughput (req/s), response code distribution, connection errors, and latency percentiles (min, avg, p50, p95, p99, max).

---

## Architecture

```
farloom-C-plus-plus/
├── CMakeLists.txt              # CMake build configuration (C++20)
├── README.md                   # Project documentation
├── CHECKLIST.md                # Feature parity & migration checklist
├── BENCHMARK.md                # Comparative benchmark report (C++ vs Rust)
├── .gitignore                  # Git ignore rules
├── public/                     # Static web assets & interactive dashboard
│   ├── index.html              # Server Manager dashboard UI
│   ├── style.css               # Retro workstation CSS design system
│   └── app.js                  # Telemetry, API console, & load test engine
├── include/
│   ├── result.hpp              # Type-safe Result<T, E> error handling
│   ├── reader.hpp              # Buffered I/O stream reader abstraction
│   ├── logger.hpp              # Thread-safe structured key=value logger
│   ├── config.hpp              # Server CLI options and static root validation
│   ├── mime.hpp                # Extension-to-MIME lookup table
│   ├── router.hpp              # Exact-path router with method matching
│   ├── routes.hpp              # Built-in route handlers (/health, /api/echo)
│   ├── static_files.hpp        # Path traversal guard and static file server
│   ├── bounded_queue.hpp       # Thread-safe bounded request queue
│   ├── thread_pool.hpp         # Fixed-size worker pool
│   ├── connection.hpp          # Connection dispatch, lifecycle, and drain
│   ├── server.hpp              # TCP listener, acceptor loop, and shutdown handle
│   └── http/
│       ├── request.hpp         # Request methods, versions, and headers
│       ├── response.hpp        # Status codes, response serializer
│       ├── percent.hpp         # RFC 3986 percent encoding & decoding
│       └── parser.hpp          # RFC 9112 HTTP/1.x stream parser
├── src/
│   ├── main.cpp                # Server entry point and exit code mapping
│   ├── config.cpp              # CLI configuration parser
│   ├── logger.cpp              # Log line formatting and stream locking
│   ├── mime.cpp                # MIME mapping implementation
│   ├── router.cpp              # Route table lookup implementation
│   ├── routes.cpp              # Built-in endpoints
│   ├── static_files.cpp        # Traversal checks & filesystem containment
│   ├── thread_pool.cpp         # Closure pool helper
│   ├── connection.cpp          # Request dispatch and socket drain
│   ├── server.cpp              # Acceptor loop and 503 backpressure
│   └── http/
│       ├── request.cpp         # Case-insensitive headers & method enum
│       ├── response.cpp        # Wire format generation & socket write
│       ├── percent.cpp         # Hex validation & decode
│       └── parser.cpp          # Request line, header, & body parser
├── loadtest/
│   ├── main.cpp                # Load tester entry point
│   ├── args.hpp / args.cpp     # Load tester argument parsing
│   └── stats.hpp / stats.cpp   # Latency percentiles & stats reporter
└── tests/
    ├── test_framework.hpp      # Lightweight assertion & test harness
    ├── unit/                   # Unit test executables
    └── integration/            # Real-TCP integration & lifecycle tests
```

### Core Design Components

1. **TCP Acceptor & Backpressure**:
   - The server listener is set to non-blocking mode.
   - The acceptor polls for new connections every 25ms to bound shutdown latency.
   - Accepted sockets are set to blocking mode with `SO_RCVTIMEO` and `SO_SNDTIMEO`.
   - If the bounded queue has capacity, the connection is submitted non-blockingly.
   - If the queue is full, the acceptor immediately writes a `503 Service Unavailable` response with `Retry-After: 1`, drains the socket, and closes it without burdening worker threads.

2. **Thread Pool & Bounded Queue**:
   - `BoundedQueue<T>` synchronizes tasks via `std::mutex` and `std::condition_variable`.
   - `ThreadPool<J>` spawns a fixed set of named worker threads (`worker-0` ... `worker-{N-1}`).
   - During shutdown, the queue is closed: existing buffered items are fully processed, and worker threads cleanly terminate once the queue is empty.

3. **HTTP/1.1 Parser**:
   - Consumes bytes via `BufferedReader` on top of POSIX `recv()`.
   - Enforces an 8192-byte head limit across request line and headers.
   - Verifies RFC 9110 token constraints, origin-form URI constraints, and ASCII target limits.
   - Lowercases header names for case-insensitive lookup while retaining original insertion order.
   - Validates `Content-Length` headers and rejects `Transfer-Encoding`.

4. **Security & Path Traversal Prevention**:
   - Raw and percent-decoded target validation rejects NUL bytes (`%00`), backslashes (`\`), `..`, drive colons (`:`), and leading `//`.
   - `std::filesystem::canonical` resolves symlinks and relative path elements.
   - Component-wise containment verification confirms that the canonical file path begins with the exact canonical static root directory components, preventing sibling prefix escapes.

---

## Build Instructions

### Prerequisites
- C++20 compliant compiler: **Apple Clang 15+** or **GCC 11+** or **LLVM Clang 15+**
- **CMake 3.20+**
- POSIX-compliant operating system (macOS or Linux)

### Building Release Binaries

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This compiles:
- `build/farloom` (Web server executable, also symlinked as `build/webserver`)
- `build/loadtest` (Load testing executable)
- `build/libfarloom_lib.a` (Core server static library)
- `build/test_*` (13 unit and integration test suites)

---

## Running the Server

Run from the project root so the default `./public` directory resolves:

```bash
# Start server with default options (127.0.0.1:7878, 4 workers, 64 queue capacity)
./build/farloom

# Start with custom parameters
./build/farloom --host 127.0.0.1 --port 8080 --workers 8 --queue-capacity 128 --static-root ./public

# Start on an ephemeral port (port 0, prints bound port in startup log line)
./build/farloom --port 0
```

### Server CLI Options

Both `--opt value` and `--opt=value` syntax are supported. The last occurrence wins.

| Option | Default | Description |
|---|---|---|
| `--host <HOST>` | `127.0.0.1` | Bind address |
| `--port <PORT>` | `7878` | Port (set `0` for an OS-assigned ephemeral port) |
| `--workers <N>` | `4` | Number of worker threads (must be >= 1) |
| `--queue-capacity <N>` | `64` | Job queue capacity before 503 load shedding (must be >= 1) |
| `--static-root <DIR>` | `./public` | Directory to serve static files from (must exist) |
| `--read-timeout-secs <SECS>` | `5` | Socket read timeout in seconds (must be >= 1) |
| `--write-timeout-secs <SECS>` | `5` | Socket write timeout in seconds (must be >= 1) |
| `--max-body-bytes <BYTES>` | `1048576` | Maximum request body size (1 MB; larger yields 413) |
| `--max-requests <N>` | None | Gracefully shut down after accepting N connections |
| `-h, --help` | | Print usage information and exit |

Exit codes:
- `0`: Normal exit or `--help`
- `2`: Invalid CLI options
- `1`: Startup failure (static root missing, port already in use)

---

## Trying It Out

### 1. Web Dashboard & Control Plane

When the server is running, open **`http://127.0.0.1:7878/`** in your web browser:

```bash
# macOS
open http://127.0.0.1:7878/

# Linux
xdg-open http://127.0.0.1:7878/
```

The interactive workstation dashboard provides:
- **Server Overview**: Real-time heartbeat telemetry, active uptime pings, and an interactive architectural pipeline breakdown.
- **API Console**: Compose requests, test endpoints (`/health`, `/api/echo`), set custom headers, and inspect wire responses.
- **Stress Tester**: Fire bursts of concurrent client requests directly from the browser to observe worker contention and queue backpressure with a segmented progress bar.
- **Security Sandbox**: Run live path-traversal attacks (`/..%2fCMakeLists.txt`, `/%00index.html`, etc.) to verify Farloom's containment defenses.

### 2. CLI & cURL Verification

```bash
# 1. Health check
curl -i http://127.0.0.1:7878/health

# 2. Echo request body
curl -i -X POST -H "Content-Type: text/plain" --data "Hello, Farloom C++!" http://127.0.0.1:7878/api/echo

# 3. Serve static index
curl -i http://127.0.0.1:7878/

# 4. Traversal attempt verification (returns 404 Not Found, logged to stderr as path_traversal)
curl -i --path-as-is "http://127.0.0.1:7878/..%2fCMakeLists.txt"
```

---

## Running Tests

Run the full automated test suite through CTest:

```bash
ctest --test-dir build --output-on-failure
```

The test suite validates:
1. `test_config`: CLI parsing, inline and separate flags, validations, and canonicalization.
2. `test_percent`: RFC 3986 percent-decoding and error offset detection.
3. `test_http_parser`: Request lines, headers, body sizing, EOF, and malformed inputs.
4. `test_http_response`: Response serialization, reason phrases, and Allow header generation.
5. `test_mime`: MIME type mapping by extension and case-insensitivity.
6. `test_router`: Exact path routing and 405 generation.
7. `test_static_files`: Static resolution, traversal prevention, and containment.
8. `test_thread_pool`: Thread concurrency, task queuing, full-queue rejection, and clean worker joins.
9. `test_logger`: Structured format, control character escaping, and worker tagging.
10. `test_loadtest_args`: Load tester argument parsing.
11. `test_loadtest_stats`: Load test metric calculation and status line parsing.
12. `test_integration`: Live TCP HTTP interactions, static files, 400, 404, 405, 413, and 500 error cases.
13. `test_lifecycle`: Real TCP queue saturation, 503 backpressure, idle timeouts, shutdown, and concurrent clients.

---

## Load Testing

The included `loadtest` utility benchmarks HTTP throughput and latency:

```bash
# Terminal 1: Run the server with constrained capacity
./build/farloom --workers 4 --queue-capacity 16

# Terminal 2: Run loadtest with 10,000 requests across 16 client threads
./build/loadtest --addr 127.0.0.1:7878 --path /health --requests 10000 --concurrency 16
```

### Load Tester Options

| Option | Default | Description |
|---|---|---|
| `--addr <HOST:PORT>` | `127.0.0.1:7878` | Server address |
| `--path <PATH>` | `/` | Request target (must start with `/`) |
| `--requests <N>` | `1000` | Total requests to send (must be >= 1) |
| `--concurrency <N>` | `8` | Concurrent client worker threads (must be >= 1) |
| `-h, --help` | | Print usage information |

Example report:
```
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

---

## Performance Comparison (C++20 vs Rust)

See [BENCHMARK.md](file:///Users/shresthkumar/farloom-C-plus-plus/BENCHMARK.md) for full benchmark documentation.

Across 10,000 requests at 16 concurrent connections on Apple Silicon:
- **Throughput**: Both implementations achieve **572+ req/s** on `/health` and **572-577 req/s** on static files (`/index.html`).
- **Average Latency**: **27.60 ms** on both implementations (identical to 2 decimal places).
- **Resident Memory (RSS)**: Under **2.0 MB** in both implementations (C++: **1.92 MB**, Rust: **2.00 MB**).
