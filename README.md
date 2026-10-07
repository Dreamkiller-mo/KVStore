# KVStore

> A persistent Key-Value Store implemented in modern C++23.

KVStore is a learning-oriented persistent Key-Value storage engine written in modern C++.

The project explores the implementation principles and engineering trade-offs behind storage systems, including:

- LSM-Tree
- Write-Ahead Logging (WAL)
- MemTable
- SSTable
- Bloom Filter
- Block Cache
- Compaction
- Crash Recovery
- Linux I/O
- Performance Profiling

Rather than focusing only on implementing individual data structures, the project emphasizes **system-level reasoning**: component responsibilities, data flow, failure handling, persistence guarantees, performance trade-offs, and architectural evolution.

---

## ✨ Current Status

The project is currently in the **WAL / core storage foundation stage**.

### Implemented

- [x] C++23 project setup
- [x] CMake-based build system
- [x] Basic `KVStore` API
- [x] `Put` request path
- [x] WAL Manager
- [x] WAL Record model
- [x] `std::variant`-based WAL operation representation
- [x] C++ Concepts for WAL operation constraints
- [x] Binary Encoder
- [x] Little-endian encoding
- [x] CRC32C integration
- [x] File abstraction with RAII
- [x] `Append` / `Sync` file operations
- [x] WAL Committer abstraction
- [x] WAL persistence through `Append + Sync`
- [x] WAL sequence number management
- [x] WAL failure-state handling
- [x] Error propagation with `Status`
- [x] `InvalidArgument` / `InvalidState` error semantics
- [x] Structured logging with `spdlog`
- [x] WAL failure-path testing
- [x] CMake dependency management with `FetchContent`

### Architecture Designed / In Progress

The following components are part of the planned storage architecture but are **not yet fully implemented**:

- [ ] MemTable
- [ ] Immutable MemTable
- [ ] SSTable
- [ ] Bloom Filter
- [ ] SSTable Index
- [ ] Data Block
- [ ] Block Cache
- [ ] Leveled Compaction
- [ ] WAL Recovery
- [ ] Crash Recovery
- [ ] Read Path

> The checklist intentionally distinguishes implemented functionality from architectural goals.

---

# 🎯 Design Goals

KVStore is primarily a learning-oriented project, but its architecture is designed around real storage-system concerns.

The main goals are:

1. **Low read latency**
2. **Reliable persistence**
3. **Crash recovery**
4. **Efficient disk layout**
5. **Scalable storage capacity**
6. **Understandable and extensible architecture**
7. **Measurable performance**

The current design prioritizes:

```text
Read Latency > Write Throughput
```

The goal is not to maximize a single benchmark number, but to understand **why** a storage engine behaves the way it does.

---

# 🏗️ Architecture

KVStore follows an LSM-Tree-inspired architecture.

```text
                         ┌──────────────────────┐
                         │       KVStore        │
                         │ Put / Get / Delete   │
                         └──────────┬───────────┘
                                    │
              ┌─────────────────────┼─────────────────────┐
              │                     │                     │
              ▼                     ▼                     ▼
        ┌───────────┐         ┌───────────┐       ┌─────────────┐
        │    WAL    │         │ MemTable  │       │ Block Cache │
        └─────┬─────┘         └─────┬─────┘       └─────────────┘
              │                     │
              │                     │ Flush
              │                     ▼
              │              ┌─────────────┐
              │              │  Immutable  │
              │              │   MemTable  │
              │              └──────┬──────┘
              │                     │
              │                     ▼
              │              ┌─────────────┐
              │              │   SSTable   │
              │              └──────┬──────┘
              │                     │
              │              ┌──────┴──────┐
              │              │             │
              ▼              ▼             ▼
        Crash Recovery   Bloom Filter   Index/Data
                                        Blocks
              │
              ▼
        ┌──────────────┐
        │  Compaction  │
        └──────┬───────┘
               │
               ▼
          New SSTables
```

The top-level `KVStore` coordinates the storage components rather than implementing low-level storage mechanisms itself.

The project also uses a **Composition Root** to assemble runtime dependencies during startup.

---

# 💾 Storage Model

KVStore uses an LSM-Tree-style storage model.

A write follows approximately this path:

```text
Client
  │
  ▼
Put(key, value)
  │
  ▼
WAL
  │
  ├── Encode
  │
  ├── Append
  │
  └── Sync
  │
  ▼
MemTable
  │
  │ memory pressure
  ▼
Immutable MemTable
  │
  │ flush
  ▼
SSTable
```

The WAL is written before the operation is considered committed to the in-memory storage layer.

This provides a recovery source for updates that have not yet been flushed into SSTables.

---

# 📝 Write-Ahead Log

The WAL is currently the main implemented storage component.

Its responsibility is to provide a durable ordered record of write operations.

## WAL Record Format

The current logical record format is:

```text
┌───────────────┐
│ Total Length  │ uint64
├───────────────┤
│ Sequence      │ uint64
├───────────────┤
│ Type          │ uint8
├───────────────┤
│ Key Length    │ uint64
├───────────────┤
│ Key           │ bytes
├───────────────┤
│ Value Length  │ uint64
├───────────────┤
│ Value         │ bytes
├───────────────┤
│ CRC32C        │ uint32
└───────────────┘
```

Supported operation types:

```cpp
enum class WALRecordType : uint8_t {
    PUT,
    DELETE
};
```

The current WAL pipeline is:

```text
WALManager
    │
    ▼
WALRecord
    │
    ▼
WALCodec
    │
    ▼
Encoder
    │
    ▼
FileCommitter
    │
    ▼
File
    │
    ├── Append
    │
    └── Sync
```

---

# 🧩 WAL Component Responsibilities

The WAL implementation intentionally separates responsibilities.

```text
WALManager
    └── coordinates WAL lifecycle and state

WALRecord
    └── represents one logical WAL record

Operation
    └── represents PUT / DELETE variants

WALCodec
    └── converts logical records into binary representation

Encoder
    └── performs low-level binary encoding

CRC32C
    └── calculates data integrity checksum

FileCommitter
    └── coordinates WAL persistence

File
    └── provides file I/O operations
```

The important design principle is:

> **File provides file operations; FileCommitter defines the persistence procedure.**

This keeps binary encoding, persistence policy, and low-level file operations independent.

---

# 🔄 WAL Write Path

The current WAL write path is:

```text
KVStore::Put()
      │
      ▼
Validate Arguments
      │
      ▼
WALManager::AppendPut()
      │
      ▼
Create WALRecord
      │
      ▼
WALCodec::Encode()
      │
      ▼
FileCommitter::Commit()
      │
      ├── File::Append()
      │
      └── File::Sync()
      │
      ▼
Update WAL Sequence
      │
      ▼
Return Status
```

The sequence number follows an important invariant:

```text
sequence_
    =
last successfully committed WAL sequence
```

The next sequence number is only committed to the manager after the persistence operation succeeds.

---

# ⚠️ Failure Handling

WAL persistence failures are treated as a serious state transition.

For example:

```text
WAL Commit
    │
    ├── Append
    │
    └── Sync
         │
         └── failure
              │
              ▼
        Status::IOError
              │
              ▼
        WALManager
        failed_ = true
              │
              ▼
        Future writes
              │
              ▼
        InvalidState
```

Once the WAL manager cannot reliably determine the persistence state of a write, it refuses subsequent writes.

This prevents the storage engine from continuing as if the WAL were healthy when its durability guarantee may no longer hold.

The failure path has been tested through fault injection by intentionally closing the WAL file before `Sync()`.

Observed behavior:

```text
WAL Commit
    ↓
Sync failed
    ↓
Put failed
    ↓
WALManager enters failed state
    ↓
Subsequent Put operations return InvalidState
```

---

# 📊 Logging

The project currently uses `spdlog` for structured runtime logging.

Example:

```text
[info] WAL AppendPut key_size=11 value_size=13
[info] WAL Encode sequence=1 bytes=61
[info] WAL Commit bytes=61
[info] WAL Commit success, bytes=61
```

Failure example:

```text
[error] WAL Commit Sync failed, status=File::Sync fd < 0
```

Logs focus on system state and metadata rather than raw key/value contents.

For example, the WAL logs:

```text
key_size=11
value_size=13
```

rather than printing the actual value.

This reduces unnecessary data exposure and keeps logs useful for diagnosing system behavior.

---

# 🔄 Crash Recovery

Crash recovery is part of the planned WAL implementation.

The intended recovery process is:

```text
WAL
 │
 ▼
Read Record
 │
 ▼
Validate Length
 │
 ▼
Validate CRC32C
 │
 ├── valid ──────────► Replay
 │
 └── invalid
        │
        ▼
   Stop Recovery
        │
        ▼
 Truncate Invalid Tail
```

The recovery process will track the end position of the last valid record.

If an incomplete or corrupted record is encountered, only the invalid tail should be discarded.

This allows previously committed records to remain recoverable.

---

# 🗑️ Delete Semantics

Deletion will use a tombstone rather than immediately removing the key from every SSTable.

Conceptually:

```text
Put("A", "value1")
        │
        ▼
     SSTable

Delete("A")
        │
        ▼
    Tombstone
```

During reads, the newest version takes precedence.

During compaction, obsolete versions and tombstones can eventually be removed when it is safe to do so.

---

# 🧱 Planned Read Path

The intended read path is:

```text
Get(key)
   │
   ▼
MemTable
   │ miss
   ▼
Immutable MemTable
   │ miss
   ▼
Block Cache
   │ miss
   ▼
SSTable
   │
   ├── Bloom Filter
   │       │
   │       └── definitely absent → skip SSTable
   │
   ├── Index
   │       │
   │       └── locate Data Block
   │
   └── Data Block
           │
           ▼
       locate key/value
```

The Bloom Filter answers:

> **Could this key exist in this SSTable?**

It does not locate the key.

The Index answers:

> **Which Data Block should we search?**

The Data Block performs the final key lookup.

---

# 🧩 Core Components

The planned storage engine consists of:

```text
KVStore
│
├── WAL
│   ├── WALManager
│   ├── WALCodec
│   ├── WALRecord
│   └── Encoder
│
├── MemTable
│   ├── IMemTable
│   └── SkipListMemTable
│
├── SSTable
│   ├── DataBlock
│   ├── Index
│   ├── BloomFilter
│   └── Footer
│
├── Cache
│   ├── BlockCache
│   └── LRUCache
│
└── Compaction
    └── LeveledCompaction
```

The architecture is intentionally component-oriented so that individual components can be:

- tested independently
- benchmarked independently
- replaced independently
- reasoned about independently

---

# 🔧 API

The current public API is intentionally small.

```cpp
class KVStore {
public:
    Status Put(std::string_view key,
               std::string_view value);

    // Planned
    // Result<Value> Get(std::string_view key);
    // Status Delete(std::string_view key);
};
```

The current implementation focuses on establishing a reliable write path first.

Future versions will add:

- `Get`
- `Delete`
- Range / Prefix Scan
- Batch operations
- TTL

---

# 📁 Project Structure

```text
KVStore/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
├── .gitignore
├── .clangd
│
├── config/
│
├── docs/
│   ├── architecture/
│   ├── decisions/
│   ├── design/
│   └── requirements/
│
├── include/
│   └── kvstore/
│       ├── api/
│       ├── common/
│       ├── composition/
│       ├── core/
│       │   ├── compaction/
│       │   ├── memtable/
│       │   ├── sstable/
│       │   └── wal/
│       └── storage/
│
├── src/
│   └── kvstore/
│       ├── api/
│       ├── composition/
│       ├── core/
│       │   ├── compaction/
│       │   ├── memtable/
│       │   ├── sstable/
│       │   └── wal/
│       └── storage/
│
├── examples/
│   ├── CMakeLists.txt
│   ├── basic_usage.cc
│   └── demo.cc
│
├── tests/
│
├── benchmarks/
│
└── thirdparty/
```

The directory layout may evolve as the implementation develops.

---

# 🛠️ Build

## Requirements

- C++23
- CMake 3.28+
- GCC / Clang
- Linux recommended

Clone the repository:

```bash
git clone git@github.com:Dreamkiller-mo/KVStore.git
cd KVStore
```

Configure and build:

```bash
cmake --preset debug
cmake --build --preset debug -j
```

Run the example:

```bash
./build/debug/examples/basic_usage
```

Run tests:

```bash
ctest --test-dir build/debug --output-on-failure
```

---

# 🧪 Testing

Testing will cover both individual components and system-level behavior.

## Unit Tests

Planned coverage includes:

- WAL encoding / decoding
- CRC32C verification
- File operations
- MemTable operations
- Bloom Filter
- Data Block
- SSTable Index
- Block Cache
- Compaction

## Integration Tests

Planned scenarios include:

```text
Put → Get
Put → Delete → Get
Put → Restart → Get
WAL Recovery
MemTable Flush
SSTable Loading
Compaction Correctness
```

## Fault Injection

Failure scenarios include:

```text
Crash during WAL write
Crash before WAL Sync
Sync failure
MemTable flush failure
Incomplete SSTable
Corrupted WAL record
Corrupted SSTable block
```

The purpose is to verify that persistence guarantees hold under abnormal termination.

---

# 📊 Performance

Performance is treated as a first-class design concern.

The primary read optimization path is:

```text
Get
 │
 ▼
MemTable
 │
 ▼
Block Cache
 │
 ▼
Bloom Filter
 │
 ▼
Index
 │
 ▼
Data Block
 │
 ▼
Disk I/O
```

Planned benchmark dimensions:

| Operation | Metrics |
|---|---|
| Put | Throughput, P50/P95/P99 latency |
| Get | Throughput, P50/P95/P99 latency |
| Delete | Throughput, latency |
| WAL | Append throughput, Sync latency |
| SSTable | Read latency |
| Cache | Hit ratio |
| Compaction | Write amplification, throughput |
| Recovery | Recovery time |

Performance optimization will follow:

```text
Benchmark
    ↓
Profile
    ↓
Identify Bottleneck
    ↓
Optimize
    ↓
Benchmark Again
```

The project intends to use:

- `perf`
- flame graphs
- CPU profiling
- I/O analysis
- memory allocation analysis

Optimization decisions should be supported by measurements rather than assumptions.

---

# 🎯 Design Principles

## Separation of Concerns

Each component should have a clear responsibility.

For example:

```text
WALManager
    └── coordinates WAL lifecycle and state

WALCodec
    └── handles logical ↔ binary conversion

Encoder
    └── performs low-level byte encoding

FileCommitter
    └── coordinates persistence

File
    └── provides file operations
```

A component should not take responsibility for unrelated concerns.

---

## Explicit Data Flow

Important system paths should remain visible:

```text
API
 ↓
WAL
 ↓
MemTable
 ↓
SSTable
 ↓
Compaction
```

Explicit data flow makes correctness, failure handling, and performance easier to reason about.

---

## Failure Is a First-Class Design Concern

The system should explicitly model important failure states rather than treating every failure as a generic error.

For example:

```text
InvalidArgument
    └── caller input violates API contract

InvalidState
    └── component state does not permit the operation

IOError
    └── underlying I/O operation failed

Corruption
    └── persistent data violates integrity expectations
```

---

## Measure Before Optimizing

Performance improvements should follow evidence:

```text
Benchmark
    ↓
Profile
    ↓
Find Bottleneck
    ↓
Change Design / Implementation
    ↓
Benchmark Again
```

---

# 🗺️ Roadmap

## Phase 1 — WAL Foundation

- [x] Project structure
- [x] C++23 build system
- [x] Basic KV API
- [x] WAL design
- [x] WAL Record
- [x] Binary Encoder
- [x] CRC32C
- [x] File abstraction
- [x] WAL Committer
- [x] WAL error handling
- [x] Structured logging
- [x] Failure-path validation
- [ ] WAL decoding
- [ ] WAL recovery
- [ ] Recovery tests

## Phase 2 — MemTable

- [ ] `IMemTable`
- [ ] Skip List MemTable
- [ ] Put / Get / Delete
- [ ] Immutable MemTable
- [ ] Memory-pressure management
- [ ] MemTable benchmarks

## Phase 3 — SSTable

- [ ] SSTable format
- [ ] Data Block
- [ ] Index
- [ ] Bloom Filter
- [ ] SSTable Builder
- [ ] SSTable Reader
- [ ] SSTable loading

## Phase 4 — Read Path

- [ ] Complete `Get`
- [ ] Block Cache
- [ ] LRU Cache
- [ ] Read-path benchmarks
- [ ] Read latency analysis

## Phase 5 — Compaction

- [ ] Leveled Compaction
- [ ] Version management
- [ ] Tombstone cleanup
- [ ] Compaction metrics
- [ ] Compaction correctness tests

## Phase 6 — Performance

- [ ] Benchmark framework
- [ ] `perf` profiling
- [ ] CPU flame graphs
- [ ] I/O profiling
- [ ] Memory profiling
- [ ] Latency optimization

## Phase 7 — Advanced Features

- [ ] Range / Prefix Scan
- [ ] Batch `Get`
- [ ] Batch `Put`
- [ ] TTL
- [ ] Parallel Compaction
- [ ] `io_uring`
- [ ] Fault-injection framework

---

# 📚 Learning Objectives

This project connects several systems topics into one complete implementation:

- Modern C++
- C++ templates and concepts
- `std::variant`
- RAII and ownership
- Memory management
- Linux I/O
- File systems
- Binary serialization
- WAL
- Crash recovery
- LSM-Tree
- SSTable
- Bloom Filter
- Cache design
- Compaction
- Concurrency
- Performance profiling
- CMake
- Git / GitHub
- Software architecture

The goal is not to study these topics as isolated knowledge points.

Instead:

```text
Computer Systems
       ↓
Modern C++
       ↓
Storage Architecture
       ↓
KVStore Implementation
       ↓
Benchmark
       ↓
Profile
       ↓
Optimization
```

The project is intended to turn theoretical knowledge into a complete, measurable systems implementation.