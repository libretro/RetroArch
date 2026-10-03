/* poll(), over sockets only. */

#ifndef GEKKO_POLL_H
#define GEKKO_POLL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int nfds_t;

struct pollfd
{
   int   fd;
   short events;
   short revents;
};

#define POLLRDNORM 0x0001
#define POLLRDBAND 0x0002
#define POLLIN     POLLRDNORM
#define POLLPRI    0x0004
#define POLLWRNORM 0x0008
#define POLLOUT    POLLWRNORM
#define POLLWRBAND 0x0010
#define POLLERR    0x0020
#define POLLHUP    0x0040
#define POLLNVAL   0x0080

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#ifdef __cplusplus
}
#endif

#endif
