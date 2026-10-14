/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <formats/rdvd_vfs.h>
#include <formats/rdvd_stream.h>
struct RFILE
{
   uint64_t size, position;
   unsigned tag, reads, closes;
   bool seek_error, read_error, oversize, close_error;
   void *destination;
};
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
int64_t filestream_seek(RFILE *file, int64_t offset, int whence)
{
   if (file->seek_error || offset < 0 || whence != RETRO_VFS_SEEK_POSITION_START) return -1;
   file->position = (uint64_t)offset;
   return 0;
}
int64_t filestream_tell(RFILE *file) { return (int64_t)file->position; }
int64_t filestream_read(RFILE *file, void *destination, int64_t bytes)
{
   uint64_t count;
   file->reads++; file->destination = destination;
   if (file->read_error) return -1;
   if (file->oversize) return bytes + 1;
   count = file->position < file->size ? file->size - file->position : 0;
   if (count > (uint64_t)bytes) count = (uint64_t)bytes;
   memset(destination, (int)file->tag, (size_t)count);
   file->position += count;
   return (int64_t)count;
}
static RFILE opened_files[2];
RFILE *filestream_open(const char *path, unsigned mode, unsigned hints)
{
   if (mode != RETRO_VFS_FILE_ACCESS_READ || hints != RETRO_VFS_FILE_ACCESS_HINT_NONE) return NULL;
   if (!strcmp(path, "one")) return opened_files;
   if (!strcmp(path, "two")) return opened_files + 1;
   return NULL;
}
int64_t filestream_get_size(RFILE *file) { return (int64_t)file->size; }
int filestream_close(RFILE *file)
{
   file->closes++;
   return file->close_error ? -1 : 0;
}
bool tpool_try_add_work(tpool_t *pool, thread_func_t func, void *arg)
{
   (void)pool; (void)func; (void)arg;
   return false;
}
static void test_reads(void)
{
   RFILE files[2], *handles[2];
   uint64_t sizes[2];
   uint8_t buffer[32];
   rdvd_vfs_t *vfs;
   rdvd_source_backend_t *backend;
   rdvd_source_t source;
   memset(files, 0, sizeof(files));
   sizes[0] = sizes[1] = 2048;
   files[0].size = files[1].size = 2048;
   files[0].tag = 0x51; files[1].tag = 0x72;
   handles[0] = files; handles[1] = files + 1;
   vfs = rdvd_vfs_bind(handles, sizes, 2);
   CHECK(vfs != NULL); if (!vfs) return;
   backend = rdvd_vfs_backend(vfs);
   CHECK(rdvd_source_init(&source, &backend->map, 2040, 40));
   CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
   CHECK(!files[0].reads && !rdvd_source_poll(&source, backend));
   CHECK(!rdvd_vfs_schedule(vfs, (tpool_t*)vfs));
   CHECK(source.pending && !source.failed && !files[0].reads);
   CHECK(!rdvd_vfs_close(&vfs) && vfs != NULL);
   CHECK(rdvd_vfs_service(vfs));
   CHECK(files[0].reads == 1 && files[0].destination == buffer);
   CHECK(buffer[0] == 0x51 && buffer[7] == 0x51);
   CHECK(rdvd_source_poll(&source, backend) && source.completed == 8);
   CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
   CHECK(rdvd_vfs_service(vfs));
   CHECK(rdvd_source_poll(&source, backend) && source.completed == 40);
   CHECK(files[1].destination == buffer && buffer[31] == 0x72);
   CHECK(!rdvd_vfs_service(vfs));
   CHECK(rdvd_source_init(&source, &backend->map, 0, 32));
   CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
   rdvd_source_cancel(&source);
   CHECK(!rdvd_vfs_close(&vfs));
   CHECK(rdvd_vfs_service(vfs) && rdvd_source_poll(&source, backend));
   CHECK(!source.completed && source.cancelled);
   CHECK(rdvd_source_init(&source, &backend->map, 0, 32));
   files[0].seek_error = true;
   CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
   CHECK(rdvd_vfs_service(vfs) && rdvd_source_poll(&source, backend) && source.failed);
   CHECK(rdvd_vfs_close(&vfs) && !vfs);
   CHECK(rdvd_vfs_close(&vfs));
}
static void test_open_close(void)
{
   const char *paths[2];
   rdvd_vfs_t *vfs;
   rdvd_source_backend_t *backend;
   rdvd_source_t source;
   uint8_t buffer[32];
   memset(opened_files, 0, sizeof(opened_files));
   opened_files[0].size = opened_files[1].size = 2048;
   paths[0] = "one"; paths[1] = "two";
   vfs = rdvd_vfs_open(paths, 2);
   CHECK(vfs != NULL); if (!vfs) return;
   backend = rdvd_vfs_backend(vfs);
   CHECK(rdvd_source_init(&source, &backend->map, 0, 32));
   CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
   CHECK(!rdvd_vfs_close(&vfs) && vfs && !opened_files[0].closes);
   rdvd_source_cancel(&source);
   CHECK(rdvd_vfs_service(vfs) && rdvd_source_poll(&source, backend));
   opened_files[0].close_error = true;
   CHECK(!rdvd_vfs_close(&vfs) && !vfs);
   CHECK(opened_files[0].closes == 1 && opened_files[1].closes == 1);
   CHECK(rdvd_vfs_close(&vfs) && opened_files[0].closes == 1);
   opened_files[0].close_error = false;
   paths[1] = "missing";
   CHECK(!rdvd_vfs_open(paths, 2) && opened_files[0].closes == 2);
   paths[1] = "two"; opened_files[1].size = 2049;
   CHECK(!rdvd_vfs_open(paths, 2));
   CHECK(opened_files[0].closes == 3 && opened_files[1].closes == 2);
   CHECK(!rdvd_vfs_open(paths, 10));
}
static void test_short_and_errors(void)
{
   static uint8_t buffer[131072];
   RFILE file, *handle;
   uint64_t size = sizeof(buffer);
   rdvd_vfs_t *vfs;
   rdvd_source_backend_t *backend;
   rdvd_source_t source;
   memset(&file, 0, sizeof(file)); file.size = size; handle = &file;
   vfs = rdvd_vfs_bind(&handle, &size, 1);
   CHECK(vfs != NULL); if (!vfs) return;
   backend = rdvd_vfs_backend(vfs);
   CHECK(rdvd_source_init(&source, &backend->map, 0, size));
   CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
   CHECK(rdvd_vfs_service(vfs) && rdvd_source_poll(&source, backend));
   CHECK(source.completed == 65536 && source.remaining == 65536);
   file.read_error = true;
   CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
   CHECK(rdvd_vfs_service(vfs) && rdvd_source_poll(&source, backend));
   CHECK(source.failed && source.completed == 65536);
   file.read_error = false; file.size = 16;
   CHECK(rdvd_source_init(&source, &backend->map, 0, 32));
   CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
   CHECK(rdvd_vfs_service(vfs) && rdvd_source_poll(&source, backend) && source.completed == 16);
   CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
   CHECK(rdvd_vfs_service(vfs) && rdvd_source_poll(&source, backend) && source.failed);
   file.oversize = true;
   CHECK(rdvd_source_init(&source, &backend->map, 0, 32));
   CHECK(rdvd_source_submit(&source, backend, buffer, sizeof(buffer)));
   CHECK(rdvd_vfs_service(vfs) && rdvd_source_poll(&source, backend) && source.failed);
   CHECK(rdvd_vfs_close(&vfs) && !file.closes);
}
static void test_stream_lifecycle(void)
{
   RFILE file, *handle;
   uint64_t size = 2048;
   uint8_t buffer[32];
   rdvd_vfs_t *vfs;
   rdvd_source_backend_t *backend;
   rdvd_source_t source;
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0);
   rdvd_stream_t stream;
   rdvd_demux_packet_t packet;
   memset(&file, 0, sizeof(file)); file.size = size; handle = &file;
   vfs = rdvd_vfs_bind(&handle, &size, 1);
   CHECK(vfs && ps); if (!vfs || !ps) return;
   backend = rdvd_vfs_backend(vfs);
   CHECK(rdvd_source_init(&source, &backend->map, 0, 32));
   CHECK(rdvd_stream_init(&stream, &source, vfs, (tpool_t*)vfs, ps, buffer, sizeof(buffer)));
   CHECK(rdvd_stream_next(&stream, &packet) == RDVD_STREAM_WAIT && !file.reads);
   CHECK(rdvd_stream_next(&stream, NULL) == RDVD_STREAM_WAIT && source.pending);
   rdvd_stream_cancel(&stream);
   CHECK(rdvd_stream_next(&stream, &packet) == RDVD_STREAM_WAIT);
   CHECK(!rdvd_vfs_close(&vfs));
   CHECK(rdvd_vfs_service(vfs));
   CHECK(rdvd_stream_next(&stream, &packet) == RDVD_STREAM_CANCELLED && !source.pending);
   CHECK(rdvd_source_init(&source, &backend->map, 0, 32));
   CHECK(rdvd_stream_init(&stream, &source, vfs, (tpool_t*)vfs, ps, buffer, sizeof(buffer)));
   file.read_error = true;
   CHECK(rdvd_stream_next(&stream, &packet) == RDVD_STREAM_WAIT);
   CHECK(rdvd_vfs_service(vfs));
   CHECK(rdvd_stream_next(&stream, &packet) == RDVD_STREAM_ERROR && !source.pending);
   CHECK(rdvd_vfs_close(&vfs)); rmpeg1_ps_free(ps);
}
int main(void)
{
   test_reads();
   test_open_close();
   test_short_and_errors();
   test_stream_lifecycle();
   printf("DVD VFS tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
