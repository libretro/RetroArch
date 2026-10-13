/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <string.h>
#include <formats/rdvd_demux.h>
bool rdvd_demux_init(rdvd_demux_t *demux, rmpeg1_ps_t *ps)
{
   if (!demux) return false;
   memset(demux, 0, sizeof(*demux));
   if (!ps) return false;
   demux->ps = ps;
   rmpeg1_ps_reset(ps);
   return true;
}
bool rdvd_demux_feed(rdvd_demux_t *demux, const uint8_t *data, size_t size)
{
   if (!demux || !demux->ps || demux->borrowing || !data || !size
         || rmpeg1_ps_borrow(demux->ps, data, size) != size) return false;
   demux->borrowing = true;
   return true;
}
bool rdvd_demux_next(rdvd_demux_t *demux, rdvd_demux_packet_t *out)
{
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!demux || !demux->ps) return false;
   if (!rmpeg1_ps_next(demux->ps, &out->pes))
   {
      demux->borrowing = false;
      return false;
   }
   out->dvd_valid = rdvd_packet_read(out->pes.stream_id, out->pes.data, out->pes.size, &out->dvd);
   return true;
}
void rdvd_demux_reset(rdvd_demux_t *demux)
{
   if (!demux || !demux->ps) return;
   rmpeg1_ps_reset(demux->ps);
   demux->borrowing = false;
}
