/* Wii: BSD sockets over IOS's IP stack, /dev/net/ip/top.
 *
 * Requests (WiiBrew, and Dolphin's IOS emulation): socket, bind,
 * connect and friends are ioctls with the socket number first in the
 * input; send and receive are ioctlvs with the data and a parameter
 * block.  Errors come back negative in IOS's own numbering, mapped
 * here onto errno.  Addresses are BSD sockaddr_in, 8 bytes.
 *
 * Bring-up: the network configuration's link status (/dev/net/ncd/
 * manage), the socket startup in /dev/net/kd/request, the interface
 * on ip/top, then the host address until DHCP has given one.
 *
 * Socket descriptors are C library ones on a "net" device, so close(),
 * read() and write() reach here; IOS reads and writes bounce through
 * 32-byte aligned buffers of their own. */

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/iosupport.h>
#include <sys/select.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>

#include <gekko/ios.h>
#include <gekko/net.h>
#include <gekko/thread.h>

enum
{
   SO_ACCEPT = 1, SO_BIND, SO_CLOSE, SO_CONNECT, SO_FCNTL, SO_GETPEERNAME,
   SO_GETSOCKNAME, SO_GETSOCKOPT, SO_SETSOCKOPT, SO_LISTEN, SO_POLL,
   SO_RECVFROM, SO_SENDTO, SO_SHUTDOWN, SO_SOCKET, SO_GETHOSTID,
   SO_GETHOSTBYNAME, SO_INITINTERFACE = 0x1f
};

#define KD_STARTUP_SOCKET 0x06
#define NCD_LINK_STATUS   0x07

#define IOS_F_GETFL    3
#define IOS_F_SETFL    4
#define IOS_O_NONBLOCK 4

#define HOSTENT_SIZE   0x460
#define HOSTENT_ADDRS  0x110
#define HOSTENT_PTRS   0x340
#define HOSTENT_MAX    8

#define CHUNK          16384   /* most one send or receive moves */

/* IOS error codes, in order from 1. */
static const unsigned char ios_errno[] = {
   E2BIG, EACCES, EADDRINUSE, EADDRNOTAVAIL, EAFNOSUPPORT, EAGAIN,
   EALREADY, EBADF, EBADMSG, EBUSY, ECANCELED, ECHILD, ECONNABORTED,
   ECONNREFUSED, ECONNRESET, EDEADLK, EDESTADDRREQ, EDOM, EDQUOT, EEXIST,
   EFAULT, EFBIG, EHOSTUNREACH, EIDRM, EILSEQ, EINPROGRESS, EINTR, EINVAL,
   EIO, EISCONN, EISDIR, ELOOP, EMFILE, EMLINK, EMSGSIZE, EMULTIHOP,
   ENAMETOOLONG, ENETDOWN, ENETRESET, ENETUNREACH, ENFILE, ENOBUFS,
   ENODATA, ENODEV, ENOENT, ENOEXEC, ENOLCK, ENOLINK, ENOMEM, ENOMSG,
   ENOPROTOOPT, ENOSPC, ENOSR, ENOSTR, ENOSYS, ENOTCONN, ENOTDIR,
   ENOTEMPTY, ENOTSOCK, ENOTSUP, ENOTTY, ENXIO, EOPNOTSUPP, EOVERFLOW,
   EPERM, EPIPE, EPROTO, EPROTONOSUPPORT, EPROTOTYPE, ERANGE, EROFS,
   ESPIPE, ESRCH, ESTALE, ETIME, ETIMEDOUT, ETXTBSY, EXDEV
};

static gk_mutex_t  net_lock = GK_MUTEX_INIT;
static int32_t     top = -1;
static int         device = -1;
static uint32_t    address;

/* ---- errors and buffers ---- */

static int fail(int err)
{
   errno = err;
   return -1;
}

/* A negative IOS result as -1 and errno, else the result. */
static int result(int32_t ret)
{
   if (ret >= 0)
      return ret;
   if (-ret <= (int32_t)sizeof(ios_errno))
      return fail(ios_errno[-ret - 1]);
   return fail(EIO);
}

/* Zeroed, 32-byte aligned and sized: nothing else shares its lines. */
static void *buf_get(size_t len)
{
   size_t n = (len + 31) & ~(size_t)31;
   void  *p = memalign(32, n ? n : 32);
   if (p)
      memset(p, 0, n ? n : 32);
   return p;
}

static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static uint32_t get32(const uint8_t *p)   { uint32_t v; memcpy(&v, p, 4); return v; }

/* ---- descriptors ---- */

static int dev_close(struct _reent *r, void *fs);
static ssize_t dev_write(struct _reent *r, void *fs, const char *p, size_t n);
static ssize_t dev_read(struct _reent *r, void *fs, char *p, size_t n);

static devoptab_t net_dev;

/* The IOS socket behind a descriptor, or -1 with errno set. */
static int32_t sock_of(int fd)
{
   __handle *h = __get_handle(fd);
   if (!h)
      return fail(EBADF);
   if (device < 0 || (int)h->device != device)
      return fail(ENOTSOCK);
   return *(int32_t*)h->fileStruct;
}

/* ---- bring-up ---- */

static int32_t ioctl_simple(uint32_t cmd, const void *in, uint32_t in_len,
      void *out, uint32_t out_len)
{
   return gk_ios_ioctl(top, cmd, in, in_len, out, out_len);
}

static int bring_up(unsigned timeout_ms)
{
   uint8_t *b = (uint8_t*)buf_get(64);
   int32_t  fd;
   unsigned waited;
   if (!b)
      return -ENOMEM;
   /* Link status first, as the system does; the answer is not needed. */
   if ((fd = gk_ios_open("/dev/net/ncd/manage", 0)) >= 0)
   {
      gk_ios_vec_t v;
      v.data = b;
      v.len  = 32;
      gk_ios_ioctlv(fd, NCD_LINK_STATUS, 0, 1, &v);
      gk_ios_close(fd);
   }
   if ((fd = gk_ios_open("/dev/net/kd/request", 0)) >= 0)
   {
      gk_ios_ioctl(fd, KD_STARTUP_SOCKET, NULL, 0, b, 32);
      gk_ios_close(fd);
   }
   free(b);
   if (top < 0 && (top = gk_ios_open("/dev/net/ip/top", 0)) < 0)
   {
      top = -1;
      return -ENETDOWN;
   }
   ioctl_simple(SO_INITINTERFACE, NULL, 0, NULL, 0);
   for (waited = 0; ; waited += 100)
   {
      int32_t ip = ioctl_simple(SO_GETHOSTID, NULL, 0, NULL, 0);
      if (ip != 0 && ip != -1)
      {
         address = (uint32_t)ip;
         return 0;
      }
      if (waited >= timeout_ms)
         return -ETIMEDOUT;
      gk_sleep_us(100000);
   }
}

int gk_net_init(unsigned timeout_ms)
{
   int ret = 0;
   gk_mutex_lock(&net_lock);
   if (device < 0)
   {
      net_dev.name       = "net";
      net_dev.structSize = sizeof(int32_t);
      net_dev.close_r    = dev_close;
      net_dev.write_r    = dev_write;
      net_dev.read_r     = dev_read;
      if ((device = AddDevice(&net_dev)) < 0)
         ret = -ENFILE;
   }
   if (!ret && !address)
      ret = bring_up(timeout_ms);
   gk_mutex_unlock(&net_lock);
   return ret;
}

uint32_t gk_net_address(void)
{
   return address;
}

long gethostid(void)
{
   return (long)address;
}

static int up(void)
{
   int ret = address ? 0 : gk_net_init(10000);
   return ret ? fail(-ret) : 0;
}

/* ---- sockets ---- */

static int32_t sock_close(int32_t s)
{
   uint8_t *b = (uint8_t*)buf_get(4);
   int32_t  ret;
   if (!b)
      return GK_IOS_ENOMEM;
   put32(b, (uint32_t)s);
   ret = ioctl_simple(SO_CLOSE, b, 4, NULL, 0);
   free(b);
   return ret;
}

int socket(int domain, int type, int protocol)
{
   uint8_t *b;
   int32_t  s;
   int      fd;
   if (up())
      return -1;
   if (!(b = (uint8_t*)buf_get(12)))
      return fail(ENOMEM);
   put32(b,     (uint32_t)domain);
   put32(b + 4, (uint32_t)type);
   put32(b + 8, (uint32_t)protocol);
   s = ioctl_simple(SO_SOCKET, b, 12, NULL, 0);
   free(b);
   if (s < 0)
      return result(s);
   if ((fd = __alloc_handle(device)) < 0)
   {
      sock_close(s);
      return fail(EMFILE);
   }
   *(int32_t*)__get_handle(fd)->fileStruct = s;
   return fd;
}

static int dev_close(struct _reent *r, void *fs)
{
   int ret = result(sock_close(*(int32_t*)fs));
   if (ret < 0)
      r->_errno = errno;
   return ret < 0 ? -1 : 0;
}

/* A socket number, then (has) an address, as bind and connect take. */
static int addr_call(uint32_t cmd, int fd, const struct sockaddr *addr,
      socklen_t len)
{
   int32_t  s = sock_of(fd), ret;
   uint8_t *b;
   if (s < 0)
      return -1;
   if (!addr || len < 8 || addr->sa_family != AF_INET)
      return fail(addr ? EAFNOSUPPORT : EFAULT);
   if (!(b = (uint8_t*)buf_get(36)))
      return fail(ENOMEM);
   put32(b,     (uint32_t)s);
   put32(b + 4, 1);
   memcpy(b + 8, addr, 8);
   b[8] = 8;
   ret = ioctl_simple(cmd, b, 36, NULL, 0);
   free(b);
   return result(ret) < 0 ? -1 : 0;
}

int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
   return addr_call(SO_BIND, fd, addr, len);
}

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
   return addr_call(SO_CONNECT, fd, addr, len);
}

/* Two words: the socket and one argument. */
static int pair_call(uint32_t cmd, int fd, uint32_t arg)
{
   int32_t  s = sock_of(fd), ret;
   uint8_t *b;
   if (s < 0)
      return -1;
   if (!(b = (uint8_t*)buf_get(8)))
      return fail(ENOMEM);
   put32(b,     (uint32_t)s);
   put32(b + 4, arg);
   ret = ioctl_simple(cmd, b, 8, NULL, 0);
   free(b);
   return result(ret);
}

int listen(int fd, int backlog)
{
   return pair_call(SO_LISTEN, fd, (uint32_t)backlog) < 0 ? -1 : 0;
}

int shutdown(int fd, int how)
{
   return pair_call(SO_SHUTDOWN, fd, (uint32_t)how) < 0 ? -1 : 0;
}

/* Fills addr from IOS's 8-byte form. */
static void addr_out(struct sockaddr *addr, socklen_t *len,
      const uint8_t *in)
{
   struct sockaddr_in sin;
   if (!addr || !len)
      return;
   memset(&sin, 0, sizeof(sin));
   memcpy(&sin, in, 8);
   sin.sin_len    = sizeof(sin);
   sin.sin_family = AF_INET;
   memcpy(addr, &sin, *len < sizeof(sin) ? *len : sizeof(sin));
   *len = sizeof(sin);
}

int accept(int fd, struct sockaddr *addr, socklen_t *len)
{
   int32_t  s = sock_of(fd), c;
   uint8_t *b;
   int      nfd;
   if (s < 0)
      return -1;
   if (!(b = (uint8_t*)buf_get(64)))
      return fail(ENOMEM);
   put32(b, (uint32_t)s);
   b[32] = 8;
   b[33] = AF_INET;
   c = ioctl_simple(SO_ACCEPT, b, 4, b + 32, 8);
   if (c >= 0)
      addr_out(addr, len, b + 32);
   free(b);
   if (c < 0)
      return result(c);
   if ((nfd = __alloc_handle(device)) < 0)
   {
      sock_close(c);
      return fail(EMFILE);
   }
   *(int32_t*)__get_handle(nfd)->fileStruct = c;
   return nfd;
}

static int name_call(uint32_t cmd, int fd, struct sockaddr *addr,
      socklen_t *len)
{
   int32_t  s = sock_of(fd), ret;
   uint8_t *b;
   if (s < 0)
      return -1;
   if (!(b = (uint8_t*)buf_get(64)))
      return fail(ENOMEM);
   put32(b, (uint32_t)s);
   b[32] = 8;
   ret = ioctl_simple(cmd, b, 4, b + 32, 8);
   if (ret >= 0)
      addr_out(addr, len, b + 32);
   free(b);
   return result(ret) < 0 ? -1 : 0;
}

int getsockname(int fd, struct sockaddr *addr, socklen_t *len)
{
   return name_call(SO_GETSOCKNAME, fd, addr, len);
}

int getpeername(int fd, struct sockaddr *addr, socklen_t *len)
{
   return name_call(SO_GETPEERNAME, fd, addr, len);
}

int setsockopt(int fd, int level, int name, const void *val, socklen_t len)
{
   int32_t  s = sock_of(fd), ret;
   uint8_t *b;
   if (s < 0)
      return -1;
   if (len > 20 || (len && !val))
      return fail(EINVAL);
   if (!(b = (uint8_t*)buf_get(36)))
      return fail(ENOMEM);
   put32(b,      (uint32_t)s);
   put32(b + 4,  (uint32_t)level);
   put32(b + 8,  (uint32_t)name);
   put32(b + 12, len);
   if (len)
      memcpy(b + 16, val, len);
   ret = ioctl_simple(SO_SETSOCKOPT, b, 36, NULL, 0);
   free(b);
   return result(ret) < 0 ? -1 : 0;
}

int getsockopt(int fd, int level, int name, void *val, socklen_t *len)
{
   int32_t  s = sock_of(fd), ret;
   uint8_t *b;
   int      i;
   if (s < 0)
      return -1;
   if (!val || !len)
      return fail(EFAULT);
   if (!(b = (uint8_t*)buf_get(64)))
      return fail(ENOMEM);
   /* The same request in and out: IOS reads it from either. */
   for (i = 0; i < 64; i += 32)
   {
      put32(b + i,     (uint32_t)s);
      put32(b + i + 4, (uint32_t)level);
      put32(b + i + 8, (uint32_t)name);
   }
   ret = ioctl_simple(SO_GETSOCKOPT, b, 12, b + 32, 32);
   if (ret >= 0)
   {
      uint32_t n = get32(b + 32 + 12);
      if (n > 20)
         n = 20;
      if (n > *len)
         n = *len;
      /* A pending error is in IOS's numbering too. */
      if (level == SOL_SOCKET && name == SO_ERROR && n == 4)
      {
         uint32_t e = get32(b + 32 + 16);
         if (e && e <= sizeof(ios_errno))
            put32(b + 32 + 16, ios_errno[e - 1]);
      }
      memcpy(val, b + 32 + 16, n);
      *len = n;
   }
   free(b);
   return result(ret) < 0 ? -1 : 0;
}

static ssize_t sock_send(int32_t s, const void *buf, size_t len, int flags,
      const struct sockaddr *to, socklen_t tolen)
{
   int32_t      ret;
   uint8_t     *p, *d;
   gk_ios_vec_t v[2];
   if (len > CHUNK)
      len = CHUNK;
   if (!(p = (uint8_t*)buf_get(32)) || !(d = (uint8_t*)buf_get(len)))
   {
      free(p);
      return fail(ENOMEM);
   }
   memcpy(d, buf, len);
   put32(p,     (uint32_t)s);
   put32(p + 4, (uint32_t)flags);
   if (to)
   {
      if (tolen < 8 || to->sa_family != AF_INET)
      {
         free(p);
         free(d);
         return fail(EAFNOSUPPORT);
      }
      put32(p + 8, 1);
      memcpy(p + 12, to, 8);
      p[12] = 8;
   }
   v[0].data = d;
   v[0].len  = (uint32_t)len;
   v[1].data = p;
   v[1].len  = 32;
   ret = gk_ios_ioctlv(top, SO_SENDTO, 2, 0, v);
   free(p);
   free(d);
   return result(ret);
}

ssize_t sendto(int fd, const void *buf, size_t len, int flags,
      const struct sockaddr *to, socklen_t tolen)
{
   int32_t s = sock_of(fd);
   return s < 0 ? -1 : sock_send(s, buf, len, flags, to, tolen);
}

ssize_t send(int fd, const void *buf, size_t len, int flags)
{
   return sendto(fd, buf, len, flags, NULL, 0);
}

static ssize_t sock_recv(int32_t s, void *buf, size_t len, int flags,
      struct sockaddr *from, socklen_t *fromlen)
{
   int32_t      ret;
   uint8_t     *p, *d;
   gk_ios_vec_t v[3];
   if (len > CHUNK)
      len = CHUNK;
   if (!(p = (uint8_t*)buf_get(64)) || !(d = (uint8_t*)buf_get(len)))
   {
      free(p);
      return fail(ENOMEM);
   }
   put32(p,     (uint32_t)s);
   put32(p + 4, (uint32_t)flags);
   p[32] = 8;
   v[0].data = p;
   v[0].len  = 8;
   v[1].data = d;
   v[1].len  = (uint32_t)len;
   v[2].data = from ? p + 32 : NULL;
   v[2].len  = from ? 8 : 0;
   ret = gk_ios_ioctlv(top, SO_RECVFROM, 1, 2, v);
   if (ret > 0)
      memcpy(buf, d, (size_t)ret);
   if (ret >= 0 && from)
      addr_out(from, fromlen, p + 32);
   free(p);
   free(d);
   return result(ret);
}

ssize_t recvfrom(int fd, void *buf, size_t len, int flags,
      struct sockaddr *from, socklen_t *fromlen)
{
   int32_t s = sock_of(fd);
   return s < 0 ? -1 : sock_recv(s, buf, len, flags, from, fromlen);
}

ssize_t recv(int fd, void *buf, size_t len, int flags)
{
   return recvfrom(fd, buf, len, flags, NULL, NULL);
}

static ssize_t dev_write(struct _reent *r, void *fs, const char *p, size_t n)
{
   ssize_t ret = sock_send(*(int32_t*)fs, p, n, 0, NULL, 0);
   if (ret < 0)
      r->_errno = errno;
   return ret;
}

static ssize_t dev_read(struct _reent *r, void *fs, char *p, size_t n)
{
   ssize_t ret = sock_recv(*(int32_t*)fs, p, n, 0, NULL, NULL);
   if (ret < 0)
      r->_errno = errno;
   return ret;
}

/* ---- waiting ---- */

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
   uint8_t *b;
   nfds_t   i, n = 0;
   int      bad = 0;
   int32_t  ret;
   if (!nfds)
   {
      if (timeout > 0)
         gk_sleep_us((uint64_t)timeout * 1000);
      return 0;
   }
   if (!(b = (uint8_t*)buf_get(32 + nfds * 12)))
      return fail(ENOMEM);
   for (i = 0; i < nfds; i++)
   {
      int32_t s;
      __handle *h = __get_handle(fds[i].fd);
      fds[i].revents = 0;
      if (fds[i].fd < 0)
         continue;
      if (!h || device < 0 || (int)h->device != device)
      {
         fds[i].revents = POLLNVAL;
         bad++;
         continue;
      }
      s = *(int32_t*)h->fileStruct;
      put32(b + 32 + n * 12,     (uint32_t)s);
      put32(b + 32 + n * 12 + 4, (uint32_t)(uint16_t)fds[i].events);
      n++;
   }
   ret = 0;
   if (n)
   {
      int64_t t = bad ? 0 : timeout;
      memcpy(b, &t, 8);
      ret = gk_ios_ioctl(top, SO_POLL, b, 8, b + 32, n * 12);
      if (ret >= 0)
      {
         nfds_t k = 0;
         for (i = 0; i < nfds; i++)
            if (fds[i].fd >= 0 && fds[i].revents != POLLNVAL)
               fds[i].revents = (short)get32(b + 32 + (k++) * 12 + 8);
      }
   }
   free(b);
   if (ret < 0)
      return result(ret);
   return ret + bad;
}

int select(int nfds, fd_set *rd, fd_set *wr, fd_set *ex, struct timeval *tv)
{
   struct pollfd *p;
   int i, n = 0, ready = 0, timeout;
   if (nfds < 0 || nfds > FD_SETSIZE)
      return fail(EINVAL);
   if (!(p = (struct pollfd*)calloc((size_t)(nfds ? nfds : 1), sizeof(*p))))
      return fail(ENOMEM);
   for (i = 0; i < nfds; i++)
   {
      short ev = 0;
      if (rd && FD_ISSET(i, rd)) ev |= POLLIN;
      if (wr && FD_ISSET(i, wr)) ev |= POLLOUT;
      if (ex && FD_ISSET(i, ex)) ev |= POLLPRI;
      if (ev)
      {
         p[n].fd     = i;
         p[n].events = ev;
         n++;
      }
   }
   timeout = tv ? (int)(tv->tv_sec * 1000 + tv->tv_usec / 1000) : -1;
   if (poll(p, (nfds_t)n, timeout) < 0)
   {
      free(p);
      return -1;
   }
   if (rd) FD_ZERO(rd);
   if (wr) FD_ZERO(wr);
   if (ex) FD_ZERO(ex);
   for (i = 0; i < n; i++)
   {
      short r = p[i].revents;
      /* Files are always ready. */
      if (r & POLLNVAL)
         r = p[i].events;
      if (rd && (p[i].events & POLLIN) && (r & (POLLIN | POLLHUP | POLLERR)))
      {
         FD_SET(p[i].fd, rd);
         ready++;
      }
      if (wr && (p[i].events & POLLOUT) && (r & (POLLOUT | POLLERR)))
      {
         FD_SET(p[i].fd, wr);
         ready++;
      }
      if (ex && (p[i].events & POLLPRI) && (r & POLLPRI))
      {
         FD_SET(p[i].fd, ex);
         ready++;
      }
   }
   free(p);
   return ready;
}

/* Blocking or not, for sockets; the C library has no fcntl of its own. */
int fcntl(int fd, int cmd, ...)
{
   int32_t s;
   int     arg = 0, ret;
   va_list ap;
   va_start(ap, cmd);
   if (cmd == F_SETFL || cmd == F_SETFD)
      arg = va_arg(ap, int);
   va_end(ap);
   if (!__get_handle(fd))
      return fail(EBADF);
   if (cmd == F_GETFD || cmd == F_SETFD)
      return 0;
   if (cmd != F_GETFL && cmd != F_SETFL)
      return fail(EINVAL);
   if ((s = sock_of(fd)) < 0)
      return cmd == F_GETFL ? O_RDWR : 0;
   {
      uint8_t *b = (uint8_t*)buf_get(12);
      int32_t  r;
      if (!b)
         return fail(ENOMEM);
      put32(b,     (uint32_t)s);
      put32(b + 4, cmd == F_GETFL ? IOS_F_GETFL : IOS_F_SETFL);
      put32(b + 8, (arg & O_NONBLOCK) ? IOS_O_NONBLOCK : 0);
      r = ioctl_simple(SO_FCNTL, b, 12, NULL, 0);
      free(b);
      if ((ret = result(r)) < 0)
         return -1;
   }
   if (cmd == F_GETFL)
      return O_RDWR | ((ret & IOS_O_NONBLOCK) ? O_NONBLOCK : 0);
   return 0;
}

/* ---- names ---- */

struct hostent *gethostbyname(const char *name)
{
   static struct hostent he;
   static char           he_name[256];
   static uint32_t       he_addr[HOSTENT_MAX];
   static char          *he_list[HOSTENT_MAX + 1];
   static char          *he_alias[1];
   size_t   len;
   uint8_t *in, *out;
   int32_t  ret;
   int      i;
   if (!name || up())
      return NULL;
   len = strlen(name) + 1;
   if (len > sizeof(he_name))
      return NULL;
   in  = (uint8_t*)buf_get(len);
   out = (uint8_t*)buf_get(HOSTENT_SIZE);
   if (!in || !out)
   {
      free(in);
      free(out);
      return NULL;
   }
   memcpy(in, name, len);
   ret = ioctl_simple(SO_GETHOSTBYNAME, in, (uint32_t)len, out, HOSTENT_SIZE);
   /* The list of address pointers ends with 0; the addresses are in
    * the same order at their own offset. */
   for (i = 0; ret >= 0 && i < HOSTENT_MAX
         && get32(out + HOSTENT_PTRS + i * 4); i++)
   {
      he_addr[i] = get32(out + HOSTENT_ADDRS + i * 4);
      he_list[i] = (char*)&he_addr[i];
   }
   free(in);
   free(out);
   if (ret < 0 || !i)
      return NULL;
   he_list[i]     = NULL;
   he_alias[0]    = NULL;
   memcpy(he_name, name, len);
   he.h_name      = he_name;
   he.h_aliases   = he_alias;
   he.h_addrtype  = AF_INET;
   he.h_length    = 4;
   he.h_addr_list = he_list;
   return &he;
}

int inet_aton(const char *s, struct in_addr *addr)
{
   uint32_t v = 0;
   int      part;
   for (part = 0; part < 4; part++)
   {
      unsigned n = 0, digits = 0;
      while (*s >= '0' && *s <= '9' && digits < 4)
      {
         n = n * 10 + (unsigned)(*s++ - '0');
         digits++;
      }
      if (!digits || n > 255)
         return 0;
      v = (v << 8) | n;
      if (part < 3 && *s++ != '.')
         return 0;
   }
   if (*s)
      return 0;
   if (addr)
      addr->s_addr = htonl(v);
   return 1;
}

in_addr_t inet_addr(const char *s)
{
   struct in_addr a;
   return inet_aton(s, &a) ? a.s_addr : INADDR_NONE;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t len)
{
   const uint8_t *a = (const uint8_t*)src;
   char tmp[INET_ADDRSTRLEN];
   if (af != AF_INET)
   {
      errno = EAFNOSUPPORT;
      return NULL;
   }
   snprintf(tmp, sizeof(tmp), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
   if (strlen(tmp) >= len)
   {
      errno = ENOSPC;
      return NULL;
   }
   strcpy(dst, tmp);
   return dst;
}

char *inet_ntoa(struct in_addr addr)
{
   static char s[INET_ADDRSTRLEN];
   inet_ntop(AF_INET, &addr, s, sizeof(s));
   return s;
}

int inet_pton(int af, const char *src, void *dst)
{
   if (af != AF_INET)
   {
      errno = EAFNOSUPPORT;
      return -1;
   }
   return inet_aton(src, (struct in_addr*)dst);
}
