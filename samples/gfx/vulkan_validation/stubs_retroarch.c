/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (stubs_retroarch.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The frontend symbols gfx/common/vulkan_common.c reaches for, plus
 * the logging hook the test scores itself on.
 *
 * vulkan_common.c is built here with -DVULKAN_DEBUG, which is the flag
 * RetroArch already uses to enable VK_LAYER_KHRONOS_validation and
 * VK_EXT_debug_utils and to install vulkan_debug_cb().  That callback
 * formats every message through RARCH_LOG, so counting validation
 * lines here needs no messenger of our own -- the driver's existing
 * debug path is the instrumentation. */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
int  g_vk_validation_errors;
int  g_vk_validation_warnings;
static void vk_log(const char *fmt, va_list ap)
{
   char line[4096];
   vsnprintf(line, sizeof(line), fmt, ap);
   /* vulkan_debug_cb() formats as "[Vulkan] <SEVERITY> <TYPE>: ...",
    * so key on the pair.  Matching "WARNING" anywhere in the line
    * instead would score VUID names like WARNING-cache-file-error,
    * which arrive at INFO severity, as warnings. */
   if (strstr(line, "ERROR Validation"))
   {
      g_vk_validation_errors++;
      fputs(line, stderr);
   }
   else if (strstr(line, "WARNING Validation"))
   {
      g_vk_validation_warnings++;
      fputs(line, stderr);
   }
}
void RARCH_LOG(const char *fmt, ...){va_list a;va_start(a,fmt);vk_log(fmt,a);va_end(a);}
void RARCH_ERR(const char *fmt, ...){va_list a;va_start(a,fmt);vk_log(fmt,a);va_end(a);}
void RARCH_WARN(const char *fmt, ...){va_list a;va_start(a,fmt);vk_log(fmt,a);va_end(a);}
void RARCH_DBG(const char *fmt, ...){(void)fmt;}

/* --- The rest of what vulkan_common.c reaches for --- */
#include <boolean.h>
#include <stddef.h>
#include <stdint.h>

void *XGetXCBConnection(void *dpy) { (void)dpy; return NULL; }

static char s_settings[1 << 18];
void *config_get_ptr(void) { return s_settings; }

void *video_state_get_ptr(void) { static char st[1 << 16]; return st; }

const char *msg_hash_to_str(int e) { (void)e; return "stub"; }

void video_driver_set_gpu_api_devices(int api, void *list) { (void)api; (void)list; }
void video_driver_set_gpu_api_version_string(const char *s) { (void)s; }
void video_driver_cache_context_ack_set(void) { }
uint32_t video_driver_get_disp_flags(void) { return 0; }
void video_driver_set_disp_flags(uint32_t f) { (void)f; }
