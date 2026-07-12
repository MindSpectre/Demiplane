# HTTP/1.1 throughput: `demiplane::http` vs Drogon

Round 1 measured 2026-07-09 (findings 1–3 below); round 2 measured 2026-07-10
(findings 4–7, fixes shipped). All numbers are reproducible with `run_bench.sh`
and `run_perf.sh` in this directory.

## Current standing (2026-07-12 evening, after Finding 13 shipped)

Median of 3 × 20M requests, zero failures (box ran ~5% slower than in the
morning matrix — compare ratios):

|                      | demiplane         | drogon        | ratio                      | was (07-09) |
|----------------------|-------------------|---------------|----------------------------|-------------|
| pipeline 1, primary  | 440,144 rps       | 458,021 rps   | drogon **1.04x** faster    | 2.08x       |
| pipeline 16, primary | **3,160,341 rps** | 3,041,914 rps | **demiplane 1.04x faster** | 12.0x       |
| pipeline 1, control  | **479,220 rps**   | (anomalous*)  | —                          | 1.92x       |

*drogon's single control rep collapsed to 122k rps with 5ms tails while its
primary reps were normal — external contamination, not comparable.

**Pipeline-1 gap progression: 2.08x → 1.20x → 1.15x → 1.13x → 1.04x.**
demiplane beats drogon at pipeline 16 (p99 103µs vs 150µs) and holds the best
tail latency at every depth. Topology is now io_context-per-thread with
round-robin connection placement (Finding 13): per-connection serialization
is the single-runner home context (no strands), deadline enforcement is one
tracker sweep per listener (no per-connection timers).

For where these numbers sit against other frameworks (crow, oat++, Go
net/http, fasthttp, axum), see Finding 10.

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

## Finding 8 — the read loop, shipped (2026-07-11)

Lever 1 above, implemented in `Http11Driver::serve`: `parser.put()` drives the
SAME beast parser (limits, chunked, RFC correctness untouched) synchronously
over buffered bytes; `async_read_some` fires only when the parser reports
`need_more` — one async op per SYSCALL instead of one composed op per MESSAGE,
with `eager()` folding the body into the same loop. Eof mapping reproduces the
composed ops' semantics (fresh message → `end_of_stream`, mid-message →
`partial_message`). New driver tests cover trickled headers, eof mid-header,
and a malformed request mid-pipeline (response order preserved).

Effect: pipeline 1 387k → 392k (+4% — the per-request syscall boundary is
irreducible at depth 1); pipeline 16 **1.56M → 2.43M (+56%)**, p50 162µs →
130µs. The depth-16 gap fell from 2.15x to 1.31x.

## Finding 9 — framework floors: stdexec measured, asio re-measured (2026-07-11)

Same bomber, same cores, same 124-byte response, run back-to-back on the same
day (drogon same-day median: 470k / 3.19M):

| floor probe                                  | pipeline 1 | pipeline 16 |
|----------------------------------------------|------------|-------------|
| stdexec + io_uring (4 rings, SO_REUSEPORT)   | ~532k      | 6.11M       |
| raw asio epoll (shared io_context + strands) | ~465k      | 5.72M       |
| raw asio io_uring (same topology, see below) | **150k**   | **2.21M**   |
| demiplane (asio + beast, after Finding 8)    | 392k       | 2.43M       |

`stdexec_probe.cpp`: hand-rolled accept/recv/send senders against stdexec's
internal `__io_task_facade` seam (stdexec has NO public socket senders — its
networking story is `sio`, immature, no TLS). CAVEAT: the probe runs one
io_uring context per thread with SO_REUSEPORT (stdexec's ring is a
single-runner by design), so TWO variables differ from the asio probe —
io_uring vs epoll AND per-thread rings vs shared-context+strands.

The missing control landed 2026-07-11 after installing liburing-devel:
`raw_asio_probe` rebuilt with `-DBOOST_ASIO_HAS_IO_URING
-DBOOST_ASIO_DISABLE_EPOLL` (verified: the process holds an io_uring fd, no
eventpoll). Result: **150k / 2.21M — 3.1x/2.6x SLOWER than asio's own epoll
backend**, p50 528µs → 1664µs. So the attribution flips: stdexec's edge is
NOT "io_uring is faster" in general — asio's io_uring integration (one ring
shared by all threads behind the reactor lock, an SQE round-trip per op, no
speculative non-blocking attempt first) collapses under this workload, while
per-thread rings with batch completion processing (stdexec's design) beat
everything. io_uring pays off only with ring-per-thread architecture.
Practical consequence: do NOT enable `BOOST_ASIO_HAS_IO_URING` for the
demiplane server; asio-epoll is the right asio backend here.

Read: NEITHER framework is demiplane's bottleneck — both floors sit above
drogon and far above demiplane. The remaining demiplane gap vs its own asio
floor (~16% at depth 1) is framework glue (router dispatch, RequestContext
construction, per-message parser construction, remaining mallocs, Date), not
the async runtime. A migration to stdexec would buy the std::execution
trajectory, not the missing 16% — and it means hand-owning sockets and TLS
(no asio-ssl equivalent) for a codebase whose measured costs are elsewhere.
Seastar was excluded without a probe: it is a whole-process framework (owns
main(), per-core shared-nothing reactors, own allocator) — adopting it
replaces the injected-executor design (spec §9) and everything in common/,
not just this component, to chase a floor asio demonstrably already reaches.

## Finding 10 — external framework baselines (2026-07-11)

Same harness as everything above (server `taskset -c 0-3`, 4 workers; bomber
on 4-11, 8 threads, 256 conns; medians of 2 reps, 10M requests at pipeline 1 /
20M at pipeline 16; single-rep demiplane+drogon anchors run in the same
session). Zero failed requests in every run.

| server                       | pipe 1   | p99    | pipe 16   | p99       | resp bytes |
|------------------------------|----------|--------|-----------|-----------|------------|
| axum 0.8 (tokio/hyper, Rust) | **449k** | 1088µs | 3.19M     | 158µs     | 120        |
| drogon (anchor)              | 446k     | 1520µs | **3.20M** | 140µs     | 124        |
| oat++ 1.4 (async API)        | 424k     | 768µs  | 99k*      | —         | 87         |
| Go fasthttp                  | 405k     | 1600µs | 3.09M     | 236µs     | 123        |
| demiplane (anchor)           | 385k     | 848µs  | 2.51M     | **128µs** | 124        |
| crow master                  | 298k     | 984µs  | 99k*      | —         | 87         |
| Go net/http (stdlib)         | 276k     | 3808µs | 335k      | 3392µs    | ~110       |

The box ran ~2-5% slower than on 07-10 (demiplane anchor 385k vs 392k, drogon
446k vs 470k) — compare within this table, not across days.

*crow and oat++ collapse to 99k rps at pipeline 16 with an identical
41ms-per-batch signature: **neither sets `TCP_NODELAY` on accepted sockets**
(zero mentions in either codebase — not even an option), so pipelined batches
hit the classic Nagle + delayed-ACK stall. This is Finding 1 again, shipping
in released frameworks. Control: an `LD_PRELOAD` shim forcing `TCP_NODELAY`
in `accept4()` lifts crow to 383k and oat++ to 481k at pipeline 16 — i.e.
the collapse was 100% the socket default, but even fixed, neither batches
pipelined responses, which is what separates the ~400k tier from the 3M tier
(Finding 6).

Probe notes, for fairness:

- axum runs a manual accept loop (the documented serve-with-hyper pattern)
  so it gets `TCP_NODELAY` like everyone else, and hyper's
  `pipeline_flush(true)` (its own batching option, off by default).
- Go servers run `GOMAXPROCS(4)`; net/http sets NODELAY by default but never
  batches (335k at p16); fasthttp batches natively.
- Response bodies are all "pong"; header sets differ per framework (crow and
  oat++ send no Date — 87 bytes vs our 124), so per-response work differs
  slightly in their favor.
- userver was skipped: it needs a large sudo-installed system dependency set
  and a ~30min build, and like seastar it is a whole-framework adoption
  (own task engine, coroutine scheduler, DI) — not a drop-in probe.

Read: the ~450k pipeline-1 cluster (axum == drogon, with oat++ and fasthttp
just behind) is the practical wall for shipped frameworks on this box — only
the glue-free floor probes (raw asio 465-468k, stdexec 532k) exceed it, so
demiplane's remaining 14% to drogon is the last of the framework glue, not a
structural deficit. At pipeline 16 demiplane's 2.51M is in the batching elite
(only drogon/axum/fasthttp are ahead, all within 1.27x) and it has the best
p99 of the entire field at both depths' top tier. The "modern framework"
comparison flatters no one: Rust's flagship stack lands exactly on drogon,
Go's stdlib is 1.4-9.6x behind, and two popular C++ frameworks ship with a
Nagle footgun that demiplane fixed in Finding 1.

Sources/scripts: `benchmark_results/http/frameworks/` (gitignored) —
`crow_probe.cpp`, `oatpp_probe.cpp`, `axum_probe/`, `go_*_probe.go`,
`nodelay_shim.c`, `run_frameworks.sh`, `run_nodelay_control.sh`,
`results*.jsonl`.

## Finding 11 — the glue round: Date cache, stamping fold, dispatch/flush de-coroutining (2026-07-11, shipped)

Round 4 attacked the ~8.9k cycles/request of per-request glue between
demiplane (54.2k cycles/req) and drogon (45.3k) — each lever measured in
isolation with `percall_stat.sh` (6M requests, pipeline 1, release-perf
build; drogon's cycles/req held at 45.3-45.7k across every A/B run):

| lever                    | change                                                                                                                                                                                                    | cycles/req | Δ                   |
|--------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------|---------------------|
| (baseline)               | —                                                                                                                                                                                                         | 54,217     | —                   |
| L1 Date cache            | per-second `thread_local` IMF-fixdate (`types/http_date`), replaces gmtime_r+snprintf per response                                                                                                        | 52,617     | **−1,600**          |
| L2 stamping fold         | Date/Server presence detected during `serialize_response`'s existing header pass (deleted `stamp_common_headers`: 2 contains + 2 erase_if scans, 2 arena string pairs); `OwnedBacking` reserves 4 entries | 51,591     | **−1,026**          |
| L3 dispatch de-coroutine | no-hooks `Router::dispatch` is a plain function tail-forwarding the handler's awaitable (was: a wrapper coroutine frame per request)                                                                      | 51,232     | −359 (−1,300 instr) |
| L5a flush inline         | `flush()` coroutine member replaced by inline `async_write` at both call sites                                                                                                                            | 51,040     | −192 (−461 instr)   |

Total: **−3,177 cycles/req (−5.9%), −6,938 instructions/req (−14%)**. The
profile pre-round showed the Date snprintf at 1.42% of ALL cycles
(`__printf_buffer` + `__printf_buffer_write`) — Finding 3's 0.8% estimate was
low. Full-matrix effect (median of 3 × 20M): pipeline-1 primary 392k → 415k;
pipeline-16 primary 2.43M → **3.42M (+41%, now ahead of drogon's 3.33M)**;
pipeline-1 control 423k → **463k — at the raw-asio floor**. The pipeline-16
jump is outsized because the removed per-request frames/scans were the only
work between batched responses.

Allocation gate tightened: the no-hooks hot path (context build → route →
handler frame → beast translation) now does **zero global-heap allocations**
(`BareRouteDispatchAddsNoGlobalHeap`) — the handler's coroutine frame recycles
via asio's frame cache; the dispatch frame that used to be the +1 is gone.

Levers assessed and closed with evidence:

- **Parser reuse (user ask)**: beast `request_parser` has no reset facility;
  reconstruction per message is already arena-bump + member init and the ctor
  does not register at a 0.02% profile floor. What DOES show (~0.5%) is
  beast's field-table insertion **during parsing** — inherent to using
  beast's parser, only removable by hand-rolling one. Not worth it at this
  gap.
- **Router lookup (user ask)**: already optimal pre-round —
  `boost::unordered_flat_map`, transparent string_view hash, zero-alloc exact
  match, method dispatch by array index. The router's cost was the dispatch
  coroutine wrapper (L3), not the lookup.
- **Status-line fast path**: `obsolete_reason`/`to_chars` below the 0.02%
  profile floor — skipped.

Remaining vs drogon (~5.4k cycles/req): asio's per-op machinery through the
type-erased `any_io_executor` awaitable frames + strand/executor dispatch
(the largest visible cluster), beast field-table parsing inserts, and
`RequestContext` move ceremony. Next levers if ever needed:
`awaitable<T, io_context::executor_type>` for `AsyncResponse` (public handler
API change), per-thread io_context (spec §9 decision, the only path past the
~465k shared-loop floor — see Finding 9/10).

## Finding 12 — awaitable de-erasure + the topology price tag (2026-07-12)

Two questions answered the same day.

**(a) De-erasing the awaitable frames (shipped).** Finding 7 noted that the
concrete-executor work (any_io_executor removed from sockets/streams) won only
~1% because `awaitable<T>` defaults to `awaitable<T, any_io_executor>` — the
coroutine FRAMES re-erased the executor on every await
(`awaitable_thread<any_io_executor>::pump`, `any_executor_base::execute`).
This round finished the job: `AsyncResponse`/`AsyncOutcome`/`AsyncVoid` are
now `awaitable<..., Strand>` (async_outcome.hpp), the request-path coroutines
(serve, dispatch, handlers, body reads, deadline_watchdog, connection
close/handshake) are all Strand-typed, and ops inside them use the
`use_strand_awaitable` token (executor.hpp). Verified: zero
`any_io_executor` / `any_executor_base` symbols in the post-change profile.

Effect (percall_stat, same-session drogon control): cycles/req ratio vs
drogon 1.117 → 1.106, instr ratio 1.134 → 1.120 — **~1% of total, as the
earlier de-erasure predicted**. The win is real but small because asio's
recycling allocator had already absorbed the frame allocations; what remains
is virtual-dispatch removal on the pump path.

API note: handler coroutines are now typed on the connection strand — code
inside a handler must await Strand-compatible awaitables (use
`use_strand_awaitable` for raw asio ops). Erased `awaitable<T>` from
elsewhere (e.g. generic helpers like `chrono::async_sleep_for`) cannot be
awaited in a handler anymore; that friction is the deliberate price of the
last 1%.

**(b) Per-thread io_context probe (the spec §9 price tag).** Why was
per-thread rejected? Because it replaces §9's injected-executor design
(Server owns ONE io_context; users can inject their own) with per-thread
accept loops + thread-local connection tracking — an architecture change.
What it buys was never isolated on asio: the stdexec probe conflated
ring-per-thread with io_uring. `raw_asio_perthread_probe.cpp` (io_context{1}
per thread + SO_REUSEPORT acceptors, no strands) vs the shared probe,
back-to-back same session, 2 reps, 20M requests:

| topology (asio-epoll both)            | pipeline 1          | pipeline 16       |
|---------------------------------------|---------------------|-------------------|
| shared io_context + strand per conn   | 489.6k              | 5.74M             |
| io_context per thread (+SO_REUSEPORT) | **545.2k (+11.4%)** | **6.19M (+7.8%)** |

So the shared scheduler (one epoll + reactor mutex + strand dispatch) costs
**~11% at the floor** — essentially ALL of stdexec's previous +14% edge
(io_uring itself contributed nothing; consistent with Finding 9's asio-uring
collapse). This is the number the §9 decision should weigh: per-thread
topology is the only remaining structural lever, worth ~11% ceiling-lift,
orthogonal to framework choice (asio does it fine), and independent of
everything shipped so far.

## Finding 13 — io_context-per-thread shipped: the topology, its two traps, and the sweep (2026-07-12)

Finding 12(b) priced per-thread asio-epoll at +11% on the strandless probe.
Shipping it into demiplane hit TWO traps the probe couldn't show, both found
by measurement:

1. **Per-connection watchdog timers stall single-runner workers.** The
   round-4 deadline watchdog (one coroutine + steady_timer per connection,
   500ms tick) cost pipeline-1 throughput a catastrophic **35%** under
   per-thread contexts (290k vs 448k without it), with 4–8ms p99 tails; the
   damage scaled with wakeup frequency (5s tick still lost 3%). On the shared
   context the same timers were harmless — 4 threads absorbed the wakeups.
   Fix: deadline enforcement moved into the ConnectionTracker — ONE sweep
   coroutine per listener walks the (already mutex-guarded) entry list every
   500ms and force-cancels expired connections through their own executors;
   connections' `deadline_` became a relaxed atomic (bare store on x86) and
   the per-connection watchdog machinery (timer member, serve_finished,
   end_watchdog) was deleted. 512 timer wakeups/sec became 2. Idle-kill
   verified: FIN at 10.5s (10s header deadline + tick).
2. **Strands on single-runner contexts are pure loss.** Keeping
   strand-per-connection over per-thread contexts LOST throughput vs the
   shared baseline (424k vs 447k): every completion pays a strand-queue
   round-trip that shared-context work-stealing used to hide. `Strand` is now
   an alias for the bare `Executor` (executor.hpp) — serialization IS the
   connection's single-runner home context. This changes the concurrency
   contract: **each injected executor must be driven by at most one thread**
   (Server doc, spec §9 amendment).

Also measured: the `io_context{1}` concurrency hint is a consistent LOSS in
demiplane (290k with timers, 440k vs 454k in the final config) despite the
bare probe liking it — run_standalone uses default-hint contexts.

Architecture shipped: `Server(cfg, vector<Executor>)` (old single-executor
ctor delegates; ctor validates non-empty), listeners take the executor set
and place connections round-robin (`execs_[next_++ % N]` at accept), accept
loop + acceptor + backoff + drain poll live on `execs.front()` (the "home"),
Server control flow (setup barrier, observers, graceful_shutdown, waits) on
`execs.front()`. run_standalone: N default-hint io_contexts, guard + jthread
each, SIGINT on the control context, §9.7 teardown per context. Shutdown
needed NO redesign: signals stay per-listener (emit dispatched onto the run
executor), tracker was already mutex+atomic, per-conn cancels already
dispatch through each connection's own executor; the tracker's internals
moved into a shared_ptr'd State so the sweep can never dangle.

Result (medians of 3×20M, same-session drogon):

|                      | pipeline 1          | pipeline 16     |
|----------------------|---------------------|-----------------|
| shared (F12)         | 1.13x behind drogon | 1.03x ahead     |
| **per-thread (F13)** | **1.04x behind**    | **1.04x ahead** |

Same-conditions spot A/B during bring-up: shared 412k → per-thread 454k
(**+10.2%**, matching the probe's +11.4% prediction). Verified: all 11 HTTP
suites in debug + ASan + TSan (BUILD_COMPONENTS=ON), zero failed requests,
byte-identical responses, idle-kill, new multi-context lifecycle tests
(graceful + force-cancel drain across 4 contexts) and a round-robin
placement proof (2 contexts, drive one, only its connections respond).

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
