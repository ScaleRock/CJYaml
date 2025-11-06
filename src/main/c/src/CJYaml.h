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

/*
 File layout:
 [ HEADER ]
 [ NODE_TABLE ]   // node_count * sizeof(NodeEntry)
 [ PAIR_TABLE ]   // pair_count * sizeof(PairEntry)
 [ INDEX_TABLE ]  // index_count * sizeof(uint32_t)
 [ HASH_INDEX ]   // hash_index_count * sizeof(HashEntry)  (optional)
 [ STRING_TABLE ] // concatenated UTF-8 strings (deduplicated)
*/

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

#pragma pack(push,1)
typedef struct {
    uint8_t node_type;     // enum: SCALAR=0, SEQUENCE=1, MAPPING=2, ALIAS=3, DOCUMENT=4
    uint8_t style_flags;
    uint16_t tag_index;
    uint64_t a;
    uint64_t b;
} NodeEntry;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct {
    uint32_t key_node_index;
    uint32_t value_node_index;
} PairEntry;
#pragma pack(pop)

struct HashEntry {
    uint64_t key_hash;
    uint32_t pair_index;
    uint32_t reserved;
};

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

/* ---------------------------
   File mapping helpers
   --------------------------- */

/* Map file into memory (POSIX mmap or Windows equivalent).
   Returns pointer to mapped memory or NULL on error. If out_size != NULL,
   the file size is written there. */
MYLIB_API void *mapFile(const char *path, size_t *out_size);

/* Unmap previously mapped memory. Returns 0 on success, -1 on failure. */
MYLIB_API int unmapFile(void *addr, size_t size);

/* ---------------------------
   Native memory helpers
   --------------------------- */

/* Free native blob (C API) */
MYLIB_API void freeBlob(void *ptr);

/* ---------------------------
   JNI wrappers
   --------------------------- */

/*
 * JNI wrappers. The JNI names below use example package/class:
 *   com.example.NativeLib
 *
 * If your Java package/class is different, either:
 *  - change the JNI function names to the correct mangled names, or
 *  - register natives using RegisterNatives.
 */

/* Create a direct java.nio.ByteBuffer from a Java byte[].
   Java signature: public static native java.nio.ByteBuffer parseToDirectByteBuffer(byte[] data);
   Caller must call freeBlob(Buffer) to free native memory (or use Cleaner on Java side). */
MYLIB_API jobject JNICALL Java_com_example_NativeLib_parseToDirectByteBuffer(JNIEnv *env, jclass cls, jbyteArray data);

/* Parse input bytes and return a java byte[] (copy). Replace stub with your parser.
   Java signature: public static native byte[] parseToByteArray(byte[] data); */
MYLIB_API jbyteArray JNICALL Java_com_example_NativeLib_parseToByteArray(JNIEnv *env, jclass cls, jbyteArray data);

/* Free native memory pointed by a direct ByteBuffer.
   Java signature: public static native void freeBlob(java.nio.ByteBuffer buf); */
MYLIB_API void JNICALL Java_com_example_NativeLib_freeBlob(JNIEnv *env, jclass cls, jobject buffer);

/* Map file to memory and return pointer as jlong (or use direct ByteBuffer variant).
   Java signature (example): private static native long mapFileNative(String path); */
MYLIB_API jlong JNICALL Java_com_example_NativeLib_mapFileNative(JNIEnv *env, jclass cls, jstring path);

/* Unmap file previously mapped.
   Java signature (example): private static native int unmapFileNative(long ptr, long size); */
MYLIB_API jint JNICALL Java_com_example_NativeLib_unmapFileNative(JNIEnv *env, jclass cls, jlong ptr, jlong size);

#ifdef __cplusplus
}
#endif

#endif /* CJYAML_H */
