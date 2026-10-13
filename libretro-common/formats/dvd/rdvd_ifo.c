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
