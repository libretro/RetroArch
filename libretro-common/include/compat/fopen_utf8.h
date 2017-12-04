#include <stdio.h>
#ifdef _WIN32
#define fopen fopen_utf8
FILE* fopen_utf8(const char * filename, const char * mode);
#endif
