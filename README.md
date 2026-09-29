# KVStore

> A high-performance, persistent Key-Value Store implemented in modern C++.

KVStore is a persistent Key-Value storage engine written in C++, designed to explore the implementation principles of modern storage systems, including **LSM-Tree, WAL, SSTable, Bloom Filter, Block Cache, Compaction, crash recovery, and performance optimization**.

The project focuses on the trade-offs between **read latency, write throughput, durability, and implementation complexity**, with an emphasis on understanding how a storage engine works from the system level.

---

## ✨ Features

### Current

* [x] Persistent Key-Value storage
* [x] `Put / Get / Delete` basic operations
* [x] Write-Ahead Logging (WAL)
* [x] MemTable
* [x] Immutable MemTable
* [x] SSTable-based persistent storage
* [x] Tombstone-based deletion
* [x] Bloom Filter
* [x] SSTable Index
* [x] Data Block
* [x] Block Cache
* [x] Leveled Compaction
* [x] Crash recovery design
* [x] CRC32C-based data integrity checking
* [x] Little-endian binary encoding
* [x] C++17 implementation

### Planned

* [ ] Range / Prefix Scan
* [ ] Batch `Get`
* [ ] Batch `Put`
* [ ] TTL
* [ ] Multiple MemTable implementations
* [ ] More cache replacement policies
* [ ] Parallel compaction
* [ ] `io_uring` based I/O
* [ ] More comprehensive benchmarks
* [ ] Performance profiling and optimization
* [ ] Fault-injection testing

> Some features listed above are part of the architecture/roadmap and may not yet be fully implemented.

---

## 🎯 Design Goals

KVStore is primarily a learning-oriented storage engine, but its architecture is designed around real-world storage-system concerns.

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

Rather than pursuing maximum write throughput at all costs.

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
        ┌───────────┐         ┌───────────┐        ┌─────────────┐
        │    WAL    │         │ MemTable  │        │ Block Cache │
        └─────┬─────┘         └─────┬─────┘        └─────────────┘
              │                     │
              │                     │ Flush
              │                     ▼
              │              ┌─────────────┐
              │              │ Immutable   │
              │              │  MemTable   │
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

The top-level `KVStore` coordinates these components instead of directly owning all low-level storage logic.

---

# 💾 Storage Model

KVStore uses an LSM-Tree-style storage architecture.

A write follows approximately this path:

```text
Client
  │
  ▼
Put(key, value)
  │
  ├──────────────► WAL
  │                  │
  │                  ▼
  │               Durable
  │               Record
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

The WAL is written before the operation becomes part of the active MemTable so that an unflushed in-memory update can be recovered after a crash.

---

## Write Path

```text
Put(key, value)
      │
      ▼
Encode WAL Record
      │
      ▼
Append WAL
      │
      ▼
Sync WAL
      │
      ▼
Update MemTable
      │
      ▼
Return
```

The exact durability policy can be adjusted according to the desired latency/durability trade-off.

---

## Read Path

A read first checks the newest data structures because newer versions may shadow older versions.

Conceptually:

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
   │       └── definitely absent → next SSTable
   │
   ├── Index
   │
   └── Data Block
```

The Bloom Filter is used to determine whether a key **may exist** in an SSTable.

It does not locate the key.

The Index is responsible for locating the relevant Data Block.

```text
Bloom Filter
    │
    └── "Definitely Not Present" → skip SSTable

Index
    │
    └── locate Data Block

Data Block
    │
    └── locate key/value
```

---

# 📝 Write-Ahead Log

The WAL provides durability and crash recovery.

The current logical record format is:

```text
┌───────────────┐
│ Total Length  │ uint64
├───────────────┤
│ Sequence      │ uint64
├───────────────┤
│ Type          │ uint8
├───────────────┤
│ Key Length    │
├───────────────┤
│ Key           │
├───────────────┤
│ Value Length  │
├───────────────┤
│ Value         │
├───────────────┤
│ CRC32C        │
└───────────────┘
```

Supported operations:

```cpp
enum class WALRecordType : uint8_t {
    PUT,
    DELETE
};
```

The WAL encoding layer is separated from file I/O.

```text
WALManager
    │
    ├── File I/O
    │
    └── WALCodec
           │
           └── Encoder
```

This separation keeps binary encoding independent from the underlying file implementation.

---

# 🔄 Crash Recovery

On startup, KVStore replays the WAL to reconstruct in-memory state.

Conceptually:

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
 Truncate Corrupted Tail
```

The recovery process tracks the end position of the last valid record.

If a corrupted or incomplete record is encountered, only the invalid tail is discarded.

This prevents a partially-written record from corrupting the recovered state.

---

# 🗑️ Delete Semantics

Deletion is represented by a **tombstone** rather than immediately removing the key from every SSTable.

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

# 🧩 Core Components

The project is organized around several independent components.

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

The intention is to keep responsibilities separated so that individual components can be tested, benchmarked, and replaced independently.

---

# 🔧 API

The core API is intentionally small.

```cpp
class KVStore {
public:
    Status Put(const Key& key, const Value& value);

    Result<Value> Get(const Key& key);

    Status Delete(const Key& key);
};
```

The current API focuses on the fundamental operations required by a persistent KV store.

Additional APIs such as range queries and batch operations are planned for future versions.

---

# 📁 Project Structure

```text
KVStore/
├── CMakeLists.txt
├── README.md
├── .gitignore
│
├── include/
│   └── kvstore/
│       ├── common/
│       ├── core/
│       │   ├── wal/
│       │   ├── memtable/
│       │   ├── sstable/
│       │   ├── cache/
│       │   └── compaction/
│       └── kvstore.h
│
├── src/
│   ├── common/
│   ├── core/
│   │   ├── wal/
│   │   ├── memtable/
│   │   ├── sstable/
│   │   ├── cache/
│   │   └── compaction/
│   └── kvstore.cpp
│
├── tests/
│   ├── common/
│   ├── wal/
│   ├── memtable/
│   ├── sstable/
│   └── integration/
│
├── benchmarks/
│
└── docs/
    ├── architecture/
    └── design/
```

The exact directory layout may evolve as the implementation develops.

---

# 🛠️ Build

## Requirements

* C++20 or later
* CMake
* GCC / Clang
* Linux recommended

Build:

```bash
git clone git@github.com:Dreamkiller-mo/KVStore.git
cd KVStore

mkdir build
cd build

cmake ..
cmake --build . -j
```

Run tests:

```bash
ctest --output-on-failure
```

---

# 🧪 Testing

Testing focuses on both individual components and storage-system behavior.

### Unit Tests

* WAL encoding / decoding
* CRC32C verification
* MemTable operations
* Bloom Filter
* Data Block
* SSTable Index
* Cache
* Compaction

### Integration Tests

* Put → Get
* Put → Delete → Get
* WAL recovery
* Crash recovery
* MemTable flush
* SSTable loading
* Compaction correctness

### Future Fault Injection

Planned failure scenarios include:

```text
Crash during WAL write
Crash before WAL Sync
Crash during MemTable flush
Incomplete SSTable
Corrupted WAL record
Corrupted SSTable block
```

The purpose is to verify that persistence guarantees hold under abnormal termination.

---

# 📊 Performance

Performance is treated as a first-class design concern.

The primary optimization target is:

```text
Read Latency
      ↓
Cache Hit
      ↓
Bloom Filter
      ↓
Index
      ↓
Data Block
      ↓
Disk I/O
```

Planned benchmark dimensions include:

| Operation  | Metrics                          |
| ---------- | -------------------------------- |
| Put        | Throughput, P50/P95/P99 latency  |
| Get        | Throughput, P50/P95/P99 latency  |
| Delete     | Throughput, latency              |
| WAL        | Append throughput, fsync latency |
| SSTable    | Read latency                     |
| Cache      | Hit ratio                        |
| Compaction | Write amplification, throughput  |
| Recovery   | Recovery time                    |

Performance optimization will be driven by measurement rather than assumptions.

Tools planned for profiling include:

* `perf`
* flame graphs
* CPU profiling
* I/O analysis
* memory allocation analysis

---

# 🎯 Design Principles

### Separation of Concerns

Each component should have a clear responsibility.

For example:

```text
WALManager
    └── manages WAL lifecycle and I/O

WALCodec
    └── converts logical records to/from bytes

Encoder
    └── performs binary encoding

CRC32C
    └── validates data integrity
```

A component should not take responsibility for unrelated concerns.

### Prefer Explicit Data Flow

The storage engine should make important data paths visible:

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

This makes correctness and performance easier to reason about.

### Measure Before Optimizing

Performance changes should be supported by:

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

---

# 🗺️ Roadmap

## Phase 1 — Core Storage

* [x] Project structure
* [x] Basic KV API
* [x] WAL design
* [x] Binary Encoder
* [x] MemTable
* [ ] Complete WAL recovery
* [ ] SSTable implementation

## Phase 2 — Read Optimization

* [ ] Bloom Filter
* [ ] Index
* [ ] Data Block
* [ ] Block Cache
* [ ] Read-path benchmarks

## Phase 3 — Compaction

* [ ] Immutable MemTable flushing
* [ ] Leveled Compaction
* [ ] Version management
* [ ] Tombstone cleanup
* [ ] Compaction metrics

## Phase 4 — Performance

* [ ] Benchmark framework
* [ ] `perf` profiling
* [ ] CPU flame graphs
* [ ] I/O profiling
* [ ] Memory profiling
* [ ] Latency optimization

## Phase 5 — Advanced Features

* [ ] Range Scan
* [ ] Batch operations
* [ ] TTL
* [ ] Parallel compaction
* [ ] `io_uring`
* [ ] Fault injection

---

# 📚 Learning Objectives

This project is also a practical exploration of several systems topics:

* Modern C++
* C++ templates and concepts
* Memory management
* Linux I/O
* File systems
* Binary serialization
* WAL and crash recovery
* LSM-Tree
* SSTable
* Bloom Filter
* Cache design
* Compaction
* Concurrency
* Performance profiling
* CMake
* Git / GitHub
* Systems architecture

The project aims to connect these topics into one complete system rather than treating them as isolated exercises.