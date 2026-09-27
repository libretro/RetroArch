/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2017 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#ifdef HAVE_SYS_UNISTD_H
#include <sys/unistd.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "compat.h"

#include <smb2.h>
#include <libsmb2.h>
#include "libsmb2-private.h"

/* Statement expressions and typeof are GNU extensions; the plain
 * pointer arithmetic below is what the _MSC_VER path already did
 * by hand and is valid C89 everywhere.
 */
#define container_of(ptr, type, member) \
        ((type *)(void *)((char *)(ptr) - offsetof(type, member)))

struct smb2_alloc_entry {
        struct smb2_alloc_entry *next;
#if 0 /* UNUSED. */
        size_t len;
#endif
        char buf[1];
};

struct smb2_alloc_header {
        struct smb2_alloc_entry *mem;
        char buf[1];
};

void *
smb2_alloc_init(struct smb2_context *smb2, size_t size)
{
        struct smb2_alloc_header *ptr;

        size += offsetof(struct smb2_alloc_header, buf);

        ptr = calloc(size, 1);
        if (ptr == NULL) {
                return NULL;
        }

        return &ptr->buf[0];
}

void *
smb2_alloc_data(struct smb2_context *smb2, void *memctx, size_t size)
{
        struct smb2_alloc_header *hdr;
        struct smb2_alloc_entry *ptr;

        size += offsetof(struct smb2_alloc_entry, buf);

        ptr = calloc(size, 1);
        if (ptr == NULL) {
                smb2_set_error(smb2, "Failed to alloc %zu bytes", size);
                return NULL;
        }

        hdr = container_of(memctx, struct smb2_alloc_header, buf);

        ptr->next = hdr->mem;
        hdr->mem = ptr;

        return &ptr->buf[0];
}

void
smb2_free_data(struct smb2_context *smb2, void *ptr)
{
        struct smb2_alloc_header *hdr;
        struct smb2_alloc_entry *ent;

        if (ptr == NULL) {
                return;
        }

        hdr = container_of(ptr, struct smb2_alloc_header, buf);

        while ((ent = hdr->mem)) {
                hdr->mem = ent->next;
                free(ent);
        }
        free(hdr);
}
