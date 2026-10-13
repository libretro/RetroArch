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
int main(void)
{
   test_prepare();
   test_completion();
   printf("DVD source tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
