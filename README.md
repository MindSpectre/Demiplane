# **Demiplane**



# *Road map*

## General
#### TODO list
- Concurrent logger into one file 
- Crypto library
- Easy toolchain for newcomers
- Readme
- Docs linked
- Benchmarks
- Coverage/profiling with cmake presets
- Optional VCPKG support
- Multithread build
## Http component
### 1. Http1.1
#### **TODO list**
- Http1.1 Compile time full support
- Async Support with gears::Outcome
- Benchmarks
- Cover with tests
- Firewalls
- Config reload
- Http1.1 client
### 2. Http2
> Unplanned
### 3. Http3
> Unplanned
## Database component
### 1. Postgres
#### **TODO list**

- **Connection pooling enhancements:** idle timeout, max lifetime, connection validation on checkout, dynamic sizing
- **Prepared statement caching:** `PREPARE`/`EXECUTE` cache keyed by SQL text
- **Pipeline mode:** stub exists at `capabilities/pipeline/`, not implemented
- **`RETURNING` clause:** dialect declares `supports_returning() = true` but no `InsertExpr::returning()` API
- **Expression-based `SET` in `UPDATE`:** e.g. `.set(u_age, u_age + 1)` — currently takes
  `vector<pair<string, FieldValue>>`
- **Type-safe result-to-struct mapping:** `ResultBlock` returns raw `get<T>(row, col)` — natural extension is mapping to
  user structs
- **Date/time/UUID/JSON types:** noted in `db_field_value.hpp` TODOs
### 2. Redis
> Unplanned
### 3. MySql
> Unplanned
### 4. Sqlite
> Unplanned