/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2018 - Francisco Javier Trujillo Mata - fjtrujy
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <compat_ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fileXio_rpc.h>

#define ULLONG_MAX UINT64_C(0xffffffffffffffff)

/* All the functions included in this file either could be:
   - Because the PS2SDK doesn't contains this specific functionality
   - Because the PS2SDK implementation is wrong

   Overrriding these methods here, make that the RetroArch will execute this code
   rather than the code in the linked libraries
 */

int islower(int c)
{
    if ((c < 'a') || (c > 'z'))
        return 0;

    // passed both criteria, so it
    // is a lower case alpha char
    return 1;
}

int tolower(int ch)
{
        if(ch >= 'A' && ch <= 'Z')
                return ('a' + ch - 'A');
        else
                return ch;
}

int toupper(int c)
{
    if (islower(c))
        c -= 32;

    return c;
}

int memcmp(const void *s1, const void *s2, unsigned int length)
{
    const char *a = s1;
    const char *b = s2;

    while (length--) {
        if (*a++ != *b++)
            return 1;
    }

    return 0;
}

void * memcpy (void *dest, const void *src, size_t len)
{
  char *d = dest;
  const char *s = src;
  while (len--)
    *d++ = *s++;
  return dest;
}

void * memset (void *dest, int val, size_t len)
{
  unsigned char *ptr = dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}

int sprintf (char *s, const char *format, ...)
{
  va_list arg;
  int done;
  va_start (arg, format);
  done = vsprintf (s, format, arg);
  va_end (arg);
  return done;
}

char * strcat(char *dest, const char *src)
{
    size_t i,j;
    for (i = 0; dest[i] != '\0'; i++)
        ;
    for (j = 0; src[j] != '\0'; j++)
        dest[i+j] = src[j];
    dest[i+j] = '\0';
    return dest;
}

char *strchr(const char *string, int c)
{
    while (*string) {
        if (*string == c)
            return (char *)string;
        string++;
    }

    if (*string == c)
        return (char *)string;

    return NULL;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == 0)
			return (0);
	return (*(unsigned char *)s1 - *(unsigned char *)--s2);
}

char * strcpy(char *to, const char *from)
{
	char *save = to;

	for (; (*to = *from) != '\0'; ++from, ++to);
	return(save);
}

size_t strcspn(const char *s1, const char *s2)
{
	const char *p, *spanp;
	char c, sc;

	/*
	 * Stop as soon as we find any character from s2.  Note that there
	 * must be a NUL in s2; it suffices to stop when we find that, too.
	 */
	for (p = s1;;) {
		c = *p++;
		spanp = s2;
		do {
			if ((sc = *spanp++) == c)
				return (p - 1 - s1);
		} while (sc != 0);
	}
	/* NOTREACHED */
}

size_t strlen(const char *str)
{
   const char *s;

	for (s = str; *s; ++s)
		;
	return (s - str);
}

char * strncat(char *dst, const char *src, size_t n)
{
   if (n != 0) {
		char *d = dst;
		const char *s = src;

		while (*d != 0)
			d++;
		do {
			if ((*d = *s++) == 0)
				break;
			d++;
		} while (--n != 0);
		*d = 0;
	}
	return (dst);
}

int strncmp(const char *s1, const char *s2, size_t n)
{
   if (n == 0)
		return (0);
	do {
		if (*s1 != *s2++)
			return (*(unsigned char *)s1 - *(unsigned char *)--s2);
		if (*s1++ == 0)
			break;
	} while (--n != 0);
	return (0);
}

char * strncpy(char *dst, const char *src, size_t n)
{
	if (n != 0) {
		char *d = dst;
		const char *s = src;

		do {
			if ((*d++ = *s++) == 0) {
				/* NUL pad the remaining n-1 bytes */
				while (--n != 0)
					*d++ = 0;
				break;
			}
		} while (--n != 0);
	}
	return (dst);
}

char * strpbrk(const char *s1, const char *s2)
{
	const char *scanp;
	int c, sc;

	while ((c = *s1++) != 0) {
		for (scanp = s2; (sc = *scanp++) != 0;)
			if (sc == c)
				return ((char *)(s1 - 1));
	}
	return (NULL);
}

/* Do not link to strrchr() from libc */
char * strrchr(const char *p, int ch)
{
	char *save;

	for (save = NULL;; ++p) {
		if (*p == (char) ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
	/* NOTREACHED */
}

size_t strspn(const char *s1, const char *s2)
{
	const char *p = s1, *spanp;
	char c, sc;

	/*
	 * Skip any characters in s2, excluding the terminating \0.
	 */
cont:
	c = *p++;
	for (spanp = s2; (sc = *spanp++) != 0;)
		if (sc == c)
			goto cont;
	return (p - 1 - s1);
}

char *strstr(const char *string, const char *substring)
{
    char *strpos;

    if (string == 0)
        return 0;

    if (strlen(substring) == 0)
        return (char *)string;

    strpos = (char *)string;

    while (*strpos != 0) {
        if (strncmp(strpos, substring, strlen(substring)) == 0)
            return strpos;

        strpos++;
    }

    return 0;
}

size_t strnlen(const char *str, size_t maxlen)
{
	const char *cp;

	for (cp = str; maxlen != 0 && *cp != '\0'; cp++, maxlen--)
		;

	return (size_t)(cp - str);
}

char *strtok(char *strToken, const char *strDelimit)
{
    static char *start;
    static char *end;

    if (strToken != NULL)
        start = strToken;
    else {
        if (*end == 0)
            return 0;

        start = end;
    }

    if (*start == 0)
        return 0;

    // Strip out any leading delimiters
    while (strchr(strDelimit, *start)) {
        // If a character from the delimiting string
        // then skip past it

        start++;

        if (*start == 0)
            return 0;
    }

    if (*start == 0)
        return 0;

    end = start;

    while (*end != 0) {
        if (strchr(strDelimit, *end)) {
            // if we find a delimiting character
            // before the end of the string, then
            // terminate the token and move the end
            // pointer to the next character
            *end = 0;
            end++;
            return start;
        }
        end++;
    }

    // reached the end of the string before finding a delimiter
    // so dont move on to the next character
    return start;
}

char * strtok_r (char *s, const char *delim, char **save_ptr)
{
  char *end;
  if (s == NULL)
    s = *save_ptr;
  if (*s == '\0')
    {
      *save_ptr = s;
      return NULL;
    }
  /* Scan leading delimiters.  */
  s += strspn (s, delim);
  if (*s == '\0')
    {
      *save_ptr = s;
      return NULL;
    }
  /* Find the end of the token.  */
  end = s + strcspn (s, delim);
  if (*end == '\0')
    {
      *save_ptr = end;
      return s;
    }
  /* Terminate the token and make *SAVE_PTR point past it.  */
  *end = '\0';
  *save_ptr = end + 1;
  return s;
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

float strtof(const char* str, char** endptr)
{
   return (float) strtod(str, endptr);
}

int link(const char *oldpath, const char *newpath)
{
	return fileXioSymlink(oldpath, newpath);
}

int unlink(const char *path)
{
	return fileXioRemove(path);
}

int rename(const char *source, const char *dest)
{
   return fileXioRename(source, dest);
}
