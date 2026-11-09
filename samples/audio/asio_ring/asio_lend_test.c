/* The ASIO ring's lend, the shipping functions on a real ring: whole
 * frames of the layout in use, published by write_end, none past the
 * size asked for, none once the driver is shutting down. */
#include <stdio.h>
#include <string.h>
#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>
#include <retro_spsc.h>
#include <sys/types.h>
#include "audio/drivers/asio_ring.h"

typedef struct
{
   retro_spsc_t ring;
   size_t ring_size;
   unsigned channels;
   retro_atomic_int_t shutdown;
   unsigned latency_picks;
} ra_asio_t;

static void asio_pick_up_latencies(ra_asio_t *ad) { ad->latency_picks++; }
#include "asio_lend.inc"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#x); return 1; } } while (0)

int main(void)
{
   ra_asio_t ad;
   void *span;
   size_t got;
   unsigned frame;

   memset(&ad, 0, sizeof(ad));
   CHECK(retro_spsc_init(&ad.ring, 4096));
   ad.ring_size = 3000;
   ad.channels  = 6;
   frame        = 6 * sizeof(float);

   got = ra_asio_write_begin(&ad, 1000, &span);
   CHECK(span && got == 1000 - 1000 % frame);
   CHECK(ad.latency_picks == 1);
   CHECK(retro_spsc_read_avail(&ad.ring) == 0);
   memset(span, 0, got);
   CHECK(ra_asio_write_end(&ad, got + 1) == (ssize_t)got);
   CHECK(retro_spsc_read_avail(&ad.ring) == got);

   /* Never past the size asked for, though the ring rounds up */
   got = ra_asio_write_begin(&ad, 100000, &span);
   CHECK(got && got + retro_spsc_read_avail(&ad.ring) <= 3000);
   CHECK(ra_asio_write_end(&ad, 0) == 0);

   retro_atomic_store_release_int(&ad.shutdown, 1);
   CHECK(ra_asio_write_begin(&ad, 100, &span) == 0 && !span);

   retro_spsc_free(&ad.ring);
   puts("PASS: ASIO ring lend");
   return 0;
}
