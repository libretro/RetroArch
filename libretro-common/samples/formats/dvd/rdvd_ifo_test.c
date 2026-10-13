/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <formats/rdvd_ifo.h>
static unsigned failures;
static uint8_t bytes[1024];
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)

static void put16(unsigned offset, unsigned value)
{
   bytes[offset] = (uint8_t)(value >> 8); bytes[offset + 1] = (uint8_t)value;
}
static void fixture(void)
{
   memset(bytes, 0, sizeof(bytes));
   bytes[2] = 2; bytes[3] = 3;
   put16(228, 322); put16(230, 236); put16(232, 238); put16(234, 310);
   bytes[236] = 1; bytes[237] = 3;
   put16(322, 1); put16(324, 1); put16(326, 1);
}
static void test_bounds(void)
{
   rdvd_pgc_t p;
   unsigned i;
   fixture();
   CHECK(rdvd_pgc_read(bytes, 354, &p) && p.data == bytes);
   CHECK(p.programs == 2 && p.cells == 3 && p.command_counts[2] == 1);
   for (i = 0; i < 354; i++)
      CHECK(!rdvd_pgc_read(bytes, i, &p) && !p.data);
   fixture(); put16(230, 235);
   CHECK(!rdvd_pgc_read(bytes, 354, &p));
   fixture(); put16(234, 238);
   CHECK(!rdvd_pgc_read(bytes, 354, &p));
   fixture(); put16(322, 65535);
   CHECK(!rdvd_pgc_read(bytes, 354, &p));
   fixture(); bytes[237] = 1;
   CHECK(!rdvd_pgc_read(bytes, 354, &p));
   fixture(); bytes[237] = 4;
   CHECK(!rdvd_pgc_read(bytes, 354, &p));
   fixture(); bytes[236] = 0;
   CHECK(!rdvd_pgc_read(bytes, 354, &p));
   memset(bytes, 0, sizeof(bytes));
   CHECK(rdvd_pgc_read(bytes, 236, &p) && !p.programs && !p.cells);
   CHECK(!rdvd_pgc_read(NULL, 236, &p));
   CHECK(!rdvd_pgc_read(bytes, 236, NULL));
}
int main(void)
{
   test_bounds();
   printf("DVD IFO tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
