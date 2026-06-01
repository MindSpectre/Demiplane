# HTTP Redesign — Design & Plan Review

**Date:** 2026-05-31
**Reviewer:** Claude (design review at user request)
**Reviewing:** `2026-05-07-http-redesign-design.md` (spec) + `2026-05-07-http-types-layer.md` (PR1 plan)

---

## Verdict

The **macro architecture is sound and clearly the product of real thought.** Keep the bones:

- The layering (transport → connection → driver → HTTP semantics → routing → app) with one-way deps.
- The build/buy line at `HttpDriver::serve()` — protocol mess stays inside one method.
- Concept-based connections / drivers, polymorphism only at the listener layer (no vtable on the byte path).
- `Outcome<Response, Errs...>` + ADL `to_http_response` + a catch-all only for true escapes.
- ALPN-multiplexed TLS; UDP listener for h3.
- Honest `setup()`/`start()`/`stop()` lifecycle, observers awaited *before* the work-guard releases.

Two structural problems sit on top of those good bones and dominate everything else:

1. **The spec and the plan have materially diverged**, and the plan **silently abandons the spec's headline promise** (zero framework heap allocs, "~1 alloc per request"). At least four divergences, the first being the expensive one:
   - **Body ownership:** spec §5.2 says *"held inline as a value type … SBO … No `unique_ptr<Body>` indirection."* Plan uses `std::unique_ptr<Body>` in both `Request` and `Response`.
   - **`target`:** spec §5.3 = `std::string_view` (view into the receive buffer). Plan Task 7 = owned `std::string`.
   - **`Headers::get_all`:** spec = `std::span<const std::string_view>`. Plan = `std::vector<std::string_view>` (allocates). (The span version is actually unimplementable over `BeastBacking`, so the plan had to diverge — but it diverged into a per-call allocation.)
   - **`read_multipart`:** spec = `(limit)`. Plan = `(limit, boundary)` — the caller must now supply the boundary, and nothing extracts it.

2. **The PR1 plan as written will not compile**, and **it was never executed** — its "Expected: N tests pass" lines are predictions. The Self-Review's reassurances inherit that untested optimism. (Details in §"Won't compile" below.)

Net: the architecture is worth building. But reconcile spec↔plan *before* writing more PRs, because the unresolved Body-ownership/arena decision is the difference between the performance design you wrote and the one you're about to build.

---

## Q4 — Performance (you asked first; it's your stated priority)

You optimized copy/heap-allocation. Good instinct — but the plan structurally undoes it, and there are several perf axes beyond allocation.

### A. The "~1 alloc per request" claim is contradicted by the plan (highest-value point)

Walk `ResponseFactory::json("{}")` through the plan's own code (Task 8/9):

```
Response r;                          // body = make_unique<EmptyBody>()      [alloc #1 — pure waste]
r.headers.add("Content-Type", ct);   // promote→pmr::vector buffer           [alloc #2]
                                     //   + pmr::string "application/json"   [alloc #3, >SSO]
r.body = make_unique<StringBody>(…); // frees EmptyBody, allocs StringBody   [alloc #4]
                                     //   + the body std::string             [user alloc]
```

That's **~4 framework heap allocations for the simplest possible response**, vs the spec's table (§11.1) claiming **1**. Two root causes:

- **`unique_ptr<Body>` indirection** — every `Request`/`Response` body is a separate heap node, and because `Response` default-initializes `body = make_unique<EmptyBody>()`, *every* response that sets a real body pays for a throwaway `EmptyBody` first.
- **Response headers have no arena path.** `Response` is built by handler/`ResponseFactory` code that has no handle to the request arena, so `Headers` falls back to `std::pmr::new_delete_resource()` — global heap. The spec's §11.1 line "Response headers (3 entries, arena) | 0" has no mechanism behind it.

**This is the decision the whole perf story hinges on.** Honest options:
- (a) Keep `unique_ptr<Body>` and global-heap response headers — then delete the zero-alloc claim and rewrite §11 to match reality. Fine for v1 correctness, but it's not the design you wrote.
- (b) Implement the SBO value-`Body` from the spec **and** thread the request arena into the `Response` construction path (factory + fluent setters take/inherit an allocator). This is the only path that hits the target. It's more work and it changes `ResponseFactory`'s signatures.

Pick one on purpose. Right now the plan picks (a) by default while the spec advertises (b).

### B. One shared `io_context` across N threads — you have measured this is wrong

Spec §9.1/§9.3: a single `asio::io_context ioc_` with `cfg_.threads` workers all calling `ioc_.run()`. That's the same shared-scheduler pattern the existing server uses.

Your own AsyncResourcePool profiling (recorded in project memory) found the bottleneck was *exactly* the shared `io_context` scheduler lock, and **per-thread `io_context` bought 2-3×**. This design re-adopts the slower pattern at the server layer.

Per your own rule — *benchmark both credible mechanisms, don't argue one on paper* — the action here is **not** "switch to per-thread." It's: before committing the listener/server model (PR4/5), benchmark **per-thread `io_context` + `SO_REUSEPORT` acceptors (one per thread)** vs **one shared `io_context` + thread pool**, on (i) a trivial echo and (ii) a realistic handler. Let the numbers on *this* workload decide. But design the Connection/Listener interfaces now so they don't *assume* a single shared context — retrofitting per-thread later is painful if the acceptor model is baked in.

### C. Header iteration over `BeastBacking` is O(n²)

`Headers::const_iterator::operator*()` (plan headers.cpp) does:

```cpp
auto it = b.fields->begin();
std::advance(it, idx_);   // O(idx_) — beast::http::fields is a linked list
```

Every dereference re-walks from `begin()`. A `for (auto [n,v] : headers)` over an incoming request is **O(headers²)** — 20-50 headers ⇒ hundreds-to-thousands of list hops per loop. Any logging/tracing middleware that dumps headers hits this. Fix: cache the list iterator in the `const_iterator` and advance it once per `++`. (Separately: `end()` calls `size()`, and `beast::http::fields::size()` complexity is worth *verifying* — don't assume O(1).)

### D. Other perf axes — propose measurements, not verdicts

- **Parametric routing is a linear scan** over *all* parametric templates (`for (auto& tmpl : parametric_)`), each a segment walk. Fine at a handful of routes; measure at ~200 `/{…}` routes and keep the trie-swap-in option (§15 already notes it) gated on that number.
- **`exact_.find(normalized)` with a `string_view` key:** `unordered_flat_map<std::string,…>::find` needs a *transparent* hash+equal (`is_transparent`) or it won't even accept a `string_view` (the `std::string(string_view)` ctor is `explicit`). Confirm transparent lookup is configured; if instead you convert to `std::string`, that's a **per-request allocation** for any path > SSO on the routing hot path.
- **`normalize_path` multi-slash collapse** must build a new buffer (a view can't represent `/a//b`→`/a/b`). Where does it live? If it's a fresh `std::string` per request, that's another hot-path alloc. Trailing-slash collapse can stay a view trim (no alloc); multi-slash can't. Spell out the buffer's home (arena).
- **`ConnectionTracker`** guards a `std::list<cancellation_signal>` with a `std::mutex`, locked on every accept and every close. At high connection churn that's a shared-lock hotspot — the same class of contention your profiling already flagged once.
- **Multipart double-copies the payload:** `drain_to_string` buffers the *entire* body into one `std::string`, then each part is copied again into `MultipartField.value`. A 10 MB upload = receive buffer + 10 MB drained string + 10 MB of part copies, all global heap. The spec wanted files to "expose payload via separate hook"; the plan dumps everything into `value` regardless of `filename`. For uploads this defeats the streaming model entirely.

### E. The perf-critical path isn't exercised in PR1

PR1 tests `Body` only through `StringBody` (owned `std::string`). The zero-copy `BeastRequestBody` (view over Beast's parsed buffer) — the thing the alloc budget depends on — doesn't exist until PR3. So the allocation target is **unverifiable until PR3+ integration/benchmarks**. Given your preference for testing against real systems, plan a benchmark at PR3 that actually counts allocations on the wire path (e.g. an allocator shim / `malloc` counter), rather than trusting the §11.1 table.

---

## Q2 — Problems users will hit (footguns)

- **`query<size_t>("page")` → linker error.** Task 13 puts `query<T>`/`path_param<T>` definitions in the `.cpp` with an explicit-instantiation list of exactly `{int, long, long long, double, std::string}`. `std::size_t` (usually `unsigned long`), `unsigned`, `float`, `std::uint32_t` → undefined reference. Pagination params are *the* common case and they don't link. Fix: move these templates to the header (you already do exactly this for the `set<T>`/`get<T>` bag, and the reasoning is identical), or document the closed set loudly.
- **Body is single-consume, silently.** `read_chunk` marks consumed; a second `read_json`/`read_to_string` (or a logging middleware that already drained it) returns empty → a confusing `JsonParseError`/empty map, not an "already consumed" signal. Frameworks users come from let you re-read `req.body`. Add a typed "body already consumed" error or a buffered-once cache.
- **`setup()` without `start()` → `std::terminate`.** `~Server` only stops if `state_ == started` (spec §9.1). After `setup()` (state `setup_done`) the work-guard is held and worker threads are running; destroying the `std::vector<std::thread>` of joinable threads calls `terminate`. Handle `setup_done` in the destructor. (PR5, but the contract is set now.)
- **`RequestContext` caches a `string_view` into a `std::string` it owns, and has a defaulted move.** The documented usage is "move-only, passed by value into handlers." The Router calls `ctx.path()` (populating `cached_path_`), then `std::move(ctx)` into the handler. For **SSO-length targets** (`"/"`, `"/users"`, `"/api/v1/x"` — i.e. most of them) the moved-from string's inline buffer is at a different address, so `cached_path_` now dangles; the handler's `ctx.path()` returns garbage/empty. Long targets (heap-backed string) survive the move — so this is an SSO-dependent heisenbug. **Same root cause as the spec↔plan `target` divergence:** the spec's view-into-receive-buffer wouldn't have this problem because the buffer is stable across context moves. Fix: restore `target`-as-view, or stop caching views (recompute, or store offsets).

---

## Q3 — Counterintuitive for end users

- **Built-in error rendering is *not* overridable.** §5.5 sells "easy to override per-domain." But the library *defines* `to_http_response(const NotFoundError&)` etc. A user who provides their own overload for the same built-in type gets an ODR violation / ambiguous call — not an override. To customize how a 404 looks you must define *your own* error type. The built-ins are effectively frozen. Either say so, or make rendering go through a customization point (a policy object / virtual) rather than a fixed free function.
- **Built-in errors render as `text/plain`, even for JSON APIs.** `to_http_response(NotFoundError{"user","42"})` → `Content-Type: text/plain`, body `"user 42 not found"`. A JSON client gets plaintext. For a framework whose ergonomic sweet spot is JSON APIs, the default error shape fights the use case.
- **Query-string silently drops malformed escapes; form bodies don't.** `?name=John%2` → `query("name")` returns *nullopt* (indistinguishable from absent), while `read_form` on the identical `application/x-www-form-urlencoded` grammar returns a typed `FormParseError`. Two behaviors for the same parse. Pick one.
- **`accepts_json()` returns true for browsers.** It matches `*/*`, and browsers send `Accept: text/html,…,*/*`. Content negotiation based on it serves JSON to a plain browser navigation. It also ignores `;q=0` (an explicit *refusal* reads as acceptance). Substring matching for `is_json()` similarly matches `application/json-patch+xml`.
- **`with_header()` always *adds*.** `Response{}.with_header("Content-Type","a").with_header("Content-Type","b")` emits **two** `Content-Type` headers. Users expect set-semantics from a fluent builder. Offer `with_header` (set) + a distinct `add_header` (append).
- **Two response-construction paths with inconsistent defaults.** `ResponseFactory::json(x)` sets `Content-Type`; `Response{}.with_body(x)` does **not**. It's easy to ship a response with no `Content-Type` via the fluent path and not notice.
- **`read_multipart` needs a boundary the user must hand-extract.** Plan signature is `(limit, boundary)`, but there's no `ctx.multipart_boundary()` — `is_multipart()` exists but you still parse the boundary out of the `Content-Type` header yourself. Add the extractor, or have `read_multipart(limit)` read the boundary from the header as the spec implied.

---

## Won't compile / never executed (calibration on the plan)

- **`Headers` has no default constructor.** Two user-declared (private) constructors suppress the implicit default; there is no `Headers() = default`. Yet `Request` (Task 7) and `Response` (Task 8) declare `Headers headers;` with no initializer and are default-constructed all over the tests and `ResponseFactory`. `Request req;` / `Response r;` are **ill-formed**. So Task 7 onward could not have built — the "Expected: N tests pass" lines are predictions, not runs. Treat the plan's Self-Review accordingly.
  - And the "obvious" fix is a trap: a `Headers() = default` would default the variant to `BeastBacking{fields=nullptr}`, so `Response r; r.headers.get(...)` / `.add(...)` dereferences a null `fields*`. You need a real **third empty-owned state** (or a default that means "empty owned with the default resource"), decided explicitly.
- **`promote_to_owned()` loses the arena.** `add/set/remove` call it with no allocator → `std::pmr::polymorphic_allocator<>{}` = global heap. Mutating an incoming (Beast-backed) header set copies everything to the global heap, not the request arena — both a correctness-of-intent gap and an allocation.

---

## Suggested resolution order (the few decisions that unblock the rest)

1. **Reconcile spec ↔ plan first.** Make them one document or explicitly mark plan-supersedes-spec deltas. Nothing else is trustworthy until they agree.
2. **Decide Body ownership + response-arena together** — this *is* the perf design (§A). Either commit to SBO-value-Body + arena-wired responses, or keep `unique_ptr` and rewrite §11's claims to match.
3. **Define `Headers`' empty state** (real third state or a defined default) so aggregates compile and `get`/`add` are null-safe.
4. **Restore `target` as a view** (or stop caching views into owned buffers) — fixes both an allocation and the SSO dangling bug.
5. **Move `query<T>`/`path_param<T>` to the header** (or document the closed type set).
6. **Benchmark, don't argue, the perf forks:** per-thread vs shared `io_context`; flat-scan vs trie routing; transparent-hash on `exact_`. Decide on numbers from this workload.

None of this blocks the architecture — it's worth building. These are the corrections that make the built thing match the designed thing.
