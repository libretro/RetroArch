/* audio/drivers/ps2_audio.c against a scripted audsrv and EE kernel:
 * the writable wait parks on the semaphore audsrv's fill-buffer
 * callback posts as the SPU takes blocks, wakes when the room is there,
 * and is bounded by an alarm when the server stops draining; the
 * callback is disarmed and the semaphore deleted on free. */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "../../../audio/audio_driver.h"
#include "ps2_mock.h"

extern audio_driver_t audio_ps2;

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

int main(void)
{
   static char pcm[16384];
   unsigned rate = 0;
   size_t got;
   void *h;

   mock_iop_start();
   h = audio_ps2.init(NULL, 48000, 64, &rate);
   CHECK(h != NULL, "init failed");
   if (!h)
      return 1;
   CHECK(mock_fillbuf_threshold() > 0, "the fill-buffer callback was not armed");
   audio_ps2.start(h, false);

   /* Fill the ring, then wait for half of it: the SPU takes a block at
    * a time and the callback wakes the wait as it does */
   audio_ps2.write(h, pcm, sizeof(pcm));
   mock_reset_counts();
   got = audio_ps2.wait_writable(h, 8192);
   CHECK(got >= 8192, "the wait returned %u bytes free, 8192 wanted", (unsigned)got);
   CHECK(mock_waits() > 0, "the wait never parked");
   CHECK(mock_callbacks() > 0, "audsrv never called back");
   CHECK(mock_alarms_rung() == 0, "the alarm rang on a draining server");
   mock_alarm_join();

   /* A server that has stopped draining: the alarm ends the wait */
   audio_ps2.write(h, pcm, sizeof(pcm));
   mock_set_draining(0);
   mock_reset_counts();
   got = audio_ps2.wait_writable(h, 8192);
   CHECK(got == 0, "a stopped server gave %u bytes", (unsigned)got);
   CHECK(mock_alarms_rung() == 1, "the alarm rang %u times", mock_alarms_rung());
   mock_alarm_join();
   mock_set_draining(1);

   /* Room there already: no wait at all */
   mock_empty_ring();
   mock_reset_counts();
   got = audio_ps2.wait_writable(h, 16);
   CHECK(got >= 16 && mock_waits() == 0, "a wait with room parked (%u waits)", mock_waits());
   mock_alarm_join();

   audio_ps2.free(h);
   CHECK(mock_fillbuf_threshold() == 0, "the callback stayed armed past free");
   CHECK(mock_sema_live() == 0, "the semaphore outlived the driver");
   mock_iop_stop();

   if (failures)
   {
      printf("[fail] ps2_audio_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] ps2_audio_test\n");
   return 0;
}
