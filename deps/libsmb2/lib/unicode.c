/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef STDC_HEADERS
#include <stddef.h>
#endif

#include "compat.h"

#include "portable-endian.h"

#include <smb2.h>
#include <libsmb2.h>
#include "libsmb2-private.h"

/* Count number of leading 1 bits in the char */
static int
l1(char c)
{
        int i = 0;
        while (c & 0x80) {
                i++;
                c <<= 1;
        }
        return i;
}

/* Validates that utf8 points to a valid utf8 codepoint.
 * Will update **utf8 to point at the next character in the string.
 * return 1 if the encoding is valid and requires one UTF-16 code unit,
 * 2 if the encoding is valid and requires two UTF-16 code units
 * -1 if it's invalid.
 * If the encoding is valid the codepoint will be returned in *cp.
 */
static int
validate_utf8_cp(const char **utf8, uint16_t *ret)
{
        int c = *(*utf8)++;
        int l, l_tmp;
        uint32_t cp;
        l = l_tmp = l1(c);
 
        switch (l) {
        case 0:
                /* 7-bit ascii is always ok */
                *ret = c & 0x7f;
                return 1;
        case 1:
                /* 10.. .... can never start a new codepoint */
                return -1;
        case 2:
        case 3:
        case 4:
                cp = c & (0x7f >> l);
                /* 2, 3 and 4 byte sequences must always be followed by exactly
                 * 1, 2 or 3 chars matching 10.. ....
                 */
                while(--l_tmp) {
                        c = *(*utf8)++;
                        if (l1(c) != 1) {
                                return -1;
                        }
                        cp <<= 6;
                        cp |= (c & 0x3f);
                }

                /* Check for overlong sequences */
                switch (l) {
                case 2:
                        if (cp < 0x80) return -1;
                        break;
                case 3:
                        if (cp < 0x800) return -1;
                        break;
                case 4:
                        if (cp < 0x10000) return -1;
                        break;
                default: break;
                }

                /* Write the code point in either one or two UTC-16 code units */
                if (cp < 0xd800 || (cp - 0xe000) < 0x2000) {
                        /* Single UTF-16 code unit */
                        *ret = cp;
                        return 1;
                } else if (cp < 0xe000) {
                        /* invalid unicode range */
                        return -1;
                } else if (cp < 0x110000) {
                        cp -= 0x10000;
                        *ret = 0xd800 | (cp >> 10);
                        *(ret+1) = 0xdc00 | (cp & 0x3ff) ;
                        return 2;
                } else {
                        /* invalid unicode range */
                        return -1;
                }
        }
        return -1;
}

/* Validate that the given string is properly formatted UTF8.
 * Returns >=0 if valid UTF8 and -1 if not.
 */
static int
validate_utf8_str(const char *utf8)
{
        const char *u = utf8;
        int i = 0;
        int cp_length;
        uint16_t cp[2];

        while (*u) {
                cp_length = validate_utf8_cp(&u, cp);
                if (cp_length < 0) {
                        return -1;
                }
                i += cp_length;
        }
        return i;
}

/* Convert a UTF8 string into UTF-16LE */
struct smb2_utf16 *
smb2_utf8_to_utf16(const char *utf8)
{
        struct smb2_utf16 *utf16;
        int i, len;

        len = validate_utf8_str(utf8);
        if (len < 0) {
                return NULL;
        }

        utf16 = (struct smb2_utf16 *)(malloc(offsetof(struct smb2_utf16, val) + 2 * len));
        if (utf16 == NULL) {
                return NULL;
        }

        utf16->len = len;
        i = 0;
        while (i < len) {
                switch(validate_utf8_cp(&utf8, &utf16->val[i])) {
                case 1:
                    utf16->val[i] = htole16(utf16->val[i]);
                    i += 1;
                    break;
                case 2:
                    utf16->val[i] = htole16(utf16->val[i]);
                    utf16->val[i+1] = htole16(utf16->val[i+1]);
                    i += 2;
                    break;
                default:
                    /* Won't happen since we wouldn't have gotten here if the UTF-8 string was invalid */
                    break;
                }
        }

        return utf16;
}

static int
utf16_size(const uint16_t *utf16, size_t utf16_len)
{
        int length = 0;
        const uint16_t *utf16_end = utf16 + utf16_len;
        while (utf16 < utf16_end) {
                uint32_t code = le16toh(*utf16++);

                if (code < 0x80) {
                        length += 1; /* One UTF-16 code unit maps to one UTF-8 code unit */
                } else if (code < 0x800) {
                        length += 2; /* One UTF-16 code unit maps to two UTF-8 code units */
                } else if (code < 0xd800 || code - 0xe000 < 0x2000) {
                        length += 3;
                } else if (code < 0xdc00) { /* Surrogate pair */
                        uint32_t trail;
                        if (utf16 == utf16_end) { /* It's possible the stream ends with a leading code unit, which is an error */
                                return length + 3; /* Replacement char */
                        }

                        trail = le16toh(*utf16);
                        if (trail - 0xdc00 < 0x400) { /* Check that 0xdc00 <= trail < 0xe000 */
                                code = 0x10000 + ((code & 0x3ff) << 10) + (trail & 0x3ff);
                                if (code < 0x10000) {
                                        length += 3; /* Two UTF-16 code units map to three UTF-8 code units */
                                } else {
                                        length += 4; /* Two UTF-16 code units map to four UTF-8 code units */
                                }
                                utf16++;
                        } else { /* Invalid trailing code unit. It's still valid on its own though so only the first unit gets replaced */
                                length += 3; /* Replacement char */
                        }
                } else { /* 0xdc00 <= code < 0xe00, which makes code a trailing code unit without a leading one, which is invalid */
                        length += 3; /* Replacement char */
                }
        }

        return length;
}

/*
 * Convert a UTF-16LE string into UTF8
 */
const char *
smb2_utf16_to_utf8(const uint16_t *utf16, size_t utf16_len)
{
        int utf8_len = 1;
        char *str, *tmp;
        const uint16_t *utf16_end;
        
        /* How many bytes do we need for utf8 ? */
        utf8_len += utf16_size(utf16, utf16_len);
        str = tmp = (char*)malloc(utf8_len);
        if (str == NULL) {
                return NULL;
        }
        str[utf8_len - 1] = 0;

        utf16_end = utf16 + utf16_len;
        while (utf16 < utf16_end) {
                uint32_t code = le16toh(*utf16++);

                if (code < 0x80) {
                        *tmp++ = code; /* One UTF-16 code unit maps to one UTF-8 code unit */
                } else if (code < 0x800) {
                        *tmp++ = 0xc0 |  (code >> 6);         /* One UTF-16 code unit maps to two UTF-8 code units */
                        *tmp++ = 0x80 | ((code     ) & 0x3f);
                } else if (code < 0xD800 || code - 0xe000 < 0x2000) {
                        *tmp++ = 0xe0 |  (code >> 12);         /* All other values where we only have one UTF-16 code unit map to 3 UTF-8 code units */
                        *tmp++ = 0x80 | ((code >>  6) & 0x3f);
                        *tmp++ = 0x80 | ((code      ) & 0x3f);
                } else if (code < 0xdc00) { /* Surrogate pair */
                        uint32_t trail;
                        if (utf16 == utf16_end) { /* It's possible the stream ends with a leading code unit, which is an error */
                                *tmp++ = 0xef; *tmp++ = 0xbf; *tmp++ = 0xbd; /* Replacement char */
                                return str;
                        }

                        trail = le16toh(*utf16);
                        if (trail - 0xdc00 < 0x400) { /* Check that 0xdc00 <= trail < 0xe000 */
                                code = 0x10000 + ((code & 0x3ff) << 10) + (trail & 0x3ff);
                                if (code < 0x10000) {
                                        *tmp++ = 0xe0 |  (code >> 12);
                                        *tmp++ = 0x80 | ((code >>  6) & 0x3f);
                                        *tmp++ = 0x80 | ((code      ) & 0x3f);
                                } else {
                                        *tmp++ = 0xF0 | (code >> 18);
                                        *tmp++ = 0x80 | ((code >> 12) & 0x3F);
                                        *tmp++ = 0x80 | ((code >> 6) & 0x3F);
                                        *tmp++ = 0x80 | (code & 0x3F);
                                }
                                utf16++;
                        } else {
                                /* Invalid trailing code unit. It's still valid on its own though so only the first unit gets replaced */
                                *tmp++ = 0xef; *tmp++ = 0xbf; *tmp++ = 0xbd; /* Replacement char */
                        }
                } else {
                        /* 0xdc00 <= code < 0xe00, which makes code a trailing code unit without a leading one, which is invalid */
                        *tmp++ = 0xef; *tmp++ = 0xbf; *tmp++ = 0xbd; /* Replacement char */
                }
        }

        return str;
}
