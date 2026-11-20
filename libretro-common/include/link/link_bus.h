/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (link_bus.h).
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

#ifndef __LIBRETRO_SDK_LINK_BUS_H
#define __LIBRETRO_SDK_LINK_BUS_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>

RETRO_BEGIN_DECLS

/* The frontend side of the link interface (RETRO_ENVIRONMENT_GET_LINK_INTERFACE)
 * for every bus of a session. Consoles are numbered from 0 in session order
 * and every call names its console. Every call answers at once: the bus never
 * waits, calls back, reads a clock or logs. Its host parks an advance that is
 * not grantable and tests it again when link_bus_changes moves. The host puts
 * only consoles of one frame span on a bus; the bus does not compare spans. */

#define LINK_BUS_MAX_BUS_PORTS 255
#define LINK_BUS_IDLE_SPANS    30

typedef struct link_bus link_bus_t;

struct link_bus_counts            /* per call; blocks is never saved or compared */
{
   uint64_t lowered_promise;        /* repair: a lower promise ignored */
   uint64_t low_stamp;              /* repair: a stamp raised to the promise */
   uint64_t request_past_promise;   /* repair: request granted as the promise */
   uint64_t request_past_frame_end; /* repair: request granted as the frame-end tick */
   uint64_t request_past_earliest;  /* repair: request granted as an advance_multi's earliest promise */
   uint64_t skipped_publish;        /* breach: a frame ended with no frame-end publish */
   uint64_t left_out;               /* breach: an attached port missing from an advance */
   uint64_t misplaced_call;         /* breach: a call made where the header allows none, or on another console's handle */
   uint64_t blocks;                 /* mallocs since link_bus_new */
};

struct link_bus_blocker           /* why an advance is not grantable */
{
   unsigned entry, port;            /* the entry and its port number */
   uint64_t request, bound;         /* both on that port's clock */
   unsigned peer_console, peer_port;
   bool     edge_unknown;           /* the peer has not given its tick at this edge */
};

struct link_bus_sent              /* one receiver's copy of a message */
{
   unsigned from_console, from_port, to_console, to_port;
   uint64_t stamp;                  /* sender's clock, after any raise */
   uint64_t landing;                /* receiver's clock; RETRO_LINK_UNBOUNDED until it is known */
   size_t   len;
   unsigned char head[8];           /* first bytes, zero-padded */
};

link_bus_t *link_bus_new(unsigned consoles);
void link_bus_free(link_bus_t *b);

/* The interface calls, made by the console named */
bool link_bus_set_ports(link_bus_t *b, unsigned console, const struct retro_link_port_info *ports);
retro_link_port_t *link_bus_attach(link_bus_t *b, unsigned console, unsigned port,
      const char *protocol_id, uint64_t clock_rate);
void link_bus_detach(link_bus_t *b, unsigned console, retro_link_port_t *h);
int link_bus_peers(link_bus_t *b, unsigned console, retro_link_port_t *h, unsigned *count);
bool link_bus_send(link_bus_t *b, unsigned console, retro_link_port_t *h, uint64_t tick,
      unsigned to, const void *buf, size_t len);
bool link_bus_recv(link_bus_t *b, unsigned console, retro_link_port_t *h, uint64_t *tick,
      unsigned *from, void *buf, size_t *len);
const void *link_bus_peek(link_bus_t *b, unsigned console, retro_link_port_t *h,
      uint64_t *tick, unsigned *from, size_t *len);

/* Advance in three steps; a single-port advance is a one-entry array.
 * publish records the entries' ticks and promises and applies the repairs;
 * grantable tests them; grant writes each entry's grant and wake flags. */
void link_bus_publish(link_bus_t *b, unsigned console, struct retro_link_wait *w, unsigned n);
bool link_bus_grantable(const link_bus_t *b, unsigned console, const struct retro_link_wait *w, unsigned n);
void link_bus_grant(link_bus_t *b, unsigned console, struct retro_link_wait *w, unsigned n);
bool link_bus_blocked_by(const link_bus_t *b, unsigned console, const struct retro_link_wait *w,
      unsigned n, struct link_bus_blocker *out);
uint64_t link_bus_changes(const link_bus_t *b, unsigned console);  /* moves on a changed publish or a send on the console's buses */
uint64_t link_bus_activity(const link_bus_t *b, unsigned console); /* the console's own sends and recvs */

/* Around each retro_run, then edge once every console has finished */
void link_bus_frame_start(link_bus_t *b, unsigned console);
void link_bus_frame_end(link_bus_t *b, unsigned console);
void link_bus_edge(link_bus_t *b);

/* At an edge only; false when refused */
bool link_bus_cable(link_bus_t *b, unsigned console_a, unsigned port_a, unsigned console_b, unsigned port_b);
bool link_bus_uncable(link_bus_t *b, unsigned console, unsigned port);
bool link_bus_hold(link_bus_t *b, unsigned console);
bool link_bus_unhold(link_bus_t *b, unsigned console);
bool link_bus_is_held(const link_bus_t *b, unsigned console);
void link_bus_renumbered(link_bus_t *b, unsigned console);   /* after retro_unserialize or retro_reset */
void link_bus_console_gone(link_bus_t *b, unsigned console); /* after retro_unload_game */

/* Release, state and inspection */
void link_bus_release(link_bus_t *b);
bool link_bus_released(const link_bus_t *b);
size_t link_bus_save_size(const link_bus_t *b);
bool link_bus_save(const link_bus_t *b, void *buf, size_t len);
bool link_bus_load(link_bus_t *b, const void *buf, size_t len);
uint64_t link_bus_hash(const link_bus_t *b);
size_t link_bus_dump(const link_bus_t *b, char *s, size_t len);   /* snprintf-like: returns the length needed */
void link_bus_get_counts(const link_bus_t *b, struct link_bus_counts *out);
size_t link_bus_journal(const link_bus_t *b, struct link_bus_sent *out, size_t cap); /* returns the total */

RETRO_END_DECLS

#endif
