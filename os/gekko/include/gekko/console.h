/* Text console on an external framebuffer, using RetroArch's built-in
 * bitmap font.  Debug output and crash reports appear on it once it
 * is attached. */

#ifndef GEKKO_CONSOLE_H
#define GEKKO_CONSOLE_H

#include <gekko/video.h>

void gk_console_attach(void *xfb, const gk_vi_mode_t *mode);
void gk_console_detach(void);
void gk_console_write(const char *s, size_t len);

#endif
