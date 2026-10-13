/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <string.h>
#include <formats/rdvd_ifo.h>

static unsigned rdvd_ifo_u16(const uint8_t *p)
{
   return ((unsigned)p[0] << 8) | p[1];
}

static uint32_t rdvd_ifo_u32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
      | ((uint32_t)p[2] << 8) | p[3];
}

bool rdvd_pgc_read(const uint8_t *data, size_t size, rdvd_pgc_t *out)
{
   rdvd_pgc_t p;
   size_t lengths[4], ends[4];
   unsigned i, j, previous = 0;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!data || size < 236) return false;
   memset(&p, 0, sizeof(p));
   p.programs = data[2]; p.cells = data[3];
   if (p.programs > p.cells || (!p.programs && p.cells)) return false;
   for (i = 0; i < 4; i++) p.offsets[i] = rdvd_ifo_u16(data + 228 + 2 * i);
   lengths[0] = 0; lengths[1] = p.programs;
   lengths[2] = p.cells * 24; lengths[3] = p.cells * 4;
   if (p.offsets[0])
   {
      const uint8_t *table;
      if (p.offsets[0] < 236 || p.offsets[0] > size || size - p.offsets[0] < 8)
         return false;
      table = data + p.offsets[0];
      for (i = 0; i < 3; i++) p.command_counts[i] = rdvd_ifo_u16(table + 2 * i);
      lengths[0] = 8 + (size_t)(p.command_counts[0] + p.command_counts[1]
            + p.command_counts[2]) * 8;
   }
   for (i = 0; i < 4; i++)
   {
      ends[i] = 0;
      if (!lengths[i]) continue;
      if (p.offsets[i] < 236 || p.offsets[i] > size || lengths[i] > size - p.offsets[i])
         return false;
      ends[i] = p.offsets[i] + lengths[i];
   }
   for (i = 0; i < 4; i++)
      for (j = i + 1; j < 4; j++)
         if (lengths[i] && lengths[j] && p.offsets[i] < ends[j] && p.offsets[j] < ends[i])
            return false;
   for (i = 0; i < p.programs; i++)
   {
      unsigned start = data[p.offsets[1] + i];
      if (start <= previous || start > p.cells) return false;
      previous = start;
   }
   p.data = data; p.size = size;
   *out = p;
   return true;
}

static bool rdvd_pgc_validate(const rdvd_pgc_t *pgc, rdvd_pgc_t *out)
{
   return pgc && rdvd_pgc_read(pgc->data, pgc->size, out);
}

bool rdvd_pgc_program_cells(const rdvd_pgc_t *pgc, unsigned number,
      unsigned *first, unsigned *last)
{
   rdvd_pgc_t p;
   if (first) *first = 0;
   if (last) *last = 0;
   if (!first || !last || !rdvd_pgc_validate(pgc, &p) || !number || number > p.programs)
      return false;
   *first = p.data[p.offsets[1] + number - 1];
   *last = number == p.programs ? p.cells : (unsigned)p.data[p.offsets[1] + number] - 1;
   return true;
}

bool rdvd_pgc_cell_read(const rdvd_pgc_t *pgc, unsigned number, rdvd_cell_t *out)
{
   rdvd_pgc_t p;
   rdvd_cell_t c;
   const uint8_t *play, *position;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!rdvd_pgc_validate(pgc, &p) || !number || number > p.cells) return false;
   play = p.data + p.offsets[2] + (number - 1) * 24;
   position = p.data + p.offsets[3] + (number - 1) * 4;
   memset(&c, 0, sizeof(c));
   c.first_sector = rdvd_ifo_u32(play + 8);
   c.first_ilvu_end = rdvd_ifo_u32(play + 12);
   c.last_vobu_start = rdvd_ifo_u32(play + 16);
   c.last_sector = rdvd_ifo_u32(play + 20);
   c.vob_id = rdvd_ifo_u16(position);
   c.cell_id = position[3];
   c.still_time = play[2]; c.command_number = play[3];
   c.block_mode = play[0] >> 6; c.block_type = (play[0] >> 4) & 3;
   c.seamless = (play[0] & 8) != 0;
   c.interleaved = (play[0] & 4) != 0;
   c.stc_discontinuity = (play[0] & 2) != 0;
   if (!c.vob_id || !c.cell_id || c.first_sector > c.last_sector
         || c.last_vobu_start < c.first_sector || c.last_vobu_start > c.last_sector
         || (c.interleaved && (c.first_ilvu_end < c.first_sector || c.first_ilvu_end > c.last_sector))
         || c.command_number > p.command_counts[2])
      return false;
   *out = c;
   return true;
}

bool rdvd_pgc_command_read(const rdvd_pgc_t *pgc, enum rdvd_pgc_command_kind kind,
      unsigned index, const uint8_t **out)
{
   rdvd_pgc_t p;
   unsigned base;
   if (!out) return false;
   *out = NULL;
   if (!rdvd_pgc_validate(pgc, &p)) return false;
   switch (kind)
   {
      case RDVD_PGC_PRE: base = 0; break;
      case RDVD_PGC_POST: base = p.command_counts[0]; break;
      case RDVD_PGC_CELL: base = p.command_counts[0] + p.command_counts[1]; break;
      default: return false;
   }
   if (index >= p.command_counts[kind]) return false;
   *out = p.data + p.offsets[0] + 8 + (size_t)(base + index) * 8;
   return true;
}

bool rdvd_pgc_palette_read(const rdvd_pgc_t *pgc, unsigned index, uint32_t *out)
{
   rdvd_pgc_t p;
   if (!out) return false;
   *out = 0;
   if (index >= 16 || !rdvd_pgc_validate(pgc, &p)) return false;
   *out = rdvd_ifo_u32(p.data + 164 + 4 * index);
   return true;
}

bool rdvd_pgci_table_read(const uint8_t *data, size_t size, rdvd_pgci_table_t *out)
{
   size_t total, header;
   uint32_t last;
   unsigned count, i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!data || size < 8) return false;
   last = rdvd_ifo_u32(data + 4);
   if (last == UINT32_MAX) return false;
   total = (size_t)last + 1;
   if (total < 8 || total > size) return false;
   count = rdvd_ifo_u16(data);
   if (count > (total - 8) / 8) return false;
   header = 8 + (size_t)count * 8;
   for (i = 0; i < count; i++)
   {
      uint32_t start = rdvd_ifo_u32(data + 8 + (size_t)i * 8 + 4);
      if (total < 236 || start < header || start > total - 236) return false;
   }
   out->data = data; out->size = total; out->entries = count;
   return true;
}

bool rdvd_pgci_entry_read(const rdvd_pgci_table_t *table, unsigned index,
      rdvd_pgci_entry_t *out)
{
   rdvd_pgci_table_t t;
   rdvd_pgci_entry_t entry;
   const uint8_t *record;
   size_t start, end;
   unsigned i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!table || !rdvd_pgci_table_read(table->data, table->size, &t) || index >= t.entries)
      return false;
   record = t.data + 8 + (size_t)index * 8;
   start = rdvd_ifo_u32(record + 4); end = t.size;
   for (i = 0; i < t.entries; i++)
   {
      size_t candidate = rdvd_ifo_u32(t.data + 8 + (size_t)i * 8 + 4);
      if (candidate > start && candidate < end) end = candidate;
   }
   memset(&entry, 0, sizeof(entry));
   if (!rdvd_pgc_read(t.data + start, end - start, &entry.pgc)) return false;
   entry.entry_id = record[0];
   entry.block_mode = record[1] >> 6;
   entry.block_type = (record[1] >> 4) & 3;
   entry.parental_mask = rdvd_ifo_u16(record + 2);
   *out = entry;
   return true;
}

bool rdvd_ifo_read(const uint8_t *data, size_t size, rdvd_ifo_t *out)
{
   rdvd_ifo_t f;
   uint64_t extent;
   uint32_t management_last;
   unsigned base, i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!data || size < 2048) return false;
   memset(&f, 0, sizeof(f));
   if (!memcmp(data, "DVDVIDEO-VMG", 12))
   {
      f.kind = RDVD_IFO_VMG; f.table_count = 7; base = 0xc4;
      f.title_sets = rdvd_ifo_u16(data + 0x3e);
      if (!f.title_sets || f.title_sets > 99) return false;
      f.first_play = rdvd_ifo_u32(data + 0x84);
   }
   else if (!memcmp(data, "DVDVIDEO-VTS", 12))
   {
      f.kind = RDVD_IFO_VTS; f.table_count = 8; base = 0xc8;
      f.title_vob_sector = rdvd_ifo_u32(data + 0xc4);
   }
   else return false;
   extent = ((uint64_t)rdvd_ifo_u32(data + 0x1c) + 1) * 2048;
   if (extent > size || extent > SIZE_MAX) return false;
   f.size = (size_t)extent;
   management_last = rdvd_ifo_u32(data + 0x80);
   if (management_last < 255 || management_last >= f.size) return false;
   if (f.first_play && (f.first_play < 512 || f.first_play > f.size - 236)) return false;
   for (i = 0; i < f.table_count; i++)
   {
      uint64_t offset = (uint64_t)rdvd_ifo_u32(data + base + i * 4) * 2048;
      if (offset && (offset > f.size || f.size - (size_t)offset < 8)) return false;
      f.tables[i] = (size_t)offset;
   }
   f.menu_vob_sector = rdvd_ifo_u32(data + 0xc0);
   f.version = data[0x21]; f.data = data;
   *out = f;
   return true;
}

bool rdvd_ifo_table_read(const rdvd_ifo_t *ifo, enum rdvd_ifo_table_kind kind,
      rdvd_ifo_span_t *out)
{
   rdvd_ifo_t f;
   size_t start, end;
   unsigned index, i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!ifo || !rdvd_ifo_read(ifo->data, ifo->size, &f)) return false;
   switch (kind)
   {
      case RDVD_IFO_TITLES: index = 0; break;
      case RDVD_IFO_PGCIT:
         if (f.kind != RDVD_IFO_VTS) return false;
         index = 1; break;
      case RDVD_IFO_MENU: index = f.kind == RDVD_IFO_VMG ? 1 : 2; break;
      case RDVD_IFO_MENU_CELLS: index = f.kind == RDVD_IFO_VMG ? 5 : 4; break;
      case RDVD_IFO_MENU_VOBUS: index = f.kind == RDVD_IFO_VMG ? 6 : 5; break;
      case RDVD_IFO_TITLE_CELLS:
         if (f.kind != RDVD_IFO_VTS) return false;
         index = 6; break;
      case RDVD_IFO_TITLE_VOBUS:
         if (f.kind != RDVD_IFO_VTS) return false;
         index = 7; break;
      default: return false;
   }
   start = f.tables[index]; end = f.size;
   if (!start) return false;
   for (i = 0; i < f.table_count; i++)
      if (f.tables[i] > start && f.tables[i] < end) end = f.tables[i];
   out->data = f.data + start; out->size = end - start;
   return true;
}


bool rdvd_ifo_title_read(const rdvd_ifo_t *ifo, unsigned number,
      rdvd_title_t *out)
{
   rdvd_ifo_t f;
   rdvd_ifo_span_t span;
   rdvd_title_t title;
   const uint8_t *record;
   uint32_t last;
   size_t total;
   unsigned count;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!ifo || !rdvd_ifo_read(ifo->data, ifo->size, &f)
         || f.kind != RDVD_IFO_VMG
         || !rdvd_ifo_table_read(&f, RDVD_IFO_TITLES, &span)) return false;
   last = rdvd_ifo_u32(span.data + 4);
   if (last == UINT32_MAX) return false;
   total = (size_t)last + 1;
   count = rdvd_ifo_u16(span.data);
   if (total < 8 || total > span.size || count > (total - 8) / 12
         || !number || number > count) return false;
   record = span.data + 8 + (size_t)(number - 1) * 12;
   memset(&title, 0, sizeof(title));
   title.playback_flags = record[0]; title.angles = record[1];
   title.chapters = rdvd_ifo_u16(record + 2);
   title.parental_id = rdvd_ifo_u16(record + 4);
   title.title_set = record[6]; title.local_title = record[7];
   title.title_set_sector = rdvd_ifo_u32(record + 8);
   if (!title.angles || title.angles > 9 || !title.title_set
         || title.title_set > f.title_sets || !title.local_title
         || title.local_title > 99) return false;
   *out = title;
   return true;
}

bool rdvd_ifo_first_play_read(const rdvd_ifo_t *ifo, rdvd_pgc_t *out)
{
   rdvd_ifo_t f;
   size_t end;
   unsigned i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!ifo || !rdvd_ifo_read(ifo->data, ifo->size, &f)
         || f.kind != RDVD_IFO_VMG || !f.first_play) return false;
   end = f.size;
   for (i = 0; i < f.table_count; i++)
   {
      if (f.tables[i] == f.first_play) return false;
      if (f.tables[i] > f.first_play && f.tables[i] < end) end = f.tables[i];
   }
   return rdvd_pgc_read(f.data + f.first_play, end - f.first_play, out);
}


bool rdvd_ifo_chapter_read(const rdvd_ifo_t *ifo, unsigned title,
      unsigned chapter, rdvd_chapter_t *out)
{
   rdvd_ifo_t f;
   rdvd_ifo_span_t span, chains;
   rdvd_pgci_table_t table;
   rdvd_pgci_entry_t entry;
   rdvd_chapter_t result;
   size_t total, header, start, end;
   uint32_t last;
   unsigned count, i;
   const uint8_t *record;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!ifo || !rdvd_ifo_read(ifo->data, ifo->size, &f)
         || f.kind != RDVD_IFO_VTS || !title || !chapter
         || !rdvd_ifo_table_read(&f, RDVD_IFO_TITLES, &span)) return false;
   last = rdvd_ifo_u32(span.data + 4);
   if (last == UINT32_MAX) return false;
   total = (size_t)last + 1;
   count = rdvd_ifo_u16(span.data);
   if (total < 8 || total > span.size || count > (total - 8) / 4
         || title > count) return false;
   header = 8 + (size_t)count * 4;
   start = rdvd_ifo_u32(span.data + 8 + (size_t)(title - 1) * 4);
   end = total;
   for (i = 0; i < count; i++)
   {
      size_t candidate = rdvd_ifo_u32(span.data + 8 + (size_t)i * 4);
      if (candidate < header || candidate > total) return false;
      if (candidate > start && candidate < end) end = candidate;
   }
   if ((end - start) % 4 || chapter > (end - start) / 4) return false;
   record = span.data + start + (size_t)(chapter - 1) * 4;
   memset(&result, 0, sizeof(result));
   result.pgc_number = rdvd_ifo_u16(record);
   result.program_number = rdvd_ifo_u16(record + 2);
   if (!result.pgc_number || !result.program_number
         || !rdvd_ifo_table_read(&f, RDVD_IFO_PGCIT, &chains)
         || !rdvd_pgci_table_read(chains.data, chains.size, &table)
         || !rdvd_pgci_entry_read(&table, result.pgc_number - 1, &entry)
         || !rdvd_pgc_program_cells(&entry.pgc, result.program_number,
               &result.first_cell, &result.last_cell)) return false;
   result.pgc = entry.pgc;
   *out = result;
   return true;
}


bool rdvd_menu_table_read(const uint8_t *data, size_t size,
      rdvd_menu_table_t *out)
{
   uint32_t last;
   size_t total, header;
   unsigned count, i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!data || size < 8) return false;
   last = rdvd_ifo_u32(data + 4);
   if (last == UINT32_MAX) return false;
   total = (size_t)last + 1;
   if (total < 8 || total > size) return false;
   count = rdvd_ifo_u16(data);
   if (count > (total - 8) / 8) return false;
   header = 8 + (size_t)count * 8;
   for (i = 0; i < count; i++)
   {
      size_t start = rdvd_ifo_u32(data + 12 + (size_t)i * 8);
      if (start < header || start > total - 8) return false;
   }
   out->data = data; out->size = total; out->languages = count;
   return true;
}


bool rdvd_menu_language_read(const rdvd_menu_table_t *table, unsigned index,
      rdvd_menu_language_t *out)
{
   rdvd_menu_table_t t;
   rdvd_menu_language_t language;
   const uint8_t *record;
   size_t start, end;
   unsigned i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!table || !rdvd_menu_table_read(table->data, table->size, &t)
         || index >= t.languages) return false;
   record = t.data + 8 + (size_t)index * 8;
   start = rdvd_ifo_u32(record + 4); end = t.size;
   for (i = 0; i < t.languages; i++)
   {
      size_t candidate = rdvd_ifo_u32(t.data + 12 + (size_t)i * 8);
      if (candidate > start && candidate < end) end = candidate;
   }
   memset(&language, 0, sizeof(language));
   if (!rdvd_pgci_table_read(t.data + start, end - start, &language.chains))
      return false;
   language.language_code = rdvd_ifo_u16(record);
   language.extension = record[2]; language.menu_flags = record[3];
   *out = language;
   return true;
}


bool rdvd_ifo_menu_read(const rdvd_ifo_t *ifo, unsigned language_index,
      enum rdvd_menu_id menu, rdvd_pgci_entry_t *out)
{
   rdvd_ifo_t f;
   rdvd_ifo_span_t span;
   rdvd_menu_table_t table;
   rdvd_menu_language_t language;
   unsigned i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!ifo || !rdvd_ifo_read(ifo->data, ifo->size, &f)) return false;
   if ((f.kind == RDVD_IFO_VMG && menu != RDVD_MENU_TITLE)
         || (f.kind == RDVD_IFO_VTS
            && (menu < RDVD_MENU_ROOT || menu > RDVD_MENU_CHAPTER))) return false;
   if (!rdvd_ifo_table_read(&f, RDVD_IFO_MENU, &span)
         || !rdvd_menu_table_read(span.data, span.size, &table)
         || !rdvd_menu_language_read(&table, language_index, &language)) return false;
   for (i = 0; i < language.chains.entries; i++)
   {
      unsigned id = language.chains.data[8 + (size_t)i * 8];
      if ((id & 0x80) && (id & 15) == (unsigned)menu)
         return rdvd_pgci_entry_read(&language.chains, i, out);
   }
   return false;
}


bool rdvd_ifo_select(const uint8_t *primary, size_t primary_size,
      const uint8_t *backup, size_t backup_size, enum rdvd_ifo_kind kind,
      rdvd_ifo_t *out, bool *used_backup)
{
   rdvd_ifo_t selected;
   if (used_backup) *used_backup = false;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (kind != RDVD_IFO_VMG && kind != RDVD_IFO_VTS) return false;
   if (rdvd_ifo_read(primary, primary_size, &selected) && selected.kind == kind)
   {
      *out = selected;
      return true;
   }
   if (!rdvd_ifo_read(backup, backup_size, &selected) || selected.kind != kind)
      return false;
   *out = selected;
   if (used_backup) *used_backup = true;
   return true;
}


bool rdvd_ifo_menu_language_find(const rdvd_ifo_t *ifo, unsigned code,
      unsigned extension, unsigned *index)
{
   rdvd_ifo_span_t span;
   rdvd_menu_table_t table;
   rdvd_menu_language_t language;
   unsigned i;
   if (!index) return false;
   *index = 0;
   if (code > 65535 || extension > 255
         || !rdvd_ifo_table_read(ifo, RDVD_IFO_MENU, &span)
         || !rdvd_menu_table_read(span.data, span.size, &table)) return false;
   for (i = 0; i < table.languages; i++)
   {
      const uint8_t *record = table.data + 8 + (size_t)i * 8;
      if (rdvd_ifo_u16(record) == code && record[2] == extension)
      {
         if (!rdvd_menu_language_read(&table, i, &language)) return false;
         *index = i;
         return true;
      }
   }
   return false;
}


bool rdvd_file_name(unsigned title_set, unsigned segment,
      enum rdvd_file_kind kind, char *out, size_t capacity)
{
   char name[13];
   const char *extension;
   if (!out || !capacity) return false;
   *out = '\0';
   if (capacity < sizeof(name) || title_set > 99 || segment > 9
         || (!title_set && segment)) return false;
   switch (kind)
   {
      case RDVD_FILE_IFO: extension = "IFO"; break;
      case RDVD_FILE_BUP: extension = "BUP"; break;
      case RDVD_FILE_VOB: extension = "VOB"; break;
      default: return false;
   }
   if (kind != RDVD_FILE_VOB && segment) return false;
   memcpy(name, title_set ? "VTS_00_0.IFO" : "VIDEO_TS.IFO", sizeof(name));
   if (title_set)
   {
      name[4] = (char)('0' + title_set / 10);
      name[5] = (char)('0' + title_set % 10);
      name[7] = (char)('0' + segment);
   }
   memcpy(name + 9, extension, 3);
   memcpy(out, name, sizeof(name));
   return true;
}


bool rdvd_vob_map_read(const uint64_t *sizes, unsigned count,
      rdvd_vob_map_t *out)
{
   uint64_t total = 0;
   unsigned i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!sizes || !count || count > 9) return false;
   for (i = 0; i < count; i++)
   {
      if (!sizes[i] || sizes[i] % 2048 || sizes[i] > UINT64_MAX - total)
         return false;
      total += sizes[i];
   }
   out->sizes = sizes; out->total_bytes = total; out->segments = count;
   return true;
}


bool rdvd_vob_span_read(const rdvd_vob_map_t *map, uint64_t offset,
      uint64_t bytes, rdvd_vob_span_t *out)
{
   rdvd_vob_map_t validated;
   unsigned i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!map || !bytes
         || !rdvd_vob_map_read(map->sizes, map->segments, &validated)
         || offset >= validated.total_bytes) return false;
   for (i = 0; i < validated.segments; i++)
   {
      uint64_t size = validated.sizes[i];
      if (offset < size)
      {
         uint64_t remaining = size - offset;
         out->segment = i + 1; out->file_offset = offset;
         out->bytes = bytes < remaining ? bytes : remaining;
         return true;
      }
      offset -= size;
   }
   return false;
}


bool rdvd_vob_cell_range(const rdvd_vob_map_t *map, const rdvd_pgc_t *pgc,
      unsigned cell_number, rdvd_vob_range_t *out)
{
   rdvd_vob_map_t validated;
   rdvd_cell_t cell;
   uint64_t start, end;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!map || !rdvd_vob_map_read(map->sizes, map->segments, &validated)
         || !rdvd_pgc_cell_read(pgc, cell_number, &cell)) return false;
   start = (uint64_t)cell.first_sector * 2048;
   end = ((uint64_t)cell.last_sector + 1) * 2048;
   if (end > validated.total_bytes) return false;
   out->offset = start; out->bytes = end - start;
   return true;
}


bool rdvd_cell_table_read(const uint8_t *data, size_t size,
      rdvd_cell_table_t *out)
{
   uint32_t last;
   size_t total;
   unsigned count, i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!data || size < 8) return false;
   last = rdvd_ifo_u32(data + 4);
   if (last == UINT32_MAX) return false;
   total = (size_t)last + 1;
   if (total < 8 || total > size || (total - 8) % 12) return false;
   count = (unsigned)((total - 8) / 12);
   if (count && !rdvd_ifo_u16(data)) return false;
   for (i = 0; i < count; i++)
   {
      const uint8_t *record = data + 8 + (size_t)i * 12;
      if (!rdvd_ifo_u16(record) || !record[2]
            || rdvd_ifo_u32(record + 4) > rdvd_ifo_u32(record + 8)) return false;
   }
   out->data = data; out->size = total;
   out->entries = count; out->vobs = rdvd_ifo_u16(data);
   return true;
}

bool rdvd_cell_address_read(const rdvd_cell_table_t *table, unsigned index,
      rdvd_cell_address_t *out)
{
   rdvd_cell_table_t validated;
   const uint8_t *record;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!table || !rdvd_cell_table_read(table->data, table->size, &validated)
         || index >= validated.entries) return false;
   record = validated.data + 8 + (size_t)index * 12;
   out->vob_id = rdvd_ifo_u16(record); out->cell_id = record[2];
   out->first_sector = rdvd_ifo_u32(record + 4);
   out->last_sector = rdvd_ifo_u32(record + 8);
   return true;
}


bool rdvd_vobu_map_read(const uint8_t *data, size_t size, rdvd_vobu_map_t *out)
{
   uint32_t last, previous = 0;
   size_t total;
   unsigned count, i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!data || size < 4) return false;
   last = rdvd_ifo_u32(data);
   if (last == UINT32_MAX) return false;
   total = (size_t)last + 1;
   if (total < 4 || total > size || (total - 4) % 4) return false;
   count = (unsigned)((total - 4) / 4);
   for (i = 0; i < count; i++)
   {
      uint32_t sector = rdvd_ifo_u32(data + 4 + (size_t)i * 4);
      if (i && sector <= previous) return false;
      previous = sector;
   }
   out->data = data; out->size = total; out->entries = count;
   return true;
}

bool rdvd_vobu_find(const rdvd_vobu_map_t *map, uint32_t target,
      uint32_t *sector, unsigned *index)
{
   rdvd_vobu_map_t validated;
   unsigned low = 0, high;
   if (sector) *sector = 0;
   if (index) *index = 0;
   if (!sector || !index || !map
         || !rdvd_vobu_map_read(map->data, map->size, &validated)) return false;
   high = validated.entries;
   while (low < high)
   {
      unsigned middle = low + (high - low) / 2;
      uint32_t value = rdvd_ifo_u32(validated.data + 4 + (size_t)middle * 4);
      if (value <= target) low = middle + 1;
      else high = middle;
   }
   if (!low) return false;
   *index = low - 1;
   *sector = rdvd_ifo_u32(validated.data + 4 + (size_t)*index * 4);
   return true;
}


bool rdvd_cell_address_find(const rdvd_cell_table_t *table, unsigned vob_id,
      unsigned cell_id, rdvd_cell_address_t *out)
{
   rdvd_cell_table_t validated;
   const uint8_t *found = NULL;
   unsigned i;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   if (!vob_id || vob_id > 65535 || !cell_id || cell_id > 255 || !table
         || !rdvd_cell_table_read(table->data, table->size, &validated)) return false;
   for (i = 0; i < validated.entries; i++)
   {
      const uint8_t *record = validated.data + 8 + (size_t)i * 12;
      if (rdvd_ifo_u16(record) == vob_id && record[2] == cell_id)
      {
         if (found) return false;
         found = record;
      }
   }
   if (!found) return false;
   out->vob_id = vob_id; out->cell_id = cell_id;
   out->first_sector = rdvd_ifo_u32(found + 4);
   out->last_sector = rdvd_ifo_u32(found + 8);
   return true;
}


bool rdvd_vobu_find_in_cell(const rdvd_vobu_map_t *map, uint32_t target,
      uint32_t first, uint32_t last, uint32_t *sector, unsigned *index)
{
   uint32_t found;
   unsigned position;
   if (sector) *sector = 0;
   if (index) *index = 0;
   if (!sector || !index || first > last || target < first || target > last
         || !rdvd_vobu_find(map, target, &found, &position) || found < first)
      return false;
   *sector = found; *index = position;
   return true;
}


bool rdvd_ifo_title_target(const rdvd_ifo_t *ifo, const rdvd_vob_map_t *source,
      unsigned title, unsigned chapter, unsigned cell_number, uint32_t target,
      rdvd_title_target_t *out)
{
   rdvd_chapter_t position;
   rdvd_title_target_t result;
   rdvd_vob_range_t range;
   rdvd_ifo_span_t span;
   rdvd_cell_table_t cells;
   rdvd_cell_address_t address;
   rdvd_vobu_map_t vobus;
   unsigned index;
   if (!out) return false;
   memset(out, 0, sizeof(*out));
   memset(&result, 0, sizeof(result));
   if (!rdvd_ifo_chapter_read(ifo, title, chapter, &position)
         || cell_number < position.first_cell || cell_number > position.last_cell
         || !rdvd_pgc_cell_read(&position.pgc, cell_number, &result.cell)
         || result.cell.interleaved || result.cell.block_type
         || !rdvd_vob_cell_range(source, &position.pgc, cell_number, &range)
         || !rdvd_ifo_table_read(ifo, RDVD_IFO_TITLE_CELLS, &span)
         || !rdvd_cell_table_read(span.data, span.size, &cells)
         || !rdvd_cell_address_find(&cells, result.cell.vob_id, result.cell.cell_id, &address)
         || result.cell.first_sector < address.first_sector
         || result.cell.last_sector > address.last_sector
         || !rdvd_ifo_table_read(ifo, RDVD_IFO_TITLE_VOBUS, &span)
         || !rdvd_vobu_map_read(span.data, span.size, &vobus)
         || !rdvd_vobu_find_in_cell(&vobus, target, result.cell.first_sector,
               result.cell.last_sector, &result.nav_sector, &index)) return false;
   result.remaining.offset = (uint64_t)result.nav_sector * 2048;
   result.remaining.bytes = range.offset + range.bytes - result.remaining.offset;
   if (!rdvd_vob_span_read(source, result.remaining.offset,
            result.remaining.bytes, &result.span)) return false;
   *out = result;
   return true;
}
