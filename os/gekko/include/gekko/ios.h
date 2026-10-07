/* Wii: requests to IOS, the system software on the Starlet.
 *
 * Buffers are ordinary cached memory; the calls flush what IOS reads
 * and invalidate what it writes.  Buffers IOS writes must be 32-byte
 * aligned and sized, or neighbouring data in a shared cache line is
 * lost.  Results are IOS's: >= 0 on success, negative error codes. */

#ifndef GEKKO_IOS_H
#define GEKKO_IOS_H

#include <gekko/gekko.h>

#define GK_IOS_READ  1
#define GK_IOS_WRITE 2
#define GK_IOS_RW    3

#define GK_IOS_ENOENT  (-6)
#define GK_IOS_ENOMEM  (-22)

typedef struct gk_ios_vec
{
   void    *data;
   uint32_t len;
} gk_ios_vec_t;

/* Completion of an asynchronous request; runs in interrupt context. */
typedef void (*gk_ios_cb)(int32_t result, void *data);

int32_t gk_ios_open(const char *path, int mode);
int32_t gk_ios_close(int32_t fd);
int32_t gk_ios_read(int32_t fd, void *buf, uint32_t len);
int32_t gk_ios_write(int32_t fd, const void *buf, uint32_t len);
int32_t gk_ios_seek(int32_t fd, int32_t offset, int whence);
int32_t gk_ios_ioctl(int32_t fd, uint32_t cmd, const void *in,
      uint32_t in_len, void *out, uint32_t out_len);
/* n_in vectors IOS reads, then n_io it writes. */
int32_t gk_ios_ioctlv(int32_t fd, uint32_t cmd, uint32_t n_in,
      uint32_t n_io, const gk_ios_vec_t *vec);

int32_t gk_ios_ioctl_async(int32_t fd, uint32_t cmd, const void *in,
      uint32_t in_len, void *out, uint32_t out_len,
      gk_ios_cb cb, void *data);
int32_t gk_ios_ioctlv_async(int32_t fd, uint32_t cmd, uint32_t n_in,
      uint32_t n_io, const gk_ios_vec_t *vec, gk_ios_cb cb, void *data);

/* The running IOS: number in the high half, revision in the low. */
uint32_t gk_ios_version(void);

/* Start IOS major (installed) in place of the running one, before
 * anything has been opened: every descriptor and request of the old
 * one is gone.  0 once the new one takes requests. */
int gk_ios_reload(uint32_t major);

#endif
