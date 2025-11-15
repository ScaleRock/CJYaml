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
Idea:
    name: John Doe
    age: 30
    languages:
      - Python
      - C
      - JavaScript
    address:
      city: Warsaw
      country: Poland

    DOCUMENT (0)
    └─ MAPPING (1)
       ├─ Pair: name -> SCALAR (2)
       ├─ Pair: age -> SCALAR (3)
       ├─ Pair: languages -> SEQUENCE (4)
       │    ├─ SCALAR (5) "Python"
       │    ├─ SCALAR (6) "C"
       │    └─ SCALAR (7) "JavaScript"
       └─ Pair: address -> MAPPING (8)
            ├─ Pair: city -> SCALAR (9)
            └─ Pair: country -> SCALAR (10)


 */


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
_Static_assert(sizeof(HeaderBlob) == 90, "HeaderBlob must be 90 bytes");

#define CJYAML_MAGIC 0x59414D4Cu  // 'Y','A','M','L'
#define HEADER_BLOB_SIZE (sizeof(HeaderBlob))

#define SCALAR 0
#define SEQUENCE 1
#define MAPPING 2
#define ALIAS 3
#define DOCUMENT 4


// SCALAR subtype (bits 0-1)
#define SCALAR_STRING 0x0
#define SCALAR_INT    0x1
#define SCALAR_FLOAT  0x2
#define SCALAR_BOOL   0x3


#pragma pack(push,1)
typedef struct {
    uint8_t node_type;
    /* enum:
            SCALAR=0,
            SEQUENCE=1,
            MAPPING=2,
            ALIAS=3,
            DOCUMENT=4
    Examples:
        SCALAR:
            name: John Doe

    SEQUENCE:
        languages:
            - Python
            - C
            - JavaScript
    MAPING:
        address:
          city: Warsaw
          country: Poland
    */
    uint8_t style_flags; // bitmask for scalar subtype, folded/literal, etc.
    /*
    style_flags bits:
        Bit 0..1: SCALAR subtype (00=string, 01=int, 10=float, 11=bool)
        Bit 2: folded literal indicator (text format)
        Bits 3..7: reserved for future use

     */
    uint16_t tag_index; // if non-zero this is an index into the string table containing the explicit YAML tag (e.g. "!!str", "!mytag"). 0 means "no tag".

    uint64_t a;
    uint64_t b;
    /*
    Interpretation of a/b:
        SCALAR: a = offset_into_string_table, b = length_in_bytes.
        SEQUENCE: a = first_index_into_index_table (uint32 indexes packed into index table; stored as uint64 to keep uniformity), b = element_count.
        MAPPING: a = first_pair_index_in_pair_table, b = pair_count.
        ALIAS: a = target_node_index (the node index that the alias points to), b = 0.
        DOCUMENT: a = root_node_index (or root node reference), b = 0.
     */

} NodeEntry;
#pragma pack(pop)
_Static_assert(sizeof(NodeEntry) == (1+1+2+8+8), "NodeEntry size mismatch");

#pragma pack(push,1)
typedef struct {
    uint32_t key_node_index; // index into node_table
    uint32_t value_node_index; // // index into node_table
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



#ifdef __cplusplus
}
#endif

#endif /* CJYAML_H */
