# HTTP/1.1 throughput: `demiplane::http` vs Drogon

Round 1 measured 2026-07-09 (findings 1–3 below); round 2 measured 2026-07-10
(findings 4–7, fixes shipped). All numbers are reproducible with `run_bench.sh`
and `run_perf.sh` in this directory.

## Current standing (2026-07-10, after findings 4–6 shipped)

Median of 3 × 20M requests, zero failures:

|                      | demiplane     | drogon        | drogon faster by | was (07-09) |
|----------------------|---------------|---------------|------------------|-------------|
| pipeline 1, primary  | 386,753 rps   | 481,284 rps   | **1.24x**        | 2.08x       |
| pipeline 16, primary | 1,561,308 rps | 3,361,018 rps | **2.15x**        | 12.0x       |
| pipeline 1, control  | 428,634 rps   | 444,305 rps   | **1.04x**        | 1.92x       |

Tail latency flipped in demiplane's favor: p99 at pipeline 1 primary is 720µs
vs drogon's 1088µs (control: 360µs vs 4928µs). Note drogon's control number is
client-starved (below its primary), so the control ratio flatters demiplane;
the primary ratio is the honest contended comparison.

## Binaries

| Target                                        | Source                    | Role                                 |
|-----------------------------------------------|---------------------------|--------------------------------------|
| `Demiplane.Benchmarks.Http.BenchServer`       | `bench_server.cpp`        | subject — `GET /ping` → `"pong"`     |
| `Demiplane.Benchmarks.Http.DrogonBenchServer` | `drogon_bench_server.cpp` | reference, same endpoint             |
| `Demiplane.Benchmarks.Http.Bomber`            | `http_bomber.cpp`         | saturating keep-alive load generator |

Drogon comes from the in-tree vcpkg manifest (feature `drogon-benchmarks`, port
1.9.13). The bomber was rewritten for this work: the version restored in PR 7
opened a fresh TCP connection per request, blocked one request at a time per
thread, and **slept `interval_ms` between requests**. It measured connection
setup and its own sleep — it could not saturate anything.

## Method

- Build: `cmake --preset release -B build/bench -DDMP_ENABLE_LOGGING=OFF -DDMP_COMPONENT_LOGGING=OFF`
- 100,000,000 requests per run, 5,000,000-request warmup excluded from the clock,
  3 repetitions, median reported. Client-side `steady_clock` only — counting on
  the server would put a contended RMW on its hot path that Drogon does not carry.
- Server pinned `taskset -c 0-3` (4 threads, 4 physical cores), 4 io threads.
- Box: Ryzen 5 7600X, 6 physical cores / 12 SMT threads, 1 NUMA node,
  `performance` governor. CPUs 0-5 are distinct cores, 6-11 their siblings.

### Fairness

Both servers answer `/ping` in **exactly 124 bytes on the wire**, verified with
`curl -D -`. Out of the box Drogon answered in 143: it appends `; charset=utf-8`
to the content type and its `Server` token is longer. Bytes written per response
is a throughput input, so `drogon_bench_server.cpp` pins the content type to
`text/plain` and the `Server` token to the same length. Nothing else in Drogon's
response path is touched.

Also equalized: Drogon runs with `enableReusePort(false)` (demiplane's
`TcpListener` sets only `reuse_address` — it has no `SO_REUSEPORT`), gzip and
brotli off, keep-alive uncapped, logging at `kFatal`. Demiplane is built with
logging compiled out.

HTTP/1.1 is guaranteed structurally on both sides: `Http2Driver::serve()` is a
scaffold that logs and closes, h2 needs TLS+ALPN, and Drogon likewise only speaks
h2 over TLS. Plaintext TCP ⇒ h1 on both.

### The core-layout caveat

The primary layout puts the client on CPUs 4-11. CPUs 6-9 are the SMT siblings of
the server's cores 0-3, so client and server contend inside the same physical
cores. **This layout cannot measure demiplane's absolute throughput on 4 cores.**
It compares two servers under identical contended conditions.

A control layout (client confined to `4,5,10,11` — 2 physical cores plus their
own siblings, zero overlap) bounds the damage:

|           | primary | control | delta     |
|-----------|---------|---------|-----------|
| demiplane | 223,817 | 242,045 | **+8.1%** |
| drogon    | 464,632 | 464,830 | +0.04%    |

Control is higher for both, so the client was never starved at 2 physical cores —
the SMT contention was costing the *server*. It costs demiplane 8.1% and Drogon
essentially nothing, so the primary layout understates demiplane, and unevenly.

## Results

Median of 3 × 100M requests. Zero failed requests across all 1.4B requests served.

|                      | demiplane   | drogon        | drogon faster by |
|----------------------|-------------|---------------|------------------|
| pipeline 1, primary  | 223,817 rps | 464,632 rps   | **2.08x**        |
| pipeline 16, primary | 270,796 rps | 3,257,960 rps | **12.0x**        |
| pipeline 1, control  | 242,045 rps | 464,830 rps   | 1.92x            |

Run-to-run spread was under 1% everywhere. Latency at pipeline 1 (p50/p99):
demiplane 1136/1328 µs, drogon 512/1088 µs.

Pipelining buys demiplane **1.21x**; it buys Drogon **7.0x**.

## Finding 1 — no `TCP_NODELAY` on accepted sockets

`components/http/` never sets it. `tcp_listener.hpp` sets only `reuse_address`,
and that on the *acceptor*. The h1 driver writes one response per request, so a
pipelined batch ships response 1 and then blocks on the peer's delayed ACK
(~40 ms) before response 2 goes out.

| demiplane, pipeline 16 | rps     | p50     |
|------------------------|---------|---------|
| without `TCP_NODELAY`  | 30,336  | 2560 µs |
| with `TCP_NODELAY`     | 342,831 | 228 µs  |

An 11x collapse. Pipeline 1 is unaffected (245k vs 241k — noise), which is why
this hid: a client that sends one request at a time and waits piggybacks the ACK
on its next request, so Nagle never engages.

A one-line fix is applied in `tcp_listener.hpp` (set `no_delay` on the accepted
socket). **All pipelined numbers above include it** — without it they measure a
kernel timer, not HTTP.

## Finding 2 — the write path does not amortize

Socket syscalls per request, counted with an `LD_PRELOAD` interposer:

|                     | pipeline 1 | pipeline 16 |
|---------------------|------------|-------------|
| demiplane `sendmsg` | 1.017      | **1.020**   |
| demiplane `recvmsg` | 1.244      | 0.214       |
| drogon `write`      | 1.022      | **0.064**   |
| drogon `readv`      | 1.022      | 0.064       |

Demiplane amortizes *reads* under pipelining (1.244 → 0.214, Beast parses
requests 2..16 out of its `flat_buffer` with no syscall) but its **write count is
flat at ~1 per request regardless of depth**. `Http11Driver::serve()` calls
`write_response()` once per request, so 16 pipelined requests cost 16 `sendmsg`
calls. Drogon coalesces the batch into one `write` — 1 syscall per ~15.6
responses, in both directions.

That is the whole of the 12x gap at depth 16.

Secondary: at depth 1 demiplane issues 1.24 `recvmsg` per request against
Drogon's 1.02. The driver's phase-2 `async_read` for the body re-enters the
socket on a bodyless GET about a quarter of the time.

## Finding 3 — the per-request cost is Asio's type-erased executor, not the Date header

`perf record --call-graph=dwarf`, pipeline 1, 20M requests. Throughput under
`perf` matched the unprofiled runs (221k / 464k), so the profile is not distorted.

Cycle share by DSO:

|             | demiplane | drogon |
|-------------|-----------|--------|
| own binary  | 38.1%     | 11.4%  |
| kernel      | 39.5%     | 59.7%  |
| `nf_tables` | 9.6%      | 19.2%  |
| libc        | 8.8%      | 3.8%   |

Normalizing by throughput (`share × 4 cores / rps`), **demiplane burns ~6.5x more
userspace CPU per request** than Drogon, but only ~1.3x more kernel time. Drogon
has pushed its userspace path down far enough that it is syscall-bound; demiplane
is bound by its own request path.

Where that userspace time goes (share of total cycles):

| bucket                                                                                                                                               | demiplane |
|------------------------------------------------------------------------------------------------------------------------------------------------------|-----------|
| Asio executor machinery (`any_io_executor`, `executor_work_guard`, `scheduler::work_finished`, `shared_target_executor`, `can_prefer`, `do_run_one`) | **12.2%** |
| Beast write serializer (`buffers_suffix`, `buffers_cat_view`, `buffers_ref`)                                                                         | **8.6%**  |
| `malloc`/`free`                                                                                                                                      | 5.6%      |
| `Date` header formatting (`gmtime_r` + `snprintf`)                                                                                                   | **0.8%**  |

The `Date` header was the predicted hotspot — `http11_driver.cpp` even carries a
TODO saying to cache it per second. **It is 0.8%.** The TODO is correct and worth
doing, but it is not why demiplane is 2x slower. Fixing it first would have been
wasted work.

The dominant cost is type erasure. `Server` takes an injected
`boost::asio::any_io_executor` (spec §9), and `TcpListener` wraps each connection
in `make_strand(exec_)`. Every handler dispatch therefore pays a virtual call plus
strand bookkeeping. Drogon uses a concrete event loop and pays neither. The
injected-executor design is a deliberate choice with a now-measured price tag.

`malloc` at 5.6% is worth a second look given the per-connection
`RequestArena` — some allocations are escaping it.

Note `nf_tables` (host firewall on loopback) taxes both servers per packet. It
inflates kernel share and is environmental; absolute numbers would be higher with
it off, and the gap would likely widen, since Drogon is the more kernel-bound of
the two.

## Round 2 — 2026-07-10

Hardware-counter ground truth (`perf stat` attached to the server, 6M requests,
pipeline 1, before the round-2 fixes):

| per request  | demiplane | drogon | ratio            |
|--------------|-----------|--------|------------------|
| cycles       | 73,239    | 44,667 | 1.64x            |
| instructions | 71,622    | 37,586 | **1.91x**        |
| IPC          | 0.98      | 0.84   | demiplane better |
| ctx switches | ~0        | ~0     | —                |

The rps gap equals the cycles/request gap exactly, and it is instruction
COUNT, not stalls: demiplane's IPC is higher. Splitting by DSO: kernel
cycles/request were near-equal (41.4k vs 36.4k, 1.14x) while userspace was
29.4k vs 7.2k — **4.07x**. Syscalls/request likewise near-equal (interposer:
1.0 sendmsg + ~1.2 recvmsg + 0.45 epoll_wait vs drogon's 1.0 write + 1.0
readv + 0.82 epoll_wait; timerfd ≈ 0.02). The entire deficit was userspace
work per request — TCP was exonerated.

## Finding 4 — beast's per-op stream timeouts (+18%)

`basic_stream::expires_after` is not a passive deadline: with an expiry set,
EVERY `async_read_some`/`async_write_some` arms `timer.async_wait(...)` at
initiation and `timer.cancel()` at completion, and the cancel posts the
aborted timeout-handler through the strand as an extra dispatch
(`impl/basic_stream.hpp`: `transfer_op`). Per request that was ~2 timer-queue
inserts + 2 cancels + 2 extra executor dispatches stacked on the 2 real I/O
ops. Removing the two `expires_after` calls alone: 288.5k → 341.5k rps.

Shipped as: `Connection::set_deadline_after()` (a plain strand-confined store,
~one clock read per phase) + a per-connection `deadline_watchdog` coroutine
(`listener_base.hpp`) ticking every 500ms and force-cancelling past-deadline
connections through the existing `conn->cancel()` kill path. One timer op per
tick per CONNECTION instead of two per REQUEST. Enforcement granularity is
the tick; config timeouts are seconds. Idle-kill verified firing at 10.0s
(header_timeout=10s) with a clean FIN.

## Finding 5 — beast's write serializer (+15%)

`http::async_write(msg)` walks lazy `buffers_cat/buffers_suffix/buffers_prefix`
views on every write — 11.4% of ALL cycles (≈30% of userspace), the single
largest userspace bucket. Drogon's equivalent (render headers into a flat
buffer, write) costs ~0.9%. Replaced with `Http11Driver::serialize_response`:
flat-render the status line + headers + body into a per-connection buffer,
byte-identical output (still 124 bytes for /ping). In isolation: 288.5k →
331.5k rps. `detail::make_beast_response` is now unused by `serve()` (kept —
it is unit-tested; delete when convenient).

## Finding 6 — pipelined response batching (the 12x, closed to 2.15x)

Finding 2's fix: responses now accumulate in the per-connection buffer and
flush with ONE write when the input buffer holds no more pipelined bytes
(drogon's model), when keep-alive ends, or at a 256 KiB batch cap. Error
responses serialize into the same buffer (order preserved) and a post-loop
flush covers every exit path. Pipeline 16: 342k → 1.56M rps; pipeline 1
behavior unchanged (the buffer is always dry there → flush per request).

Combined effect of findings 4+5+6 at pipeline 1: 288.5k → 386.8k rps (+34%),
p999 1424µs → 928µs.

## Finding 7 — asio is NOT the ceiling (control experiment)

A ~120-line raw-asio probe with demiplane's EXACT topology — one shared
io_context, 4 worker threads, strand per connection, one `awaitable<void>`
coroutine per connection, hand parser, flat writes, batching, no timers —
measured under the identical harness:

|           | pipeline 1 | pipeline 16 |
|-----------|------------|-------------|
| raw asio  | 481k rps   | 5.84M rps   |
| drogon    | 481k rps   | 3.36M rps   |
| demiplane | 387k rps   | 1.56M rps   |

Same-topology asio+coroutines+strands MATCHES drogon at depth 1 and beats it
1.7x at depth 16. The remaining demiplane gap (~10k cycles/request) is not
asio and not the executor topology; it sits in the beast read path and
framework glue: `async_read_header`'s composed-op ceremony per message, the
per-message `request_parser` construction, per-op `bind_cancellation_slot`,
and the coroutine frames running on `awaitable<void>` =
`awaitable<void, any_io_executor>` — the awaitable layer type-erases the
executor again regardless of the socket's concrete strand type (visible as
`awaitable_thread<any_io_executor>::pump` in the profile; this is why the
executor de-erasure of the socket types alone moved only ~1%).

Next levers, in expected-value order, if the remaining 1.24x matters:

1. Parse from a raw buffer loop (beast `basic_parser::put` on buffered bytes;
   composed async op only at the actual syscall boundary) — removes the
   per-message op ceremony that dominates the depth-16 gap.
2. `awaitable<T, io_context::executor_type>` through the driver/router path.
3. Per-thread io_context (drogon's topology) — eliminates strands entirely;
   measured 2–3x for AsyncResourcePool, but conflicts with the injected-
   executor design (spec §9), so it is an architecture decision, not a patch.

## Reproducing

```
cmake --preset release -B build/bench -DDMP_ENABLE_LOGGING=OFF -DDMP_COMPONENT_LOGGING=OFF
cmake --build build/bench --target Demiplane.Benchmarks.Http.{Bomber,BenchServer,DrogonBenchServer}
./benchmarks/http/run_bench.sh build/bench 100000000     # ~70 min
cmake --preset release-perf && cmake --build build/release-perf --target ...
./benchmarks/http/run_perf.sh build/release-perf 20000000 1
```

`run_perf.sh` uses `build/release-perf`, which sets `DMP_KEEP_FRAME_POINTERS=ON`.
That matters: root `CMakeLists.txt` appends `-fomit-frame-pointer` in Release as a
*target* compile option, which lands after `CMAKE_CXX_FLAGS` on the command line
and would beat a preset that merely added `-fno-omit-frame-pointer` to the flags.

## TSan

Separately: `bench_server` built with `-DDMP_ENABLE_LOGGING=OFF` and run under
ThreadSanitizer (4 threads, 64 connections, 600k requests across pipeline 1 and 8)
reports **zero races** and exits 0. With logging compiled out, `COMPONENT_LOG_*`
expands to `DummyStream()`, so `ComponentLoggerManager::get()` is never called,
so `nexus::instance()` never runs and the Nexus janitor thread never starts. The
three previously-known races were Nexus, not HTTP.

```
cmake --preset tsan -B build/tsan-nolog -DBUILD_COMPONENTS=ON -DDO_BENCHMARKS=ON \
      -DBUILD_EXAMPLES=OFF -DDMP_ENABLE_LOGGING=OFF -DDMP_COMPONENT_LOGGING=OFF
```

Both overrides are required: the `tsan` preset sets `BUILD_COMPONENTS=OFF` *and*
`DO_BENCHMARKS=OFF`. Use a fresh build dir — `DMP_COMPONENT_LOGGING` is only
*declared* when `DMP_ENABLE_LOGGING` is ON, but root `CMakeLists.txt` tests it
unconditionally, so a stale cache entry silently re-enables the define.
