/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <formats/rdvd_source.h>
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
static void test_prepare(void)
{
   uint64_t sizes[2];
   uint8_t buffer[32];
   rdvd_vob_map_t map;
   rdvd_source_t source;
   rdvd_source_request_t request;
   sizes[0] = 2048; sizes[1] = 4096;
   CHECK(rdvd_vob_map_read(sizes, 2, &map));
   CHECK(rdvd_source_init(&source, &map, 2040, 100));
   memset(buffer, 0xa5, sizeof(buffer));
   CHECK(rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   CHECK(request.destination == buffer && request.bytes == 8);
   CHECK(request.file_offset == 2040 && request.segment == 1 && request.ticket == 1);
   CHECK(buffer[0] == 0xa5 && buffer[31] == 0xa5);
   CHECK(!rdvd_source_prepare(&source, buffer, sizeof(buffer), &request) && !request.destination);
   CHECK(rdvd_source_complete(&source, source.ticket, 8));
   CHECK(!rdvd_source_init(&source, &map, 6144, 1) && source.failed);
   CHECK(!rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   CHECK(rdvd_source_init(&source, &map, 6144, 0));
   CHECK(!rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   CHECK(!rdvd_source_init(&source, &map, UINT64_MAX, 1));
   CHECK(rdvd_source_init(&source, &map, 0, 1));
   CHECK(!rdvd_source_prepare(&source, NULL, 1, &request));
   CHECK(!rdvd_source_prepare(&source, buffer, 0, &request));
   source.ticket = UINT64_MAX;
   CHECK(!rdvd_source_prepare(&source, buffer, 1, &request) && source.failed);
}
static void test_completion(void)
{
   uint64_t sizes[2], ticket;
   uint8_t buffer[32];
   rdvd_vob_map_t map;
   rdvd_source_t source;
   rdvd_source_request_t request;
   sizes[0] = 2048; sizes[1] = 2048;
   CHECK(rdvd_vob_map_read(sizes, 2, &map));
   CHECK(rdvd_source_init(&source, &map, 2040, 40));
   CHECK(rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   ticket = request.ticket;
   CHECK(!rdvd_source_complete(&source, ticket + 1, 8) && source.pending);
   CHECK(rdvd_source_complete(&source, ticket, 3));
   CHECK(source.completed == 3 && source.remaining == 37 && source.offset == 2043);
   CHECK(!rdvd_source_complete(&source, ticket, 3));
   CHECK(rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   CHECK(request.file_offset == 2043 && request.bytes == 5 && request.ticket != ticket);
   CHECK(!rdvd_source_complete(&source, ticket, 5) && source.pending);
   CHECK(rdvd_source_complete(&source, request.ticket, 5));
   CHECK(rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   CHECK(request.segment == 2 && !request.file_offset && request.bytes == 32);
   CHECK(rdvd_source_complete(&source, request.ticket, 32));
   CHECK(source.completed == 40 && !source.remaining && !source.failed);
   CHECK(!rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   CHECK(rdvd_source_init(&source, &map, 0, 100));
   CHECK(rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   CHECK(rdvd_source_complete(&source, request.ticket, 16));
   CHECK(rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   CHECK(rdvd_source_complete(&source, request.ticket, -1));
   CHECK(source.failed && source.completed == 16 && source.remaining == 84);
   CHECK(rdvd_source_init(&source, &map, 0, 1));
   CHECK(rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   CHECK(rdvd_source_complete(&source, request.ticket, 0) && source.failed);
   CHECK(rdvd_source_init(&source, &map, 0, 1));
   CHECK(rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   CHECK(rdvd_source_complete(&source, request.ticket, 2) && source.failed);
   CHECK(rdvd_source_init(&source, &map, 0, 1));
   CHECK(rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   rdvd_source_cancel(&source);
   CHECK(source.pending && source.cancelled);
   CHECK(rdvd_source_complete(&source, request.ticket, 1));
   CHECK(!source.pending && !source.completed && source.remaining == 1);
   CHECK(!rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   CHECK(rdvd_source_init(&source, &map, 0, 1));
   rdvd_source_cancel(&source);
   CHECK(!rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
   rdvd_source_cancel(NULL);
}
static void test_title_reads(void)
{
   static uint8_t file[12288];
   uint64_t sizes[2];
   uint8_t buffer[2048], *ptt, *pgcit, *pgc, *cadt, *vobus;
   rdvd_ifo_t ifo;
   rdvd_vob_map_t map;
   rdvd_source_t source;
   rdvd_source_request_t request;
   unsigned count = 0;
   memset(file, 0, sizeof(file)); memcpy(file, "DVDVIDEO-VTS", 12);
   file[0x1f] = 5; file[0x83] = 255;
   file[0xcb] = 1; file[0xcf] = 2; file[0xe3] = 4; file[0xe7] = 5;
   ptt = file + 2048; pgcit = file + 4096; pgc = pgcit + 16;
   cadt = file + 8192; vobus = file + 10240;
   ptt[1] = 1; ptt[7] = 15; ptt[11] = 12; ptt[13] = ptt[15] = 1;
   pgcit[1] = 1; pgcit[6] = 1; pgcit[7] = 24; pgcit[15] = 16;
   pgc[2] = pgc[3] = 1; pgc[231] = 236; pgc[233] = 237;
   pgc[234] = 1; pgc[235] = 5; pgc[236] = 1;
   pgc[248] = 10; pgc[256] = 15; pgc[260] = 20;
   pgc[262] = pgc[264] = 1;
   cadt[1] = 1; cadt[7] = 19; cadt[9] = cadt[10] = 1;
   cadt[15] = 10; cadt[19] = 20;
   vobus[3] = 15; vobus[7] = 10; vobus[11] = 15; vobus[15] = 20;
   sizes[0] = 16 * 2048; sizes[1] = 5 * 2048;
   CHECK(rdvd_ifo_read(file, sizeof(file), &ifo));
   CHECK(rdvd_vob_map_read(sizes, 2, &map));
   CHECK(rdvd_source_init_title(&source, &ifo, &map, 1, 1, 1, 18));
   CHECK(source.offset == 15 * 2048 && source.remaining == 6 * 2048);
   while (rdvd_source_prepare(&source, buffer, sizeof(buffer), &request))
   {
      CHECK(request.destination == buffer && request.bytes == 2048);
      CHECK(request.segment == (count ? 2 : 1));
      CHECK(request.file_offset == (count ? (count - 1) * 2048 : 15 * 2048));
      /* Simulate a backend writing directly to the reserved consumer storage. */
      memset(request.destination, (int)request.segment, request.bytes);
      CHECK(rdvd_source_complete(&source, request.ticket, (int64_t)request.bytes));
      CHECK(buffer[0] == request.segment && buffer[2047] == request.segment);
      count++;
   }
   CHECK(count == 6 && source.completed == 6 * 2048 && !source.failed);
   CHECK(!rdvd_source_init_title(&source, &ifo, &map, 1, 1, 1, 9) && source.failed);
   CHECK(!rdvd_source_prepare(&source, buffer, sizeof(buffer), &request));
}
static bool fake_submit(void *context, void *file, uint64_t offset,
      void *destination, size_t bytes, uint64_t ticket)
{
   (void)context; (void)file; (void)offset; (void)destination;
   (void)bytes; (void)ticket;
   return false;
}
static enum rdvd_backend_status fake_poll(void *context, uint64_t *ticket, int64_t *bytes)
{
   (void)context; (void)ticket; (void)bytes;
   return RDVD_BACKEND_PENDING;
}
static void test_backend_init(void)
{
   uint64_t sizes[2];
   unsigned storage[2];
   void *files[2];
   rdvd_backend_ops_t ops;
   rdvd_source_backend_t backend;
   sizes[0] = sizes[1] = 2048;
   files[0] = storage; files[1] = storage + 1;
   ops.submit = fake_submit; ops.poll = fake_poll;
   CHECK(rdvd_source_backend_init(&backend, &ops, NULL, files, sizes, 2));
   CHECK(backend.files == files && backend.map.sizes == sizes);
   CHECK(backend.ops.submit == fake_submit && backend.map.total_bytes == 4096);
   files[1] = NULL;
   CHECK(!rdvd_source_backend_init(&backend, &ops, NULL, files, sizes, 2) && !backend.files);
   files[1] = storage + 1; ops.poll = NULL;
   CHECK(!rdvd_source_backend_init(&backend, &ops, NULL, files, sizes, 2));
   ops.poll = fake_poll; sizes[1] = 2049;
   CHECK(!rdvd_source_backend_init(&backend, &ops, NULL, files, sizes, 2));
   CHECK(!rdvd_source_backend_init(&backend, &ops, NULL, files, sizes, 10));
}
int main(void)
{
   test_prepare();
   test_completion();
   test_title_reads();
   test_backend_init();
   printf("DVD source tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
