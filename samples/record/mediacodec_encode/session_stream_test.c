/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define HAVE_RTMP
#define HAVE_RFLV
#define RECORD_SESSION_STREAM_TEST
#define main file_session_acceptance
#include "session_test.c"
#undef main
struct net_rtmp_client { int live; };
struct record_stream_mediacodec
{
   unsigned live, retired, busy;
};
static record_stream_mediacodec_t stream_output;
static unsigned live_calibrations, network_steps;
static int network_result;
static const record_mediacodec_output_ops_t test_output_ops = {0};
static bool session_output_retired(void)
{
   if (!stream_output.live || stream_output.retired)
      return true;
   assert(video.live && !file.live && !mux.live);
   if (stream_output.busy)
   {
      stream_output.busy--;
      return false;
   }
   stream_output.retired = 1;
   return true;
}
record_stream_mediacodec_t *record_stream_mediacodec_new(
      record_mediacodec_encoder_t *v, record_mediacodec_encoder_t *a,
      net_rtmp_client_t *client, const record_mediacodec_stream_config_t *config)
{
   assert(v == &video && v->live && (!a || a == &audio));
   assert(client->live && config && !stream_output.live && !file.live);
   if (fault == 12)
      return NULL;
   stream_output.live = 1; stream_output.retired = stream_output.busy = 0;
   session_network_active = true;
   return &stream_output;
}
const record_mediacodec_output_ops_t *record_stream_mediacodec_ops(void)
{
   return &test_output_ops;
}
record_mediacodec_worker_t *record_mediacodec_worker_new_output(
      record_mediacodec_encoder_t *v, record_mediacodec_encoder_t *a,
      record_audio_queue_t *q, const record_mediacodec_output_ops_t *ops,
      void *context, unsigned channels, unsigned rate)
{
   assert(v == &video && v->live && ops == &test_output_ops && context == &stream_output);
   assert((a && q && channels == 2 && rate == 48000) || (!a && !q));
   if (fault == 6)
      return NULL;
   worker.live = 1; worker.closing = 0; worker.pcm = q;
   return &worker;
}
bool record_mediacodec_worker_calibrate_live(record_mediacodec_worker_t *w,
      record_aac_probe_t *p)
{
   live_calibrations++;
   return record_mediacodec_worker_calibrate(w, p);
}
int record_stream_mediacodec_step(record_stream_mediacodec_t *p,
      uint32_t time, net_rtmp_client_view_t *wait)
{
   assert(p == &stream_output && p->live && time == 17);
   network_steps++; wait->fd = 23; wait->read = true;
   return network_result;
}
bool record_stream_mediacodec_free(record_stream_mediacodec_t *p)
{
   if (!p) return true;
   assert(p == &stream_output && p->live && !worker.live && video.live);
   if (!session_output_retired()) return false;
   p->live = 0; session_network_active = false; return true;
}
static record_session_mediacodec_t *open_stream(bool has_audio)
{
   mediacodec_encoder_request_t request = {0};
   static struct net_rtmp_client client = {1};
   record_mediacodec_stream_config_t config = {0};
   config.max_packet_size = 1024; config.bit_depth = 8;
   return record_session_mediacodec_new_stream(NULL, &request, 2,
         has_audio ? 48000 : 0, has_audio ? 2 : 0, has_audio ? 128000 : 0,
         has_audio ? 2 : 0, has_audio ? 256 : 0, &client, &config);
}
int main(void)
{
   record_session_mediacodec_t *s;
   net_rtmp_client_view_t wait;
   unsigned mode;
   int before, detail;
   assert(file_session_acceptance() == 0);
   for (fault = 1; fault <= 12; fault++)
   {
      if (fault == 4 || fault == 5 || fault == 9 || fault == 10 || fault == 11)
         continue;
      assert(!open_stream(true)); balanced(); assert(!stream_output.live);
   }
   fault = 0;
   for (mode = 0; mode < 3; mode++)
   {
      s = open_stream(mode != 2); assert(s && !file.live && !mux.live);
      network_result = mode == 1 ? -1 : 0;
      assert(record_session_mediacodec_network_step(s, 17, &wait) == network_result);
      assert(wait.fd == 23 && wait.read);
      stream_output.busy = 2;
      before = stops;
      if (mode != 1) record_session_mediacodec_cancel(s);
      assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_BUSY);
      assert(worker.live && video.live && stops == before);
      assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_BUSY);
      assert(worker.live && video.live && stops == before);
      assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_BUSY);
      assert(!worker.live && !stream_output.live && video.live);
      assert(stops == before + (mode == 2 ? 1 : 2));
      assert(record_session_mediacodec_audio_finish(s, mode == 2 ? 0 : 7));
      assert(record_session_mediacodec_gpu_retired(s, 0));
      assert(record_session_mediacodec_gpu_released(s));
      assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_ERROR);
      assert(record_session_mediacodec_error(s, &detail)
            == (mode == 1 ? RECORD_MC_SESSION_NETWORK : RECORD_MC_SESSION_CANCELLED));
      assert(record_session_mediacodec_free(s)); balanced();
   }
   assert(live_calibrations >= 2 && network_steps == 3);
   s = open_stream(true); assert(s);
   before = completes;
   network_result = 2;
   assert(record_session_mediacodec_network_step(s, 17, &wait) == 2);
   assert(record_session_mediacodec_audio_finish(s, 7));
   reads_retired = 1;
   assert(record_session_mediacodec_gpu_retired(s, 100000));
   assert(record_session_mediacodec_gpu_released(s));
   assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_END);
   assert(!stream_output.live && completes == before);
   assert(record_session_mediacodec_free(s)); balanced();
   puts("PASS: file regressions plus network session startup rollback, live calibration selection, socket interests and delayed output retirement before codec stop; output/native owners mocked");
   return 0;
}
