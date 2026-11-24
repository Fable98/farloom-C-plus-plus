# Farloom C++20 Feature-Parity Checklist

## 1. CLI & Configuration
- [x] Options parsing: `--host`, `--port`, `--workers`, `--queue-capacity`, `--static-root`, `--read-timeout-secs`, `--write-timeout-secs`, `--max-body-bytes`, `--max-requests`, `-h`/`--help`
- [x] Both `--opt value` and `--opt=value` accepted
- [x] Last occurrence wins
- [x] Positive integer validations (workers, queue_capacity, timeouts, max_requests > 0)
- [x] Exit codes: 0 for success / --help, 2 for config errors, 1 for startup/runtime errors
- [x] Help message formatting identical to original
- [x] Static root validation and canonicalization

## 2. HTTP/1.1 Engine & Parser
- [x] Parsing request line: Method, Target (Path + optional Query), Version (HTTP/1.0, HTTP/1.1)
- [x] Method parsing: GET, POST, Other(string) - case sensitive
- [x] Target origin-form verification (must start with `/`, reject raw control/whitespace chars, '#' disallowed)
- [x] RFC 3986 percent-decoding & validation: `%XX` hex checks, preserves non-encoded chars including `+`
- [x] Header parsing: RFC 9110 token check, colon delimiter, trim spaces/tabs, reject obsolete line folding, reject control chars in values
- [x] Lowercase header names for lookup, retain insertion order
- [x] Content-Length parsing: single or matching duplicate values, reject invalid chars / conflicting values, reject Transfer-Encoding
- [x] Head limit (default 8192 bytes) enforcement -> 400 Bad Request
- [x] Max body bytes enforcement -> 413 Payload Too Large (without consuming body)
- [x] Partial/malformed requests handling -> 400 Bad Request
- [x] Premature EOF handling -> Incomplete / ConnectionClosed
- [x] Socket timeout handling -> Timeout
- [x] Response status codes & reason phrases: 200 OK, 400 Bad Request, 404 Not Found, 405 Method Not Allowed, 413 Payload Too Large, 500 Internal Server Error, 503 Service Unavailable
- [x] Response serialization: HTTP/1.1 status line, headers, `Content-Length`, `Connection: close`, flush
- [x] Error responses default body: `{code} {reason}\n` with `Content-Type: text/plain; charset=utf-8`
- [x] Allow header generation for 405 (sorted, e.g. "GET, POST")

## 3. Server Networking & Concurrency
- [x] POSIX TCP socket creation, options (`SO_REUSEADDR`), non-blocking listener, bind, listen
- [x] Ephemeral port support (port 0)
- [x] Acceptor loop with polling interval (25 ms) for responsive shutdown
- [x] Non-blocking accept handling `EAGAIN` / `EWOULDBLOCK` / `EINTR`
- [x] Accepted socket preparation: switch to blocking, set SO_RCVTIMEO and SO_SNDTIMEO
- [x] Bounded request queue (`std::mutex`, `std::condition_variable`, fixed capacity, non-blocking `try_submit`)
- [x] Immediate 503 rejection when queue is full with `Retry-After: 1`
- [x] Worker thread pool with fixed size, named threads (`worker-0`..`worker-{N-1}`)
- [x] Safe task execution: catch exceptions / errors, log error, keep worker thread alive
- [x] Graceful close (`close_gracefully`): half-close write side (`SHUT_WR`), drain incoming data up to budget (250ms worker, 50ms rejector) / max bytes (64KB) to prevent TCP RST

## 4. Routing & Static Files
- [x] Exact path routing: match raw path, route lookup
- [x] Built-in route: `GET /health` -> `{"status":"ok"}` (200 OK, application/json)
- [x] Built-in route: `POST /api/echo` -> body copied with request's `Content-Type` (or `application/octet-stream`)
- [x] 405 Method Not Allowed for known routes with wrong method, listing sorted `Allow` methods
- [x] Static file handler for unmatched paths:
  - [x] Percent-decode path
  - [x] Path traversal protections: reject `..`, `\`, `\0`, leading `//`, `:`
  - [x] Directory index: append `index.html` for directories
  - [x] Canonical path resolution & containment check (`starts_with(canonical_root)`)
  - [x] Regular file check
  - [x] Safe error mapping (404 for missing / not-a-file / traversal, 500 for other I/O errors)
  - [x] MIME type detection by extension (html, css, js, json, txt, png, jpg, gif, svg, ico, default application/octet-stream)
  - [x] Support binary file transfer with exact bytes and Content-Length

## 5. Logging
- [x] Single-write log output to stdout / stderr with mutex locking to prevent interleaving
- [x] Timestamps in milliseconds since Unix epoch (`ts=...`)
- [x] Value escaping: CR, LF, tabs, quotes, backslashes, controls
- [x] `startup` event on stdout (`addr`, `workers`, `queue_capacity`, `static_root`)
- [x] `access` event on stdout (`worker`, `client`, `method`, `path`, `status`, `dur_ms`)
- [x] `rejected` event on stdout (`client`, `status=503`)
- [x] `worker_exit` event on stdout (`worker`)
- [x] `path_traversal` warning on stderr (`worker`, `client`, `path`, `rule`)
- [x] `error` event on stderr (`worker`, `msg`)
- [x] Per-thread worker labeling (`acceptor` vs `worker-N`)

## 6. Graceful Shutdown
- [x] Stop acceptor loop via `ShutdownHandle`
- [x] Close listener socket
- [x] Drain queued connections in the bounded queue
- [x] Join worker threads cleanly
- [x] Support `--max-requests` clean shutdown

## 7. Load Testing Tool
- [x] CLI options: `--addr`, `--path`, `--requests`, `--concurrency`, `-h`/`--help`
- [x] Both `--opt value` and `--opt=value`
- [x] Defaults: addr=127.0.0.1:7878, path=/, requests=1000, concurrency=8
- [x] Thread pool / worker threads dispatching requests
- [x] Atomic counter / ticket distribution
- [x] Parse HTTP response status line
- [x] Aggregate metrics: total requests, counts per status code, errors, elapsed time, requests per second
- [x] Latency metrics: average, min, max, p50, p95, p99 percentiles
- [x] Formatted report matching original output format
- [x] Exit codes: 0 for success, 1 for no responses, 2 for arg errors
