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

/* Text layout, written once for every video driver's font.
 *
 * This is a statement block, not a function: a driver includes it in
 * the body of its own render function, where it expands among that
 * function's locals. The loop the compiler sees is the one the driver
 * used to write out by hand - no call, no callback and no intermediate
 * buffer per glyph - while the line splitting, the UTF-8 walk, the
 * glyph lookup with its '?' stand-in, the alignment width and the pen
 * advance exist in one place.
 *
 * In scope where it is included:
 *
 *   msg, msg_len    the text; lines end at '\n', the text at msg_len or
 *                   at a NUL, whichever comes first
 *   get_glyph       the font's get_glyph
 *   font_data       its handle
 *   glyph_q         the glyph drawn for a codepoint the font cannot
 *                   draw, or NULL to skip those
 *
 * Defined by the driver before including, all undefined again after:
 *
 *   FONT_LAYOUT_ALIGNED
 *       Expression, nonzero when each line has to be measured before it
 *       is drawn (right or centred text, or a driver that places a
 *       line's shadows ahead of its glyphs).
 *   FONT_LAYOUT_LINE(line, width, count, bytes)
 *       Starts line number 'line'; 'width' is the sum of its glyphs'
 *       advance_x, in the font's pixels, and 'count' the number of
 *       glyphs it draws, both 0 when not ALIGNED; 'bytes' is its length
 *       in the text.
 *   FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y)
 *       Emits 'glyph' with the pen at (pen_x, pen_y) from the start of
 *       the line, in the font's pixels.
 *   FONT_LAYOUT_DIRTY(glyph)          optional
 *       Runs after every lookup, in the measuring pass as well, for a
 *       driver that uploads the atlas as glyphs arrive.
 *   FONT_LAYOUT_LINE_END()            optional
 *       Ends the line, for a driver that draws a line at a time.
 *   FONT_LAYOUT_SKIP(line, bytes)     optional
 *       Expression, nonzero when line number 'line', 'bytes' long, is
 *       not drawn at all - out of view, say - so that its glyphs are
 *       not even looked up.
 *
 * fl_m, the first byte of the current line, may be read by these hooks:
 * a driver that draws a line in passes of its own, a shadow run and then
 * a text run, takes the line from FONT_LAYOUT_SKIP and has the block
 * skip its own walk.
 *
 * C89: the block declares its locals at its top, so it may appear
 * wherever a statement may. Its names all begin fl_. */

#ifndef FONT_LAYOUT_DIRTY
#define FONT_LAYOUT_DIRTY(glyph)
#endif
#ifndef FONT_LAYOUT_LINE_END
#define FONT_LAYOUT_LINE_END()
#endif
#ifndef FONT_LAYOUT_SKIP
#define FONT_LAYOUT_SKIP(line, bytes) 0
#endif

{
   const char *fl_m   = (msg);
   const char *fl_end = fl_m + (msg_len);
   int         fl_line = 0;

   for (;;)
   {
      const char *fl_delim = fl_m;
      const char *fl_scan;
      int fl_width         = 0;
      int fl_count         = 0;
      int fl_dx            = 0;
      int fl_dy            = 0;

      while (fl_delim < fl_end && *fl_delim != '\n' && *fl_delim != '\0')
         fl_delim++;

      if (FONT_LAYOUT_SKIP(fl_line, (size_t)(fl_delim - fl_m)))
         goto fl_next;

      if (FONT_LAYOUT_ALIGNED)
      {
         fl_scan = fl_m;
         while (fl_scan < fl_delim)
         {
            const struct font_glyph *fl_g;
            uint32_t fl_code = utf8_walk(&fl_scan);

            if (!(fl_g = get_glyph(font_data, fl_code)))
               if (!(fl_g = glyph_q))
                  continue;
            FONT_LAYOUT_DIRTY(fl_g);
            fl_width += fl_g->advance_x;
            fl_count++;
         }
      }

      FONT_LAYOUT_LINE(fl_line, fl_width, fl_count,
            (size_t)(fl_delim - fl_m));
      /* a driver's FONT_LAYOUT_LINE need not use either */
      (void)fl_width;
      (void)fl_count;

      fl_scan = fl_m;
      while (fl_scan < fl_delim)
      {
         const struct font_glyph *fl_g;
         uint32_t fl_code = utf8_walk(&fl_scan);

         if (!(fl_g = get_glyph(font_data, fl_code)))
            if (!(fl_g = glyph_q))
               continue;
         FONT_LAYOUT_DIRTY(fl_g);
         FONT_LAYOUT_GLYPH(fl_g, fl_dx, fl_dy);
         fl_dx += fl_g->advance_x;
         fl_dy += fl_g->advance_y;
      }

      FONT_LAYOUT_LINE_END();

fl_next:
      if (fl_delim >= fl_end || *fl_delim == '\0')
         break;
      fl_m = fl_delim + 1;
      fl_line++;
   }
}

#undef FONT_LAYOUT_ALIGNED
#undef FONT_LAYOUT_LINE
#undef FONT_LAYOUT_GLYPH
#undef FONT_LAYOUT_DIRTY
#undef FONT_LAYOUT_LINE_END
#undef FONT_LAYOUT_SKIP
