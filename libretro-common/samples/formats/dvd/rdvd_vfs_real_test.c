/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <formats/rdvd_vfs.h>
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
int main(void)
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
   for (i = 0; i < 2; i++) CHECK(filestream_delete(paths[i]) == 0);
   printf("DVD real VFS tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
