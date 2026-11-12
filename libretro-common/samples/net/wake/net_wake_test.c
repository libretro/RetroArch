/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <net/net_wake.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(__APPLE__) || defined(__unix__) || defined(__linux__)
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <unistd.h>
static unsigned live,fcntl_calls,fail_fcntl,write_calls,read_calls;
static int descriptors[2],write_error,read_error;
static bool fail_malloc,fail_pipe,read_eof;
static void *wake_malloc(size_t bytes)
{
   void *result;
   if (fail_malloc) return NULL;
   result=malloc(bytes);
   if (result) live++;
   return result;
}
static void wake_free(void *data)
{
   if (data) { assert(live); live--; }
   free(data);
}
static int wake_pipe(int *fds)
{
   int result;
   if (fail_pipe) { errno=EMFILE; return -1; }
   result=pipe(fds);
   if (!result) { descriptors[0]=fds[0]; descriptors[1]=fds[1]; }
   return result;
}
static int wake_fcntl(int fd,int command,...)
{
   va_list args;
   int value=0;
   va_start(args,command);
   if (command==F_SETFL || command==F_SETFD) value=va_arg(args,int);
   va_end(args);
   if (++fcntl_calls==fail_fcntl) { errno=EIO; return -1; }
   return fcntl(fd,command,value);
}
static ssize_t wake_write(int fd,const void *data,size_t size)
{
   write_calls++;
   assert(size==1);
   if (write_error) { errno=write_error; return -1; }
   return write(fd,data,size);
}
static ssize_t wake_read(int fd,void *data,size_t size)
{
   read_calls++;
   assert(size<=256);
   if (read_eof) return 0;
   if (read_error) { errno=read_error; return -1; }
   return read(fd,data,size);
}
#define malloc wake_malloc
#define free wake_free
#define pipe wake_pipe
#define fcntl wake_fcntl
#define write wake_write
#define read wake_read
#endif
#include "../../../net/net_wake.c"
#if defined(__APPLE__) || defined(__unix__) || defined(__linux__)
#undef malloc
#undef free
#undef pipe
#undef fcntl
#undef write
#undef read

static void constructor_failures(void)
{
   unsigned i;
   fail_malloc=true;
   assert(!net_wake_new() && !live);
   fail_malloc=false;
   fail_pipe=true;
   assert(!net_wake_new() && !live);
   fail_pipe=false;
   for (i=1;i<=8;i++)
   {
      fcntl_calls=0; fail_fcntl=i;
      assert(!net_wake_new() && !live && errno==EIO);
      assert(fcntl(descriptors[0],F_GETFD)<0 && errno==EBADF);
      assert(fcntl(descriptors[1],F_GETFD)<0 && errno==EBADF);
   }
   fail_fcntl=0;
}

int main(void)
{
   net_wake_t *wake;
   struct pollfd fds[2];
   int sockets[2];
   char byte='a';
   unsigned i,count,before;
   constructor_failures();
   wake=net_wake_new();
   assert(wake && live==1 && net_wake_read_fd(wake)>=0);
   assert((fcntl(wake->reader,F_GETFL)&O_NONBLOCK) && (fcntl(wake->writer,F_GETFL)&O_NONBLOCK));
   assert((fcntl(wake->reader,F_GETFD)&FD_CLOEXEC) && (fcntl(wake->writer,F_GETFD)&FD_CLOEXEC));
   assert(!socketpair(AF_UNIX,SOCK_STREAM,0,sockets));
   fds[0].fd=net_wake_read_fd(wake); fds[0].events=POLLIN;
   fds[1].fd=sockets[0]; fds[1].events=POLLIN;
   assert(poll(fds,2,0)==0 && net_wake_ack(wake));
   assert(net_wake_notify(wake) && poll(fds,2,0)==1 && (fds[0].revents&POLLIN));
   assert(net_wake_ack(wake) && poll(fds,2,0)==0);
   for (i=0;i<100000;i++) assert(net_wake_notify(wake));
   count=0;
   while (poll(fds,2,0)>0)
   {
      assert((fds[0].revents&POLLIN) && net_wake_ack(wake) && ++count<1000);
   }
   assert(count);
   assert(write(sockets[1],&byte,1)==1 && poll(fds,2,0)==1 && (fds[1].revents&POLLIN));
   assert(read(sockets[0],&byte,1)==1 && poll(fds,2,0)==0);
   fds[1].events=POLLOUT;
   assert(poll(fds,2,0)==1 && (fds[1].revents&POLLOUT));
   fds[1].events=POLLIN;
   assert(net_wake_notify(wake) && write(sockets[1],&byte,1)==1 && poll(fds,2,0)==2);
   assert(net_wake_ack(wake) && read(sockets[0],&byte,1)==1 && poll(fds,2,0)==0);
   before=write_calls; write_error=EAGAIN;
   assert(net_wake_notify(wake) && write_calls==before+1);
   write_error=EINTR; before=write_calls;
   assert(!net_wake_notify(wake) && write_calls==before+1);
   write_error=0;
   before=read_calls; read_error=EINTR;
   assert(net_wake_ack(wake) && read_calls==before+1);
   read_error=EIO;
   assert(!net_wake_ack(wake));
   read_error=0; read_eof=true;
   assert(!net_wake_ack(wake));
   read_eof=false;
   assert(!close(sockets[1]) && poll(fds,2,0)==1 && (fds[1].revents&(POLLIN|POLLHUP)));
   assert(read(sockets[0],&byte,1)==0 && !close(sockets[0]));
   net_wake_free(wake);
   assert(!live && !net_wake_notify(NULL) && !net_wake_ack(NULL) && net_wake_read_fd(NULL)==-1);
   net_wake_free(NULL);
   puts("PASS pollable producer wake: nonblocking flags, bounded ack, socket readiness, saturation and cold rollback");
   return 0;
}
#else
int main(void)
{
   assert(!net_wake_new() && net_wake_read_fd(NULL)==-1 && !net_wake_notify(NULL) && !net_wake_ack(NULL));
   net_wake_free(NULL);
   puts("PASS unsupported pollable-wake admission; POSIX runtime not exercised");
   return 0;
}
#endif
