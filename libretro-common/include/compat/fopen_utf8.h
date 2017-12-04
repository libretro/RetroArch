#ifndef __FOPEN_UTF8_H
#define __FOPEN_UTF8_H

#include <stdio.h>

#ifdef _WIN32
/* defined to error rather than fopen_utf8, to make it clear to everyone reading the code that not worrying about utf16 is fine */
/* TODO: enable */
/* #define fopen (use fopen_utf8 instead) */
FILE* fopen_utf8(const char * filename, const char * mode);
#else
#define fopen_utf8 fopen
#endif
#endif
