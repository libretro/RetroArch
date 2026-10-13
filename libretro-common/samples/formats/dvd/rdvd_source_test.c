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
int main(void)
{
   test_prepare();
   printf("DVD source tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
