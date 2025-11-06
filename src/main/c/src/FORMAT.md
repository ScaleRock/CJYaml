# YAML Blob Format and Implementation Plan

This document describes a complete, fixed-width, little-endian binary format (the **YAML Blob**) and an implementation plan to parse YAML 1.2.2 into that format using a native C parser with a Java wrapper (JNI). It explains **every field, structure and responsibility**, the runtime flow, serialization steps, tag resolution, merge handling, alias handling, hash-index section, JNI surface, Java helper API, memory lifecycle and testing/debugging tools.

---

## Table of contents

1. Goals and assumptions
2. High-level architecture
3. Blob layout (on-disk / in-memory contiguous blob)
4. Blob header: fields explained
5. NodeEntry: fields and their meanings
6. PairEntry and Index table
7. Hash index section (fast key lookup)
8. String table and deduplication
9. In-memory build-time structures (C)
10. Event-driven parsing and IR construction flow
11. Merge-key resolution strategy
12. Tag resolution strategy
13. Aliases and anchors handling
14. Serialization: building the final blob (step-by-step)
15. Checksum and validation
16. mmap support and memory model
17. JNI API surface and Java wrapper design
18. Java reader API and common helpers
19. Examples and walkthroughs (from YAML to blob and lookup)
20. CLI and debug tools
21. Testing, fuzzing, and memory-safety
22. Performance considerations and optimizations
23. File & symbol naming conventions
24. Implementation checklist

---

## 1. Goals and assumptions

* Full YAML 1.2.2 semantics are targeted (anchors/aliases, merge keys, tags, multi-documents), but for this first iteration we will only support **single-document** blobs (multi-doc can be added later).
* API: **DirectByteBuffer** main surface to Java (zero-copy read access). A `parseToByteArray` convenience function will be provided that copies the blob into a Java `byte[]`.
* Binary encoding: **fixed-width** fields (no varints), **little-endian**.
* Node record layout: `uint8 + uint8 + uint16 + uint32 + uint64 + uint64` (packed), 24 bytes per node entry.
* Scalars are **always stored as strings** in the string table (deduplicated at build time).
* Aliases are stored as `NODE_ALIAS` nodes with `a = target_node_index`.
* Merge keys (`<<`) are resolved during parsing: the target mappings are copied into the destination mapping under parsing control according to YAML rules (destination keys win).
* No compression by default. Optionally add a compression wrapper later.
* A hash-index section is included for faster key lookup: sorted array of `(key_hash:uint64, pair_index:uint32)` entries.
* Provide `mmap` support programmatically to map saved blobs.

---

## 2. High-level architecture

* **Native layer (C)**: parse YAML with an event-driven parser, build an in-memory IR (nodes, pairs, indices, string pool map), perform merges and tag resolution, serialize IR into one contiguous blob and return pointer+size.
* **JNI bridge**: expose functions to Java: `parseToDirectByteBuffer`, `parseToByteArray`, `freeBlob`, `mapFile`, `unmapFile`. JNI will call the native parse and return a `NewDirectByteBuffer` pointing to the allocated blob.
* **Java layer**: `YamlDocument` (AutoCloseable) wraps the `ByteBuffer` and implements `getString`, `getInt`, `getSequence`, `getMappingIterator`, and a `YamlReader` low-level helper that reads `NodeEntry`, `PairEntry`, `IndexEntry` and `HashEntry` from the ByteBuffer.

---

## 3. Blob layout (on-disk / in-memory contiguous blob)

The blob is a single continuous buffer laid out as follows:

```
[ HEADER ]
[ NODE_TABLE ]   // node_count * sizeof(NodeEntry)
[ PAIR_TABLE ]   // pair_count * sizeof(PairEntry)
[ INDEX_TABLE ]  // index_count * sizeof(uint32_t)
[ HASH_INDEX ]   // hash_index_count * sizeof(HashEntry)  (optional but included here)
[ STRING_TABLE ] // concatenated UTF-8 strings (deduplicated)
```

All offsets in the header are absolute offsets from the beginning of the blob.

---

## 4. Blob header: fields explained

`BlobHeader` (all numeric types are fixed width and little-endian):

| Field               |     Type | Size | Description                                                                |
| ------------------- | -------: | ---: | -------------------------------------------------------------------------- |
| magic               | uint32_t |   4B | File magic: e.g. `0x59414D4C` (ASCII "YAML")                               |
| version_major       | uint16_t |   2B | Format major version                                                       |
| flags               | uint32_t |   4B | Bitflags: bit0=endian(0=little), bit1=compression(0=none), others reserved |
| node_table_offset   | uint64_t |   8B | Offset to node table                                                       |
| node_count          | uint64_t |   8B | Number of nodes                                                            |
| pair_table_offset   | uint64_t |   8B | Offset to pair table                                                       |
| pair_count          | uint64_t |   8B | Number of pairs                                                            |
| index_table_offset  | uint64_t |   8B | Offset to index table                                                      |
| index_count         | uint64_t |   8B | Number of index entries                                                    |
| hash_index_offset   | uint64_t |   8B | Offset to hash-index array (0 if not present)                              |
| hash_index_size     | uint64_t |   8B | Number of hash-index entries                                               |
| string_table_offset | uint64_t |   8B | Offset to string table                                                     |
| string_table_size   | uint64_t |   8B | Size of string table in bytes                                              |


Total header size (fixed) = 4 + 2 + 4 + (8*10) = 90 bytes (verify; exact total depends on your layout details). 

**Responsibility:** The header consistently describes where each section starts and how many items live there. On read, the Java-side `YamlReader` will read the header first and then use the offsets/counts to index into the blob.

---

## 5. NodeEntry: fields and their meanings

`NodeEntry` is the canonical node record. It is packed and fixed-size. Layout:

```c
#pragma pack(push,1)
typedef struct {
  uint8_t node_type;     // enum: SCALAR=0, SEQUENCE=1, MAPPING=2, ALIAS=3, DOCUMENT=4
  uint8_t style_flags;   // bitmask for scalar subtype, folded/literal, etc.
  uint16_t tag_index;    // index into string_table (0 == no tag)
  uint64_t a;            // semantics depend on node_type
  uint64_t b;            // semantics depend on node_type
} NodeEntry;
#pragma pack(pop)
```

**Interpretation of a/b:**

* SCALAR: `a = offset_into_string_table`, `b = length_in_bytes`.
* SEQUENCE: `a = first_index_into_index_table` (uint32 indexes packed into index table; stored as uint64 to keep uniformity), `b = element_count`.
* MAPPING: `a = first_pair_index_in_pair_table`, `b = pair_count`.
* ALIAS: `a = target_node_index` (the node index that the alias points to), `b = 0`.
* DOCUMENT: `a = root_node_index` (or root node reference), `b = 0`.

**style_flags bits** (suggested):

* Bit 0..1: SCALAR subtype (00=string, 01=int, 10=float, 11=bool)
* Bit 2: folded literal indicator (text format)
* Bits 3..7: reserved for future use

**tag_index:** if non-zero this is an index into the string table containing the explicit YAML tag (e.g. "!!str", "!mytag"). `0` means "no tag".

**anchor_id:** optionally store a numeric id assigned to anchors encountered during parsing. Anchor mapping (name->node_index) is kept in the build-phase; anchor_id can be useful for debug output or for diagnostic tools.

**Responsibility:** `NodeEntry` provides all necessary info to locate data for any node without requiring other pointers. While building the blob, all internal pointers are converted to offsets/indices.

---

## 6. PairEntry and Index table

**PairEntry** (for mappings):

```c
#pragma pack(push,1)
typedef struct {
  uint32_t key_node_index;   // index into node_table
  uint32_t value_node_index; // index into node_table
} PairEntry;
#pragma pack(pop)
```

**Index table** (for sequences) is a flat `uint32_t[]` array where each entry is a `node_index` pointing to an element node.

**Responsibility:** PairEntry stores the mapping between key node (typically a scalar) and value node (any node), enabling mapping traversal. Index table stores sequence elements by node index.

---

## 7. Hash index section (fast key lookup)

To speed up key lookups in mappings, the blob includes a hash-index section. Implementation choice: **sorted array of `(key_hash:uint64, pair_index:uint32)`**.

**Layout for each HashEntry (binary-search friendly):**

```
struct HashEntry { uint64_t key_hash; uint32_t pair_index; uint32_t reserved; }
```

(Reserved aligns the struct to 16 bytes; you may choose a tighter layout.)

**Build-time steps to populate hash-index:**

* For every PairEntry, read the key node (must be a scalar), read its string from the string table, compute a fast hash (e.g. xxHash64 or a simple 64-bit FNV/XXH64).
* Insert `(hash, pair_index)` into vector.
* After all pairs are processed, sort this vector by `key_hash`.
* Write the sorted vector into the blob and set `hash_index_offset` & `hash_index_size` in header.

**Read-time lookup (Java):**

* Compute key_hash for the requested key string.
* Binary search the HashEntry array for hash hits.
* For each candidate pair_index with equal hash (collisions possible), load the pair's key_node_index, read key string from string_table and compare exact bytes. If match, return pair_index.

**Responsibility:** Dramatically reduces average-case lookup time for mappings (instead of linear scan of pair_count), especially beneficial for large maps.

---

## 8. String table and deduplication

**String table** is a single concatenated UTF-8 buffer containing all distinct strings (keys, scalar values, tags) used by the document. During build, a `string_pool_map` (`string -> offset`) must be maintained to avoid storing duplicates. The string table is appended last so that node entries can store offsets into it.

**Deduplication strategy** (build-time):

* Use a hash map keyed by the string bytes (or by hashed value with equality fallback).
* When encountering a string (scalar text, tag, key name), consult the map: if found, reuse the offset; if not found, append to `string_builder` and store the new offset.
* Keep track of each node's `string_offset` and `string_length` via the `a` and `b` fields in the node entry.

**Responsibility:** Minimizes duplicate strings (common keys like "host"), reduces blob size and speeds up comparisons when you compare by offset+length.

---

## 9. In-memory build-time structures (C)

During parsing you should maintain transient structures before serialization. Suggested names (C) and responsibilities:

* `BuildContext` (struct) — root context containing all temporary vectors and maps.
* `vector<NodeTemp> node_list` — temporary node objects (contain type, pointers to strings or child lists, flags, tag pointer, anchor name or id).
* `vector<PairTemp> pair_list` — temporary pairs (key_node_temp_index, value_node_temp_index).
* `vector<uint32_t> index_list` — accumulated sequence element node indices (concatenated for all sequences; nodes will store `first_index`/`count`).
* `StringPoolMap` (hash map) — maps string bytes -> string_offset (or to a temporary `string_id`) and deduplicates.
* `AnchorMap` — maps anchor_name -> node_index (for anchors) and supports forward references tracking.
* `vector<MergeOp>` — optional postpone list of merges if the target mapping is not yet realized.

**Responsibility:** These structures allow you to collect the full IR in memory, perform merges and tag resolution, then compute final offsets and write the contiguous blob in a single pass.

---

## 10. Event-driven parsing and IR construction flow

This section explains the step-by-step workflow to parse YAML and produce the in-memory IR ready for serialization.

### 10.1 Initialize

* Create `BuildContext` with empty vectors and maps.
* Initialize YAML event parser (libyaml recommended).
* Setup a stack `node_stack` to keep the current parent node index and context type (mapping or sequence).

### 10.2 Process parser events

* **DOCUMENT-START**: allocate a DOCUMENT node (optional); treat the top-level mapping or sequence as the root and remember its node index.
* **MAPPING-START**: create a new `node_temp` with type=MAPPING, push node index on `node_stack`.
* **MAPPING-END**: finalize the mapping node (it may have pair entries accumulated). Pop `node_stack`.
* **SEQUENCE-START**: create new node_temp type=SEQUENCE; push node index; create an empty list area in the `index_list` and store a placeholder for `first_index`.
* **SEQUENCE-END**: compute `count` for the sequence by subtracting current `index_list` size from sequence start index.
* **SCALAR**: create a SCALAR node_temp with a reference to the string bytes (copy or intern), compute a tag if present and run tag-resolution heuristics (mark style_flags). For dedup: register the string in `StringPoolMap` and save a string_id or offset placeholder in the node_temp.
* **ANCHOR**: when an anchor is attached to a node, after creating the node assign `AnchorMap[name] = node_index`.
* **ALIAS**: create a node_temp with type=ALIAS; if the anchor name exists in `AnchorMap` set `a = anchor_node_index`, otherwise store forward reference to be resolved later.

### 10.3 Pair creation in mappings

When encountering key then value inside a mapping:

* Key is a SCALAR node (ensure it is created).
* Value can be scalar/sequence/mapping/alias.
* Create a `PairTemp{key_node_index,value_node_index}` and push to `pair_list`.

### 10.4 Merge processing

* If key equals `<<` (merge key): schedule or execute merging:

  * If the merge value is an alias to mapping, dereference target mapping's pairs and copy pairs into current mapping (excluding keys that already exist in dest mapping).
  * If the merge value is a sequence of mappings, iterate and merge in order.
* If a target mapping is not yet fully built, postpone merge operations to a second pass.

### 10.5 Tag resolution and scalar typing

* On SCALAR creation, if an explicit tag exists, set `tag_index` (intern tag string) and set `style_flags` according to the tag.
* If tag is absent, run implicit type detection rules in the order you choose (recommended: `null`, `bool`, `int`, `float`, `string`) and set `style_flags` accordingly. Keep the original scalar text in string table.

### 10.6 Final pass: resolve forward aliases and postponed merges

* Walk `forward_aliases` and fill `a` fields by looking up anchors in `AnchorMap`. If an anchor is missing, raise a parse error.
* For postponed merges, perform the copy now from completed target mappings.
* Now you have complete `node_list`, `pair_list`, `index_list`, `string_pool` mapping offsets (or temporary ids), `hash_entries` list if you prebuilt it.

**Responsibility:** After this flow you should have a full in-memory IR that can be serialized deterministically into a contiguous blob.

---

## 11. Merge-key resolution strategy (detailed)

* When you see a `<<` key in a mapping, the YAML semantics say to merge mappings from the value(s) into the current mapping. Keys in the destination mapping override keys from the merged mappings.

**Algorithm:**

1. Determine merge sources: either a single mapping (via alias) or a sequence of mappings.
2. For each source mapping in order: iterate its pairs. For each pair:

   * Check if destination mapping already contains the key (use a temporary `dest_keys` set for the mapping build). If it does, skip copying this pair (destination wi
