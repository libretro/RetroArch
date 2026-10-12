/* What gfx/common/d3d11_video_decode.c calls of the frontend, for the
 * samples that build it in. */
#ifndef D3D11_VIDEO_DECODE_FRONTEND_STUBS_H
#define D3D11_VIDEO_DECODE_FRONTEND_STUBS_H

#include <stdio.h>
#include <stdarg.h>

static int stub_quiet;

settings_t *config_get_ptr(void)
{
   return NULL;
}

static void stub_vlog(const char *pre, const char *fmt, va_list ap)
{
   if (stub_quiet)
      return;
   fputs(pre, stdout);
   vprintf(fmt, ap);
}

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   stub_vlog("      ", fmt, ap);
   va_end(ap);
}

void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   stub_vlog("      [WARN] ", fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   stub_vlog("      [ERROR] ", fmt, ap);
   va_end(ap);
}

#endif
