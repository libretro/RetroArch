/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2018 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#ifdef STDC_HEADERS
#include <stddef.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "compat.h"

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-private.h"

int
smb2_decode_reparse_data_buffer(struct smb2_context *smb2,
                                void *memctx,
                                struct smb2_reparse_data_buffer *rp,
                                struct smb2_iovec *vec)
{
        uint16_t suboffset, sublen, printoffset, printlen;
        const char *tmp;

        if (vec->len < 8) {
                return -1;
        }

        smb2_get_uint32(vec, 0, &rp->reparse_tag);
        smb2_get_uint16(vec, 4, &rp->reparse_data_length);

        if ((uint16_t)vec->len < rp->reparse_data_length + 8) {
                return -1;
        }
        switch (rp->reparse_tag) {
        case SMB2_REPARSE_TAG_SYMLINK:
                if (vec->len < 20) {
                        return -1;
                }
                smb2_get_uint32(vec, 16, &rp->symlink.flags);

                smb2_get_uint16(vec, 8, &suboffset);
                smb2_get_uint16(vec, 10, &sublen);
                if (suboffset + sublen + 12 > rp->reparse_data_length) {
                        return -1;
                }

                tmp = smb2_utf16_to_utf8((uint16_t *)(void *)(&vec->buf[suboffset + 20]),
                                   sublen / 2);
                rp->symlink.subname = smb2_alloc_data(smb2, rp,
                                                      strlen(tmp) + 1);
                if (rp->symlink.subname == NULL) {
                        free(discard_const(tmp));
                        return -1;
                }
                strcpy(rp->symlink.subname, tmp);
                free(discard_const(tmp));

                smb2_get_uint16(vec, 12, &printoffset);
                smb2_get_uint16(vec, 14, &printlen);
                if (printoffset + printlen + 12 > rp->reparse_data_length) {
                        return -1;
                }
                tmp = smb2_utf16_to_utf8((uint16_t *)(void *)(&vec->buf[printoffset + 20]),
                                   printlen / 2);
                rp->symlink.printname = smb2_alloc_data(smb2, rp,
                                                        strlen(tmp) + 1);
                if (rp->symlink.printname == NULL) {
                        free(discard_const(tmp));
                        return -1;
                }
                strcpy(rp->symlink.printname, tmp);
                free(discard_const(tmp));
        }

        return 0;
}
