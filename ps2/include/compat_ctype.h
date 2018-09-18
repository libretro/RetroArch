#ifndef COMPAT_CTYPE_H
#define COMPAT_CTYPE_H

unsigned long long strtoull(const char * __restrict nptr, char ** __restrict endptr, int base);
char * strtok_r(char *str, const char *delim, char **nextp);

int link(const char *oldpath, const char *newpath);

#endif
