/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtags.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Tag reader: see formats/rtags.h for what it reads and how values
 * come out.
 *
 * Every container's text is read through one byte source - a span of
 * the file, the same span with ID3's unsynchronisation undone as it is
 * read, or an Ogg packet across the pages it spans - and decoded
 * character by character into the caller's buffer, which is where the
 * value is cut when it does not fit. Nothing is copied but the value. */

#include <string.h>

#include <formats/rtags.h>

/* ---------------------------------------------------------------- */
/* genres                                                           */
/* ---------------------------------------------------------------- */

static const char *const rtags_genres[] = {
   "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk",
   "Grunge", "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other",
   "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial",
   "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack",
   "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion",
   "Trance", "Classical", "Instrumental", "Acid", "House", "Game",
   "Sound Clip", "Gospel", "Noise", "AlternRock", "Bass", "Soul", "Punk",
   "Space", "Meditative", "Instrumental Pop", "Instrumental Rock",
   "Ethnic", "Gothic", "Darkwave", "Techno-Industrial", "Electronic",
   "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult",
   "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle",
   "Native American", "Cabaret", "New Wave", "Psychedelic", "Rave",
   "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz",
   "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk",
   "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebop",
   "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde",
   "Gothic Rock", "Progressive Rock", "Psychedelic Rock",
   "Symphonic Rock", "Slow Rock", "Big Band", "Chorus",
   "Easy Listening", "Acoustic", "Humour", "Speech", "Chanson", "Opera",
   "Chamber Music", "Sonata", "Symphony", "Booty Bass", "Primus",
   "Porn Groove", "Satire", "Slow Jam", "Club", "Tango", "Samba",
   "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle",
   "Duet", "Punk Rock", "Drum Solo", "A Cappella", "Euro-House",
   "Dance Hall", "Goa", "Drum & Bass", "Club-House", "Hardcore Techno",
   "Terror", "Indie", "BritPop", "Negerpunk", "Polsk Punk", "Beat",
   "Christian Gangsta Rap", "Heavy Metal", "Black Metal", "Crossover",
   "Contemporary Christian", "Christian Rock", "Merengue", "Salsa",
   "Thrash Metal", "Anime", "Jpop", "Synthpop", "Abstract", "Art Rock",
   "Baroque", "Bhangra", "Big Beat", "Breakbeat", "Chillout",
   "Downtempo", "Dub", "EBM", "Eclectic", "Electro", "Electroclash",
   "Emo", "Experimental", "Garage", "Global", "IDM", "Illbient",
   "Industro-Goth", "Jam Band", "Krautrock", "Leftfield", "Lounge",
   "Math Rock", "New Romantic", "Nu-Breakz", "Post-Punk", "Post-Rock",
   "Psytrance", "Shoegaze", "Space Rock", "Trop Rock", "World Music",
   "Neoclassical", "Audiobook", "Audio Theatre", "Neue Deutsche Welle",
   "Podcast", "Indie Rock", "G-Funk", "Dubstep", "Garage Rock",
   "Psybient"
};

#define RTAGS_GENRES (sizeof(rtags_genres) / sizeof(rtags_genres[0]))

const char *rtags_genre_name(unsigned index)
{
   return index < RTAGS_GENRES ? rtags_genres[index] : NULL;
}

/* ---------------------------------------------------------------- */
/* the value being written                                          */
/* ---------------------------------------------------------------- */

typedef struct
{
   char  *s;
   size_t cap;    /* bytes of text room: the buffer less its NUL */
   size_t n;
   int    full;   /* a character did not fit: nothing more goes in */
} rtags_out;

static void out_cp(rtags_out *o, uint32_t cp)
{
   unsigned char b[4];
   size_t        k, i;
   if (cp > 0x10ffff || (cp >= 0xd800 && cp < 0xe000))
      cp = 0xfffd;
   if (cp < 0x80)
   {
      b[0] = (unsigned char)cp;
      k    = 1;
   }
   else if (cp < 0x800)
   {
      b[0] = (unsigned char)(0xc0 | (cp >> 6));
      b[1] = (unsigned char)(0x80 | (cp & 0x3f));
      k    = 2;
   }
   else if (cp < 0x10000)
   {
      b[0] = (unsigned char)(0xe0 | (cp >> 12));
      b[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
      b[2] = (unsigned char)(0x80 | (cp & 0x3f));
      k    = 3;
   }
   else
   {
      b[0] = (unsigned char)(0xf0 | (cp >> 18));
      b[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3f));
      b[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
      b[3] = (unsigned char)(0x80 | (cp & 0x3f));
      k    = 4;
   }
   if (o->full || o->n + k > o->cap)
   {
      o->full = 1;
      return;
   }
   for (i = 0; i < k; i++)
      o->s[o->n++] = (char)b[i];
   o->s[o->n] = '\0';
}

static void out_ascii(rtags_out *o, const char *s)
{
   while (*s)
      out_cp(o, (unsigned char)*s++);
}

static void out_clear(rtags_out *o)
{
   o->n    = 0;
   o->full = 0;
   if (o->s)
      o->s[0] = '\0';
}

static void out_u64(rtags_out *o, uint64_t v)
{
   char   d[21];
   size_t i = sizeof(d) - 1;
   d[i] = '\0';
   do
   {
      d[--i] = (char)('0' + (int)(v % 10));
      v     /= 10;
   } while (v);
   out_ascii(o, d + i);
}

static void out_int(rtags_out *o, long v)
{
   if (v < 0)
   {
      out_cp(o, '-');
      out_u64(o, (uint64_t)(-(v + 1)) + 1);
   }
   else
      out_u64(o, (uint64_t)v);
}

/* drops trailing spaces, as FFmpeg does for ID3v1's padded fields */
static void out_trim(rtags_out *o)
{
   while (o->n && o->s[o->n - 1] == ' ')
      o->s[--o->n] = '\0';
}

/* ---------------------------------------------------------------- */
/* the byte source                                                  */
/* ---------------------------------------------------------------- */

enum { SRC_SPAN = 0, SRC_UNSYNC, SRC_OGG };

typedef struct
{
   const uint8_t *p;      /* SPAN, UNSYNC: the bytes; OGG: the file */
   size_t         n;
   size_t         i;
   int            mode;
   int            back;   /* a byte read and given back, or -1 */
   int            prev_ff;
   /* OGG: the packet being read */
   size_t         page;   /* the page's offset */
   size_t         data;   /* the next byte's offset */
   unsigned       seg, nseg;
   unsigned       seg_left;
   uint32_t       serial;
   int            packet_end;
   int            dead;   /* the stream ends (or breaks) here */
} rtags_src;

static void src_span(rtags_src *s, const uint8_t *p, size_t n, int unsync)
{
   memset(s, 0, sizeof(*s));
   s->p    = p;
   s->n    = n;
   s->mode = unsync ? SRC_UNSYNC : SRC_SPAN;
   s->back = -1;
}

static uint32_t rd_le32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t rd_be32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Ogg: the next page of the stream at or after @from; 0 when none */
static int ogg_page(rtags_src *s, size_t from)
{
   while (from + 27 <= s->n)
   {
      const uint8_t *h = s->p + from;
      size_t         body, k;
      unsigned       nseg;
      if (memcmp(h, "OggS", 4) || h[4] != 0)
         return 0;
      nseg = h[26];
      if (from + 27 + nseg > s->n)
         return 0;
      for (body = 0, k = 0; k < nseg; k++)
         body += h[27 + k];
      if (rd_le32(h + 14) == s->serial && nseg)
      {
         s->page     = from;
         s->nseg     = nseg;
         s->seg      = 0;
         s->seg_left = h[27];
         s->data     = from + 27 + nseg;
         return 1;
      }
      from += 27 + nseg + body;
   }
   return 0;
}

static int ogg_byte(rtags_src *s)
{
   for (;;)
   {
      if (s->packet_end || s->dead)
         return -1;
      if (s->seg_left)
      {
         if (s->data >= s->n)
         {
            s->dead = 1;
            return -1;
         }
         s->seg_left--;
         return s->p[s->data++];
      }
      /* this segment is read: a short one ends the packet */
      if (s->p[s->page + 27 + s->seg] < 255)
      {
         s->packet_end = 1;
         return -1;
      }
      if (++s->seg < s->nseg)
         s->seg_left = s->p[s->page + 27 + s->seg];
      else if (!ogg_page(s, s->data))
         s->dead = 1;
   }
}

/* Ogg: on to the packet after the one being read */
static int ogg_next_packet(rtags_src *s)
{
   while (ogg_byte(s) >= 0) { }
   if (s->dead)
      return 0;
   s->packet_end = 0;
   if (++s->seg < s->nseg)
      s->seg_left = s->p[s->page + 27 + s->seg];
   else if (!ogg_page(s, s->data))
      return 0;
   return 1;
}

/* The next byte, @left counting down what the caller may read; -1 at
 * the end of either */
static int src_byte(rtags_src *s, size_t *left)
{
   int b;
   if (!*left)
      return -1;
   if (s->back >= 0)
   {
      b       = s->back;
      s->back = -1;
   }
   else if (s->mode == SRC_OGG)
   {
      if ((b = ogg_byte(s)) < 0)
         return -1;
   }
   else
   {
      if (s->i >= s->n)
         return -1;
      b = s->p[s->i++];
      if (s->mode == SRC_UNSYNC)
      {
         /* 0xff 0x00 stands for 0xff */
         if (s->prev_ff && b == 0)
         {
            if (s->i >= s->n)
               return -1;
            b = s->p[s->i++];
         }
         s->prev_ff = b == 0xff;
      }
   }
   (*left)--;
   return b;
}

static void src_unget(rtags_src *s, int b, size_t *left)
{
   s->back = b;
   (*left)++;
}

static void src_skip(rtags_src *s, size_t n)
{
   if (s->mode == SRC_SPAN && s->back < 0)
      s->i = n < s->n - s->i ? s->i + n : s->n;
   else
      while (src_byte(s, &n) >= 0) { }
}

/* @k bytes, little-endian (or big-endian); 0 when the source ends */
static int src_uint(rtags_src *s, size_t *left, unsigned k, int be, uint32_t *v)
{
   unsigned i;
   *v = 0;
   for (i = 0; i < k; i++)
   {
      int b = src_byte(s, left);
      if (b < 0)
         return 0;
      if (be)
         *v = (*v << 8) | (uint32_t)b;
      else
         *v |= (uint32_t)b << (8 * i);
   }
   return 1;
}

/* ---------------------------------------------------------------- */
/* text                                                             */
/* ---------------------------------------------------------------- */

enum
{
   TXT_LATIN1 = 0,
   TXT_UTF16,     /* with a byte order mark */
   TXT_UTF16BE,
   TXT_UTF8,      /* falling back to ISO 8859-1 byte by byte */
   TXT_UTF16LE
};

/* One character of UTF-8, or the byte as ISO 8859-1 when it does not
 * start a whole one; -1 at the end */
static long get_utf8(rtags_src *s, size_t *left)
{
   int      b = src_byte(s, left), k, i;
   uint32_t cp;
   if (b < 0)
      return -1;
   if (b < 0x80)
      return b;
   if      ((b & 0xe0) == 0xc0) { k = 1; cp = (uint32_t)b & 0x1f; }
   else if ((b & 0xf0) == 0xe0) { k = 2; cp = (uint32_t)b & 0x0f; }
   else if ((b & 0xf8) == 0xf0) { k = 3; cp = (uint32_t)b & 0x07; }
   else
      return b;
   for (i = 0; i < k; i++)
   {
      int c = src_byte(s, left);
      if (c < 0 || (c & 0xc0) != 0x80)
      {
         /* not UTF-8 after all: a lone lead is a byte of its own, a
          * cut run a replacement character; the byte that ended it
          * starts the next character */
         if (c >= 0)
            src_unget(s, c, left);
         return i == 0 ? b : 0xfffd;
      }
      cp = (cp << 6) | ((uint32_t)c & 0x3f);
   }
   if (     (k == 1 && cp < 0x80)
         || (k == 2 && cp < 0x800)
         || (k == 3 && (cp < 0x10000 || cp > 0x10ffff)))
      return 0xfffd;
   return (long)cp;
}

static long get_utf16(rtags_src *s, size_t *left, int be)
{
   uint32_t u, u2;
   if (!src_uint(s, left, 2, be, &u))
      return -1;
   if (u >= 0xd800 && u < 0xdc00)
   {
      if (!src_uint(s, left, 2, be, &u2))
         return 0xfffd;
      if (u2 >= 0xdc00 && u2 < 0xe000)
         return (long)(0x10000 + ((u - 0xd800) << 10) + (u2 - 0xdc00));
      return u2 == 0 ? 0 : 0xfffd;
   }
   return (long)u;
}

/* Text in @enc into @o, to a NUL or the end of @left, which counts
 * down what is read */
static void get_text(rtags_src *s, size_t *left, int enc, rtags_out *o)
{
   int be = enc == TXT_UTF16BE;
   if (enc == TXT_UTF16)
   {
      uint32_t bom;
      if (!src_uint(s, left, 2, 1, &bom))
         return;
      if (bom == 0xfffe)
         be = 0;
      else if (bom == 0xfeff)
         be = 1;
      else
         return;
   }
   for (;;)
   {
      long cp;
      switch (enc)
      {
         case TXT_LATIN1:
            cp = src_byte(s, left);
            break;
         case TXT_UTF8:
            cp = get_utf8(s, left);
            break;
         default:
            cp = get_utf16(s, left, be);
            break;
      }
      if (cp <= 0)
         return;
      out_cp(o, (uint32_t)cp);
      if (o->full)
         return;
   }
}

/* A number at the start of @t, as sscanf's "%d" takes one: blanks,
 * a sign, digits. Returns 1 for one, with @v (0 past 0x7fffffff) */
static int lead_int(const char *t, long *v)
{
   int neg = 0, any = 0;
   while (*t == ' ' || (*t >= '\t' && *t <= '\r'))
      t++;
   if (*t == '+' || *t == '-')
      neg = *t++ == '-';
   *v = 0;
   while (*t >= '0' && *t <= '9')
   {
      if (*v < 0x10000000L)
         *v = *v * 10 + (*t - '0');
      any = 1;
      t++;
   }
   if (neg)
      *v = -*v;
   return any;
}

/* An ID3 genre given by its number - "(17)", "17" - as its name */
static void genre_by_number(rtags_out *o)
{
   long g;
   if (!o->n)
      return;
   if (!(o->s[0] == '(' && lead_int(o->s + 1, &g)) && !lead_int(o->s, &g))
      return;
   if (g < 0 || (unsigned long)g >= RTAGS_GENRES)
      return;
   out_clear(o);
   out_ascii(o, rtags_genres[g]);
}

/* ---------------------------------------------------------------- */
/* ID3v2                                                            */
/* ---------------------------------------------------------------- */

/* The size of the ID3v2 tag at @p, header and footer in; 0 for none.
 * OpenMG's "ea3" tag is an ID3v2 tag by another name. */
static size_t id3v2_size(const uint8_t *p, size_t n)
{
   if (n < 10 || (memcmp(p, "ID3", 3) && memcmp(p, "ea3", 3))
         || p[3] == 0xff || p[4] == 0xff
         || ((p[6] | p[7] | p[8] | p[9]) & 0x80))
      return 0;
   return 10 + (((size_t)p[6] << 21) | ((size_t)p[7] << 14)
         | ((size_t)p[8] << 7) | p[9]) + ((p[5] & 0x10) ? 10 : 0);
}

/* the frames each field is read from: 2.3/2.4, then 2.2 */
static const char *const id3_frames[RTAGS_FIELDS][2] = {
   { "TIT2", "TT2" },
   { "TPE1", "TP1" },
   { "TALB", "TAL" },
   { "TCON", "TCO" },
   { "TRCK", "TRK" },
   { "TPOS", "TPA" },
   { "TDRC", ""    }
};

typedef struct
{
   int       field;
   rtags_out out;
   /* the date's parts: TYER, TDAT, TIME; and TDRC in out */
   char      part[3][5];
   int       done;
} id3_find;

static int is_4digits(const char *t)
{
   int i;
   for (i = 0; i < 4; i++)
      if (t[i] < '0' || t[i] > '9')
         return 0;
   return t[4] == '\0';
}

/* a text frame: the one asked for, or a part of a date */
static void id3_frame(id3_find *f, const char *id, int v22,
      rtags_src *s, size_t *left)
{
   static const char *const date_parts[3][2] = {
      { "TYER", "TYE" }, { "TDAT", "TDA" }, { "TIME", "TIM" }
   };
   uint32_t enc;
   int      k;

   if (f->done)
      return;
   if (f->field == RTAGS_DATE)
      for (k = 0; k < 3; k++)
         if (!strcmp(id, date_parts[k][v22]) && !f->part[k][0])
         {
            char      t[8];
            rtags_out o;
            o.s = t; o.cap = sizeof(t) - 1; o.n = 0; o.full = 0; t[0] = '\0';
            if (!src_uint(s, left, 1, 0, &enc))
               return;
            get_text(s, left, (int)enc, &o);
            /* only a value of four digits counts, as in FFmpeg */
            if (is_4digits(t))
               memcpy(f->part[k], t, 5);
            else
               f->part[k][0] = 'x';
            return;
         }
   if (strcmp(id, id3_frames[f->field][v22]) || f->out.n)
      return;
   if (!src_uint(s, left, 1, 0, &enc))
      return;
   get_text(s, left, (int)enc, &f->out);
   if (f->field == RTAGS_GENRE)
      genre_by_number(&f->out);
   if (f->out.n && f->field != RTAGS_DATE)
      f->done = 1;
}

static void id3v2_read(const uint8_t *p, size_t n, id3_find *f)
{
   unsigned  ver     = p[3];
   unsigned  flags   = p[5];
   size_t    size    = id3v2_size(p, n) - ((flags & 0x10) ? 10 : 0) - 10;
   int       v22     = ver == 2;
   unsigned  id_len  = v22 ? 3 : 4;
   unsigned  hdr_len = v22 ? 6 : 10;
   rtags_src s;
   size_t    left;

   if (size > n - 10)
      size = n - 10;
   if (ver < 2 || ver > 4 || (v22 && (flags & 0x40)))
      return;   /* unknown, or 2.2 compressed */

   /* 2.2 and 2.3 unsynchronise the tag as a whole, 2.4 each frame */
   src_span(&s, p + 10, size, ver < 4 && (flags & 0x80));
   left = (size_t)-1;

   if (!v22 && (flags & 0x40))
   {
      uint32_t ext;
      if (!src_uint(&s, &left, 4, 1, &ext))
         return;
      if (ver == 4)
         ext = ((ext & 0x7f000000) >> 3) | ((ext & 0x7f0000) >> 2)
             | ((ext & 0x7f00) >> 1) | (ext & 0x7f);
      src_skip(&s, ver == 4 ? (ext >= 4 ? ext - 4 : 0) : ext);
   }

   while (!f->done)
   {
      unsigned char h[10];
      char          id[5];
      unsigned      i, fflags = 0;
      size_t        fsize;
      for (i = 0; i < hdr_len; i++)
      {
         int b = src_byte(&s, &left);
         if (b < 0)
            return;
         h[i] = (unsigned char)b;
      }
      if (h[0] == 0)
         return;   /* padding */
      for (i = 0; i < id_len; i++)
      {
         if (!((h[i] >= 'A' && h[i] <= 'Z') || (h[i] >= '0' && h[i] <= '9')))
            return;
         id[i] = (char)h[i];
      }
      id[id_len] = '\0';
      if (v22)
         fsize = ((size_t)h[3] << 16) | ((size_t)h[4] << 8) | h[5];
      else if (ver == 3)
         fsize = rd_be32(h + 4);
      else
         fsize = ((size_t)(h[4] & 0x7f) << 21) | ((size_t)(h[5] & 0x7f) << 14)
               | ((size_t)(h[6] & 0x7f) << 7) | (h[7] & 0x7f);
      if (!v22)
         fflags = ((unsigned)h[8] << 8) | h[9];

      if (id[0] != 'T'
            || (ver == 3 && (fflags & 0x00c0))     /* compressed, encrypted */
            || (ver == 4 && (fflags & 0x000c)))
      {
         src_skip(&s, fsize);
         continue;
      }

      if (ver == 4)
      {
         /* the frame's stored bytes, unsynchronised if it says so */
         rtags_src fs;
         size_t    fleft = (size_t)-1, at = s.i;
         if (fsize > s.n - at)
            fsize = s.n - at;
         src_span(&fs, s.p + at, fsize, (fflags & 0x0002) || (flags & 0x80));
         src_skip(&s, fsize);
         if (fflags & 0x0040)   /* group */
            src_skip(&fs, 1);
         if (fflags & 0x0001)   /* data length */
            src_skip(&fs, 4);
         id3_frame(f, id, 0, &fs, &fleft);
      }
      else
      {
         /* 2.2, 2.3: the frame's size counts its bytes as read */
         size_t fleft = fsize;
         if (fflags & 0x0020)   /* group */
         {
            if (src_byte(&s, &fleft) < 0)
               return;
         }
         id3_frame(f, id, v22, &s, &fleft);
         /* whatever of the frame was not read */
         while (src_byte(&s, &fleft) >= 0) { }
         if (fleft)
            return;
      }
   }
}

/* An ID3v2 tag's value for @field: its length, 0 for none */
static size_t id3v2_get(const uint8_t *p, size_t n, int field, rtags_out *o)
{
   id3_find f;
   memset(&f, 0, sizeof(f));
   f.field = field;
   f.out   = *o;
   out_clear(&f.out);
   id3v2_read(p, n, &f);

   if (field == RTAGS_DATE && f.part[0][0] && f.part[0][0] != 'x')
   {
      /* TYER, then TDAT's DDMM, then TIME's hhmm */
      char d[17];
      memcpy(d, f.part[0], 4);
      d[4] = '\0';
      if (f.part[1][0] && f.part[1][0] != 'x')
      {
         d[4] = '-'; d[5] = f.part[1][2]; d[6] = f.part[1][3];
         d[7] = '-'; d[8] = f.part[1][0]; d[9] = f.part[1][1];
         d[10] = '\0';
         if (f.part[2][0] && f.part[2][0] != 'x')
         {
            d[10] = ' '; d[11] = f.part[2][0]; d[12] = f.part[2][1];
            d[13] = ':'; d[14] = f.part[2][2]; d[15] = f.part[2][3];
            d[16] = '\0';
         }
      }
      out_clear(&f.out);
      out_ascii(&f.out, d);
   }
   *o = f.out;
   return o->n;
}

/* ---------------------------------------------------------------- */
/* ID3v1                                                            */
/* ---------------------------------------------------------------- */

static size_t id3v1_get(const uint8_t *data, size_t len, int field, rtags_out *o)
{
   static const unsigned char at[4][2] = { { 3, 30 }, { 33, 30 }, { 63, 30 }, { 93, 4 } };
   const uint8_t *t;
   rtags_src      s;
   int            k = -1;

   if (len <= 128 || memcmp(data + len - 128, "TAG", 3))
      return 0;
   t = data + len - 128;
   out_clear(o);
   switch (field)
   {
      case RTAGS_TITLE:  k = 0; break;
      case RTAGS_ARTIST: k = 1; break;
      case RTAGS_ALBUM:  k = 2; break;
      case RTAGS_DATE:   k = 3; break;
      case RTAGS_TRACK:
         /* 1.1: a zero, then the track */
         if (t[125] == 0 && t[126] != 0)
            out_int(o, t[126]);
         return o->n;
      case RTAGS_GENRE:
         if (t[127] < RTAGS_GENRES)
            out_ascii(o, rtags_genres[t[127]]);
         return o->n;
      default:
         return 0;
   }
   {
      size_t left = at[k][1];
      src_span(&s, t + at[k][0], left, 0);
      get_text(&s, &left, TXT_UTF8, o);
   }
   out_trim(o);
   return o->n;
}

/* ---------------------------------------------------------------- */
/* Vorbis comments                                                  */
/* ---------------------------------------------------------------- */

static const char *const vc_keys[RTAGS_FIELDS] = {
   "TITLE", "ARTIST", "ALBUM", "GENRE", "TRACKNUMBER", "DISCNUMBER", "DATE"
};

/* A comment header's values for @field, joined by ';' */
static void vorbis_comments(rtags_src *s, int field, rtags_out *o)
{
   const char *want = vc_keys[field];
   size_t      left = (size_t)-1;
   uint32_t    vendor, count, i;

   if (!src_uint(s, &left, 4, 0, &vendor))
      return;
   src_skip(s, vendor);
   if (!src_uint(s, &left, 4, 0, &count))
      return;
   for (i = 0; i < count && !o->full; i++)
   {
      uint32_t len;
      size_t   clen, k = 0;
      int      match = 1, b;
      if (!src_uint(s, &left, 4, 0, &len))
         return;
      clen = len;
      /* the key, matched against the one wanted as it is read */
      for (;;)
      {
         char c;
         if ((b = src_byte(s, &clen)) < 0 || b == '=')
            break;
         c = (char)(b >= 'a' && b <= 'z' ? b - 32 : b);
         if (!want[k] || want[k] != c)
            match = 0;
         else
            k++;
      }
      if (b == '=' && match && !want[k])
      {
         if (o->n)
            out_cp(o, ';');
         get_text(s, &clen, TXT_UTF8, o);
      }
      src_skip(s, clen);
      if (s->mode == SRC_OGG ? s->dead || s->packet_end : s->i >= s->n)
         return;
   }
}

static size_t flac_get(const uint8_t *p, size_t n, int field, rtags_out *o)
{
   size_t at = 4;
   while (at + 4 <= n)
   {
      unsigned type = p[at] & 0x7f;
      int      last = p[at] & 0x80;
      size_t   len  = ((size_t)p[at + 1] << 16) | ((size_t)p[at + 2] << 8) | p[at + 3];
      at += 4;
      if (type == 4)
      {
         rtags_src s;
         src_span(&s, p + at, len < n - at ? len : n - at, 0);
         vorbis_comments(&s, field, o);
         return o->n;
      }
      if (last || len > n - at)
         break;
      at += len;
   }
   return 0;
}

static size_t ogg_get(const uint8_t *p, size_t n, int field, rtags_out *o)
{
   rtags_src s;
   char      id[8];
   size_t    left = 8;
   int       i, b;

   memset(&s, 0, sizeof(s));
   s.p    = p;
   s.n    = n;
   s.mode = SRC_OGG;
   s.back = -1;
   if (n < 27 || !(p[5] & 0x02))
      return 0;   /* the first page starts a stream */
   s.serial = rd_le32(p + 14);
   if (!ogg_page(&s, 0))
      return 0;

   /* the identification header says what the comments look like */
   for (i = 0; i < 8; i++)
   {
      if ((b = src_byte(&s, &left)) < 0)
         return 0;
      id[i] = (char)b;
   }
   if (!ogg_next_packet(&s))
      return 0;
   left = (size_t)-1;
   if (!memcmp(id, "\x01vorbis", 7))
   {
      for (i = 0; i < 7; i++)
         if (src_byte(&s, &left) != "\x03vorbis"[i])
            return 0;
   }
   else if (!memcmp(id, "OpusHead", 8))
   {
      for (i = 0; i < 8; i++)
         if (src_byte(&s, &left) != "OpusTags"[i])
            return 0;
   }
   else if (!memcmp(id, "\x7f" "FLAC", 5))
   {
      /* the packet after the STREAMINFO is a metadata block, the
       * comments by the specification */
      if ((src_byte(&s, &left) & 0x7f) != 4)
         return 0;
      for (i = 0; i < 3; i++)
         src_byte(&s, &left);
   }
   else
      return 0;
   vorbis_comments(&s, field, o);
   return o->n;
}

/* ---------------------------------------------------------------- */
/* WAVE                                                             */
/* ---------------------------------------------------------------- */

static size_t wav_get(const uint8_t *p, size_t n, int field, rtags_out *o)
{
   static const char *const info[RTAGS_FIELDS][2] = {
      { "INAM", "" }, { "IART", "" }, { "IPRD", "" }, { "IGNR", "" },
      { "IPRT", "ITRK" }, { "", "" }, { "ICRD", "" }
   };
   size_t at = 12, id3 = 0, id3_len = 0;

   while (at + 8 <= n)
   {
      size_t len = rd_le32(p + at + 4), body = at + 8;
      if (len > n - body)
         len = n - body;
      if (!memcmp(p + at, "LIST", 4) && len >= 4 && !memcmp(p + body, "INFO", 4))
      {
         size_t sub = body + 4;
         while (sub + 8 <= body + len)
         {
            size_t slen = rd_le32(p + sub + 4);
            if (slen > body + len - sub - 8)
               slen = body + len - sub - 8;
            if (     (info[field][0][0] && !memcmp(p + sub, info[field][0], 4))
                  || (info[field][1][0] && !memcmp(p + sub, info[field][1], 4)))
            {
               rtags_src s;
               size_t    left = slen;
               out_clear(o);
               src_span(&s, p + sub + 8, slen, 0);
               get_text(&s, &left, TXT_UTF8, o);
               if (o->n)
                  return o->n;
            }
            sub += 8 + slen + (slen & 1);
         }
      }
      else if (!memcmp(p + at, "id3 ", 4) || !memcmp(p + at, "ID3 ", 4))
      {
         id3     = body;
         id3_len = len;
      }
      at = body + len + (len & 1);
   }
   if (id3_len && id3v2_size(p + id3, id3_len))
      return id3v2_get(p + id3, id3_len, field, o);
   return 0;
}

/* ---------------------------------------------------------------- */
/* MP4, QuickTime                                                   */
/* ---------------------------------------------------------------- */

/* The box of @type among those in [@at, @end): its payload */
static int mp4_find(const uint8_t *p, size_t at, size_t end, const char *type,
      size_t *body, size_t *body_end)
{
   while (at + 8 <= end)
   {
      uint64_t size = rd_be32(p + at);
      size_t   hdr  = 8;
      if (size == 1)
      {
         if (at + 16 > end)
            return 0;
         size = ((uint64_t)rd_be32(p + at + 8) << 32) | rd_be32(p + at + 12);
         hdr  = 16;
      }
      else if (size == 0)
         size = end - at;
      if (size < hdr || size > end - at)
         return 0;
      if (!memcmp(p + at + 4, type, 4))
      {
         *body     = at + hdr;
         *body_end = at + (size_t)size;
         return 1;
      }
      at += (size_t)size;
   }
   return 0;
}

/* an item's data box into @o, as FFmpeg reads that item */
static void mp4_item(const uint8_t *p, size_t at, size_t end, const char *key,
      rtags_out *o)
{
   size_t   d, d_end, len;
   uint32_t type;
   if (!mp4_find(p, at, end, "data", &d, &d_end) || d_end - d < 8)
      return;
   type = rd_be32(p + d) & 0xffffff;
   d   += 8;   /* type, locale */
   len  = d_end - d;
   if (!memcmp(key, "trkn", 4) || !memcmp(key, "disk", 4))
   {
      int cur, total = 0;
      if (len < 4)
         return;
      cur = (int16_t)((p[d + 2] << 8) | p[d + 3]);
      if (len >= 6)
         total = (int16_t)((p[d + 4] << 8) | p[d + 5]);
      out_int(o, cur);
      if (total)
      {
         out_cp(o, '/');
         out_int(o, total);
      }
   }
   else if (!memcmp(key, "gnre", 4))
   {
      if (len >= 2 && p[d + 1] >= 1 && p[d + 1] <= RTAGS_GENRES)
         out_ascii(o, rtags_genres[p[d + 1] - 1]);
   }
   else
   {
      rtags_src s;
      src_span(&s, p + d, len, 0);
      get_text(&s, &len, type == 2 ? TXT_UTF16BE : TXT_UTF8, o);
   }
}

static const char *const mp4_keys[RTAGS_FIELDS][2] = {
   { "\xa9nam", ""        },
   { "\xa9" "ART", "\xa9" "aut" },
   { "\xa9" "alb", ""     },
   { "\xa9gen", "gnre"    },
   { "trkn", "\xa9trk"    },
   { "disk", ""           },
   { "\xa9" "day", ""     }
};

/* an ilst's item for @field */
static int mp4_ilst(const uint8_t *p, size_t at, size_t end, int field, rtags_out *o)
{
   int k;
   for (k = 0; k < 2; k++)
   {
      size_t b, b_end;
      if (!mp4_keys[field][k][0] || !mp4_find(p, at, end, mp4_keys[field][k], &b, &b_end))
         continue;
      out_clear(o);
      mp4_item(p, b, b_end, mp4_keys[field][k], o);
      if (o->n)
         return 1;
   }
   return 0;
}

/* a meta box's ilst: ISO's meta is a full box, QuickTime's not */
static int mp4_meta(const uint8_t *p, size_t at, size_t end, int field, rtags_out *o)
{
   size_t b, b_end;
   if (end - at >= 8 && memcmp(p + at + 4, "hdlr", 4))
      at += 4;
   return mp4_find(p, at, end, "ilst", &b, &b_end)
       && mp4_ilst(p, b, b_end, field, o);
}

static size_t mp4_get(const uint8_t *p, size_t n, int field, rtags_out *o)
{
   size_t moov, moov_end, b, b_end;
   int    k;
   if (!mp4_find(p, 0, n, "moov", &moov, &moov_end))
      return 0;
   if (mp4_find(p, moov, moov_end, "udta", &b, &b_end))
   {
      size_t m, m_end;
      if (mp4_find(p, b, b_end, "meta", &m, &m_end) && mp4_meta(p, m, m_end, field, o))
         return o->n;
      /* QuickTime's own: a length, a language, the text */
      for (k = 0; k < 2; k++)
      {
         size_t s, s_end;
         if (     (unsigned char)mp4_keys[field][k][0] == 0xa9
               && mp4_find(p, b, b_end, mp4_keys[field][k], &s, &s_end)
               && s_end - s >= 4 && memcmp(p + s + 4, "data", 4))
         {
            size_t    len = ((size_t)p[s] << 8) | p[s + 1];
            rtags_src src;
            if (len > s_end - s - 4)
               len = s_end - s - 4;
            out_clear(o);
            src_span(&src, p + s + 4, len, 0);
            get_text(&src, &len, TXT_UTF8, o);
            if (o->n)
               return o->n;
         }
      }
   }
   if (mp4_find(p, moov, moov_end, "meta", &b, &b_end) && mp4_meta(p, b, b_end, field, o))
      return o->n;
   return 0;
}

/* ---------------------------------------------------------------- */
/* ASF                                                              */
/* ---------------------------------------------------------------- */

static const uint8_t asf_header[16] = {
   0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
   0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c
};
static const uint8_t asf_content[16] = {
   0x33, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
   0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c
};
static const uint8_t asf_ext_content[16] = {
   0x40, 0xa4, 0xd0, 0xd2, 0x07, 0xe3, 0xd2, 0x11,
   0x97, 0xf0, 0x00, 0xa0, 0xc9, 0x5e, 0xa8, 0x50
};

static uint64_t rd_le64(const uint8_t *p)
{
   return (uint64_t)rd_le32(p) | ((uint64_t)rd_le32(p + 4) << 32);
}

/* a UTF-16LE name against an ASCII one, a NUL after it or not */
static int asf_name_is(const uint8_t *u, size_t len, const char *name)
{
   size_t i = 0;
   for (; i + 1 < len; i += 2)
   {
      unsigned c = u[i] | ((unsigned)u[i + 1] << 8);
      if (!c)
         break;
      if (!*name || c != (unsigned char)*name)
         return 0;
      name++;
   }
   return !*name;
}

/* FFmpeg's names (its "date" a key it writes as it is) and WM/Year */
static const char *const asf_keys[RTAGS_FIELDS][2] = {
   { "", "" }, { "", "" },
   { "WM/AlbumTitle", ""        },
   { "WM/Genre", ""             },
   { "WM/TrackNumber", ""       },
   { "WM/PartOfSet", ""         },
   { "WM/Year", "date"          }
};

static size_t asf_get(const uint8_t *p, size_t n, int field, rtags_out *o)
{
   uint64_t size = rd_le64(p + 16);
   size_t   end  = size < n ? (size_t)size : n;
   size_t   at   = 30;

   while (at + 24 <= end)
   {
      uint64_t osize = rd_le64(p + at + 16);
      size_t   body  = at + 24, oend;
      if (osize < 24 || osize > end - at)
         break;
      oend = at + (size_t)osize;
      if (!memcmp(p + at, asf_content, 16) && oend - body >= 10
            && (field == RTAGS_TITLE || field == RTAGS_ARTIST))
      {
         size_t    tlen = p[body] | ((size_t)p[body + 1] << 8);
         size_t    alen = p[body + 2] | ((size_t)p[body + 3] << 8);
         size_t    s    = body + 10 + (field == RTAGS_ARTIST ? tlen : 0);
         size_t    len  = field == RTAGS_ARTIST ? alen : tlen;
         rtags_src src;
         if (s <= oend)
         {
            if (len > oend - s)
               len = oend - s;
            out_clear(o);
            src_span(&src, p + s, len, 0);
            get_text(&src, &len, TXT_UTF16LE, o);
            if (o->n)
               return o->n;
         }
      }
      else if (!memcmp(p + at, asf_ext_content, 16) && oend - body >= 2
            && asf_keys[field][0][0])
      {
         unsigned count = p[body] | ((unsigned)p[body + 1] << 8), i;
         size_t   d     = body + 2;
         for (i = 0; i < count && d + 2 <= oend; i++)
         {
            size_t   nlen = p[d] | ((size_t)p[d + 1] << 8), vlen, v;
            unsigned vtype;
            int      k;
            if (nlen > oend - d - 2 || oend - d - 2 - nlen < 4)
               break;
            vtype = p[d + 2 + nlen] | ((unsigned)p[d + 3 + nlen] << 8);
            vlen  = p[d + 4 + nlen] | ((size_t)p[d + 5 + nlen] << 8);
            v     = d + 6 + nlen;
            if (vlen > oend - v)
               break;
            for (k = 0; k < 2; k++)
            {
               if (!asf_keys[field][k][0] || !asf_name_is(p + d + 2, nlen, asf_keys[field][k]))
                  continue;
               out_clear(o);
               switch (vtype)
               {
                  case 0:   /* text */
                  {
                     rtags_src src;
                     size_t    left = vlen;
                     src_span(&src, p + v, vlen, 0);
                     get_text(&src, &left, TXT_UTF16LE, o);
                     break;
                  }
                  case 2:   /* BOOL, 4 bytes here */
                  case 3:   /* DWORD */
                     if (vlen >= 4)
                        out_u64(o, rd_le32(p + v));
                     break;
                  case 4:   /* QWORD */
                     if (vlen >= 8)
                        out_u64(o, rd_le64(p + v));
                     break;
                  case 5:   /* WORD */
                     if (vlen >= 2)
                        out_u64(o, (uint64_t)(p[v] | (p[v + 1] << 8)));
                     break;
                  default:
                     break;
               }
               if (o->n)
                  return o->n;
            }
            d = v + vlen;
         }
      }
      at = oend;
   }
   return 0;
}

/* ---------------------------------------------------------------- */

enum
{
   CONT_RAW = 0,   /* MP3, ADTS, AC-3 ...: tags at the ends */
   CONT_FLAC,
   CONT_OGG,
   CONT_WAV,
   CONT_MP4,
   CONT_ASF
};

static int container(const uint8_t *p, size_t n)
{
   static const char *const mp4_top[] = {
      "ftyp", "moov", "mdat", "free", "skip", "wide", "pnot", "uuid"
   };
   size_t k;
   if (n >= 4 && !memcmp(p, "fLaC", 4))
      return CONT_FLAC;
   if (n >= 27 && !memcmp(p, "OggS", 4))
      return CONT_OGG;
   if (n >= 12 && !memcmp(p, "RIFF", 4) && !memcmp(p + 8, "WAVE", 4))
      return CONT_WAV;
   if (n >= 30 && !memcmp(p, asf_header, 16))
      return CONT_ASF;
   if (n >= 8)
      for (k = 0; k < sizeof(mp4_top) / sizeof(mp4_top[0]); k++)
         if (!memcmp(p + 4, mp4_top[k], 4))
            return CONT_MP4;
   return CONT_RAW;
}

size_t rtags_get(const uint8_t *data, size_t len, enum rtags_field field,
      char *out, size_t out_size)
{
   rtags_out o;
   size_t    at = 0, tag;
   int       cont;

   if (!out || !out_size)
      return 0;
   out[0] = '\0';
   if (!data || (unsigned)field >= RTAGS_FIELDS)
      return 0;
   o.s    = out;
   o.cap  = out_size - 1;
   o.n    = 0;
   o.full = 0;

   /* any ID3v2 tags in front, then what they are in front of */
   while ((tag = id3v2_size(data + at, len - at)) != 0 && tag <= len - at)
      at += tag;
   cont = container(data + at, len - at);

   switch (cont)
   {
      case CONT_FLAC: if (flac_get(data + at, len - at, field, &o)) return o.n; break;
      case CONT_OGG:  if (ogg_get(data + at, len - at, field, &o))  return o.n; break;
      case CONT_WAV:  if (wav_get(data + at, len - at, field, &o))  return o.n; break;
      case CONT_MP4:  if (mp4_get(data + at, len - at, field, &o))  return o.n; break;
      case CONT_ASF:  if (asf_get(data + at, len - at, field, &o))  return o.n; break;
      default: break;
   }

   for (at = 0; (tag = id3v2_size(data + at, len - at)) != 0 && tag <= len - at; at += tag)
      if (id3v2_get(data + at, tag, field, &o))
         return o.n;

   if (cont == CONT_RAW && id3v1_get(data, len, field, &o))
      return o.n;

   out_clear(&o);
   return 0;
}
