#include "CJYaml.h"

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#else
    #include <windows.h>
    #define _CRT_SECURE_NO_WARNINGS
#endif

/* xxhash prototype - ensure xxhash library/header is available in build */
extern uint64_t XXH64(const void* input, size_t length, uint64_t seed);

/* -------------------------
    Parser
   -------------------------*/
static void write_u32_le(uint8_t *buf, const size_t pos, const uint32_t v) {
    buf[pos+0] = (uint8_t)(v);
    buf[pos+1] = (uint8_t)(v >> 8);
    buf[pos+2] = (uint8_t)(v >> 16);
    buf[pos+3] = (uint8_t)(v >> 24);
}
static void write_u16_le(uint8_t *buf, const size_t pos, const uint16_t v) {
    buf[pos+0] = (uint8_t)(v);
    buf[pos+1] = (uint8_t)(v >> 8);
}
static void write_u64_le(uint8_t *buf, const size_t pos, const uint64_t v) {
    for (int i=0;i<8;i++) buf[pos+i] = (uint8_t)(v >> (8*i));
}
static uint64_t fnv1a64(const void *data, const uint64_t len) {
    const uint8_t *p = data;
    uint64_t h = 14695981039346656037ULL;
    for (uint64_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static void *safe_malloc(const size_t n) {
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "FATAL: malloc(%zu) failed\n", n);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    return p;
}
static void *safe_realloc(void *ptr, const size_t n) {
    void *p = realloc(ptr, n);
    if (!p) {
        fprintf(stderr, "FATAL: realloc(%zu) failed\n", n);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    return p;
}

/* Poprawione wektory z sprawdzaniem alokacji */
static void nodes_init(NodeVec *v) {
    v->data = NULL;
    v->count = 0;
    v->cap = 0;
}

// Helper to grow a dynamic array if needed
// - data_ptr: pointer to the array pointer (e.g., &vec->data)
// - count: current number of elements
// - cap_ptr: pointer to the current capacity
// - elem_size: size of one element in bytes
static void grow_array_if_needed(void **data_ptr, const size_t count, size_t *cap_ptr, const size_t elem_size) {
    if (count == *cap_ptr) {
        size_t new_capacity;
        if (*cap_ptr < 1024) {
            new_capacity = *cap_ptr ? (*cap_ptr * 2) : 16;  // double for small arrays
        } else if (*cap_ptr < 10000) {
            new_capacity = *cap_ptr + (*cap_ptr / 2);       // ~1.5x growth
        } else {
            new_capacity = *cap_ptr + (*cap_ptr / 5);       // ~1.2x growth
        }

        *data_ptr = safe_realloc(*data_ptr, new_capacity * elem_size);
        *cap_ptr = new_capacity;
    }
}


static void nodes_push(NodeVec *vec, const NodeEntry node) {
    grow_array_if_needed((void**)&vec->data, vec->count, &vec->cap, sizeof(NodeEntry));
    vec->data[vec->count++] = node;
}

static void pairs_push(PairVec *vec, PairEntry pair) {
    grow_array_if_needed((void**)&vec->data, vec->count, &vec->cap, sizeof(PairEntry));
    vec->data[vec->count++] = pair;
}

static void index_push(IndexVec *vec, uint32_t value) {
    grow_array_if_needed((void**)&vec->data, vec->count, &vec->cap, sizeof(uint32_t));
    vec->data[vec->count++] = value;
}



static void pairs_init(PairVec *v) {
    v->data = NULL;
    v->count = 0;
    v->cap = 0;
}


static void index_init(IndexVec *v) {
    v->data = NULL;
    v->count = 0;
    v->cap = 0;
}




static void strings_init(StringVec *v) { v->data = NULL; v->lens = NULL; v->count = 0; v->cap = 0; }



static void strings_push(StringVec *vec, const char *str, const size_t len) {
    // Appends a new string to a StringVec, expanding the capacity if needed.
    // The capacity grows differently depending on the current size:
    //   - double (x2) until 1024 elements
    //   - 1.5x (approximate using integer math) until 10,000 elements
    //   - 1.2x (approximate using integer math) for larger sizes
    // - vec: pointer to the StringVec to append to
    // - str: pointer to the string to add
    // - len: length of the string

    if (vec->count == vec->cap) {
        size_t new_capacity;

        if (vec->cap < 1024) {
            new_capacity = vec->cap ? vec->cap * 2 : 16;       // fast growth for small vectors
        } else if (vec->cap < 10000) {
            new_capacity = vec->cap + vec->cap / 2;           // ~1.5x growth using integer math
        } else {
            new_capacity = vec->cap + vec->cap / 5;           // ~1.2x growth using integer math
        }

        vec->data = safe_realloc(vec->data, new_capacity * sizeof(char*));
        vec->lens = safe_realloc(vec->lens, new_capacity * sizeof(size_t));
        vec->cap = new_capacity;
    }

    char *copy = safe_malloc(len ? len + 1 : 1);
    if (len) memcpy(copy, str, len);
    copy[len] = '\0';

    vec->data[vec->count] = copy;
    vec->lens[vec->count] = len;
    vec->count++;
}




static void hash_init(HashVec *v) { v->data = NULL; v->count = 0; v->cap = 0; }
static void hash_push(HashVec *v, HashEntry h) {
    if (v->count == v->cap) {
        const size_t nc = v->cap ? v->cap * 2 : 16;
        v->data = (HashEntry*) safe_realloc(v->data, nc * sizeof(HashEntry));
        v->cap = nc;
    }
    v->data[v->count++] = h;
}


// Find string in StringVec (linear scan) -> return offset into string table (to be computed later).
// We store dedup keys as the string bytes themselves; for offset we compute cumulative sizes later.
static ssize_t strings_find(StringVec *v, const char *s, const size_t len) {
    for (size_t i=0;i<v->count;i++){
        if (v->lens[i] == len && memcmp(v->data[i], s, len)==0) return (ssize_t)i;
    }
    return -1;
}

static void builder_init(BlobBuilder *bb) {
    nodes_init(&bb->nodes);
    pairs_init(&bb->pairs);
    index_init(&bb->indices);
    strings_init(&bb->strings);
}

static void builder_free(BlobBuilder *bb) {
    if (bb->nodes.data) free(bb->nodes.data);
    if (bb->pairs.data) free(bb->pairs.data);
    if (bb->indices.data) free(bb->indices.data);
    if (bb->strings.data) {
        for (size_t i=0;i<bb->strings.count;i++) free(bb->strings.data[i]);
        free(bb->strings.data);
    }
    if (bb->strings.lens) free(bb->strings.lens);
}

static uint64_t builder_add_string(BlobBuilder *bb, const char *s, const size_t len) {
    const size_t idx = strings_find(&bb->strings, s, len);
    if (idx != SIZE_MAX) return (uint64_t)idx;  // found, return existing index
    strings_push(&bb->strings, s, len);
    return (uint64_t)(bb->strings.count - 1);
}


// add scalar node; but NodeEntry->a/b must be offset/len in string table.
// We'll temporarily store a = index of string in bb->strings; b = length. Later we convert a to absolute offset.
static uint32_t builder_add_scalar(BlobBuilder *bb, const char *s, const size_t len, const uint8_t style_flags, const uint16_t tag_index) {
    const uint64_t str_index = builder_add_string(bb, s, len);
    NodeEntry n;
    n.node_type = SCALAR;
    n.style_flags = style_flags;
    n.tag_index = tag_index;
    n.a = str_index; // temporarily store string index
    n.b = len;
    nodes_push(&bb->nodes, n);
    return (uint32_t)(bb->nodes.count - 1);
}

// append pair (key_node_index, value_node_index)
static uint32_t builder_append_pair(BlobBuilder *bb, const uint32_t key_idx, const uint32_t val_idx) {
    PairEntry p;
    p.key_node_index = key_idx;
    p.value_node_index = val_idx;
    pairs_push(&bb->pairs, p);
    return (uint32_t)(bb->pairs.count - 1);
}

// add sequence node: stores a=first_index_index (uint64) (index into index table), b=element_count
static uint32_t builder_add_sequence(BlobBuilder *bb, const uint32_t *elements, const size_t elem_count) {
    const uint64_t first = bb->indices.count;
    for (size_t i=0;i<elem_count;i++) index_push(&bb->indices, elements[i]);
    NodeEntry n;
    n.node_type = 1;
    n.style_flags = 0;
    n.tag_index = 0;
    n.a = first;
    n.b = elem_count;
    nodes_push(&bb->nodes, n);
    return (uint32_t)(bb->nodes.count - 1);
}

// add mapping node: we will append given pair indices to pair table, and node.a = first_pair_index, b = pair_count
static uint32_t builder_add_mapping(BlobBuilder *bb, size_t pair_count) {
    const uint64_t first = bb->pairs.count;
    // append actual pairs by copying from provided pair_indices which refer to previously created pairs in the same bb or are temporary
    // In our usage we will create PairEntry locally and append directly using pairs_push. For convenience, here's an approach:
    // assume pair_indices point to existing pairs (created by builder_append_pair), so we simply create NodeEntry referencing them.
    // If we want to pass new pairs inline, prefer to call builder_append_pair for each.
    NodeEntry n;
    n.node_type = 2;
    n.style_flags = 0;
    n.tag_index = 0;
    n.a = first;
    n.b = pair_count;
    nodes_push(&bb->nodes, n);
    return (uint32_t)(bb->nodes.count - 1);
}


// comparator (file-scope) used by qsort
static int cmp_hashentry(const void *pa, const void *pb) {
    const HashEntry *a = (const HashEntry*)pa;
    const HashEntry *b = (const HashEntry*)pb;
    if (a->key_hash < b->key_hash) return -1;
    if (a->key_hash > b->key_hash) return 1;
    if (a->pair_index < b->pair_index) return -1;
    if (a->pair_index > b->pair_index) return 1;
    return 0;
}

// Build final blob in memory and write to filename
static int builder_build_and_write(BlobBuilder *bb, const char *filename, const uint32_t magic, const uint16_t version, const uint32_t flags, const int include_hash_index) {
    // First convert strings vector into a single concatenated byte array and compute offsets for each string
    size_t string_table_size = 0;
    for (size_t i = 0; i < bb->strings.count; ++i) string_table_size += bb->strings.lens[i];

    uint8_t *string_table = NULL;
    uint64_t *string_offsets = NULL;
    if (string_table_size) {
        string_table = (uint8_t*)malloc(string_table_size);
        if (!string_table) return 0;
        string_offsets = (uint64_t*)malloc(bb->strings.count * sizeof(uint64_t));
        if (!string_offsets) { free(string_table); return 0; }

        size_t cursor = 0;
        for (size_t i = 0; i < bb->strings.count; ++i) {
            string_offsets[i] = cursor;
            memcpy(string_table + cursor, bb->strings.data[i], bb->strings.lens[i]);
            cursor += bb->strings.lens[i];
        }
        assert(cursor == string_table_size);
    }

    // Convert nodes: for scalar nodes, replace a (was string index) with absolute offset into string table
    for (size_t i = 0; i < bb->nodes.count; ++i) {
        NodeEntry *n = &bb->nodes.data[i];
        if (n->node_type == 0) { // SCALAR
            const uint64_t str_index = n->a;
            if (str_index >= bb->strings.count) {
                // invalid, but set to empty string reference
                n->a = 0;
                n->b = 0;
            } else {
                if (!string_offsets) return 0;
                n->a = string_offsets[str_index];
                // n->b already length
            }
        }
        // sequences and mappings keep their indexes as counts/indices already relative to node/pair/index arrays
    }

    // Build hash index entries from pairs (only scalar keys)
    HashVec hvec; hash_init(&hvec);
    if (include_hash_index) {
        for (uint32_t i = 0; i < bb->pairs.count; ++i) {
            PairEntry *p = &bb->pairs.data[i];
            if (p->key_node_index >= bb->nodes.count) continue;
            const NodeEntry *kn = &bb->nodes.data[p->key_node_index];
            if (kn->node_type != 0) continue; // only scalar keys
            const uint64_t off = kn->a;
            const uint64_t len = kn->b;
            if (off + len > string_table_size) continue;
            if (!string_table) return 0;
            const uint64_t h = fnv1a64(string_table + off, len);
            HashEntry he; he.key_hash = h; he.pair_index = i; he.reserved = 0;
            hash_push(&hvec, he);
        }
        // sort by key_hash then pair_index -- use file-scope comparator
        if (hvec.count > 0) {
            qsort(hvec.data, hvec.count, sizeof(HashEntry), cmp_hashentry);
        }
    }

    // compute sizes and offsets
    const size_t header_size = sizeof(HeaderBlob); // 90
    const size_t node_table_size = bb->nodes.count * sizeof(NodeEntry);
    const size_t pair_table_size = bb->pairs.count * sizeof(PairEntry);
    const size_t index_table_size = bb->indices.count * sizeof(uint32_t);
    const size_t hash_index_size = (include_hash_index ? (hvec.count * sizeof(HashEntry)) : 0);
    const size_t st_size = string_table_size;

    const uint64_t node_table_offset = header_size;
    const uint64_t pair_table_offset = node_table_offset + node_table_size;
    const uint64_t index_table_offset = pair_table_offset + pair_table_size;
    const uint64_t hash_index_offset = index_table_offset + index_table_size;
    const uint64_t string_table_offset = hash_index_offset + hash_index_size;

    size_t total_size = 0;
    if (string_table_offset > SIZE_MAX - st_size) {
        // overflow
        if (string_table) free(string_table);
        if (string_offsets) free(string_offsets);
        if (hvec.data) free(hvec.data);
        return 0;
    }
    total_size = string_table_offset + st_size;

    uint8_t *blob = (uint8_t*)malloc(total_size);
    if (!blob) {
        fprintf(stderr, "No memory\n");
        if (string_table) free(string_table);
        if (string_offsets) free(string_offsets);
        if (hvec.data) free(hvec.data);
        return 0;
    }
    memset(blob, 0, total_size);

    // fill header manually (little-endian)
    write_u32_le(blob, 0, magic);
    write_u16_le(blob, 4, version);
    write_u32_le(blob, 6, flags);
    size_t off = 10;
    write_u64_le(blob, off + 0, node_table_offset); off += 8;
    write_u64_le(blob, off + 0, bb->nodes.count);       off += 8;
    write_u64_le(blob, off + 0, pair_table_offset);     off += 8;
    write_u64_le(blob, off + 0, bb->pairs.count);       off += 8;
    write_u64_le(blob, off + 0, index_table_offset);    off += 8;
    write_u64_le(blob, off + 0, bb->indices.count);     off += 8;
    write_u64_le(blob, off + 0, include_hash_index ? hash_index_offset : 0); off += 8;
    write_u64_le(blob, off + 0, include_hash_index ? hvec.count : 0);         off += 8;
    write_u64_le(blob, off + 0, string_table_offset);    off += 8;
    write_u64_le(blob, off + 0, st_size);                off += 8;
    assert(off == sizeof(HeaderBlob));

    // copy node table
    size_t dst = (size_t)node_table_offset;
    for (size_t i = 0; i < bb->nodes.count; ++i) {
        memcpy(blob + dst, &bb->nodes.data[i], sizeof(NodeEntry));
        dst += sizeof(NodeEntry);
    }
    // copy pair table
    dst = (size_t)pair_table_offset;
    for (size_t i = 0; i < bb->pairs.count; ++i) {
        memcpy(blob + dst, &bb->pairs.data[i], sizeof(PairEntry));
        dst += sizeof(PairEntry);
    }
    // copy index table (uint32 LE)
    dst = (size_t)index_table_offset;
    for (size_t i = 0; i < bb->indices.count; ++i) {
        write_u32_le(blob, dst, bb->indices.data[i]);
        dst += sizeof(uint32_t);
    }
    // copy hash index
    dst = (size_t)hash_index_offset;
    for (size_t i = 0; i < hvec.count; ++i) {
        memcpy(blob + dst, &hvec.data[i], sizeof(HashEntry));
        dst += sizeof(HashEntry);
    }
    // copy string table
    if (st_size) memcpy(blob + string_table_offset, string_table, st_size);

    // write to file
    FILE *f = fopen(filename, "wb");
    if (!f) {
        free(blob);
        if (string_table) free(string_table);
        if (string_offsets) free(string_offsets);
        if (hvec.data) free(hvec.data);
        return 0;
    }
    const size_t written = fwrite(blob, 1, total_size, f);
    if (written != total_size) {
        fclose(f);
        free(blob);
        if (string_table) free(string_table);
        if (string_offsets) free(string_offsets);
        if (hvec.data) free(hvec.data);
        return 0;
    }
    fclose(f);


    // cleanup
    free(blob);
    if (string_table) free(string_table);
    if (string_offsets) free(string_offsets);
    if (hvec.data) free(hvec.data);
    return 1;
}
// Build blob in-memory (returns malloc'd buffer) — replace the file-writing section with this.
// Caller must free(*out_buf) when done.
static unsigned char *builder_build_to_memory(BlobBuilder *bb, size_t *out_size, const uint32_t magic, const uint16_t version, const uint32_t flags, const int include_hash_index) {
    if (!out_size) return NULL;
    *out_size = 0;

    // Build string table
    size_t string_table_size = 0;
    for (size_t i = 0; i < bb->strings.count; ++i) string_table_size += bb->strings.lens[i];

    uint8_t *string_table = NULL;
    uint64_t *string_offsets = NULL;
    if (string_table_size) {
        string_table = (uint8_t*)malloc(string_table_size);
        if (!string_table) return NULL;
        string_offsets = (uint64_t*)malloc(bb->strings.count * sizeof(uint64_t));
        if (!string_offsets) { free(string_table); return NULL; }

        size_t cursor = 0;
        for (size_t i = 0; i < bb->strings.count; ++i) {
            string_offsets[i] = cursor;
            memcpy(string_table + cursor, bb->strings.data[i], bb->strings.lens[i]);
            cursor += bb->strings.lens[i];
        }
        if (cursor != string_table_size) { free(string_table); free(string_offsets); return NULL; }
    }

    // Convert scalar node string indices -> absolute offsets
    for (size_t i = 0; i < bb->nodes.count; ++i) {
        NodeEntry *n = &bb->nodes.data[i];
        if (n->node_type == 0) {
            uint64_t str_index = n->a;
            if (str_index >= bb->strings.count) {
                n->a = 0; n->b = 0;
            } else {
                if (!string_offsets) { free(string_table); free(string_offsets); return NULL; }
                n->a = string_offsets[str_index];
            }
        }
    }

    // Build hash entries
    HashVec hvec; hash_init(&hvec);
    if (include_hash_index) {
        for (uint32_t i = 0; i < bb->pairs.count; ++i) {
            PairEntry *p = &bb->pairs.data[i];
            if (p->key_node_index >= bb->nodes.count) continue;
            const NodeEntry *kn = &bb->nodes.data[p->key_node_index];
            if (kn->node_type != 0) continue;
            const uint64_t off = kn->a;
            const uint64_t len = kn->b;
            if (off + len > string_table_size) continue;
            if (!string_table) continue;
            const uint64_t h = fnv1a64(string_table + off, len);
            HashEntry he; he.key_hash = h; he.pair_index = i; he.reserved = 0;
            hash_push(&hvec, he);
        }
        if (hvec.count > 0) qsort(hvec.data, hvec.count, sizeof(HashEntry), cmp_hashentry);
    }

    // compute sizes/offsets
    const size_t header_size = sizeof(HeaderBlob);
    const size_t node_table_size = bb->nodes.count * sizeof(NodeEntry);
    const size_t pair_table_size = bb->pairs.count * sizeof(PairEntry);
    const size_t index_table_size = bb->indices.count * sizeof(uint32_t);
    const size_t hash_index_size = include_hash_index ? (hvec.count * sizeof(HashEntry)) : 0;
    const size_t st_size = string_table_size;

    const uint64_t node_table_offset = header_size;
    const uint64_t pair_table_offset = node_table_offset + node_table_size;
    const uint64_t index_table_offset = pair_table_offset + pair_table_size;
    const uint64_t hash_index_offset = index_table_offset + index_table_size;
    const uint64_t string_table_offset = hash_index_offset + hash_index_size;

    if (string_table_offset > SIZE_MAX - st_size) { free(string_table); free(string_offsets); if (hvec.data) free(hvec.data); return NULL; }
    size_t total_size = (size_t)(string_table_offset + st_size);

    unsigned char *buf = (unsigned char*)malloc(total_size);
    if (!buf) { free(string_table); free(string_offsets); if (hvec.data) free(hvec.data); return NULL; }
    memset(buf, 0, total_size);

    // write header (little-endian)
    write_u32_le(buf, 0, magic);
    write_u16_le(buf, 4, version);
    write_u32_le(buf, 6, flags);
    size_t off = 10;
    write_u64_le(buf, off + 0, node_table_offset); off += 8;
    write_u64_le(buf, off + 0, bb->nodes.count);       off += 8;
    write_u64_le(buf, off + 0, pair_table_offset);     off += 8;
    write_u64_le(buf, off + 0, bb->pairs.count);       off += 8;
    write_u64_le(buf, off + 0, index_table_offset);    off += 8;
    write_u64_le(buf, off + 0, bb->indices.count);     off += 8;
    write_u64_le(buf, off + 0, include_hash_index ? hash_index_offset : 0); off += 8;
    write_u64_le(buf, off + 0, include_hash_index ? hvec.count : 0);         off += 8;
    write_u64_le(buf, off + 0, string_table_offset);    off += 8;
    write_u64_le(buf, off + 0, st_size);                off += 8;
    assert(off == sizeof(HeaderBlob));

    // copy node table
    size_t dst = (size_t)node_table_offset;
    for (size_t i = 0; i < bb->nodes.count; ++i) {
        memcpy(buf + dst, &bb->nodes.data[i], sizeof(NodeEntry));
        dst += sizeof(NodeEntry);
    }
    // copy pair table
    dst = (size_t)pair_table_offset;
    for (size_t i = 0; i < bb->pairs.count; ++i) {
        memcpy(buf + dst, &bb->pairs.data[i], sizeof(PairEntry));
        dst += sizeof(PairEntry);
    }
    // copy index table (uint32 LE)
    dst = (size_t)index_table_offset;
    for (size_t i = 0; i < bb->indices.count; ++i) {
        write_u32_le(buf, dst, bb->indices.data[i]);
        dst += sizeof(uint32_t);
    }
    // copy hash index
    dst = (size_t)hash_index_offset;
    for (size_t i = 0; i < hvec.count; ++i) {
        memcpy(buf + dst, &hvec.data[i], sizeof(HashEntry));
        dst += sizeof(HashEntry);
    }
    // copy string table
    if (st_size) memcpy(buf + string_table_offset, string_table, st_size);

    // cleanup temporary allocations used during build
    free(string_table);
    free(string_offsets);
    if (hvec.data) free(hvec.data);

    *out_size = total_size;
    return buf;
}


static size_t trim_span(const unsigned char *src, const size_t len, size_t *begin, size_t *end) {
    /*
    This loop trims whitespace from both ends of a string by moving two pointers: b from the start and e from the end.
    It checks each side in every iteration and stops when both ends point to non-whitespace characters.
    After the loop, b, e marks the trimmed substring, and the function returns its length.
     */
    size_t b = 0;
    size_t e = len;

    while (b < e) {
        int left_space = isspace(src[b]);
        int right_space = isspace(src[e - 1]);

        if (!left_space && !right_space) break;

        if (left_space) ++b;
        if (right_space) --e;
    }

    *begin = b;
    *end = e;
    return (e > b) ? (e - b) : 0;
}


static bool is_comment_or_empty(const unsigned char *s, const size_t b, const size_t e) {
    /*
    This function checks whether a substring `[b, e)` of a string is non-empty and not a comment.
    It skips leading whitespace and returns `false` if the substring is empty or starts with `#`.
    Otherwise, it returns `true`, indicating that the line contains meaningful content.
     */
    if (b >= e) return false;
    size_t i = b;
    while (i < e && isspace((int)s[i])) ++i;
    if (i >= e) return false;
    if (s[i] == '#') return false;
    return true;
}


static char *memdup_str(const unsigned char *data, const size_t b, const size_t e) {
    /*
        This function creates a new heap-allocated C string by copying the memory region `[b, e)` from the input `data`.
        It allocates `len + 1` bytes, copies the content, and appends a null terminator for convenience.
        If memory allocation fails, it returns `NULL`; otherwise, it returns the new zero-terminated string.
     */
    const size_t len = (e > b) ? (e - b) : 0;
    char *p = malloc(len + 1);
    if (!p) return NULL;
    memcpy(p, data + b, len);
    p[len] = '\0';
    return p;
}



static unsigned char *parse(const void *mappedFile, const size_t fileSize, size_t *out_size) {
    if (!mappedFile || fileSize == 0 || !out_size) {
        out_size = 0;
        return NULL;
    }

    const unsigned char *data = mappedFile;
    BlobBuilder bb;
    builder_init(&bb);


    uint32_t last_key_node = (uint32_t)-1;
    int expecting_sequence_for_last_key = 0;

    // Parse line by line
    size_t pos = 0;
    while (pos < fileSize) {
        // find end of line
        size_t line_start = pos;
        size_t line_end = pos;
        while (line_end < fileSize && data[line_end] != '\n' && data[line_end] != '\r') ++line_end;

        // trim
        size_t b, e;
        trim_span(data, line_end - line_start, &b, &e);
        b += line_start; e += line_start; // adjust to absolute offsets

        if (is_comment_or_empty(data, b, e)) {
            // check for sequence item "- item"
            // find first non-space char
            size_t i = b;
            while (i < e && isspace((int)data[i])) ++i;
            if (i < e && data[i] == '-') {
                // sequence item
                // content after '-'
                size_t item_b = i + 1;
                while (item_b < e && isspace((int)data[item_b])) ++item_b;
                size_t item_e = e;
                size_t tb, te;
                trim_span(data, item_e - item_b, &tb, &te);
                tb += item_b;
                te += item_b;

                char *item_str = memdup_str(data, tb, te);
                if (!item_str) goto err;

                uint32_t item_node = builder_add_scalar(&bb, item_str, strlen(item_str), 0, 0);
                free(item_str);

                if (!expecting_sequence_for_last_key) {
                    // start a new anonymous sequence (no key) - create seq node that contains this single element for now
                    uint32_t elems[1] = { item_node };
                    uint32_t seq_idx = builder_add_sequence(&bb, elems, 1);
                    // append a pair with empty key? For simplicity we map special key "" to the sequence
                    uint32_t empty_k = builder_add_scalar(&bb, "", 0, 0, 0);
                    builder_append_pair(&bb, empty_k, seq_idx);
                    last_key_node = empty_k;
                    expecting_sequence_for_last_key = 1; // sequence started
                } else {
                    // append item to last sequence: we must find sequence node index stored as value in last pair
                    // last_key_node is index of key; find corresponding pair index by scanning pairs (inefficient but fine for demo)
                    uint32_t seq_pair_index = (uint32_t)-1;
                    for (size_t pi = 0; pi < bb.pairs.count; ++pi) {
                        if (bb.pairs.data[pi].key_node_index == last_key_node) {
                            seq_pair_index = (uint32_t)pi;
                            break;
                        }
                    }
                    if (seq_pair_index == (uint32_t)-1) {
                        // fallback: create new sequence and pair
                        uint32_t elems[1] = { item_node };
                        uint32_t seq_idx = builder_add_sequence(&bb, elems, 1);
                        builder_append_pair(&bb, last_key_node, seq_idx);
                    } else {
                        // get current value node index (should be sequence node index)
                        uint32_t value_node_idx = bb.pairs.data[seq_pair_index].value_node_index;
                        // value_node_idx references a NodeEntry of type SEQUENCE; we need to append item to index_table and increase element_count
                        if (value_node_idx < bb.nodes.count && bb.nodes.data[value_node_idx].node_type == 1) {
                            // append to index table
                            index_push(&bb.indices, item_node);
                            // update node.b (element_count)
                            bb.nodes.data[value_node_idx].b += 1;
                            // note: node.a (first_index) remains valid since index table is flat and we appended at end
                        } else {
                            // not a sequence (or not exist), create new sequence and replace pair value
                            uint32_t elems[1] = { item_node };
                            uint32_t seq_idx = builder_add_sequence(&bb, elems, 1);
                            bb.pairs.data[seq_pair_index].value_node_index = seq_idx;
                        }
                    }
                }
            } else {
                // mapping "key: value" (split at first ':')
                size_t colon = b;
                while (colon < e && data[colon] != ':') ++colon;
                if (colon < e && data[colon] == ':') {
                    // key = [b, colon)
                    size_t kb, ke;
                    trim_span(data + b, colon - b, &kb, &ke);
                    kb += b; ke += b;
                    // value = after colon
                    size_t vb = colon + 1;
                    // skip spaces after colon
                    while (vb < e && isspace((int)data[vb])) ++vb;
                    size_t vbegin, vend;
                    trim_span(data + vb, e - vb, &vbegin, &vend);
                    vbegin += vb; vend += vb;

                    char *kstr = memdup_str(data, kb, ke);
                    char *vstr = memdup_str(data, vbegin, vend);
                    if (!kstr || !vstr) { free(kstr); free(vstr); goto err; }

                    uint32_t knode = builder_add_scalar(&bb, kstr, strlen(kstr), 0, 0);
                    uint32_t vnode = builder_add_scalar(&bb, vstr, strlen(vstr), 0, 0);
                    free(kstr); free(vstr);

                    builder_append_pair(&bb, knode, vnode);

                    // update last_key_node for possible following sequence items
                    last_key_node = knode;
                    expecting_sequence_for_last_key = 0; // value was scalar, not sequence
                } else {
                    // no colon found: treat as plain scalar/document root (store as single scalar)
                    size_t tb, te;
                    trim_span(data + b, e - b, &tb, &te);
                    tb += b; te += b;
                    char *s = memdup_str(data, tb, te);
                    if (!s) goto err;
                    uint32_t n = builder_add_scalar(&bb, s, strlen(s), 0, 0);
                    free(s);
                    // append as a pair with empty key
                    uint32_t empty_k = builder_add_scalar(&bb, "", 0, 0, 0);
                    builder_append_pair(&bb, empty_k, n);
                    last_key_node = empty_k;
                    expecting_sequence_for_last_key = 0;
                }
            }
        }

        // advance pos past EOL (handle CRLF)
        pos = line_end;
        if (pos < fileSize && data[pos] == '\r') ++pos;
        if (pos < fileSize && data[pos] == '\n') ++pos;
        // alternatively just skip single newline; the above handles both \r\n and \n
    }

    // After collecting pairs, create a top-level mapping node that spans all pairs
    if (bb.pairs.count > 0) {
        NodeEntry top;
        top.node_type = 2; top.style_flags = 0; top.tag_index = 0;
        top.a = 0; // first pair index
        top.b = bb.pairs.count;
        nodes_push(&bb.nodes, top);
        uint32_t top_idx = (uint32_t)(bb.nodes.count - 1);

        // create document node referencing top
        NodeEntry doc;
        doc.node_type = 4; doc.style_flags = 0; doc.tag_index = 0;
        doc.a = top_idx; doc.b = 0;
        nodes_push(&bb.nodes, doc);
    } else {
        // empty document -> create empty document node pointing to nothing (or a null root)
        NodeEntry doc;
        doc.node_type = 4; doc.style_flags = 0; doc.tag_index = 0;
        doc.a = 0; doc.b = 0;
        nodes_push(&bb.nodes, doc);
    }

    size_t blob_size = 0;
    unsigned char *blob_buf = builder_build_to_memory(&bb, &blob_size, 0x59414D4Cu, 1, 0, 1);
    if (!blob_buf) {
        builder_free(&bb);
        if (out_size) *out_size = 0;
        return NULL;
    }
    builder_free(&bb);
    *out_size = blob_size;
    return blob_buf;


err:
    builder_free(&bb);
    *out_size = 0;
    return NULL;
}

/* -------------------------
   Hash helpers
   ------------------------- */

MYLIB_API uint64_t compute_hash_from_bytes(const void *data, const uint64_t len) {
    if (data == NULL || len == 0) return 0;
    return XXH64(data, len, 0);
}

MYLIB_API uint64_t compute_hash_from_node(const void *nodes_base,
                                               const uint64_t node_count,
                                               const uint32_t node_index,
                                               const void *string_table_base,
                                               const uint64_t string_table_size) {
    if (nodes_base == NULL || string_table_base == NULL) return 0;
    if ((uint64_t)node_index >= node_count) return 0;

    const NodeEntry *nodes = nodes_base;
    const NodeEntry *n = &nodes[node_index];

    if (n->node_type != 0) return 0; // not SCALAR

    const uint64_t offset = n->a;
    const uint64_t len = n->b;

    if (offset > string_table_size) return 0;
    if (len > string_table_size) return 0;
    if (offset + len > string_table_size) return 0;

    const void *ptr = (const char*)string_table_base + (size_t)offset;
    return XXH64(ptr, len, 0);
}


MYLIB_API void *mapFile(const char *path, size_t *out_size) {
    if (out_size) *out_size = 0;
    if (path == NULL) return NULL;

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    const int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return NULL;
    }

    if (st.st_size == 0) {
        close(fd);
        if (out_size) *out_size = 0;
        return NULL;
    }

    void *map = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (map == MAP_FAILED) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    if (out_size) *out_size = (size_t)st.st_size;
    return map;
#else
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (out_size) *out_size = 0;
        return NULL;
    }

    LARGE_INTEGER liSize;
    if (!GetFileSizeEx(hFile, &liSize)) {
        CloseHandle(hFile);
        if (out_size) *out_size = 0;
        return NULL;
    }

    if (liSize.QuadPart == 0) {
        CloseHandle(hFile);
        if (out_size) *out_size = 0;
        return NULL;
    }

    if ((uint64_t)liSize.QuadPart > (uint64_t)SIZE_MAX) {
        // rozmiar pliku nie mieści się w size_t
        CloseHandle(hFile);
        if (out_size) *out_size = 0;
        return NULL;
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL );
    if (hMap == NULL) {
        CloseHandle(hFile);
        if (out_size) *out_size = 0;
        return NULL;
    }

    void *view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0 );
    if (view == NULL) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        if (out_size) *out_size = 0;
        return NULL;
    }


    CloseHandle(hMap);
    CloseHandle(hFile);

    if (out_size) *out_size = (size_t)liSize.QuadPart;
    return view;
#endif
}

MYLIB_API int unmapFile(void *addr, size_t size) {
    if (addr == NULL) return -1;
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    if (size == 0) return -1;
    if (munmap(addr, size) == 0) return 0;
    return -1;
#else
    (void)size;
    if (UnmapViewOfFile(addr)) return 0;
    return -1;
#endif
}

/* -------------------------
   Native memory helpers
   ------------------------- */

MYLIB_API void freeBlob(void *ptr) {
    if (!ptr) return;
    free(ptr);
}

/* -------------------------
   JNI helpers
   ------------------------- */

/* Helper: create direct ByteBuffer or free pointer if creation fails */
static jobject create_direct_bytebuffer_or_free(JNIEnv *env, void *buf, const jlong len) {
    const jobject bb = (*env)->NewDirectByteBuffer(env, buf, len);
    if (bb == NULL) {
        free(buf);
        return NULL;
    }
    return bb;
}

/* -------------------------
   JNI wrappers (stubs)
   ------------------------- */


JNIEXPORT jobject JNICALL
Java_com_github_scalerock_cjyaml_CJYaml_NativeLib_1parseToDirectByteBuffer(JNIEnv *env, const jclass cls, const jstring path) {
    (void)cls;
    if (path == NULL) return NULL;

    const char *cpath = (*env)->GetStringUTFChars(env, path, NULL);
    if (cpath == NULL) return NULL; // Out of memory

    size_t mapped_size = 0;
    void *mapped = mapFile(cpath, &mapped_size);
    if (mapped == NULL) {
        (*env)->ReleaseStringUTFChars(env, path, cpath);
        return NULL;
    }

    /* Parse file contents into a new buffer */
    size_t parsed_size = 0;
    void *buf = parse(mapped, mapped_size, &parsed_size);

    /* If parse() allocates a new buffer, we can unmap the original file now */
    if (unmapFile(mapped, mapped_size) != 0) {
        if (buf) free(buf);
        (*env)->ReleaseStringUTFChars(env, path, cpath);
        return NULL;
    }

    if (buf == NULL) {
        (*env)->ReleaseStringUTFChars(env, path, cpath);
        return NULL;
    }

    /* Safely convert size_t → jlong */
    jlong jlen;
    if (parsed_size <= (size_t)LLONG_MAX) {
        jlen = (jlong)parsed_size;
    } else {
        free(buf);
        (*env)->ReleaseStringUTFChars(env, path, cpath);
        return NULL;
    }

    /* Always release the UTF string after use */
    (*env)->ReleaseStringUTFChars(env, path, cpath);

    /* Wrap the native buffer as a DirectByteBuffer.
       The helper should free buf on error automatically. */
    return create_direct_bytebuffer_or_free(env, buf, jlen);
}


JNIEXPORT jbyteArray JNICALL
Java_com_github_scalerock_cjyaml_CJYaml_NativeLib_1parseToByteArray(JNIEnv *env, const jclass cls, jstring path) {
    (void)cls;
    if (path == NULL) return NULL;

    const char *cpath = (*env)->GetStringUTFChars(env, path, NULL);
    if (cpath == NULL) return NULL;

    size_t mapped_size = 0;
    void *mapped = mapFile(cpath, &mapped_size);
    if (mapped == NULL) {
        (*env)->ReleaseStringUTFChars(env, path, cpath);
        return NULL;
    }

    /* Parse the file */
    size_t parsed_size = 0;
    void *buf = parse(mapped, mapped_size, &parsed_size);

    /* Unmap the original file after parsing */
    if (unmapFile(mapped, mapped_size) != 0) {
        if (buf) free(buf);
        (*env)->ReleaseStringUTFChars(env, path, cpath);
        return NULL;
    }

    if (buf == NULL) {
        (*env)->ReleaseStringUTFChars(env, path, cpath);
        return NULL;
    }

    /* Ensure size fits in jsize */
    if (parsed_size > (size_t)INT_MAX) {
        free(buf);
        (*env)->ReleaseStringUTFChars(env, path, cpath);
        return NULL;
    }
    const jsize len = (jsize)parsed_size;

    /* Create a Java byte array for the parsed data */
    const jbyteArray out = (*env)->NewByteArray(env, len);
    if (out == NULL) {
        free(buf);
        (*env)->ReleaseStringUTFChars(env, path, cpath);
        return NULL;
    }

    /* Copy from native buffer to Java byte[] */
    (*env)->SetByteArrayRegion(env, out, 0, len, (const jbyte *)buf);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        free(buf);
        (*env)->DeleteLocalRef(env, out);
        (*env)->ReleaseStringUTFChars(env, path, cpath);
        return NULL;
    }

    /* Free native memory and release Java string */
    free(buf);
    (*env)->ReleaseStringUTFChars(env, path, cpath);

    return out;
}

JNIEXPORT jbyteArray JNICALL
Java_com_github_scalerock_cjyaml_CJYaml_NativeLib_1parseToByteArrayFromOpenFile(JNIEnv *env, const jclass cls, const jstring fileContent) {
    (void)cls;
    if (fileContent == NULL) return NULL;

    const char *cpath = (*env)->GetStringUTFChars(env, fileContent, NULL);
    if (cpath == NULL) return NULL;


    /* Parse the file */
    size_t parsed_size = 0;
    const size_t fileSize = (*env)->GetStringUTFLength(env, fileContent);
    void *buf = parse(fileContent, fileSize, &parsed_size);
    (*env)->ReleaseStringUTFChars(env, fileContent, cpath);
    if (buf == NULL) {
        return NULL;
    }

    /* Ensure size fits in jsize */
    if (parsed_size > (size_t)INT_MAX) {
        free(buf);
        return NULL;
    }
    const jsize len = (jsize)parsed_size;

    /* Create a Java byte array for the parsed data */
    const jbyteArray out = (*env)->NewByteArray(env, len);
    if (out == NULL) {
        free(buf);

        return NULL;
    }

    /* Copy from native buffer to Java byte[] */
    (*env)->SetByteArrayRegion(env, out, 0, len, (const jbyte *)buf);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        free(buf);
        (*env)->DeleteLocalRef(env, out);
        (*env)->ReleaseStringUTFChars(env, fileContent, cpath);
        return NULL;
    }

    /* Free native memory and release Java string */
    free(buf);

    return out;
}


/*
 * freeBlob
 *
 * JNI wrapper to free native memory referenced by a DirectByteBuffer.
 * This must be called from Java when the buffer is no longer needed,
 * unless you use a Cleaner or finalizer on the Java side.
 */
JNIEXPORT void JNICALL
Java_com_github_scalerock_cjyaml_CJYaml_NativeLib_1freeBlob(JNIEnv *env, const jclass cls, jobject buffer) {
    (void)cls;
    if (buffer == NULL) return;

    void *addr = (*env)->GetDirectBufferAddress(env, buffer);
    if (addr != NULL) free(addr);
}

