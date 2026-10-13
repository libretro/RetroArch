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
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
typedef struct test_work { rdvd_vfs_t *vfs; bool performed; } test_work_t;
static void service_worker(void *context)
{
   test_work_t *work = (test_work_t*)context;
   work->performed = rdvd_vfs_service(work->vfs);
}
static bool run_worker(rdvd_vfs_t *vfs)
{
   test_work_t work;
   sthread_t *thread;
   work.vfs = vfs; work.performed = false;
   thread = sthread_create(service_worker, &work);
   if (!thread) return false;
   sthread_join(thread);
   return work.performed;
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
      CHECK(rdvd_vfs_close(&vfs) && !vfs);
   }
   for (i = 0; i < 2; i++) CHECK(filestream_delete(paths[i]) == 0);
   printf("DVD real VFS tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
