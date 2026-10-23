/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Assertions are required
#endif
#include <assert.h>
#include <limits.h>
#include "../../../formats/h265/rh265.c"

static rh265_shdr header;
static const uint8_t flags[16] = {0xc0};

static void valid_counts(int first, int second, int is_b)
{
   rh265_bits bits;
   int list, i, c;
   memset(&header, 0x5a, sizeof(header));
   header.nb_refs[0] = first;
   header.nb_refs[1] = second;
   rh265_bits_init(&bits, flags, sizeof(flags));
   assert(rh265_parse_pred_weight_table(&bits, &header, is_b) == 0);
   for (list = 0; list < (is_b ? 2 : 1); list++)
      for (i = 0; i < header.nb_refs[list]; i++)
      {
         assert(header.luma_w[list][i] == 1 && !header.luma_o[list][i]);
         for (c = 0; c < 2; c++)
            assert(header.chroma_w[list][i][c] == 1
                  && !header.chroma_o[list][i][c]);
      }
   assert(header.list_entry[0][0] == 0x5a);
}

int main(void)
{
   static const int invalid[] = {-1, 0, RH265_MAX_REFS + 1, INT_MAX};
   rh265_bits bits;
   unsigned i;
   int list;
   valid_counts(1, 0, 0);
   valid_counts(RH265_MAX_REFS, 0, 0);
   valid_counts(1, RH265_MAX_REFS, 1);
   valid_counts(RH265_MAX_REFS, RH265_MAX_REFS, 1);
   for (list = 0; list < 2; list++)
      for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
      {
         memset(&header, 0x5a, sizeof(header));
         header.nb_refs[0] = header.nb_refs[1] = 1;
         header.nb_refs[list] = invalid[i];
         rh265_bits_init(&bits, flags, sizeof(flags));
         assert(rh265_parse_pred_weight_table(&bits, &header, 1) == -1);
         assert(header.luma_w[list][0] == 0x5a5a);
         assert(header.chroma_w[list][0][0] == 0x5a5a);
      }
   return 0;
}
