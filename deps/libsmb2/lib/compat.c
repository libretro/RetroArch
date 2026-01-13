/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2020 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#include "compat.h"

#if defined(_WINDOWS) || defined(_XBOX)
#include <errno.h>
#include <stdlib.h>

#ifdef _WINDOWS
#define login_num ENXIO
#define getpid_num() GetCurrentProcessId()
#else
#define login_num 0
#define getpid_num() 0	
#endif
#define smb2_random rand
#define smb2_srandom srand

#ifdef _XBOX
int gethostname(char *name, size_t len)
{
#ifdef XBOX_PLATFORM
	    strncpy(name, "XBOX", len);
#else
        strncpy(name, "XBOX_360", len);
#endif
		return 0;
}
#endif

#endif

#ifdef __DREAMCAST__
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#define login_num ENXIO
#endif

#ifdef ESP_PLATFORM

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <lwip/sockets.h>
#include <sys/uio.h>
#include <errno.h>

#define login_num ENXIO
#define smb2_random esp_random
#define smb2_srandom(seed)

#endif

#if defined(__amigaos4__) || defined(__AMIGA__) || defined(__AROS__)
#ifndef __amigaos4__
#define NEED_READV
#define NEED_WRITEV
#include <proto/bsdsocket.h>
#define read(fd, buf, count) recv(fd, buf, count, 0)
#define write(fd, buf, count) send(fd, buf, count, 0)
#ifndef __AROS__
#define select(nfds, readfds, writefds, exceptfds, timeout) WaitSelect(nfds, readfds, writefds, exceptfds, timeout, NULL)
#endif
#ifdef libnix
StdFileDes *_lx_fhfromfd(int d) { return NULL; }
struct MinList __filelist = { (struct MinNode *) &__filelist.mlh_Tail, NULL, (struct MinNode *) &__filelist.mlh_Head };
#endif
#endif

#define login_num ENXIO

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#endif

#ifdef PICO_PLATFORM

#include "lwip/def.h"
#include <unistd.h>
#include <lwip/sockets.h>

#define login_num 1 

#endif /* PICO_PLATFORM */

#ifdef __PS2__

#ifdef _EE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#else
#include <sysclib.h>
#include <thbase.h>
#include <stdio.h>
#include <stdarg.h>
#endif
#include <errno.h>

#define login_num ENXIO

#ifdef _IOP
#define getpid_num() 27

static unsigned long int next = 1; 

int gethostname(char *name, size_t len)
{
        strncpy(name, "PS2", len);
        return 0;
}

time_t time(time_t *tloc)
{
        u32 sec, usec;
        iop_sys_clock_t sys_clock;

        GetSystemTime(&sys_clock);
        SysClock2USec(&sys_clock, &sec, &usec);

        return sec;
}

int asprintf(char **strp, const char *fmt, ...)
{
        int len;
        char *str;
        va_list args;        

        va_start(args, fmt);
        str = malloc(256);
        len = sprintf(str, fmt, args);
        va_end(args);
        *strp = str;
        return len;
}

int errno;

int iop_connect(int sockfd, struct sockaddr *addr, socklen_t addrlen)
{
        int rc;
        int err = 0;
        socklen_t err_size = sizeof(err);

        if ((rc = lwip_connect(sockfd, addr, addrlen)) < 0) {
                if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
			       (char *)&err, &err_size) != 0 || err != 0) {
                        errno = err;
                }
        }

        return rc;
}

#endif

#endif /* __PS2__ */

#ifdef __ANDROID__
/* getlogin_r() was added in API 28 */
#if __ANDROID_API__ < 28
#include <errno.h>
#define NEED_GETLOGIN_R
#define login_num ENXIO
#endif
#endif

#ifdef PS3_PPU_PLATFORM

#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>

#define login_num ENXIO
#define smb2_random rand
#define smb2_srandom srand

#endif /* PS3_PPU_PLATFORM */

#ifdef __vita__
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define login_num ENXIO

#endif

#if defined(__SWITCH__) || defined(__3DS__) || defined(__wii__) || defined(__gamecube__) || defined(__WIIU__) || defined(__NDS__)

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <alloca.h>
#include <stdio.h>
#if !defined(__wii__) && !defined(__gamecube__)
#include <sys/socket.h>
#endif
#if defined(__NDS__)
#include <netinet/in.h>
#endif
#if defined(__SWITCH__)
#include <switch/types.h>
#elif defined(__3DS__)
#include <3ds/types.h>	
#elif defined(__wii__) || defined(__gamecube__)
#include <gctypes.h>
#elif defined(__WIIU__)
#include <wut_types.h>
#elif defined(__NDS__)
#include <mm_types.h>
#endif

#define login_num ENXIO

#if defined(__wii__) || defined(__gamecube__)
s32 getsockopt(int sockfd, int level, int optname, void *optval,
socklen_t *optlen)
{
#ifdef __gamecube__
         return net_getsockopt(sockfd, level, optname, optval, (socklen_t)optlen);
#else
	 return 0;
#endif


}
#endif

#endif /* __SWITCH__ */

#ifdef NEED_GETADDRINFO
int smb2_getaddrinfo(const char *node, const char*service,
                const struct addrinfo *hints,
                struct addrinfo **res)
{
        struct sockaddr_in *sin;
#if defined(__amigaos4__) || defined(__AMIGA__) || defined(__AROS__)
        struct hostent *host;
        int i, ip[4];
#endif

#if defined(__amigaos4__) || defined(__AMIGA__) || defined(__AROS__)
        sin = calloc(1, sizeof(struct sockaddr_in));
#else
        sin = malloc(sizeof(struct sockaddr_in));
#endif
#if !defined(_XBOX) && !defined(__NDS__) && !defined(__USE_WINSOCK__)
        sin->sin_len = sizeof(struct sockaddr_in);
#endif
        sin->sin_family=AF_INET;

#if defined(__amigaos4__) || defined(__AMIGA__) || defined(__AROS__)
        /* Some error checking would be nice */
        if (sscanf(node, "%d.%d.%d.%d", ip, ip+1, ip+2, ip+3) == 4) {
                for (i = 0; i < 4; i++) {
                        ((char *)&sin->sin_addr.s_addr)[i] = ip[i];
                }
        } else {
                host = gethostbyname(node);
                if (host == NULL) {
                        return -1;
                }
                if (host->h_addrtype != AF_INET) {
                        return -2;
                }
                memcpy(&sin->sin_addr.s_addr, host->h_addr, 4);
        }

        sin->sin_port=0;
        if (service) {
                sin->sin_port=htons(atoi(service));
        }

        *res = calloc(1, sizeof(struct addrinfo));
#else
        /* Some error checking would be nice */
        sin->sin_addr.s_addr = inet_addr(node);

        sin->sin_port=0;
        if (service) {
                sin->sin_port=htons(atoi(service));
        } 

        *res = malloc(sizeof(struct addrinfo));
        memset(*res, 0, sizeof(struct addrinfo));
#endif
        (*res)->ai_family = AF_INET;
        (*res)->ai_addrlen = sizeof(struct sockaddr_in);
        (*res)->ai_addr = (struct sockaddr *)sin;

        return 0;
}
#endif

#ifdef NEED_FREEADDRINFO
void smb2_freeaddrinfo(struct addrinfo *res)
{
        free(res->ai_addr);
        free(res);
}
#endif

#ifdef NEED_RANDOM
#ifdef ESP_PLATFORM
long random(void)
#else
int random(void)
#endif
{ 
#ifdef _IOP
    next = next * 1103515245 + 12345; 
    return (unsigned int)(next/65536) % 32768; 
#else
    return smb2_random();
#endif
} 
#endif

#ifdef NEED_SRANDOM
void srandom(unsigned int seed) 
{ 
#ifdef _IOP
    next = seed; 
#else
    smb2_srandom(seed);
#endif
}
#endif

#ifdef NEED_GETPID
int getpid()
{
     return getpid_num();
};
#endif

#ifdef NEED_GETLOGIN_R
int getlogin_r(char *buf, size_t size)
{
     return login_num;
}
#endif

#ifdef NEED_WRITEV
ssize_t writev(t_socket fd, const struct iovec* vector, int count)
{
        /* Find the total number of bytes to be written.  */
        size_t bytes = 0;
        int i;
        char *buffer;
        size_t to_copy;
        char *bp;
	ssize_t bytes_written;
        for (i = 0; i < count; ++i) {
                /* Check for ssize_t overflow.  */
                if (((ssize_t)-1) - bytes < vector[i].iov_len) {
                        errno = EINVAL;
                        return -1;
                }
                bytes += vector[i].iov_len;
        }

        buffer = (char *)malloc(bytes);
        if (buffer == NULL)
                /* XXX I don't know whether it is acceptable to try writing
                the data in chunks.  Probably not so we just fail here.  */
                return -1;
        /* Copy the data into BUFFER.  */
        to_copy = bytes;
        bp = buffer;
        for (i = 0; i < count; ++i) {
                size_t copy = (vector[i].iov_len < to_copy) ? vector[i].iov_len : to_copy;

                memcpy((void *)bp, (void *)vector[i].iov_base, copy);
                
		bp += copy;

                to_copy -= copy;
                if (to_copy == 0)
                        break;
        }
        bytes_written = write((int)fd, buffer, bytes);
        free(buffer);
        return bytes_written;
}
#endif

#ifdef NEED_READV
ssize_t readv(t_socket fd, const struct iovec* vector, int count)
{
        /* Find the total number of bytes to be read.  */
        size_t bytes = 0;
        int i;
        char *buffer;
        ssize_t bytes_read;
        char *bp;
        for (i = 0; i < count; ++i)
        {
                /* Check for ssize_t overflow.  */
                if (((ssize_t)-1) - bytes < vector[i].iov_len) {
                        errno = EINVAL;
                        return -1;
                }
                bytes += vector[i].iov_len;
        }
        buffer = (char *)malloc(bytes);
        if (buffer == NULL)
                return -1;

        /* Read the data.  */
        bytes_read = read((int)fd, buffer, bytes);
        if (bytes_read < 0) {
                free(buffer);
                return -1;
        }

        /* Copy the data from BUFFER into the memory specified by VECTOR.  */
        bytes = bytes_read;
	bp = buffer;
        for (i = 0; i < count; ++i) {
            size_t copy = (vector[i].iov_len < bytes) ? vector[i].iov_len : bytes;

            memcpy((void *)vector[i].iov_base, (void *)bp, copy);	
	    bp += copy;
	    bytes -= copy;
	    if (bytes == 0)
	    break;
	}

        free(buffer);
        return bytes_read;
}
#endif

#ifdef NEED_POLL
int poll(struct pollfd *fds, unsigned int nfds, int timo)
{
        struct timeval timeout, *toptr;
        fd_set ifds, ofds, efds, *ip, *op;
        unsigned int i, maxfd = 0;
        int  rc;

        FD_ZERO(&ifds);
        FD_ZERO(&ofds);
        FD_ZERO(&efds);
#if defined(__amigaos4__) || defined(__AMIGA__) || defined(__AROS__)
        op = ip = 0;
        for (i = 0; i < nfds; ++i) {
                int fd = fds[i].fd;
                fds[i].revents = 0;
                if (!SMB2_VALID_SOCKET(fd))
                        continue;
                if(fds[i].events & (POLLIN|POLLPRI)) {
                        ip = &ifds;
                        FD_SET(fd, ip);
                }
                if(fds[i].events & POLLOUT)  {
                        op = &ofds;
                        FD_SET(fd, op);
                }
                FD_SET(fd, &efds);
                if (fd > maxfd) {
                        maxfd = fd;
                }
        }
#else
        for (i = 0, op = ip = 0; i < nfds; ++i) {
                fds[i].revents = 0;
                if(fds[i].events & (POLLIN|POLLPRI)) {
                        ip = &ifds;
                        FD_SET(fds[i].fd, ip);
                }
                if(fds[i].events & POLLOUT)  {
                        op = &ofds;
                        FD_SET(fds[i].fd, op);
                }
                FD_SET(fds[i].fd, &efds);
                if (fds[i].fd > (int)maxfd) {
                    maxfd = fds[i].fd;
		}
        } 
#endif

#if defined(__amigaos4__) || defined(__AMIGA__) || defined(__AROS__)
        if(timo >= 0) {
                toptr = &timeout;
                timeout.tv_sec = (unsigned)timo / 1000;
                timeout.tv_usec = ((unsigned)timo % 1000) * 1000;
        }

#else
        if(timo < 0) {
                toptr = NULL;
        } else {
                toptr = &timeout;
                timeout.tv_sec = timo / 1000;
                timeout.tv_usec = (timo - timeout.tv_sec * 1000) * 1000;       
        }
#endif

        rc = select(maxfd + 1, ip, op, &efds, toptr);

        if(rc <= 0)
                return rc;

#if defined(__amigaos4__) || defined(__AMIGA__) || defined(__AROS__)
        rc = 0;
        for (i = 0; i < nfds; ++i) {
                int fd = fds[i].fd;
                short events = fds[i].events;
                short revents = 0;
                if (!SMB2_VALID_SOCKET(fd))
                        continue;
                if(events & (POLLIN|POLLPRI) && FD_ISSET(fd, &ifds))
                        revents |= POLLIN;
                if(events & POLLOUT && FD_ISSET(fd, &ofds))
                        revents |= POLLOUT;
                if(FD_ISSET(fd, &efds))
                        revents |= POLLHUP;
                if (revents) {
                        fds[i].revents = revents;
                        rc++;
                }
        }
#else
        if(rc > 0)  {
                for (i = 0; i < nfds; ++i) {
                        int fd = fds[i].fd;
                        if(fds[i].events & (POLLIN|POLLPRI) && FD_ISSET(fd, &ifds))
                                fds[i].revents |= POLLIN;
                        if(fds[i].events & POLLOUT && FD_ISSET(fd, &ofds))
                                fds[i].revents |= POLLOUT;
                        if(FD_ISSET(fd, &efds))
                                fds[i].revents |= POLLHUP;
                }
        }
#endif
        return rc;
}
#endif

#ifdef NEED_STRDUP
char *strdup(const char *s)
{
        char *str;
        int len;

        len = strlen(s) + 1;
        str = malloc(len);
        if (str == NULL) {
#ifndef _IOP
                errno = ENOMEM;
#endif /* !_IOP */
                return NULL;
        }
        /* len already includes the NULL terminator */
        memcpy(str, s, len);
        return str;
}
#endif /* NEED_STRDUP */

#ifdef NEED_BE64TOH
long long int be64toh(long long int x)
{
  long long int val;

  val = ntohl(x & 0xffffffff);
  val <<= 32;
  val ^= ntohl(x >> 32);
  return val;
}
#endif /* NEED_BE64TOH */
