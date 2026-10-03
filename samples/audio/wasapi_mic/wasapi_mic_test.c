/* The WASAPI microphone's capture fetch, against a scripted capture
 * client.
 *
 * In shared mode the capture FIFO's length comes from the user's
 * setting, and the engine hands over a packet at a time - up to its own
 * buffer. A packet goes into the FIFO whole or waits for room, so one
 * larger than the whole FIFO never went in: the microphone delivered
 * nothing for the rest of the session. The FIFO is now never shorter
 * than the engine buffer.
 *
 * The driver's text is generated from audio/drivers/wasapi.c with its
 * Windows includes swapped for the fake's (see the Makefile) and
 * included here whole, so the test reaches its static functions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wasapi_mic_sim.c"

static unsigned failures = 0;

#define CHECK(cond, msg, a, b) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s (%ld, %ld)\n", __FILE__, __LINE__, \
               msg, (long)(a), (long)(b)); \
         failures++; \
      } \
   } while (0)

/* --- the capture client: a queue of packets of a fixed length -------- */

#define FRAME_BYTES    4u      /* mono float */
#define ENGINE_FRAMES  480u    /* 10 ms at 48 kHz: the engine buffer */
#define PACKET_FRAMES  480u    /* what the engine hands over at a time */

static BYTE     g_packet[PACKET_FRAMES * FRAME_BYTES];
static unsigned g_packets;      /* packets waiting */
static unsigned g_taken;        /* packets the driver released whole */

static HRESULT cc_qi(IAudioCaptureClient *t, REFIID iid, void **out) { (void)t; (void)iid; *out = NULL; return E_NOINTERFACE; }
static DWORD   cc_addref(IAudioCaptureClient *t) { (void)t; return 1; }
static DWORD   cc_release(IAudioCaptureClient *t) { (void)t; return 0; }
static HRESULT cc_getbuffer(IAudioCaptureClient *t, BYTE **pp, UINT32 *n, DWORD *flags, UINT64 *p, UINT64 *q)
{
   (void)t; (void)p; (void)q;
   *flags = 0;
   *pp    = g_packets ? g_packet : NULL;
   *n     = g_packets ? PACKET_FRAMES : 0;
   return S_OK;
}
/* All of the packet or none of it, as WASAPI requires. */
static HRESULT cc_releasebuffer(IAudioCaptureClient *t, UINT32 n)
{
   (void)t;
   if (n == PACKET_FRAMES && g_packets)
   {
      g_packets--;
      g_taken++;
   }
   return S_OK;
}
static HRESULT cc_getnextpacketsize(IAudioCaptureClient *t, UINT32 *n)
{
   (void)t;
   *n = g_packets ? PACKET_FRAMES : 0;
   return S_OK;
}
static const IAudioCaptureClientVtbl capture_vtbl = {
   cc_qi, cc_addref, cc_release, cc_getbuffer, cc_releasebuffer, cc_getnextpacketsize };
static IAudioCaptureClient g_capture = { &capture_vtbl, NULL };

static void open_shared(wasapi_microphone_handle_t *mic, unsigned sh_frames)
{
   memset(mic, 0, sizeof(*mic));
   mic->capture            = &g_capture;
   mic->frame_size         = FRAME_BYTES;
   mic->engine_buffer_size = ENGINE_FRAMES * FRAME_BYTES;
   mic->buffer             = fifo_new(wasapi_microphone_fifo_bytes(
            sh_frames, mic->frame_size, mic->engine_buffer_size));
}

int main(void)
{
   static uint8_t out[ENGINE_FRAMES * FRAME_BYTES * 4];
   wasapi_microphone_handle_t mic;
   int got;

   /* A shared-buffer setting of 64 frames, shorter than one packet:
    * the packet still goes in, and keeps going in. */
   open_shared(&mic, 64);
   g_packets = 3;
   g_taken   = 0;
   got = wasapi_microphone_fetch_fifo(&mic);
   CHECK(got >= (int)(PACKET_FRAMES * FRAME_BYTES),
         "a packet longer than the setting is fetched", got, PACKET_FRAMES * FRAME_BYTES);
   CHECK(g_taken >= 1, "the packet was released whole", g_taken, 1);
   fifo_read(mic.buffer, out, (size_t)got);
   got = wasapi_microphone_fetch_fifo(&mic);
   CHECK(got >= (int)(PACKET_FRAMES * FRAME_BYTES),
         "and the next one after it is read", got, PACKET_FRAMES * FRAME_BYTES);
   fifo_free(mic.buffer);

   /* A setting longer than the engine buffer is kept as set. */
   CHECK(wasapi_microphone_fifo_bytes(2048, FRAME_BYTES, ENGINE_FRAMES * FRAME_BYTES)
         == 2048 * FRAME_BYTES, "a longer setting is kept",
         wasapi_microphone_fifo_bytes(2048, FRAME_BYTES, ENGINE_FRAMES * FRAME_BYTES),
         2048 * FRAME_BYTES);

   /* With the FIFO full, a packet waits rather than being dropped, and
    * goes in once the FIFO is read. */
   open_shared(&mic, 64);
   g_packets = 4;
   g_taken   = 0;
   wasapi_microphone_fetch_fifo(&mic);
   CHECK(g_packets > 0, "a packet with no room waits", g_packets, 1);
   {
      unsigned waiting = g_packets;
      fifo_read(mic.buffer, out, FIFO_READ_AVAIL(mic.buffer));
      wasapi_microphone_fetch_fifo(&mic);
      CHECK(g_packets < waiting, "and goes in once there is room", g_packets, waiting);
   }
   fifo_free(mic.buffer);

   if (failures)
   {
      printf("[fail] wasapi_mic_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] wasapi_mic_test\n");
   return 0;
}
