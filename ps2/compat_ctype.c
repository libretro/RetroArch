#include <compat_ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fileXio_rpc.h>

#define ULLONG_MAX UINT64_C(0xffffffffffffffff)

char * strtok_r(char *str, const char *delim, char **nextp)
{
    char *ret;

    if (str == NULL) {
        str = *nextp;
    }

    str += strspn(str, delim);

    if (*str == '\0') {
        return NULL;
    }

    ret = str;

    str += strcspn(str, delim);

    if (*str) {
        *str++ = '\0';
    }

    *nextp = str;

    return ret;
}

unsigned long long strtoull(const char * __restrict nptr, char ** __restrict endptr, int base)
{
	const char *s;
	unsigned long long acc;
	char c;
	unsigned long long cutoff;
	int neg, any, cutlim;

	/*
	 * See strtoq for comments as to the logic used.
	 */
	s = nptr;
	do {
		c = *s++;
	} while (isspace((unsigned char)c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else {
		neg = 0;
		if (c == '+')
			c = *s++;
	}
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;
	acc = any = 0;
	if (base < 2 || base > 36)
		goto noconv;

	cutoff = ULLONG_MAX / base;
	cutlim = ULLONG_MAX % base;
	for ( ; ; c = *s++) {
		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'A' && c <= 'Z')
			c -= 'A' - 10;
		else if (c >= 'a' && c <= 'z')
			c -= 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
			any = -1;
		else {
			any = 1;
			acc *= base;
			acc += c;
		}
	}
	if (any < 0) {
		acc = ULLONG_MAX;
		errno = ERANGE;
	} else if (!any) {
noconv:
		errno = EINVAL;
	} else if (neg)
		acc = -acc;
	if (endptr != NULL)
		*endptr = (char *)(any ? s - 1 : nptr);
	return (acc);
}

int link(const char *oldpath, const char *newpath)
{
	return fileXioSymlink(oldpath, newpath);
}
