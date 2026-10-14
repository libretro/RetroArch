/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <formats/rdvd_ac3.h>
#include <formats/rdvd_lpcm.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <formats/rdvd_vfs.h>
#include <formats/rdvd_stream.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
static bool run_worker(rdvd_vfs_t *vfs)
{
   tpool_t *pool = tpool_create(1);
   bool posted;
   if (!pool) return false;
   posted = rdvd_vfs_schedule(vfs, pool);
   tpool_wait(pool);
   tpool_destroy(pool);
   return posted;
}
static retro_atomic_int_t gate_open, gate_started;
static void gated_job(void *unused)
{
   (void)unused;
   retro_atomic_store_release_int(&gate_started, 1);
   while (!retro_atomic_load_acquire_int(&gate_open)) sthread_yield();
}
static void test_queued_cancel(rdvd_vfs_t *vfs, rdvd_source_backend_t *backend)
{
   tpool_t *pool = tpool_create(1);
   rdvd_source_t source;
   uint8_t buffer[32];
   retro_time_t start;
   CHECK(pool != NULL); if (!pool) return;
   retro_atomic_store_release_int(&gate_open, 0);
   retro_atomic_store_release_int(&gate_started, 0);
   CHECK(tpool_add_work(pool, gated_job, NULL));
   start = cpu_features_get_time_usec();
   while (!retro_atomic_load_acquire_int(&gate_started)
         && cpu_features_get_time_usec() - start < 10000000) sthread_yield();
   CHECK(retro_atomic_load_acquire_int(&gate_started));
   CHECK(rdvd_source_init(&source, &backend->map, 0, 32));
   CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
   CHECK(rdvd_vfs_schedule(vfs, pool));
   CHECK(!rdvd_vfs_schedule(vfs, pool));
   CHECK(!rdvd_source_poll(&source, backend));
   rdvd_source_cancel(&source);
   CHECK(!rdvd_vfs_close(&vfs));
   retro_atomic_store_release_int(&gate_open, 1);
   tpool_wait(pool);
   CHECK(rdvd_source_poll(&source, backend));
   CHECK(!source.pending && !source.completed && source.cancelled);
   tpool_destroy(pool);
}
static void test_stream_files(const char *const *paths)
{
   uint8_t data[2048], buffer[32];
   const uint8_t pes[30] = {0,0,1,0xbd,0,12,0x80,0,0,0x80,1,0,0,0x11,0x22,0x33,0x44,0x55,
      0,0,1,0xe0,0,6,0x80,0,0,0xaa,0xbb,0xcc};
   RFILE *file;
   rdvd_vfs_t *vfs;
   rdvd_source_backend_t *backend;
   rdvd_source_t source;
   rdvd_stream_t stream;
   rdvd_demux_packet_t packet;
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0);
   tpool_t *pool = tpool_create(1);
   enum rdvd_stream_status status = RDVD_STREAM_WAIT;
   unsigned i, audio = 0, video = 0;
   retro_time_t start;
   CHECK(ps && pool); if (!ps || !pool) return;
   for (i = 0; i < 2; i++)
   {
      memset(data, 0, sizeof(data));
      if (!i) memcpy(data + 2040, pes, 8);
      else memcpy(data, pes + 8, 22);
      file = filestream_open(paths[i], RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      CHECK(file != NULL); if (!file) return;
      CHECK(filestream_write(file, data, sizeof(data)) == sizeof(data));
      CHECK(filestream_close(file) == 0);
   }
   vfs = rdvd_vfs_open(paths, 2);
   CHECK(vfs != NULL); if (!vfs) return;
   backend = rdvd_vfs_backend(vfs);
   CHECK(rdvd_source_init(&source, &backend->map, 2040, 30));
   CHECK(rdvd_stream_init(&stream, &source, vfs, pool, ps, buffer, sizeof(buffer)));
   start = cpu_features_get_time_usec();
   while (cpu_features_get_time_usec() - start < 10000000)
   {
      status = rdvd_stream_next(&stream, &packet);
      if (status == RDVD_STREAM_PACKET)
      {
         CHECK(!source.pending);
         if (packet.dvd_valid && packet.dvd.kind == RDVD_PACKET_AC3)
         {
            audio++;
            CHECK(packet.dvd.size == 5 && packet.dvd.data[0] == 0x11);
         }
         if (packet.pes.type == RMPEG1_PS_VIDEO)
         {
            video++;
            CHECK(packet.pes.size == 3 && packet.pes.data == buffer + 19);
         }
      }
      else if (status != RDVD_STREAM_WAIT) break;
      else sthread_yield();
   }
   CHECK(status == RDVD_STREAM_END && audio == 1 && video == 1 && source.completed == 30);
   tpool_wait(pool); tpool_destroy(pool);
   CHECK(rdvd_vfs_close(&vfs)); rmpeg1_ps_free(ps);
}
static void test_vob_stream(const char *path, const char *reference)
{
   uint8_t buffer[2048];
   uint8_t *assembly = (uint8_t*)malloc(RDVD_AC3_BUFFER_BYTES);
   float *pcm = (float*)malloc(1536 * 8 * sizeof(float));
   float *expected = (float*)malloc(1536 * 8 * sizeof(float));
   RFILE *oracle = reference ? filestream_open(reference, RETRO_VFS_FILE_ACCESS_READ,
      RETRO_VFS_FILE_ACCESS_HINT_NONE) : NULL;
   rdvd_lpcm_t lpcm;
   rdvd_lpcm_block_t lpcm_block;
   enum rdvd_lpcm_result lpcm_result;
   size_t lpcm_frames = 0;
   unsigned lpcm_packets = 0, lpcm_borrowed = 0, lpcm_assembled = 0;
   rac3_decoder_t *decoder = rac3_decoder_new();
   rdvd_ac3_t ac3;
   rdvd_ac3_frame_t frame;
   enum rdvd_ac3_result audio_result;
   unsigned frames = 0, borrowed = 0, assembled = 0;
   rdvd_vfs_t *vfs = rdvd_vfs_open(&path, 1);
   rdvd_source_backend_t *backend;
   rdvd_source_t source;
   rdvd_stream_t stream;
   rdvd_demux_packet_t packet;
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0);
   tpool_t *pool = tpool_create(1);
   enum rdvd_stream_status status = RDVD_STREAM_WAIT;
   unsigned video = 0, audio = 0, pci = 0, dsi = 0;
   retro_time_t start;
   CHECK(vfs && ps && pool && assembly && pcm && expected && decoder);
   CHECK(!reference || oracle);
   if (!vfs || !ps || !pool || !assembly || !pcm || !expected || !decoder || (reference && !oracle)) return;
   CHECK(rdvd_ac3_init(&ac3, 0, assembly, RDVD_AC3_BUFFER_BYTES));
   CHECK(rdvd_lpcm_init(&lpcm, 0));
   backend = rdvd_vfs_backend(vfs);
   CHECK(rdvd_source_init(&source, &backend->map, 0, backend->map.total_bytes));
   CHECK(rdvd_stream_init(&stream, &source, vfs, pool, ps, buffer, sizeof(buffer)));
   start = cpu_features_get_time_usec();
   while (cpu_features_get_time_usec() - start < 10000000)
   {
      status = rdvd_stream_next(&stream, &packet);
      if (status == RDVD_STREAM_PACKET)
      {
         if (packet.pes.type == RMPEG1_PS_VIDEO) video++;
         if (packet.dvd_valid)
         {
            if (packet.dvd.kind == RDVD_PACKET_AC3)
            {
               audio++;
               CHECK(rdvd_ac3_feed(&ac3, &packet.dvd));
               while ((audio_result = rdvd_ac3_next(&ac3, &frame)) == RDVD_AC3_FRAME)
               {
                  rac3_frame_info_t info;
                  size_t count, i;
                  CHECK(rac3_frame_crc_ok(frame.data, frame.size));
                  count = rac3_decode_frame(decoder, frame.data, frame.size, pcm, &info);
                  CHECK(count == 1536 && info.channels <= 6);
                  for (i = 0; i < count * info.channels; i++)
                     CHECK(pcm[i] == pcm[i] && pcm[i] > -16.0f && pcm[i] < 16.0f);
                  if (frame.data == assembly) assembled++;
                  else borrowed++;
                  frames++;
               }
               CHECK(audio_result == RDVD_AC3_MORE && !ac3.borrowing);
            }
            if (packet.dvd.kind == RDVD_PACKET_LPCM)
            {
               lpcm_packets++;
               CHECK(rdvd_lpcm_feed(&lpcm, &packet.dvd));
               while ((lpcm_result = rdvd_lpcm_next(&lpcm, 1536, &lpcm_block)) == RDVD_LPCM_BLOCK)
               {
                  size_t count, samples, i;
                  count = rlpcm_decode_f32(&lpcm_block.format, lpcm_block.data,
                     lpcm_block.size, pcm, lpcm_block.frames);
                  CHECK(count == lpcm_block.frames);
                  samples = count * lpcm_block.format.channels;
                  for (i = 0; i < samples; i++)
                     CHECK(pcm[i] == pcm[i] && pcm[i] >= -1.0f && pcm[i] <= 1.0f);
                  if (oracle)
                  {
                     CHECK(filestream_read(oracle, expected, samples * sizeof(float))
                        == (int64_t)(samples * sizeof(float)));
                     CHECK(!memcmp(pcm, expected, samples * sizeof(float)));
                  }
                  if (lpcm_block.data == lpcm.buffer) lpcm_assembled++;
                  else lpcm_borrowed++;
                  lpcm_frames += count;
               }
               CHECK(lpcm_result == RDVD_LPCM_MORE && !lpcm.borrowing);
            }
            if (packet.dvd.kind == RDVD_PACKET_PCI) pci++;
            if (packet.dvd.kind == RDVD_PACKET_DSI) dsi++;
         }
      }
      else if (status != RDVD_STREAM_WAIT) break;
      else sthread_yield();
   }
   CHECK(status == RDVD_STREAM_END && video && (audio || lpcm_packets) && pci && dsi);
   CHECK((frames || lpcm_frames) && !ac3.used && !lpcm.used);
   if (oracle)
   {
      uint8_t extra;
      CHECK(lpcm_frames && filestream_read(oracle, &extra, 1) == 0);
      CHECK(filestream_close(oracle) == 0);
   }
   printf("Worker LPCM: frames=%lu borrowed=%u assembled=%u\n",
      (unsigned long)lpcm_frames, lpcm_borrowed, lpcm_assembled);
   free(expected);
   printf("Worker AC3 frames: total=%u borrowed=%u assembled=%u\n", frames, borrowed, assembled);
   rac3_decoder_free(decoder); free(pcm); free(assembly);
   printf("Worker VOB stream: video=%u AC3=%u PCI=%u DSI=%u\n", video, audio, pci, dsi);
   tpool_wait(pool); tpool_destroy(pool);
   CHECK(rdvd_vfs_close(&vfs)); rmpeg1_ps_free(ps);
}
int main(int argc, char **argv)
{
   char names[2][64];
   const char *paths[2];
   uint8_t data[2048], buffer[32];
   unsigned i;
   unsigned long pid;
   RFILE *file;
   rdvd_vfs_t *vfs;
   rdvd_source_t source;
   rdvd_source_backend_t *backend;
#ifdef _WIN32
   pid = (unsigned long)GetCurrentProcessId();
#else
   pid = (unsigned long)getpid();
#endif
   for (i = 0; i < 2; i++)
   {
      sprintf(names[i], "rdvd-vfs-%lu-%u.tmp", pid, i);
      paths[i] = names[i];
      file = filestream_open(paths[i], RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      CHECK(file != NULL); if (!file) return 1;
      memset(data, i ? 0x76 : 0x42, sizeof(data));
      CHECK(filestream_write(file, data, sizeof(data)) == sizeof(data));
      CHECK(filestream_write(file, data, sizeof(data)) == sizeof(data));
      CHECK(filestream_close(file) == 0);
   }
   vfs = rdvd_vfs_open(paths, 2);
   CHECK(vfs != NULL);
   if (vfs)
   {
      backend = rdvd_vfs_backend(vfs);
      CHECK(backend->map.total_bytes == 8192);
      CHECK(rdvd_source_init(&source, &backend->map, 4090, 20));
      CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
      CHECK(!rdvd_source_poll(&source, backend));
      CHECK(run_worker(vfs) && rdvd_source_poll(&source, backend));
      CHECK(source.completed == 6 && buffer[0] == 0x42 && buffer[5] == 0x42);
      CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
      CHECK(run_worker(vfs) && rdvd_source_poll(&source, backend));
      CHECK(source.completed == 20 && !source.remaining && !source.failed);
      CHECK(buffer[0] == 0x76 && buffer[13] == 0x76);
      test_queued_cancel(vfs, backend);
      CHECK(rdvd_vfs_close(&vfs) && !vfs);
   }
   test_stream_files(paths);
   if (argc > 1) test_vob_stream(argv[1], argc > 2 ? argv[2] : NULL);
   for (i = 0; i < 2; i++) CHECK(filestream_delete(paths[i]) == 0);
   printf("DVD real VFS tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
