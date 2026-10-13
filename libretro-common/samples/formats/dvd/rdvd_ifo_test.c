/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <string.h>
#include <formats/rdvd_ifo.h>
static unsigned failures;
static uint8_t bytes[1024];
static uint8_t file_bytes[8192];
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

static void file_fixture(bool vts)
{
   memset(file_bytes, 0, sizeof(file_bytes));
   memcpy(file_bytes, vts ? "DVDVIDEO-VTS" : "DVDVIDEO-VMG", 12);
   file_bytes[0x1f] = 3; file_bytes[0x82] = 3; file_bytes[0x83] = 0xff;
   file_bytes[0x3f] = 1;
   file_bytes[vts ? 0xcb : 0xc7] = 1;
   file_bytes[vts ? 0xcf : 0xcb] = 2;
   if (vts) file_bytes[0xd3] = 3;
}
static void test_file(void)
{
   rdvd_ifo_t f;
   rdvd_ifo_span_t span;
   unsigned i;
   file_fixture(false);
   CHECK(rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f) && f.kind == RDVD_IFO_VMG);
   CHECK(f.title_sets == 1 && f.data == file_bytes);
   CHECK(rdvd_ifo_table_read(&f, RDVD_IFO_TITLES, &span));
   CHECK(span.data == file_bytes + 2048 && span.size == 2048);
   CHECK(!rdvd_ifo_table_read(&f, RDVD_IFO_PGCIT, &span) && !span.data);
   for (i = 0; i < sizeof(file_bytes); i++) CHECK(!rdvd_ifo_read(file_bytes, i, &f));
   file_fixture(true);
   CHECK(rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f) && f.kind == RDVD_IFO_VTS);
   CHECK(rdvd_ifo_table_read(&f, RDVD_IFO_PGCIT, &span) && span.data == file_bytes + 4096 && span.size == 2048);
   CHECK(rdvd_ifo_table_read(&f, RDVD_IFO_MENU, &span) && span.data == file_bytes + 6144);
   file_bytes[0xcf] = 4;
   CHECK(!rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f));
   file_fixture(true); file_bytes[0x1c] = 0xff;
   CHECK(!rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f));
   file_fixture(false); file_bytes[0] = 0;
   CHECK(!rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f));
}
static void test_titles(void)
{
   rdvd_ifo_t f;
   rdvd_title_t title;
   rdvd_pgc_t pgc;
   uint8_t *table;
   file_fixture(false);
   table = file_bytes + 2048;
   table[1] = 1; table[7] = 19;
   table[9] = 1; table[14] = 1; table[15] = 2;
   table[16] = 0xff; /* External sector need not fit the IFO. */
   CHECK(rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f));
   CHECK(rdvd_ifo_title_read(&f, 1, &title));
   CHECK(title.angles == 1 && title.chapters == 0 && title.local_title == 2);
   CHECK(title.title_set_sector == UINT32_C(0xff000000));
   CHECK(!rdvd_ifo_title_read(&f, 0, &title) && !title.title_set);
   CHECK(!rdvd_ifo_title_read(&f, 2, &title));
   table[9] = 10;
   CHECK(!rdvd_ifo_title_read(&f, 1, &title));
   table[9] = 1; table[7] = 18;
   CHECK(!rdvd_ifo_title_read(&f, 1, &title));
   table[7] = 19; table[14] = 2;
   CHECK(!rdvd_ifo_title_read(&f, 1, &title));
   CHECK(!rdvd_ifo_first_play_read(&f, &pgc) && !pgc.data);
   file_bytes[0x86] = 2; /* First-play begins at byte 512. */
   CHECK(rdvd_ifo_first_play_read(&f, &pgc));
   CHECK(pgc.data == file_bytes + 512 && pgc.size == 1536);
   file_bytes[512 + 2] = 1; /* No program map. */
   CHECK(!rdvd_ifo_first_play_read(&f, &pgc) && !pgc.data);
   file_fixture(true);
   CHECK(rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f));
   CHECK(!rdvd_ifo_title_read(&f, 1, &title));
   CHECK(!rdvd_ifo_first_play_read(&f, &pgc));
}
static void test_chapters(void)
{
   rdvd_ifo_t f;
   rdvd_chapter_t chapter;
   uint8_t *ptt, *chains;
   file_fixture(true); fixture();
   ptt = file_bytes + 2048; chains = file_bytes + 4096;
   ptt[1] = 3; ptt[7] = 27;
   ptt[11] = 24; ptt[15] = 20; ptt[19] = 24;
   ptt[21] = 1; ptt[23] = 1; ptt[25] = 1; ptt[27] = 2;
   chains[1] = 1; chains[6] = 1; chains[7] = 0x71;
   chains[15] = 16; memcpy(chains + 16, bytes, 354);
   CHECK(rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f));
   CHECK(rdvd_ifo_chapter_read(&f, 1, 1, &chapter));
   CHECK(chapter.pgc_number == 1 && chapter.program_number == 2);
   CHECK(chapter.first_cell == 3 && chapter.last_cell == 3);
   CHECK(chapter.pgc.data == chains + 16);
   CHECK(rdvd_ifo_chapter_read(&f, 2, 1, &chapter));
   CHECK(chapter.first_cell == 1 && chapter.last_cell == 2);
   CHECK(rdvd_ifo_chapter_read(&f, 3, 1, &chapter));
   CHECK(!rdvd_ifo_chapter_read(&f, 1, 2, &chapter) && !chapter.pgc.data);
   CHECK(!rdvd_ifo_chapter_read(&f, 0, 1, &chapter));
   CHECK(!rdvd_ifo_chapter_read(&f, 1, 0, &chapter));
   ptt[25] = 2;
   CHECK(!rdvd_ifo_chapter_read(&f, 1, 1, &chapter));
   ptt[25] = 1; ptt[27] = 3;
   CHECK(!rdvd_ifo_chapter_read(&f, 1, 1, &chapter));
   ptt[27] = 2; ptt[15] = 19;
   CHECK(!rdvd_ifo_chapter_read(&f, 1, 1, &chapter));
   ptt[15] = 23;
   CHECK(!rdvd_ifo_chapter_read(&f, 2, 1, &chapter));
}
static uint8_t menu_bytes[1024];
static void menu_fixture(void)
{
   memset(menu_bytes, 0, sizeof(menu_bytes));
   menu_bytes[1] = 3; menu_bytes[6] = 3; menu_bytes[7] = 0xff;
   menu_bytes[8] = 'e'; menu_bytes[9] = 'n'; menu_bytes[11] = 0xf8;
   menu_bytes[14] = 2; /* English at 512. */
   menu_bytes[16] = 'f'; menu_bytes[17] = 'r'; menu_bytes[23] = 32;
   menu_bytes[24] = 'e'; menu_bytes[25] = 'n'; menu_bytes[30] = 2;
}
static void test_menu_table(void)
{
   rdvd_menu_table_t table;
   unsigned i;
   menu_fixture();
   CHECK(rdvd_menu_table_read(menu_bytes, sizeof(menu_bytes), &table));
   CHECK(table.data == menu_bytes && table.size == 1024 && table.languages == 3);
   for (i = 0; i < sizeof(menu_bytes); i++)
      CHECK(!rdvd_menu_table_read(menu_bytes, i, &table) && !table.data);
   menu_bytes[23] = 31;
   CHECK(!rdvd_menu_table_read(menu_bytes, sizeof(menu_bytes), &table));
   menu_fixture(); menu_bytes[14] = 4;
   CHECK(!rdvd_menu_table_read(menu_bytes, sizeof(menu_bytes), &table));
   menu_fixture(); memset(menu_bytes + 4, 0xff, 4);
   CHECK(!rdvd_menu_table_read(menu_bytes, sizeof(menu_bytes), &table));
   memset(menu_bytes, 0, sizeof(menu_bytes)); menu_bytes[7] = 7;
   CHECK(rdvd_menu_table_read(menu_bytes, 8, &table) && !table.languages);
   CHECK(!rdvd_menu_table_read(NULL, 8, &table));
}
static void menu_chains_fixture(void)
{
   uint8_t *chains;
   menu_fixture(); fixture();
   chains = menu_bytes + 512;
   chains[1] = 1; chains[6] = 1; chains[7] = 0x71;
   chains[8] = 0x83; chains[15] = 16;
   memcpy(chains + 16, bytes, 354);
   chains = menu_bytes + 32;
   chains[1] = 1; chains[6] = 1; chains[7] = 0x71;
   chains[8] = 0x83; chains[15] = 16;
   memcpy(chains + 16, bytes, 354);
}
static void test_menu_languages(void)
{
   rdvd_menu_table_t table;
   rdvd_menu_language_t language;
   rdvd_pgci_entry_t entry;
   menu_chains_fixture();
   CHECK(rdvd_menu_table_read(menu_bytes, sizeof(menu_bytes), &table));
   CHECK(rdvd_menu_language_read(&table, 0, &language));
   CHECK(language.language_code == 0x656e && language.menu_flags == 0xf8);
   CHECK(language.chains.data == menu_bytes + 512 && language.chains.size == 370);
   CHECK(rdvd_pgci_entry_read(&language.chains, 0, &entry));
   CHECK(entry.pgc.data == menu_bytes + 528);
   CHECK(rdvd_menu_language_read(&table, 1, &language));
   CHECK(language.language_code == 0x6672 && language.chains.data == menu_bytes + 32);
   CHECK(rdvd_menu_language_read(&table, 2, &language));
   CHECK(language.chains.data == menu_bytes + 512);
   CHECK(!rdvd_menu_language_read(&table, 3, &language) && !language.chains.data);
   table.languages = 99; /* Descriptor counts are not trusted. */
   CHECK(!rdvd_menu_language_read(&table, 3, &language));
   menu_bytes[31] = 1; /* Shared offset moves to 513: bounds previous record. */
   CHECK(!rdvd_menu_language_read(&table, 0, &language));
   CHECK(!rdvd_menu_language_read(NULL, 0, &language));
}
static void test_menu_lookup(void)
{
   rdvd_ifo_t f;
   rdvd_pgci_entry_t entry;
   const uint8_t *command;
   uint8_t *chains;
   file_fixture(true); menu_chains_fixture();
   memcpy(file_bytes + 6144, menu_bytes, sizeof(menu_bytes));
   chains = file_bytes + 6144 + 512;
   CHECK(rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f));
   CHECK(rdvd_ifo_menu_read(&f, 0, RDVD_MENU_ROOT, &entry));
   CHECK(entry.pgc.data == chains + 16);
   CHECK(rdvd_pgc_command_read(&entry.pgc, RDVD_PGC_PRE, 0, &command));
   CHECK(command == chains + 16 + 330);
   CHECK(!rdvd_ifo_menu_read(&f, 0, RDVD_MENU_AUDIO, &entry) && !entry.pgc.data);
   CHECK(!rdvd_ifo_menu_read(&f, 3, RDVD_MENU_ROOT, &entry));
   CHECK(!rdvd_ifo_menu_read(&f, 0, RDVD_MENU_TITLE, &entry));
   chains[8] = 3; /* Non-entry chains do not advertise a menu. */
   CHECK(!rdvd_ifo_menu_read(&f, 0, RDVD_MENU_ROOT, &entry));
   chains[8] = 0x83; chains[16 + 2] = 255;
   CHECK(!rdvd_ifo_menu_read(&f, 0, RDVD_MENU_ROOT, &entry) && !entry.pgc.data);
   file_fixture(false); menu_chains_fixture();
   memcpy(file_bytes + 4096, menu_bytes, sizeof(menu_bytes));
   file_bytes[4096 + 512 + 8] = 0x82;
   CHECK(rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f));
   CHECK(rdvd_ifo_menu_read(&f, 0, RDVD_MENU_TITLE, &entry));
   CHECK(!rdvd_ifo_menu_read(&f, 0, RDVD_MENU_ROOT, &entry));
   CHECK(!rdvd_ifo_menu_read(&f, 0, (enum rdvd_menu_id)-1, &entry));
   CHECK(!rdvd_ifo_menu_read(NULL, 0, RDVD_MENU_TITLE, &entry));
}
static void test_backup(void)
{
   static uint8_t backup[8192];
   rdvd_ifo_t selected;
   bool used;
   file_fixture(false); memcpy(backup, file_bytes, sizeof(backup));
   CHECK(rdvd_ifo_select(file_bytes, sizeof(file_bytes), backup, sizeof(backup),
            RDVD_IFO_VMG, &selected, &used));
   CHECK(selected.data == file_bytes && !used);
   file_bytes[0] = 0;
   CHECK(rdvd_ifo_select(file_bytes, sizeof(file_bytes), backup, sizeof(backup),
            RDVD_IFO_VMG, &selected, &used));
   CHECK(selected.data == backup && used);
   CHECK(rdvd_ifo_select(NULL, 0, backup, sizeof(backup),
            RDVD_IFO_VMG, &selected, NULL));
   CHECK(!rdvd_ifo_select(NULL, 0, backup, 2048,
            RDVD_IFO_VMG, &selected, &used) && !selected.data && !used);
   file_fixture(true);
   CHECK(rdvd_ifo_select(file_bytes, sizeof(file_bytes), backup, sizeof(backup),
            RDVD_IFO_VMG, &selected, &used) && used);
   CHECK(!rdvd_ifo_select(NULL, 0, backup, sizeof(backup),
            RDVD_IFO_VTS, &selected, &used) && !used);
   CHECK(!rdvd_ifo_select(NULL, 0, backup, sizeof(backup),
            RDVD_IFO_NONE, &selected, &used));
   CHECK(!rdvd_ifo_select(NULL, 0, backup, sizeof(backup),
            RDVD_IFO_VMG, NULL, &used) && !used);
}
static void test_language_find(void)
{
   rdvd_ifo_t f;
   unsigned index;
   file_fixture(true); menu_chains_fixture();
   menu_bytes[26] = 1;
   memcpy(file_bytes + 6144, menu_bytes, sizeof(menu_bytes));
   CHECK(rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f));
   CHECK(rdvd_ifo_menu_language_find(&f, 0x656e, 0, &index) && index == 0);
   CHECK(rdvd_ifo_menu_language_find(&f, 0x6672, 0, &index) && index == 1);
   CHECK(rdvd_ifo_menu_language_find(&f, 0x656e, 1, &index) && index == 2);
   CHECK(!rdvd_ifo_menu_language_find(&f, 0x6465, 0, &index) && !index);
   CHECK(!rdvd_ifo_menu_language_find(&f, 0x656e, 2, &index));
   CHECK(!rdvd_ifo_menu_language_find(&f, 65536, 0, &index));
   CHECK(!rdvd_ifo_menu_language_find(&f, 0x656e, 256, &index));
   file_bytes[6144 + 512 + 4] = 0xff;
   CHECK(!rdvd_ifo_menu_language_find(&f, 0x656e, 0, &index));
   CHECK(rdvd_ifo_menu_language_find(&f, 0x6672, 0, &index) && index == 1);
   CHECK(!rdvd_ifo_menu_language_find(NULL, 0, 0, &index));
   CHECK(!rdvd_ifo_menu_language_find(&f, 0, 0, NULL));
}
static void test_file_names(void)
{
   char name[16];
   unsigned n;
   memset(name, 'x', sizeof(name));
   CHECK(rdvd_file_name(0, 0, RDVD_FILE_IFO, name, sizeof(name)));
   CHECK(!strcmp(name, "VIDEO_TS.IFO") && name[13] == 'x');
   CHECK(rdvd_file_name(0, 0, RDVD_FILE_BUP, name, sizeof(name)));
   CHECK(!strcmp(name, "VIDEO_TS.BUP"));
   CHECK(rdvd_file_name(0, 0, RDVD_FILE_VOB, name, sizeof(name)));
   CHECK(!strcmp(name, "VIDEO_TS.VOB"));
   CHECK(rdvd_file_name(1, 0, RDVD_FILE_IFO, name, sizeof(name)));
   CHECK(!strcmp(name, "VTS_01_0.IFO"));
   CHECK(rdvd_file_name(99, 0, RDVD_FILE_BUP, name, sizeof(name)));
   CHECK(!strcmp(name, "VTS_99_0.BUP"));
   CHECK(rdvd_file_name(12, 9, RDVD_FILE_VOB, name, sizeof(name)));
   CHECK(!strcmp(name, "VTS_12_9.VOB"));
   for (n = 1; n < 13; n++)
      CHECK(!rdvd_file_name(1, 0, RDVD_FILE_IFO, name, n) && !name[0]);
   CHECK(!rdvd_file_name(100, 0, RDVD_FILE_IFO, name, sizeof(name)));
   CHECK(!rdvd_file_name(1, 10, RDVD_FILE_VOB, name, sizeof(name)));
   CHECK(!rdvd_file_name(0, 1, RDVD_FILE_VOB, name, sizeof(name)));
   CHECK(!rdvd_file_name(1, 1, RDVD_FILE_BUP, name, sizeof(name)));
   CHECK(!rdvd_file_name(1, 0, (enum rdvd_file_kind)-1, name, sizeof(name)));
   CHECK(!rdvd_file_name(1, 0, RDVD_FILE_IFO, NULL, 13));
   CHECK(!rdvd_file_name(1, 0, RDVD_FILE_IFO, name, 0));
}
static void test_vob_map(void)
{
   uint64_t sizes[9];
   rdvd_vob_map_t map;
   unsigned i;
   for (i = 0; i < 9; i++) sizes[i] = (uint64_t)1073741824;
   CHECK(rdvd_vob_map_read(sizes, 9, &map));
   CHECK(map.sizes == sizes && map.segments == 9);
   CHECK(map.total_bytes == (uint64_t)1073741824 * 9);
   CHECK(!rdvd_vob_map_read(sizes, 10, &map) && !map.sizes);
   CHECK(!rdvd_vob_map_read(sizes, 0, &map));
   sizes[0] = 0;
   CHECK(!rdvd_vob_map_read(sizes, 1, &map));
   sizes[0] = 2049;
   CHECK(!rdvd_vob_map_read(sizes, 1, &map));
   sizes[0] = UINT64_MAX - 2047; sizes[1] = 2048;
   CHECK(!rdvd_vob_map_read(sizes, 2, &map));
   CHECK(!rdvd_vob_map_read(NULL, 1, &map));
   CHECK(!rdvd_vob_map_read(sizes, 1, NULL));
}
static void test_vob_spans(void)
{
   uint64_t sizes[3];
   uint64_t offset, left, read;
   rdvd_vob_map_t map;
   rdvd_vob_span_t span;
   sizes[0] = 4096; sizes[1] = 2048; sizes[2] = 4096;
   CHECK(rdvd_vob_map_read(sizes, 3, &map));
   CHECK(rdvd_vob_span_read(&map, 4095, 10, &span));
   CHECK(span.segment == 1 && span.file_offset == 4095 && span.bytes == 1);
   CHECK(rdvd_vob_span_read(&map, 4096, 10, &span));
   CHECK(span.segment == 2 && !span.file_offset && span.bytes == 10);
   CHECK(rdvd_vob_span_read(&map, 10239, UINT64_MAX, &span) && span.bytes == 1);
   CHECK(!rdvd_vob_span_read(&map, 10240, 1, &span) && !span.segment);
   CHECK(!rdvd_vob_span_read(&map, UINT64_MAX, 1, &span));
   CHECK(!rdvd_vob_span_read(&map, 0, 0, &span));
   offset = 2048; left = 8192; read = 0;
   while (left && rdvd_vob_span_read(&map, offset, left, &span))
   {
      offset += span.bytes; left -= span.bytes; read += span.bytes;
   }
   CHECK(!left && read == 8192 && offset == 10240);
   map.total_bytes = 1;
   CHECK(rdvd_vob_span_read(&map, 6144, 2048, &span) && span.segment == 3);
   sizes[1] = 2049;
   CHECK(!rdvd_vob_span_read(&map, 0, 1, &span) && !span.bytes);
   CHECK(!rdvd_vob_span_read(NULL, 0, 1, &span));
}
static void test_vob_cells(void)
{
   uint64_t sizes[2];
   rdvd_vob_map_t map;
   rdvd_pgc_t pgc;
   rdvd_vob_range_t range;
   rdvd_vob_span_t span;
   fixture();
   bytes[249] = 10; bytes[257] = 15; bytes[261] = 20;
   put16(310, 1); bytes[313] = 1;
   sizes[0] = 16 * 2048; sizes[1] = 5 * 2048;
   CHECK(rdvd_pgc_read(bytes, 354, &pgc));
   CHECK(rdvd_vob_map_read(sizes, 2, &map));
   CHECK(rdvd_vob_cell_range(&map, &pgc, 1, &range));
   CHECK(range.offset == 10 * 2048 && range.bytes == 11 * 2048);
   CHECK(rdvd_vob_span_read(&map, range.offset, range.bytes, &span));
   CHECK(span.segment == 1 && span.bytes == 6 * 2048);
   CHECK(rdvd_vob_span_read(&map, range.offset + span.bytes,
            range.bytes - span.bytes, &span));
   CHECK(span.segment == 2 && span.bytes == 5 * 2048 && !span.file_offset);
   sizes[1] = 4 * 2048;
   CHECK(!rdvd_vob_cell_range(&map, &pgc, 1, &range) && !range.bytes);
   sizes[1] = 5 * 2048;
   CHECK(!rdvd_vob_cell_range(&map, &pgc, 0, &range));
   CHECK(!rdvd_vob_cell_range(&map, &pgc, 2, &range));
   memset(bytes + 258, 0xff, 4);
   sizes[0] = ((uint64_t)UINT32_MAX + 1) * 2048; sizes[1] = 2048;
   CHECK(rdvd_vob_cell_range(&map, &pgc, 1, &range));
   CHECK(range.offset + range.bytes == ((uint64_t)UINT32_MAX + 1) * 2048);
   CHECK(!rdvd_vob_cell_range(NULL, &pgc, 1, &range));
   CHECK(!rdvd_vob_cell_range(&map, NULL, 1, &range));
}
static void test_address_domains(void)
{
   rdvd_ifo_t f;
   rdvd_ifo_span_t span;
   file_fixture(false);
   file_bytes[0xdb] = 1; file_bytes[0xdf] = 3;
   CHECK(rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f));
   CHECK(rdvd_ifo_table_read(&f, RDVD_IFO_MENU_CELLS, &span));
   CHECK(span.data == file_bytes + 2048 && span.size == 2048);
   CHECK(rdvd_ifo_table_read(&f, RDVD_IFO_MENU_VOBUS, &span));
   CHECK(span.data == file_bytes + 6144);
   CHECK(!rdvd_ifo_table_read(&f, RDVD_IFO_TITLE_CELLS, &span));
   CHECK(!rdvd_ifo_table_read(&f, RDVD_IFO_TITLE_VOBUS, &span));
   file_fixture(true);
   file_bytes[0xdb] = 1; file_bytes[0xdf] = 2;
   file_bytes[0xe3] = 3; file_bytes[0xe7] = 1;
   CHECK(rdvd_ifo_read(file_bytes, sizeof(file_bytes), &f));
   CHECK(rdvd_ifo_table_read(&f, RDVD_IFO_MENU_CELLS, &span) && span.data == file_bytes + 2048);
   CHECK(rdvd_ifo_table_read(&f, RDVD_IFO_MENU_VOBUS, &span) && span.data == file_bytes + 4096);
   CHECK(rdvd_ifo_table_read(&f, RDVD_IFO_TITLE_CELLS, &span) && span.data == file_bytes + 6144);
   CHECK(rdvd_ifo_table_read(&f, RDVD_IFO_TITLE_VOBUS, &span) && span.data == file_bytes + 2048);
   file_bytes[0xe7] = 0;
   CHECK(!rdvd_ifo_table_read(&f, RDVD_IFO_TITLE_VOBUS, &span) && !span.data);
}
static void test_cell_addresses(void)
{
   uint8_t data[32];
   rdvd_cell_table_t table;
   rdvd_cell_address_t address;
   unsigned i;
   memset(data, 0, sizeof(data)); data[1] = 1; data[7] = 31;
   data[9] = data[10] = data[21] = 1; data[22] = 2;
   data[15] = 10; data[19] = 20;
   data[27] = 30; data[31] = 40;
   CHECK(rdvd_cell_table_read(data, sizeof(data), &table));
   CHECK(table.data == data && table.entries == 2 && table.vobs == 1);
   CHECK(rdvd_cell_address_read(&table, 1, &address));
   CHECK(address.vob_id == 1 && address.cell_id == 2);
   CHECK(address.first_sector == 30 && address.last_sector == 40);
   CHECK(!rdvd_cell_address_read(&table, 2, &address) && !address.vob_id);
   for (i = 0; i < sizeof(data); i++) CHECK(!rdvd_cell_table_read(data, i, &table));
   CHECK(rdvd_cell_table_read(data, sizeof(data), &table));
   table.entries = 99;
   CHECK(!rdvd_cell_address_read(&table, 2, &address));
   data[31] = 29;
   CHECK(!rdvd_cell_address_read(&table, 0, &address));
   data[31] = 40; data[22] = 0;
   CHECK(!rdvd_cell_table_read(data, sizeof(data), &table));
   data[22] = 2; data[7] = 30;
   CHECK(!rdvd_cell_table_read(data, sizeof(data), &table));
   memset(data, 0, sizeof(data)); data[7] = 7;
   CHECK(rdvd_cell_table_read(data, sizeof(data), &table) && !table.entries);
   CHECK(!rdvd_cell_address_read(&table, 0, &address));
}
static void test_vobu_map(void)
{
   uint8_t data[16];
   rdvd_vobu_map_t map;
   uint32_t sector;
   unsigned index, i;
   memset(data, 0, sizeof(data)); data[3] = 15;
   data[7] = 10; data[11] = 20; data[15] = 30;
   CHECK(rdvd_vobu_map_read(data, sizeof(data), &map) && map.entries == 3);
   CHECK(rdvd_vobu_find(&map, 20, &sector, &index) && sector == 20 && index == 1);
   CHECK(rdvd_vobu_find(&map, 29, &sector, &index) && sector == 20 && index == 1);
   CHECK(rdvd_vobu_find(&map, UINT32_MAX, &sector, &index) && sector == 30 && index == 2);
   CHECK(!rdvd_vobu_find(&map, 9, &sector, &index) && !sector && !index);
   for (i = 0; i < sizeof(data); i++) CHECK(!rdvd_vobu_map_read(data, i, &map));
   CHECK(rdvd_vobu_map_read(data, sizeof(data), &map));
   map.entries = 99;
   CHECK(rdvd_vobu_find(&map, 30, &sector, &index) && index == 2);
   data[15] = 20;
   CHECK(!rdvd_vobu_find(&map, 30, &sector, &index));
   data[15] = 19;
   CHECK(!rdvd_vobu_map_read(data, sizeof(data), &map));
   data[15] = 30; data[3] = 14;
   CHECK(!rdvd_vobu_map_read(data, sizeof(data), &map));
   memset(data, 0, sizeof(data)); data[3] = 7;
   CHECK(rdvd_vobu_map_read(data, 8, &map));
   CHECK(rdvd_vobu_find(&map, 0, &sector, &index) && !sector);
   data[3] = 3;
   CHECK(rdvd_vobu_map_read(data, 4, &map) && !map.entries);
   CHECK(!rdvd_vobu_find(&map, 0, &sector, &index));
   memset(data, 0xff, 4);
   CHECK(!rdvd_vobu_map_read(data, sizeof(data), &map));
   CHECK(!rdvd_vobu_map_read(NULL, 4, &map));
}
static void test_cell_find(void)
{
   uint8_t data[32];
   rdvd_cell_table_t table;
   rdvd_cell_address_t address;
   memset(data, 0, sizeof(data)); data[1] = 1; data[7] = 31;
   data[9] = data[10] = data[21] = 1; data[22] = 2;
   data[15] = 10; data[19] = 20; data[27] = 30; data[31] = 40;
   CHECK(rdvd_cell_table_read(data, sizeof(data), &table));
   CHECK(rdvd_cell_address_find(&table, 1, 2, &address) && address.first_sector == 30);
   CHECK(!rdvd_cell_address_find(&table, 1, 3, &address) && !address.vob_id);
   CHECK(!rdvd_cell_address_find(&table, 0, 1, &address));
   CHECK(!rdvd_cell_address_find(&table, 65536, 1, &address));
   CHECK(!rdvd_cell_address_find(&table, 1, 256, &address));
   data[22] = 1;
   CHECK(!rdvd_cell_address_find(&table, 1, 1, &address));
   data[22] = 2; data[19] = 9;
   CHECK(!rdvd_cell_address_find(&table, 1, 2, &address));
   CHECK(!rdvd_cell_address_find(NULL, 1, 1, &address));
}
static void test_vobu_cell_find(void)
{
   uint8_t data[16];
   rdvd_vobu_map_t map;
   uint32_t sector;
   unsigned index;
   memset(data, 0, sizeof(data)); data[3] = 15;
   data[7] = 10; data[11] = 20; data[15] = 30;
   CHECK(rdvd_vobu_map_read(data, sizeof(data), &map));
   CHECK(rdvd_vobu_find_in_cell(&map, 25, 20, 29, &sector, &index));
   CHECK(sector == 20 && index == 1);
   CHECK(!rdvd_vobu_find_in_cell(&map, 25, 21, 29, &sector, &index) && !sector && !index);
   CHECK(!rdvd_vobu_find_in_cell(&map, 19, 20, 29, &sector, &index));
   CHECK(!rdvd_vobu_find_in_cell(&map, 30, 20, 29, &sector, &index));
   CHECK(!rdvd_vobu_find_in_cell(&map, 25, 29, 20, &sector, &index));
   CHECK(rdvd_vobu_find_in_cell(&map, 20, 20, 20, &sector, &index) && sector == 20);
   CHECK(!rdvd_vobu_find_in_cell(&map, 20, 20, 20, NULL, &index) && !index);
   CHECK(!rdvd_vobu_find_in_cell(&map, 20, 20, 20, &sector, NULL) && !sector);
}
static void test_title_target(void)
{
   static uint8_t file[12288];
   uint64_t sizes[2];
   uint8_t *ptt, *pgcit, *cadt, *vobus;
   rdvd_ifo_t ifo;
   rdvd_vob_map_t source;
   rdvd_title_target_t result;
   file_fixture(true); fixture();
   memset(file, 0, sizeof(file)); memcpy(file, file_bytes, sizeof(file_bytes));
   file[0x1f] = 5; file[0xe3] = 4; file[0xe7] = 5;
   ptt = file + 2048; pgcit = file + 4096;
   cadt = file + 8192; vobus = file + 10240;
   ptt[1] = 1; ptt[7] = 15; ptt[11] = 12; ptt[13] = ptt[15] = 1;
   pgcit[1] = 1; pgcit[6] = 1; pgcit[7] = 0x71; pgcit[15] = 16;
   bytes[249] = 10; bytes[257] = 15; bytes[261] = 20;
   put16(310, 1); bytes[313] = 1;
   memcpy(pgcit + 16, bytes, 354);
   cadt[1] = 1; cadt[7] = 19; cadt[9] = cadt[10] = 1;
   cadt[15] = 10; cadt[19] = 20;
   vobus[3] = 15; vobus[7] = 10; vobus[11] = 15; vobus[15] = 20;
   sizes[0] = 16 * 2048; sizes[1] = 5 * 2048;
   CHECK(rdvd_ifo_read(file, sizeof(file), &ifo));
   CHECK(rdvd_vob_map_read(sizes, 2, &source));
   CHECK(rdvd_ifo_title_target(&ifo, &source, 1, 1, 1, 18, &result));
   CHECK(result.nav_sector == 15 && result.cell.cell_id == 1);
   CHECK(result.span.segment == 1 && result.span.file_offset == 15 * 2048);
   CHECK(result.span.bytes == 2048 && result.remaining.bytes == 6 * 2048);
   CHECK(rdvd_ifo_title_target(&ifo, &source, 1, 1, 1, 20, &result));
   CHECK(result.span.segment == 2 && result.span.file_offset == 4 * 2048);
   CHECK(!rdvd_ifo_title_target(&ifo, &source, 1, 1, 1, 9, &result) && !result.span.bytes);
   CHECK(!rdvd_ifo_title_target(&ifo, &source, 1, 1, 3, 18, &result));
   sizes[1] = 4 * 2048;
   CHECK(!rdvd_ifo_title_target(&ifo, &source, 1, 1, 1, 18, &result));
   sizes[1] = 5 * 2048; cadt[19] = 19;
   CHECK(!rdvd_ifo_title_target(&ifo, &source, 1, 1, 1, 18, &result));
   cadt[19] = 20; cadt[10] = 2;
   CHECK(!rdvd_ifo_title_target(&ifo, &source, 1, 1, 1, 18, &result));
   cadt[10] = 1; pgcit[16 + 238] = 0x10;
   CHECK(!rdvd_ifo_title_target(&ifo, &source, 1, 1, 1, 18, &result));
   pgcit[16 + 238] = 4;
   CHECK(!rdvd_ifo_title_target(&ifo, &source, 1, 1, 1, 18, &result));
   pgcit[16 + 238] = 0; vobus[7] = 9;
   CHECK(!rdvd_ifo_title_target(&ifo, &source, 1, 1, 1, 10, &result));
}
int main(void)
{
   test_bounds();
   test_accessors();
   test_table();
   test_file();
   test_titles();
   test_chapters();
   test_menu_table();
   test_menu_languages();
   test_menu_lookup();
   test_backup();
   test_language_find();
   test_file_names();
   test_vob_map();
   test_vob_spans();
   test_vob_cells();
   test_address_domains();
   test_cell_addresses();
   test_vobu_map();
   test_cell_find();
   test_vobu_cell_find();
   test_title_target();
   printf("DVD IFO tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
