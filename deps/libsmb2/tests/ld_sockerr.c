/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2024 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE

#include <asm/fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <unistd.h>

#include <sys/syscall.h>
#include <dlfcn.h>

int readv_close = -1;

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
        static int call_idx = 0;
        static int (*real_readv)(int fd, const struct iovec *iov, int iovcnt) = NULL;

        if (real_readv == NULL) {
                real_readv = dlsym(RTLD_NEXT, "readv");
                
                /* Close the socket at this call to readv */
                if (getenv("READV_CLOSE") != NULL) {
                        readv_close = atoi(getenv("READV_CLOSE"));
                }
        }
        
        call_idx++;

        if (call_idx == readv_close) {
                /* write some garbage */
                write(fd, &call_idx, sizeof(call_idx));
                errno = EBADF;
                return -1;
        }
        return real_readv(fd, iov, iovcnt);
}

