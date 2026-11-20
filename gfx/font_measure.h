/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Text measurement, written once: the width gfx/font_layout.h gives a
 * right or centred line, for a caller that wants it without drawing.
 *
 * A statement block, like gfx/font_layout.h, included where it is used.
 * font_renderer_get_message_width() in font_driver.c is built from it
 * and serves every video driver whose atlas goes up apart from its
 * lookups; a driver that uploads an atlas cell as each glyph is looked
 * up includes the block itself with FONT_MEASURE_DIRTY, so its loop has
 * no call per glyph either.
 *
 * In scope where it is included:
 *
 *   msg, msg_len    the text; it ends at msg_len or at a NUL
 *   get_glyph       the font's get_glyph
 *   font_data       its handle
 *
 * A codepoint the font cannot draw is measured as '?', looked up the
 * first time one is met, and skipped when the font has no '?' either.
 * With HAVE_LANGEXTRA, Arabic is measured as it is drawn: each letter
 * in the presentation form its neighbours give it, from
 * font_driver_arabic_shapes as the reshaper picks it, and a lam with
 * the alef after it as their one ligature.
 *
 * Defined by the caller before including, undefined again after:
 *
 *   FONT_MEASURE_SUM
 *       An int lvalue each glyph's advance_x is added to.
 *   FONT_MEASURE_DIRTY(glyph)         optional
 *       Runs after every lookup.
 *
 * C89: the block declares its locals at its top. Its names begin fm_. */

#ifndef FONT_MEASURE_DIRTY
#define FONT_MEASURE_DIRTY(glyph)
#endif

{
   /* fm_q is the '?' glyph once looked up; &fm_unset until then */
   static const struct font_glyph fm_unset = { 0 };
   const char                    *fm_s     = (msg);
   const char                    *fm_end   = fm_s + (msg_len);
   const struct font_glyph       *fm_q     = &fm_unset;

   while (fm_s < fm_end && *fm_s)
   {
      const struct font_glyph *fm_g;
      uint32_t                 fm_code;

      if ((unsigned char)*fm_s < 0x80)
         fm_code = (unsigned char)*fm_s++;
      /* Two bytes, decoded as utf8_walk() does */
      else if ((unsigned char)*fm_s < 0xE0
            && (unsigned char)*fm_s >= 0xC0 && fm_s[1])
      {
#ifdef HAVE_LANGEXTRA
         /* The rest, from the first Arabic letter, is measured below */
         if (((unsigned char)*fm_s & 0xFC) == 0xD8)
            break;
#endif
         fm_code = (((unsigned char)fm_s[0] & 0x1F) << 6)
                 |  ((unsigned char)fm_s[1] & 0x3F);
         fm_s   += 2;
      }
      else
         fm_code = utf8_walk(&fm_s);

      if (!(fm_g = get_glyph(font_data, fm_code)))
      {
         if (fm_q == &fm_unset)
            fm_q = get_glyph(font_data, '?');
         if (!(fm_g = fm_q))
            continue;
      }
      FONT_MEASURE_DIRTY(fm_g);
      FONT_MEASURE_SUM += fm_g->advance_x;
   }

#ifdef HAVE_LANGEXTRA
   /* From the first Arabic letter on. Letters are taken in typed order,
    * each joined to the one before it and the one after it as the
    * reshaper joins them. fm_st packs the letter before (diacritics
    * skipped; 0, whose row is empty, for none) in bits 0-7, the letter
    * before the last lam in bits 8-15 and that lam's advance above. */
   if (fm_s < fm_end && *fm_s)
   {
      unsigned long fm_st = 0;
      while (fm_s < fm_end && *fm_s)
      {
         const struct font_glyph *fm_g;
         const unsigned char     *fm_c = (const unsigned char*)fm_s;
         uint32_t                 fm_code;
         uint32_t                 fm_r = 0;

         if (     (fm_c[0] & 0xFC) == 0xD8
               && fm_s + 1 < fm_end && (fm_c[1] & 0xC0) == 0x80)
         {
            const unsigned char *fm_n  = fm_c + 2;
            unsigned             fm_id = ((fm_c[0] & 0x03) << 6)
               | (fm_c[1] & 0x3F);
            unsigned             fm_pv = (unsigned)(fm_st & 0xFF);
            unsigned             fm_ni = 0;
            fm_code = 0x600 | fm_id;
            fm_s   += 2;
            /* After a lam, an alef (U+0622, 0623, 0625, 0627) makes
             * their ligature, joined as the lam is; the lam counted
             * already is taken back */
            if (     fm_pv == 0x44 && fm_id - 0x22 < 6
                  && ((0x2B >> (fm_id - 0x22)) & 1))
            {
               fm_r = 0xFEF5 + ((fm_id - 0x22 + 1) & ~1u)
                  + (font_driver_arabic_shapes[(fm_st >> 8) & 0xFF][2]
                        != 0);
               FONT_MEASURE_SUM -= (int)(short)(fm_st >> 16);
            }
            else
            {
               unsigned fm_k;
               /* The letter after, diacritics skipped */
               while (     (const char*)fm_n + 1 < fm_end
                        && (fm_n[0] & 0xFC) == 0xD8)
               {
                  fm_ni = ((fm_n[0] & 0x03) << 6) | (fm_n[1] & 0x3F);
                  if (fm_ni <= 0x4A || fm_ni >= 0x60)
                     break;
                  fm_n += 2;
               }
               fm_k = (font_driver_arabic_shapes[fm_pv][2] != 0)
                    | ((font_driver_arabic_shapes[fm_ni][1] != 0) << 1);
               if (!(fm_r = font_driver_arabic_shapes[fm_id][fm_k]))
                  fm_r = font_driver_arabic_shapes[fm_id][fm_k & 1];
            }
            if (fm_id == 0x44)
               fm_st = (fm_st & 0xFF) | ((fm_st & 0xFF) << 8);
            if (fm_id <= 0x4A || fm_id >= 0x60)
               fm_st = (fm_st & ~0xFFul) | fm_id;
         }
         else
         {
            fm_code = utf8_walk_inline(&fm_s);
            fm_st  &= ~0xFFul;
         }

         if (!(fm_g = get_glyph(font_data, fm_r ? fm_r : fm_code)))
         {
            if (fm_q == &fm_unset)
               fm_q = get_glyph(font_data, '?');
            fm_g = fm_q;
         }
         if (fm_g)
         {
            FONT_MEASURE_DIRTY(fm_g);
            FONT_MEASURE_SUM += fm_g->advance_x;
            if (fm_code == 0x644)
               fm_st = (fm_st & 0xFFFFul) | ((unsigned long)
                     ((unsigned)fm_g->advance_x & 0xFFFFu) << 16);
         }
         else if (fm_code == 0x644)
            fm_st &= 0xFFFFul;
      }
   }
#endif
}

#undef FONT_MEASURE_SUM
#undef FONT_MEASURE_DIRTY
