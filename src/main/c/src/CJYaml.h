#ifndef CJYAML_H
#define CJYAML_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <jni.h>

/* Export macro */
#ifdef _WIN32
  #define MYLIB_API __declspec(dllexport)
#else
  #if defined(__GNUC__) && __GNUC__ >= 4
    #define MYLIB_API __attribute__((visibility("default")))
  #else
    #define MYLIB_API
  #endif
#endif

#define POSIX_C_SOURCE 200809L
/*
 File layout:
 [ HEADER ]
 [ NODE_TABLE ]   // node_count * sizeof(NodeEntry)
 [ PAIR_TABLE ]   // pair_count * sizeof(PairEntry)
 [ INDEX_TABLE ]  // index_count * sizeof(uint32_t)
 [ HASH_INDEX ]   // hash_index_count * sizeof(HashEntry)  (optional)
 [ STRING_TABLE ] // concatenated UTF-8 strings (deduplicated)
*/
#pragma pack(push, 1)
    typedef struct HeaderBlob {
        uint32_t magic;
        uint16_t version;
        uint32_t flags;

        uint64_t node_table_offset;
        uint64_t node_count;

        uint64_t pair_table_offset;
        uint64_t pair_count;

        uint64_t index_table_offset;
        uint64_t index_count;

        uint64_t hash_index_offset;
        uint64_t hash_index_size;

        uint64_t string_table_offset;
        uint64_t string_table_size;
    } HeaderBlob;
#pragma pack(pop)
_Static_assert(sizeof(HeaderBlob) == 90, "HeaderBlob must be 96 bytes");



#pragma pack(push,1)
typedef struct {
    uint8_t node_type;     // enum: SCALAR=0, SEQUENCE=1, MAPPING=2, ALIAS=3, DOCUMENT=4
    uint8_t style_flags;
    uint16_t tag_index;
    uint64_t a;
    uint64_t b;
} NodeEntry;
#pragma pack(pop)
_Static_assert(sizeof(NodeEntry) == (1+1+2+8+8), "NodeEntry size mismatch");

#pragma pack(push,1)
typedef struct {
    uint32_t key_node_index;
    uint32_t value_node_index;
} PairEntry;
#pragma pack(pop)
_Static_assert(sizeof(PairEntry) == 8, "PairEntry size mismatch");

#pragma pack(push,1)
typedef struct HashEntry {
    uint64_t key_hash;
    uint32_t pair_index;
    uint32_t reserved;
} HashEntry;
#pragma pack(pop)
_Static_assert(sizeof(HashEntry) == 16, "HashEntry size mismatch");



typedef struct {
   NodeEntry *data;
   size_t count;
   size_t cap;
} NodeVec;
typedef struct {
    PairEntry *data;
    size_t count;
    size_t cap;
} PairVec;

typedef struct {
    uint32_t *data;
    size_t count;
    size_t cap;
} IndexVec;
typedef struct {
   char **data;    // pointers to allocated null-terminated strings
   size_t *lens;   // lengths for each string
   size_t count;
   size_t cap;
} StringVec;

typedef struct {
    HashEntry *data;
    size_t count;
    size_t cap;
} HashVec;


typedef struct {
   NodeVec nodes;
   PairVec pairs;
   IndexVec indices;
   StringVec strings; // unique strings (dedup)
      // temporary mapping of node scalar -> string index is implicit because scalar node stores offset (we'll fill after building string table)
} BlobBuilder;





/* ---------------------------
   Hash helpers
   --------------------------- */

/* Hash raw bytes */
MYLIB_API uint64_t compute_hash_from_bytes(const void *data, uint64_t len);

/* Hash a SCALAR node safely  */
MYLIB_API uint64_t compute_hash_from_node_safe(const void *nodes_base,
                                               uint64_t node_count,
                                               uint32_t node_index,
                                               const void *string_table_base,
                                               uint64_t string_table_size);

#ifdef __cplusplus
}
#endif

#endif /* CJYAML_H */
