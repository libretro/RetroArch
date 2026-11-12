/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_WORKER_OUTPUT_FIXTURE_H
#define RECORD_WORKER_OUTPUT_FIXTURE_H
struct record_busy_output
{
   record_mux_mediacodec_t *mux;
   record_mediacodec_packet_t saved[2];
   const record_mediacodec_packet_t *borrow[2];
   unsigned busy[2], accepted[2], retire_busy, retires;
};
static bool busy_track(void *data, bool audio, void *format)
{
   struct record_busy_output *o = (struct record_busy_output*)data;
   return record_mux_mediacodec_track(o->mux, audio, format);
}
static int busy_start(void *data)
{
   return record_mux_mediacodec_start(((struct record_busy_output*)data)->mux);
}
static int busy_write(void *data, bool audio, const record_mediacodec_packet_t *packet)
{
   struct record_busy_output *o = (struct record_busy_output*)data;
   unsigned at = audio ? 1 : 0;
   if (o->borrow[at])
   {
      assert(o->borrow[at] == packet && o->saved[at].data == packet->data);
      assert(o->saved[at].size == packet->size && o->saved[at].pts_us == packet->pts_us);
      assert(o->saved[at].flags == packet->flags);
   }
   if (o->busy[at])
   {
      o->borrow[at] = packet;
      o->saved[at] = *packet;
      o->busy[at]--;
      return 0;
   }
   if (!record_mux_mediacodec_write(o->mux, audio, packet))
      return -1;
   o->borrow[at] = NULL;
   o->accepted[at]++;
   return 1;
}
static bool busy_end(void *data, bool audio, int64_t end_us)
{
   return record_mux_mediacodec_end(((struct record_busy_output*)data)->mux, audio, end_us);
}
static int busy_finish(void *data)
{
   return record_mux_mediacodec_finish(((struct record_busy_output*)data)->mux);
}
static bool busy_retire(void *data)
{
   struct record_busy_output *o = (struct record_busy_output*)data;
   o->retires++;
   if (o->retire_busy)
   {
      o->retire_busy--;
      return false;
   }
   o->borrow[0] = o->borrow[1] = NULL;
   return true;
}
static const record_mediacodec_output_ops_t busy_ops = {
   busy_track, busy_start, busy_write, busy_end, busy_finish, busy_retire
};
#endif
