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
   put16(228, 236); put16(236, 1); /* Command-only first-play chains. */
   CHECK(rdvd_pgc_read(bytes, 252, &p) && !p.programs && !p.cells && p.command_counts[0] == 1);
}

static void test_accessors(void)
{
   rdvd_pgc_t p;
   rdvd_cell_t cell;
   const uint8_t *command;
   uint32_t color;
   unsigned first, last, i, n;
   fixture();
   for (i = 0; i < 3; i++)
   {
      unsigned play = 238 + i * 24, position = 310 + i * 4;
      bytes[play + 11] = (uint8_t)(10 + i * 20);
      bytes[play + 19] = (uint8_t)(15 + i * 20);
      bytes[play + 23] = (uint8_t)(20 + i * 20);
      put16(position, 7); bytes[position + 3] = (uint8_t)(i + 1);
      for (n = 0; n < 8; n++) bytes[330 + 8 * i + n] = (uint8_t)(0xa0 + i * 8 + n);
   }
   bytes[165] = 16; bytes[166] = bytes[167] = 128;
   bytes[225] = 235; bytes[226] = bytes[227] = 128;
   CHECK(rdvd_pgc_read(bytes, 354, &p));
   CHECK(rdvd_pgc_program_cells(&p, 1, &first, &last) && first == 1 && last == 2);
   CHECK(rdvd_pgc_program_cells(&p, 2, &first, &last) && first == 3 && last == 3);
   CHECK(!rdvd_pgc_program_cells(&p, 3, &first, &last) && !first && !last);
   for (i = 0; i < 3; i++)
   {
      CHECK(rdvd_pgc_cell_read(&p, i + 1, &cell));
      CHECK(cell.first_sector == 10 + i * 20 && cell.last_sector == 20 + i * 20);
      CHECK(cell.vob_id == 7 && cell.cell_id == i + 1);
      CHECK(rdvd_pgc_command_read(&p, (enum rdvd_pgc_command_kind)i, 0, &command));
      CHECK(command == bytes + 330 + 8 * i);
      for (n = 0; n < 8; n++) CHECK(command[n] == 0xa0 + i * 8 + n);
   }
   CHECK(rdvd_pgc_palette_read(&p, 0, &color) && color == UINT32_C(0x00108080));
   CHECK(rdvd_pgc_palette_read(&p, 15, &color) && color == UINT32_C(0x00eb8080));
   CHECK(!rdvd_pgc_palette_read(&p, 16, &color) && !color);
   CHECK(!rdvd_pgc_command_read(&p, RDVD_PGC_PRE, 1, &command) && !command);
   CHECK(!rdvd_pgc_command_read(&p, (enum rdvd_pgc_command_kind)-1, 0, &command));
   CHECK(!rdvd_pgc_cell_read(&p, 0, &cell));
   CHECK(!rdvd_pgc_cell_read(&p, 4, &cell));
   bytes[238 + 23] = 9;
   CHECK(!rdvd_pgc_cell_read(&p, 1, &cell) && !cell.vob_id);
   bytes[238 + 23] = 20; bytes[238 + 3] = 2;
   CHECK(!rdvd_pgc_cell_read(&p, 1, &cell));
   p.offsets[0] = (unsigned)-1; /* Accessors re-read the immutable wire header. */
   CHECK(rdvd_pgc_command_read(&p, RDVD_PGC_PRE, 0, &command) && command == bytes + 330);
}

static void test_table(void)
{
   static uint8_t table[1024];
   rdvd_pgci_table_t t;
   rdvd_pgci_entry_t e;
   const uint8_t *command;
   unsigned n;
   fixture();
   memset(table, 0, sizeof(table));
   table[1] = 3; table[6] = 3; table[7] = 0x61; /* 866-byte table. */
   table[8] = 0x83; table[11] = 0xff;
   table[14] = 2; /* Entry zero starts at 512. */
   table[23] = 32; /* Entry one starts earlier, at 32. */
   table[30] = 2; /* Entry two shares 512. */
   memcpy(table + 32, bytes, 354); memcpy(table + 512, bytes, 354);
   CHECK(rdvd_pgci_table_read(table, sizeof(table), &t) && t.size == 866 && t.entries == 3);
   CHECK(rdvd_pgci_entry_read(&t, 0, &e));
   CHECK(e.pgc.data == table + 512 && e.pgc.size == 354);
   CHECK(e.entry_id == 0x83 && e.parental_mask == 0xff);
   CHECK(rdvd_pgc_command_read(&e.pgc, RDVD_PGC_PRE, 0, &command));
   CHECK(command == table + 512 + 330);
   CHECK(rdvd_pgci_entry_read(&t, 1, &e) && e.pgc.data == table + 32 && e.pgc.size == 480);
   CHECK(rdvd_pgci_entry_read(&t, 2, &e) && e.pgc.data == table + 512);
   CHECK(!rdvd_pgci_entry_read(&t, 3, &e) && !e.pgc.data);
   for (n = 0; n < 866; n++) CHECK(!rdvd_pgci_table_read(table, n, &t));
   CHECK(rdvd_pgci_table_read(table, sizeof(table), &t));
   table[31] = 1; /* A later record at 513 bounds the selected record to one byte. */
   CHECK(!rdvd_pgci_entry_read(&t, 0, &e));
   table[14] = 0; table[15] = 1;
   CHECK(!rdvd_pgci_table_read(table, sizeof(table), &t));
   table[4] = table[5] = table[6] = table[7] = 0xff;
   CHECK(!rdvd_pgci_table_read(table, sizeof(table), &t));
   memset(table, 0, sizeof(table)); table[7] = 7;
   CHECK(rdvd_pgci_table_read(table, 8, &t) && !t.entries);
   CHECK(!rdvd_pgci_entry_read(&t, 0, &e));
   CHECK(!rdvd_pgci_table_read(NULL, 8, &t));
   CHECK(!rdvd_pgci_entry_read(NULL, 0, &e));
}
int main(void)
{
   test_bounds();
   test_accessors();
   test_table();
   printf("DVD IFO tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
