/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <string.h>
#include <formats/rdvd_ifo.h>

static unsigned rdvd_ifo_u16(const uint8_t *p)
{
   return ((unsigned)p[0] << 8) | p[1];
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
