/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <net/net_wake.h>
#include <stdlib.h>
#if defined(__APPLE__) || defined(__unix__) || defined(__linux__)
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

struct net_wake
{
   int reader,writer;
};

static bool net_wake_configure(int fd)
{
   int flags=fcntl(fd,F_GETFL);
   if (flags<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)
      return false;
   flags=fcntl(fd,F_GETFD);
   return flags>=0 && fcntl(fd,F_SETFD,flags|FD_CLOEXEC)>=0;
}

net_wake_t *net_wake_new(void)
{
   net_wake_t *wake=(net_wake_t*)malloc(sizeof(*wake));
   int descriptors[2];
   int error;
   if (!wake)
      return NULL;
   if (pipe(descriptors)<0)
   {
      error=errno;
      free(wake);
      errno=error;
      return NULL;
   }
   wake->reader=descriptors[0];
   wake->writer=descriptors[1];
   if (net_wake_configure(wake->reader) && net_wake_configure(wake->writer))
      return wake;
   error=errno;
   close(wake->reader);
   close(wake->writer);
   free(wake);
   errno=error;
   return NULL;
}

int net_wake_read_fd(const net_wake_t *wake)
{
   return wake?wake->reader:-1;
}

bool net_wake_notify(net_wake_t *wake)
{
   const char byte=1;
   ssize_t written;
   if (!wake)
      return false;
   written=write(wake->writer,&byte,1);
   return written==1 || (written<0 && (errno==EAGAIN || errno==EWOULDBLOCK));
}

bool net_wake_ack(net_wake_t *wake)
{
   char bytes[256];
   ssize_t count;
   if (!wake)
      return false;
   count=read(wake->reader,bytes,sizeof(bytes));
   return count>0 || (count<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR));
}

void net_wake_free(net_wake_t *wake)
{
   if (!wake)
      return;
   close(wake->reader);
   close(wake->writer);
   free(wake);
}
#else
net_wake_t *net_wake_new(void)
{
   return NULL;
}
int net_wake_read_fd(const net_wake_t *wake)
{
   (void)wake;
   return -1;
}
bool net_wake_notify(net_wake_t *wake)
{
   (void)wake;
   return false;
}
bool net_wake_ack(net_wake_t *wake)
{
   (void)wake;
   return false;
}
void net_wake_free(net_wake_t *wake)
{
   (void)wake;
}
#endif
