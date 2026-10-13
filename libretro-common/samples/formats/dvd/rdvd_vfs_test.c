/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <formats/rdvd_vfs.h>
struct RFILE
{
   uint64_t size, position;
   unsigned tag, reads;
   bool seek_error, read_error, oversize;
   void *destination;
};
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
int64_t filestream_seek(RFILE *file, int64_t offset, int whence)
{
   if (file->seek_error || offset < 0 || whence != RETRO_VFS_SEEK_POSITION_START) return -1;
   file->position = (uint64_t)offset;
   return offset;
}
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
int main(void)
{
   test_reads();
   printf("DVD VFS tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
