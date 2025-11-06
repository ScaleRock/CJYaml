#include "CJYaml.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


/* Try to get file size using fseek/ftell.
 * Returns file size on success, or -1 if not possible (e.g. pipe, stdin). */
static long get_file_size(FILE *fp) {
    if (fseek(fp, 0, SEEK_END) != 0)
        return -1;

    long size = ftell(fp);
    if (size < 0)
        return -1;

    if (fseek(fp, 0, SEEK_SET) != 0)
        return -1;

    return size;
}


/* Read entire file when the size is already known. */
static char *read_exact_size(FILE *fp, long size) {
    char *buf = malloc((size_t)size + 1);
    if (!buf) return NULL;

    size_t read_bytes = fread(buf, 1, (size_t)size, fp);
    if (read_bytes != (size_t)size && ferror(fp)) {
        free(buf);
        return NULL;
    }

    buf[read_bytes] = '\0';
    return buf;
}


/* Fallback: read file in chunks when size is unknown. */
static char *read_by_chunks(FILE *fp) {
    size_t cap = READ_CHUNK;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;

    while (1) {
        /* Make sure there is room for the next chunk + null terminator */
        if (len + READ_CHUNK + 1 > cap) {
            size_t newcap = cap * 2;
            if (newcap < len + READ_CHUNK + 1)
                newcap = len + READ_CHUNK + 1;

            char *tmp = realloc(buf, newcap);
            if (!tmp) {
                free(buf);
                return NULL;
            }
            buf = tmp;
            cap = newcap;
        }

        size_t n = fread(buf + len, 1, READ_CHUNK, fp);
        if (n > 0) len += n;

        if (n < READ_CHUNK) {
            if (feof(fp)) break;
            if (ferror(fp)) {
                free(buf);
                return NULL;
            }
        }
    }

    /* Shrink buffer to exact size + null terminator */
    char *final = realloc(buf, len + 1);
    if (!final) {
        buf[len] = '\0';
        return buf;
    }

    final[len] = '\0';
    return final;
}


/* Public function: read all file contents into a char*.
 * Returns null-terminated string, or NULL on error. */
static char *read_file_all(FILE *fp) {
    if (!fp) {
        errno = EINVAL;
        return NULL;
    }

    long size = get_file_size(fp);
    if (size >= 0) {
        return read_exact_size(fp, size);
    }

    /* If file is not seekable, clear error and fallback */
    clearerr(fp);
    rewind(fp);

    return read_by_chunks(fp);
}







MYLIB_API uint8_t **pareseFile(const char *path){
    FILE *fp = NULL;
    if((fp = fopen(path, "r")) == NULL) return NULL;

    char *fileContent = read_file_all(fp);
    if(fileContent == NULL){
        free(fileContent);
        return NULL;
    }
    
    fclose(fp);


    return NULL;
}

MYLIB_API uint8_t **parseFromOpenFile(const char *fileContent) {
    return NULL;
}
