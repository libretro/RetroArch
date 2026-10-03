/* audio/drivers/oss.c's write_avail() and buffer_size() on a device that
 * cannot answer.
 *
 * Both ask SNDCTL_DSP_GETOSPACE, and write_avail() is asked every frame:
 * a device that has gone away failed it every frame, and each failure
 * was a logged error. And buffer_size() answered 1 rather than 0, which
 * is how a driver says it has no size - audio_driver.h: zero turns rate
 * control off for the session; a one-byte buffer gave it a setpoint of
 * half a byte.
 *
 * The driver is this translation unit; a closed descriptor makes the
 * real ioctl fail. */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../../../audio/drivers/oss.c"

static unsigned failures  = 0;
static unsigned log_lines = 0;

#define CHECK(cond, msg, a, b) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s (%ld, %ld)\n", __FILE__, __LINE__, \
               msg, (long)(a), (long)(b)); \
         failures++; \
      } \
   } while (0)

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; log_lines++; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; log_lines++; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; log_lines++; }
uint32_t audio_driver_requested_layout(void) { return 0x3; }
unsigned audio_layout_channels(uint32_t layout) { (void)layout; return 2; }

int main(void)
{
   oss_audio_t oss;
   unsigned frame;

   memset(&oss, 0, sizeof(oss));
   oss.fd = -1;   /* a device that has gone away */

   for (frame = 0; frame < 60; frame++)
      CHECK(oss_write_avail(&oss) == 0, "no room from a device that cannot say",
            oss_write_avail(&oss), 0);
   CHECK(oss_buffer_size(&oss) == 0, "no size from a device that cannot say",
         oss_buffer_size(&oss), 0);
   CHECK(log_lines == 0, "a second of frames logs nothing", log_lines, 0);

   if (failures)
   {
      printf("[fail] oss_report_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] oss_report_test\n");
   return 0;
}
