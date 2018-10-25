#ifndef COMPAT_CTYPE_H
#define COMPAT_CTYPE_H

char *strtok_r(char *str, const char *delim, char **saveptr);

unsigned long long strtoull(const char * __restrict nptr, char ** __restrict endptr, int base);

int link(const char *oldpath, const char *newpath);

#endif
