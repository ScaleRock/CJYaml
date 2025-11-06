#ifndef CJYAML_H
#define CJYAML_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


#ifdef _WIN32
  // Windows DLL
  #define MYLIB_API __declspec(dllexport)
#else
  // Linux / Unix .so
  #if __GNUC__ >= 4
    #define MYLIB_API __attribute__((visibility("default")))
  #else
    #define MYLIB_API
  #endif
#endif

#define READ_CHUNK 8192


/*
[ HEADER ]
[ NODE_TABLE ]   // node_count * sizeof(NodeEntry)
[ PAIR_TABLE ]   // pair_count * sizeof(PairEntry)
[ INDEX_TABLE ]  // index_count * sizeof(uint32_t)
[ HASH_INDEX ]   // hash_index_count * sizeof(HashEntry)  (optional but included here)
[ STRING_TABLE ] // concatenated UTF-8 strings (deduplicated)
 */
typedef struct HederBlob {
  uint32_t magic; // Size: 4B; File magic: e.g 0x59414D4C (ASCII "YAML")
  uint16_t version; // Size: 2B; Format major version
  uint32_t flags; // Size: 4B; Bitflags: bit0=endian(0=little), bit1=compression(0=none), others reserved
  uint64_t node_table_offset; // Size: 8B; Offset to node table
  uint64_t node_count; // Size: 8B;
  uint64_t pair_table_offset; // Size: 8B; Offset to pair table
  uint64_t pair_count; // Size: 8B; Number of pairs
  uint64_t index_table_offset; // Size: 8B; Offset to index table
  uint64_t index_count; // Size: 8B
  uint64_t hash_index_offset; // Size: 8B; Offset to hash-index array (0 if not present)
  uint64_t hash_index_size; // Size 8B
  uint64_t string_table_offset; // 8B
  uint64_t string_table_size; // 8B

}HederBlod;

#pragma pack(push,1)
  typedef struct {
    uint8_t node_type;     // enum: SCALAR=0, SEQUENCE=1, MAPPING=2, ALIAS=3, DOCUMENT=4
    uint8_t style_flags;   // bitmask for scalar subtype, folded/literal, etc.
    uint16_t tag_index;    // index into string_table (0 == no tag)
    uint64_t a;            // semantics depend on node_type
    uint64_t b;            // semantics depend on node_type
  } NodeEntry;
#pragma pack(pop)


#ifdef __cplusplus
}
#endif


#endif