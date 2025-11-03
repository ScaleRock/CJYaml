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

typedef enum { INT, FLOAT, STRING} YAMLSquare_t;
typedef struct  YAMLSquare {
    char *name;
    void *data;

    YAMLSquare_t type;
}YAMLSquare;


typedef struct YAMLTrea{
    YAMLSquare **ValueList;
    struct YAMLTrea **SubNodes;

    uint64_t SubNodesCount;
    uint64_t ValueListCount;
} YAMLTrea;



#ifdef __cplusplus
}
#endif


#endif