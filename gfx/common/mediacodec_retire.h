/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIACODEC_RETIRE_H
#define MEDIACODEC_RETIRE_H
#include <boolean.h>
#include <retro_inline.h>
#ifndef ANDROID_IMAGE_QUEUE_TEST_NO_FENCES
#include <poll.h>
#include <errno.h>
#endif
/* Cold cleanup executor only; never the frame or decoding caller. */
static INLINE bool mediacodec_retire_fence(int fd)
{
#ifdef ANDROID_IMAGE_QUEUE_TEST_NO_FENCES
   (void)fd;
   return false;
#else
   struct pollfd p;
   int result;
   if (fd < 0)
      return false;
   p.fd = fd;
   p.events = POLLIN;
   p.revents = 0;
   do
      result = poll(&p, 1, -1);
   while (result < 0 && errno == EINTR);
   return result > 0 && !(p.revents & POLLNVAL)
         && (p.revents & (POLLIN | POLLERR | POLLHUP));
#endif
}
#endif
