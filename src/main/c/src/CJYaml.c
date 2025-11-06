#include "CJYaml.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>


#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
#else
    #include <windows.h>
    #define _CRT_SECURE_NO_WARNINGS
#endif

/* xxhash prototype - ensure xxhash library/header is available in build */
extern uint64_t XXH64(const void* input, size_t length, uint64_t seed);

/* -------------------------
   Hash helpers
   ------------------------- */

MYLIB_API uint64_t compute_hash_from_bytes(const void *data, const uint64_t len) {
    if (data == NULL || len == 0) return 0;
    return XXH64(data, len, 0);
}

MYLIB_API uint64_t compute_hash_from_node_safe(const void *nodes_base,
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

    const void *ptr = ((const char*)string_table_base + (size_t)offset);
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
    HANDLE hFile = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
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

    HANDLE hMap = CreateFileMappingA(
        hFile,
        NULL,
        PAGE_READONLY,
        0,
        0,
        NULL
    );
    if (hMap == NULL) {
        CloseHandle(hFile);
        if (out_size) *out_size = 0;
        return NULL;
    }

    void *view = MapViewOfFile(
        hMap,
        FILE_MAP_READ,
        0, 0,
        0
    );
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
    jobject bb = (*env)->NewDirectByteBuffer(env, buf, len);
    if (bb == NULL) {
        free(buf);
        return NULL;
    }
    return bb;
}

/* -------------------------
   JNI wrappers (stubs)
   ------------------------- */

/* parseToDirectByteBuffer
   For now: wraps input byte[] into native malloc buffer and returns DirectByteBuffer.
   TODO: replace with real parsing logic returning parsed blob.
*/
MYLIB_API jobject JNICALL NativeLib_parseToDirectByteBuffer(JNIEnv *env, const jclass cls, const jbyteArray data) {
    (void)cls;

    if (data == NULL) return NULL;

    const jsize len = (*env)->GetArrayLength(env, data);
    if (len <= 0) return NULL;

    void *buf = malloc((size_t)len);
    if (buf == NULL) return NULL;

    (*env)->GetByteArrayRegion(env, data, 0, len, (jbyte*)buf);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        free(buf);
        return NULL;
    }

    /* TODO: call your parser here to transform 'buf' into parsed binary blob.
       For now we simply return the raw bytes as direct buffer. */
    return create_direct_bytebuffer_or_free(env, buf, len);
}

/* parseToByteArray
   For now: returns a copy of input array.
   TODO: replace with actual parse output bytes.
*/
MYLIB_API jbyteArray JNICALL NativeLib_parseToByteArray(JNIEnv *env, const jclass cls, const jbyteArray data) {
    (void)cls;

    if (data == NULL) return NULL;

    jsize len = (*env)->GetArrayLength(env, data);
    if (len < 0) return NULL;

    jbyteArray out = (*env)->NewByteArray(env, len);
    if (out == NULL) return NULL;

    jbyte *tmp = malloc((size_t)len);
    if (!tmp) {
        (*env)->DeleteLocalRef(env, out);
        return NULL;
    }

    (*env)->GetByteArrayRegion(env, data, 0, len, tmp);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        free(tmp);
        (*env)->DeleteLocalRef(env, out);
        return NULL;
    }

    (*env)->SetByteArrayRegion(env, out, 0, len, tmp);
    free(tmp);
    return out;
}

/* freeBlob JNI wrapper: free native memory referenced by direct ByteBuffer */
MYLIB_API void JNICALL NativeLib_freeBlob(JNIEnv *env, const jclass cls, jobject buffer) {
    (void)cls;

    if (buffer == NULL) return;

    void *addr = (*env)->GetDirectBufferAddress(env, buffer);
    if (addr != NULL) free(addr);
}

/* mapFileNative: returns pointer as jlong (or 0 on error).
   Note: passing raw pointers to Java is unsafe but common in JNI.
   Alternative: return NewDirectByteBuffer(env, mapped, size).
*/
MYLIB_API jlong JNICALL NativeLib_mapFileNative(JNIEnv *env, const jclass cls, const jstring jpath) {
    (void)cls;
    if (jpath == NULL) return 0;

    const char *path = (*env)->GetStringUTFChars(env, jpath, NULL);
    if (path == NULL) return 0;

    size_t size = 0;
    void *mapped = mapFile(path, &size);

    (*env)->ReleaseStringUTFChars(env, jpath, path);

    if (mapped == NULL) return 0;

    return (intptr_t)mapped;
}

/* unmapFileNative: unmap previously mapped pointer */
MYLIB_API jint JNICALL NativeLib_unmapFileNative(JNIEnv *env, jclass cls, jlong ptr, jlong size) {
    (void)env;
    (void)cls;
    if (ptr == 0 || size == 0) return -1;
    void *addr = (void*)ptr;
    return (unmapFile(addr, (size_t)size) == 0) ? 0 : -1;
}
