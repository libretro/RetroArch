/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_thumbnail_match.h"

#define GTM_NONE     ((size_t)-1)
#define GTM_COUNT(a) (sizeof(a) / sizeof((a)[0]))

enum gtm_kind
{
   GTM_SEP = 0,
   GTM_SKIP,
   GTM_LETTERS,
   GTM_FRACTION
};

struct gtm_out
{
   char *s;
   size_t len;
   size_t size;
};

/* U+00C0..U+017F as one ASCII letter; ' ' is punctuation, '*' two letters */
static const char gtm_latin[] =
   "aaaaaa*ceeeeiiii"
   "dnooooo ouuuuy**"
   "aaaaaa*ceeeeiiii"
   "dnooooo ouuuuy*y"
   "aaaaaaccccccccdd"
   "ddeeeeeeeeeegggg"
   "gggghhhhiiiiiiii"
   "ii**jjkkklllllll"
   "lllnnnnnnnnnoooo"
   "oo**rrrrrrssssss"
   "ssttttttuuuuuuuu"
   "uuuuwwyyyzzzzzzs";

static const char *gtm_latin_pair(unsigned cp)
{
   switch (cp)
   {
      case 0xC6:
      case 0xE6:
         return "ae";
      case 0xDE:
      case 0xFE:
         return "th";
      case 0xDF:
         return "ss";
      case 0x132:
      case 0x133:
         return "ij";
      default:
         break;
   }
   return "oe";
}

static enum gtm_kind gtm_char(const unsigned char *s, const unsigned char *e,
      char *out, size_t *out_len, size_t *adv)
{
   unsigned c = s[0];
   *adv       = 1;
   *out_len   = 0;
   if (c < 0x80)
   {
      if (c >= 'A' && c <= 'Z')
         c += 'a' - 'A';
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
      {
         out[0]   = (char)c;
         *out_len = 1;
         return GTM_LETTERS;
      }
      return (c == '\'') ? GTM_SKIP : GTM_SEP;
   }
   if (c >= 0xC2 && c <= 0xC5 && s + 1 < e && (s[1] & 0xC0) == 0x80)
   {
      unsigned cp = ((c & 0x1F) << 6) | (s[1] & 0x3F);
      char m;
      *adv = 2;
      if (cp >= 0xBC && cp <= 0xBE)
      {
         out[0]   = (cp == 0xBE) ? '3' : '1';
         out[1]   = (cp == 0xBD) ? '2' : '4';
         *out_len = 2;
         return GTM_FRACTION;
      }
      if (cp == 0xB2 || cp == 0xB3 || cp == 0xB9)
      {
         out[0]   = (cp == 0xB9) ? '1' : (cp == 0xB2) ? '2' : '3';
         *out_len = 1;
         return GTM_LETTERS;
      }
      if (cp < 0xC0)
         return GTM_SEP;
      m = gtm_latin[cp - 0xC0];
      if (m == ' ')
         return GTM_SEP;
      if (m == '*')
      {
         const char *pair = gtm_latin_pair(cp);
         out[0]   = pair[0];
         out[1]   = pair[1];
         *out_len = 2;
      }
      else
      {
         out[0]   = m;
         *out_len = 1;
      }
      return GTM_LETTERS;
   }
   if (c == 0xE2 && e - s > 2 && s[1] == 0x80)
   {
      unsigned d = s[2];
      if (d == 0x98 || d == 0x99)
      {
         *adv = 3;
         return GTM_SKIP;
      }
      if ((d >= 0x90 && d <= 0x95) || d == 0x9C || d == 0x9D)
      {
         *adv = 3;
         return GTM_SEP;
      }
   }
   out[0]   = (char)c;
   *out_len = 1;
   return GTM_LETTERS;
}

static void gtm_put(struct gtm_out *o, const char *s, size_t n)
{
   size_t room = o->size - 1 - o->len;
   if (n > room)
      n = room;
   memcpy(o->s + o->len, s, n);
   o->len       += n;
   o->s[o->len]  = '\0';
}

/* Unspaced, a space still parts two numbers: "1 & 2" is not "12". */
static void gtm_space(struct gtm_out *o, char c, bool spaced)
{
   if (o->len && (spaced || (c >= '0' && c <= '9'
         && o->s[o->len - 1] >= '0' && o->s[o->len - 1] <= '9')))
      gtm_put(o, " ", 1);
}

/* Appends the next word of [*p, e) to o, after gtm_space. In a title,
 * "][" is the word "ii" (Apple II style).
 * Returns the word's offset in o, GTM_NONE at the end. */
static size_t gtm_token(struct gtm_out *o, const unsigned char **p,
      const unsigned char *e, bool spaced, bool title)
{
   const unsigned char *s = *p;
   size_t at              = GTM_NONE;
   while (s < e)
   {
      char buf[2];
      size_t n, adv;
      enum gtm_kind kind;
      if (title && e - s > 1 && s[0] == ']' && s[1] == '[')
      {
         if (at == GTM_NONE)
         {
            gtm_space(o, 'i', spaced);
            at = o->len;
            gtm_put(o, "ii", 2);
            s += 2;
         }
         break;
      }
      kind = gtm_char(s, e, buf, &n, &adv);
      if ((kind == GTM_SEP || kind == GTM_FRACTION) && at != GTM_NONE)
         break;
      if (kind == GTM_LETTERS || kind == GTM_FRACTION)
      {
         if (at == GTM_NONE)
         {
            gtm_space(o, buf[0], spaced);
            at = o->len;
         }
         if (kind == GTM_FRACTION)
         {
            gtm_put(o, buf, 1);
            gtm_put(o, " ", 1);
            gtm_put(o, buf + 1, 1);
         }
         else
            gtm_put(o, buf, n);
      }
      s += adv;
      if (kind == GTM_FRACTION)
         break;
   }
   *p = s;
   return at;
}

static bool gtm_equal_nocase(const char *a, const char *b, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      unsigned x = (unsigned char)a[i];
      unsigned y = (unsigned char)b[i];
      if (x >= 'A' && x <= 'Z')
         x += 'a' - 'A';
      if (y >= 'A' && y <= 'Z')
         y += 'a' - 'A';
      if (x != y)
         return false;
   }
   return true;
}

/* Start of the next " - ", ": " or "_ " in [s, e), else e.
 * " _ " is a thumbnail's " & ", not its ": ". */
static const char *gtm_segment_end(const char *s, const char *e, size_t *sep)
{
   const char *p;
   for (p = s; p < e; p++)
   {
      if (e - p > 2 && p[0] == ' ' && p[1] == '-' && p[2] == ' ')
      {
         *sep = 3;
         return p;
      }
      if (     e - p > 1 && p[1] == ' '
            && (p[0] == ':' || (p[0] == '_' && (p == s || p[-1] != ' '))))
      {
         *sep = 2;
         return p;
      }
   }
   *sep = 0;
   return e;
}

/* "Legend of Zelda, The": the article, with *cut at the comma. */
static const char *gtm_article(const char *s, const char *e, const char **cut)
{
   static const char *const articles[] = { "the", "an", "a" };
   size_t i;
   for (i = 0; i < GTM_COUNT(articles); i++)
   {
      size_t n = strlen(articles[i]);
      const char *c;
      if ((size_t)(e - s) <= n + 2)
         continue;
      c = e - n - 2;
      if (c[0] == ',' && c[1] == ' ' && gtm_equal_nocase(c + 2, articles[i], n))
      {
         *cut = c;
         return articles[i];
      }
   }
   return NULL;
}

static bool gtm_is_dropped(const char *s, size_t n)
{
   return (n == 1 && s[0] == 'a')
      || (n == 2 && !memcmp(s, "an", 2))
      || (n == 3 && (!memcmp(s, "the", 3) || !memcmp(s, "and", 3)));
}

static const char *gtm_roman(const char *s, size_t n)
{
   static const char *const romans[] = {
      "ii", "iii", "iv", "vi", "vii", "viii", "ix", "xi", "xii", "xiii",
      "xiv", "xv", "xvi", "xvii", "xviii", "xix", "xx" };
   static const char *const numbers[] = {
      "2", "3", "4", "6", "7", "8", "9", "11", "12", "13",
      "14", "15", "16", "17", "18", "19", "20" };
   size_t i;
   for (i = 0; i < GTM_COUNT(romans); i++)
      if (strlen(romans[i]) == n && !memcmp(romans[i], s, n))
         return numbers[i];
   return NULL;
}

/* Loose: spaces only between numbers, no articles or "and",
 * II-XX as digits at the end. */
static void gtm_fold_segment(struct gtm_out *o, const char *s, const char *e,
      bool loose, bool title)
{
   const unsigned char *p   = (const unsigned char*)s;
   const unsigned char *end = (const unsigned char*)e;
   size_t last              = GTM_NONE;
   size_t at;
   while ((at = gtm_token(o, &p, end, !loose, title)) != GTM_NONE)
   {
      if (loose && gtm_is_dropped(o->s + at, o->len - at))
      {
         o->len       = at;
         o->s[o->len] = '\0';
         continue;
      }
      last = at;
   }
   if (loose && last != GTM_NONE)
   {
      const char *num = gtm_roman(o->s + last, o->len - last);
      if (num)
      {
         o->len       = last;
         o->s[o->len] = '\0';
         gtm_space(o, num[0], false);
         gtm_put(o, num, strlen(num));
      }
   }
}

static void gtm_fold_title(struct gtm_out *o, const char *s, const char *e,
      bool loose)
{
   size_t sep;
   const char *seg_end = gtm_segment_end(s, e, &sep);
   const char *cut     = seg_end;
   const char *article = gtm_article(s, seg_end, &cut);
   if (article && !loose)
      gtm_put(o, article, strlen(article));
   gtm_fold_segment(o, s, cut, loose, true);
   while (seg_end < e)
   {
      s       = seg_end + sep;
      seg_end = gtm_segment_end(s, e, &sep);
      gtm_fold_segment(o, s, seg_end, loose, true);
   }
}

/* The title ends at the first " (" or " [". */
static const char *gtm_title_end(const char *s, const char *e)
{
   const char *p;
   for (p = s; e - p > 2; p++)
      if (p[1] == ' ' && (p[2] == '(' || p[2] == '['))
         return p + 1;
   return e;
}

static size_t gtm_fold_name(struct gtm_out *o, const char *s, const char *e,
      bool loose, size_t *tlen)
{
   const char *t = gtm_title_end(s, e);
   gtm_fold_title(o, s, t, loose);
   *tlen = o->len;
   gtm_fold_segment(o, t, e, false, false);
   return o->len;
}

size_t gfx_thumbnail_match_fold(const char *in, char *out, size_t len)
{
   struct gtm_out o;
   size_t tlen;
   if (!out || !len)
      return 0;
   out[0] = '\0';
   if (!in)
      return 0;
   o.s    = out;
   o.len  = 0;
   o.size = len;
   return gtm_fold_name(&o, in, in + strlen(in), false, &tlen);
}

#define GTM_NAME_MAX   1024
#define GTM_MAX_TAGS   16
#define GTM_POOL_BLOCK 65536

#define GTM_STORE  0x01
#define GTM_NOTAGS 0x02
#define GTM_WEST   0x04
#define GTM_EAST   0x08

#define GTM_USA    0x00000001UL
#define GTM_EUROPE 0x00000002UL
#define GTM_JAPAN  0x00000004UL
#define GTM_WORLD  0x00000008UL
#define GTM_OTHER  0x80000000UL
/* Japan, Asia, China, Hong Kong, Korea, Taiwan */
#define GTM_EAST_BITS 0x00022914UL

enum gtm_tier
{
   GTM_T0 = 0,
   GTM_T2,
   GTM_T2B,
   GTM_T3,
   GTM_T3B
};

struct gtm_rec
{
   const char *name;
   const char *key;   /* T2 fold; the title key is its first tlen bytes */
   const char *loose; /* T2b fold; the loose title is its first ltlen bytes */
   const char *ident; /* identity tags, folded, sorted, '\001'-joined */
   uint32_t region;
   uint16_t stem_len;
   uint16_t tlen;
   uint16_t ltlen;
   uint16_t rev;
   uint8_t disc;
   uint8_t ext;
   uint8_t flags;
   uint8_t ntags;
};

struct gtm_pool
{
   struct gtm_pool *next;
   size_t used;
   size_t size;
};

struct gtm_parse
{
   size_t key_len;
   size_t loose_len;
   size_t ident_len;
   size_t tlen;
   size_t ltlen;
   size_t tag_len;
   size_t tags[GTM_MAX_TAGS];
   uint32_t region;
   unsigned rev;
   unsigned disc;
   unsigned ntags;
   unsigned nident;
   unsigned flags;
   char stem[GTM_NAME_MAX];
   char key[GTM_NAME_MAX + 8];
   char loose[GTM_NAME_MAX + 8];
   char ident[GTM_NAME_MAX * 2];
   char tagbuf[GTM_NAME_MAX * 2];
};

struct gfx_thumbnail_match_index
{
   struct gtm_rec *recs;
   const struct gtm_rec **by_loose;
   struct gtm_pool *pool;
   struct gtm_parse *parse; /* freed when finalize begins */
   void *scratch;
   gfx_thumbnail_match_sort_t sort;
   size_t count;
   size_t cap;
   size_t filled; /* by_loose entries set */
   bool finalized;
};

struct gtm_code
{
   const char *name;
   uint32_t bits;
};

static const struct gtm_code gtm_regions[] = {
   { "USA",           0x00000001UL }, { "Europe",        0x00000002UL },
   { "Japan",         0x00000004UL }, { "World",         0x00000008UL },
   { "Asia",          0x00000010UL }, { "Australia",     0x00000020UL },
   { "Brazil",        0x00000040UL }, { "Canada",        0x00000080UL },
   { "China",         0x00000100UL }, { "France",        0x00000200UL },
   { "Germany",       0x00000400UL }, { "Hong Kong",     0x00000800UL },
   { "Italy",         0x00001000UL }, { "Korea",         0x00002000UL },
   { "Netherlands",   0x00004000UL }, { "Spain",         0x00008000UL },
   { "Sweden",        0x00010000UL }, { "Taiwan",        0x00020000UL },
   { "UK",            0x00040000UL }, { "Russia",        0x00080000UL },
   { "Scandinavia",   0x00100000UL }, { "Latin America", 0x00200000UL },
   { "Greece",        0x00400000UL }, { "Denmark",       0x00800000UL },
   { "Finland",       0x01000000UL }, { "Norway",        0x02000000UL },
   { "Portugal",      0x04000000UL }, { "Poland",        0x08000000UL },
   { "Mexico",        0x10000000UL }, { "Argentina",     0x20000000UL },
   { "India",         0x40000000UL }, { "Austria",       GTM_OTHER },
   { "Belgium",       GTM_OTHER },    { "Croatia",       GTM_OTHER },
   { "Czech",         GTM_OTHER },    { "Hungary",       GTM_OTHER },
   { "Ireland",       GTM_OTHER },    { "Israel",        GTM_OTHER },
   { "New Zealand",   GTM_OTHER },    { "Slovakia",      GTM_OTHER },
   { "Switzerland",   GTM_OTHER },    { "Turkey",        GTM_OTHER },
   { "Ukraine",       GTM_OTHER },    { "South Africa",  GTM_OTHER },
   { "Chile",         GTM_OTHER },    { "Colombia",      GTM_OTHER },
   { "Peru",          GTM_OTHER },    { "Thailand",      GTM_OTHER },
   { "Singapore",     GTM_OTHER },    { "Malaysia",      GTM_OTHER },
   { "Indonesia",     GTM_OTHER },    { "Philippines",   GTM_OTHER },
   { "Vietnam",       GTM_OTHER },    { "Iceland",       GTM_OTHER },
   { "Estonia",       GTM_OTHER },    { "Latvia",        GTM_OTHER },
   { "Lithuania",     GTM_OTHER },    { "Romania",       GTM_OTHER },
   { "Bulgaria",      GTM_OTHER },    { "Serbia",        GTM_OTHER },
   { "Slovenia",      GTM_OTHER },    { "United Arab Emirates", GTM_OTHER },
   { "Saudi Arabia",  GTM_OTHER },    { "Unknown",       GTM_OTHER }
};

/* The GTM_OTHER codes of the Japan/Asia family */
static const struct gtm_code gtm_other_east[] = {
   { "Thailand",    GTM_EAST }, { "Singapore",   GTM_EAST },
   { "Malaysia",    GTM_EAST }, { "Indonesia",   GTM_EAST },
   { "Philippines", GTM_EAST }, { "Vietnam",     GTM_EAST }
};

/* TOSEC country codes, '-'-joined */
static const struct gtm_code gtm_tosec[] = {
   { "US", 0x00000001UL }, { "EU", 0x00000002UL }, { "JP", 0x00000004UL },
   { "AS", 0x00000010UL }, { "AU", 0x00000020UL }, { "BR", 0x00000040UL },
   { "CA", 0x00000080UL }, { "CN", 0x00000100UL }, { "FR", 0x00000200UL },
   { "DE", 0x00000400UL }, { "HK", 0x00000800UL }, { "IT", 0x00001000UL },
   { "KR", 0x00002000UL }, { "NL", 0x00004000UL }, { "ES", 0x00008000UL },
   { "SE", 0x00010000UL }, { "TW", 0x00020000UL }, { "GB", 0x00040000UL },
   { "RU", 0x00080000UL }, { "GR", 0x00400000UL }, { "DK", 0x00800000UL },
   { "FI", 0x01000000UL }, { "NO", 0x02000000UL }, { "PT", 0x04000000UL },
   { "PL", 0x08000000UL }
};

/* GoodTools region codes */
static const struct gtm_code gtm_goodtools[] = {
   { "U",   0x00000001UL }, { "E",  0x00000002UL }, { "J",  0x00000004UL },
   { "W",   0x00000008UL }, { "UE", 0x00000003UL }, { "JU", 0x00000005UL },
   { "JE",  0x00000006UL }, { "JUE", 0x00000007UL }, { "A",  0x00000020UL },
   { "B",   0x00000040UL }, { "C",  0x00000100UL }, { "F",  0x00000200UL },
   { "G",   0x00000400UL }, { "I",  0x00001000UL }, { "K",  0x00002000UL },
   { "H",   0x00004000UL }, { "S",  0x00008000UL }
};

/* The GoodTools codes typed in lowercase: "(i)" is a numeral, "(h)" a hack */
static const struct gtm_code gtm_goodtools_lower[] = {
   { "U",   0x00000001UL }, { "E",  0x00000002UL }, { "J",  0x00000004UL },
   { "W",   0x00000008UL }, { "UE", 0x00000003UL }, { "JU", 0x00000005UL },
   { "JE",  0x00000006UL }, { "JUE", 0x00000007UL }
};

static const char *const gtm_exts[] = {
   ".png", ".jpg", ".jpeg", ".bmp", ".tga",
#ifdef HAVE_RWEBP
   ".webp",
#endif
#ifdef HAVE_RWEBM
   ".webm", ".mkv",
#endif
#ifdef HAVE_RMP4
   ".mp4",
#endif
   NULL
};

static bool gtm_same(const char *a, const char *b, size_t n, bool nocase)
{
   return nocase ? gtm_equal_nocase(a, b, n) : !memcmp(a, b, n);
}

static uint32_t gtm_code_bits(const struct gtm_code *t, size_t count,
      const char *s, size_t n, bool nocase)
{
   size_t i;
   for (i = 0; i < count; i++)
      if (strlen(t[i].name) == n && gtm_same(t[i].name, s, n, nocase))
         return t[i].bits;
   return 0;
}

/* One code's region families: World is in both, Unknown in neither. */
static unsigned gtm_family(uint32_t bits, const char *s, size_t n,
      bool nocase)
{
   if (bits == GTM_OTHER)
   {
      if (n == 7 && gtm_same(s, "Unknown", 7, nocase))
         return 0;
      return gtm_code_bits(gtm_other_east, GTM_COUNT(gtm_other_east), s, n,
            nocase) ? GTM_EAST : GTM_WEST;
   }
   return ((bits & (GTM_EAST_BITS | GTM_WORLD)) ? GTM_EAST : 0)
      | ((bits & ~GTM_EAST_BITS) ? GTM_WEST : 0);
}

/* Every sep-separated element of [c, c + n) is in t, else 0.
 * *fam gets their families. */
static uint32_t gtm_code_list(const struct gtm_code *t, size_t count,
      const char *c, size_t n, char sep, unsigned *fam, bool nocase)
{
   const char *e = c + n;
   uint32_t mask = 0;
   unsigned f    = 0;
   while (c < e)
   {
      const char *q = c;
      uint32_t bits;
      while (q < e && *q != sep)
         q++;
      if (!(bits = gtm_code_bits(t, count, c, (size_t)(q - c), nocase)))
         return 0;
      mask |= bits;
      f    |= gtm_family(bits, c, (size_t)(q - c), nocase);
      c     = q;
      if (c < e)
      {
         c++;
         if (sep == ',' && c < e && *c == ' ')
            c++;
      }
   }
   *fam = f;
   return mask;
}

static uint32_t gtm_region(const char *c, size_t n, unsigned *fam,
      bool nocase)
{
   uint32_t mask = gtm_code_list(gtm_regions, GTM_COUNT(gtm_regions), c, n,
         ',', fam, nocase);
   /* TOSEC writes countries in uppercase; lowercase is a language */
   if (!mask && !nocase)
      mask = gtm_code_list(gtm_tosec, GTM_COUNT(gtm_tosec), c, n, '-', fam,
            false);
   if (!mask)
   {
      mask = nocase
         ? gtm_code_bits(gtm_goodtools_lower, GTM_COUNT(gtm_goodtools_lower),
               c, n, true)
         : gtm_code_bits(gtm_goodtools, GTM_COUNT(gtm_goodtools), c, n,
               false);
      *fam = gtm_family(mask, c, n, nocase);
   }
   return mask;
}

static size_t gtm_span_digits(const char *s, size_t n)
{
   size_t i = 0;
   while (i < n && s[i] >= '0' && s[i] <= '9')
      i++;
   return i;
}

static bool gtm_digits(const char *s, size_t n)
{
   return n && gtm_span_digits(s, n) == n;
}

static unsigned gtm_number(const char *s, size_t n)
{
   unsigned v = 0;
   size_t i;
   for (i = 0; i < n && v < 6000; i++)
      v = v * 10 + (unsigned)(s[i] - '0');
   return v;
}

static bool gtm_has_prefix(const char *c, size_t n, const char *p,
      bool nocase)
{
   size_t k = strlen(p);
   return n >= k && gtm_same(c, p, k, nocase);
}

static bool gtm_has_suffix(const char *c, size_t n, const char *p,
      bool nocase)
{
   size_t k = strlen(p);
   return n >= k && gtm_same(c + n - k, p, k, nocase);
}

static bool gtm_contains(const char *c, size_t n, const char *p, bool nocase)
{
   size_t k = strlen(p);
   size_t i;
   for (i = 0; i + k <= n; i++)
      if (gtm_same(c + i, p, k, nocase))
         return true;
   return false;
}

/* A is 1 */
static unsigned gtm_letter(char c, bool nocase)
{
   if (c >= 'A' && c <= 'Z')
      return (unsigned)(c - 'A' + 1);
   if (nocase && c >= 'a' && c <= 'z')
      return (unsigned)(c - 'a' + 1);
   return 0;
}

/* Ignoring case, any two letters would pass, publisher initials too;
 * no "tr" (translation) or "vi" (numeral). */
static const char gtm_langs[] =
   "ar bg bs ca cs da de el en es et fi fr he hr hu it ja ko lt lv nl no pl "
   "pt ro ru sh sk sl sr sv th zh";

/* No-Intro "En,Fr,De"; ignoring case, listed codes in lowercase, also
 * TOSEC's "en-de" */
static bool gtm_language(const char *c, size_t n, bool nocase)
{
   size_t i, j;
   if (n < 2 || (n + 1) % 3)
      return false;
   for (i = 0; i < n; i += 3)
   {
      if (nocase)
      {
         for (j = 0; j + 2 < sizeof(gtm_langs); j += 3)
            if (!memcmp(gtm_langs + j, c + i, 2))
               break;
         if (j + 2 >= sizeof(gtm_langs))
            return false;
      }
      else if (c[i] < 'A' || c[i] > 'Z' || c[i + 1] < 'a' || c[i + 1] > 'z')
         return false;
      if (i + 2 < n && c[i + 2] != ',' && (!nocase || c[i + 2] != '-'))
         return false;
   }
   return true;
}

/* TOSEC "1989", "1989-11", "198x-11-2x" */
static bool gtm_date(const char *c, size_t n)
{
   size_t i;
   if ((n != 4 && n != 7 && n != 10) || (c[0] != '1' && c[0] != '2')
         || c[1] < '0' || c[1] > '9')
      return false;
   for (i = 2; i < n; i++)
   {
      if (i == 4 || i == 7)
      {
         if (c[i] != '-')
            return false;
      }
      else if ((c[i] < '0' || c[i] > '9') && c[i] != 'x')
         return false;
   }
   return true;
}

static bool gtm_alt(const char *c, size_t n, bool nocase)
{
   return (n == 3 && gtm_same(c, "Alt", 3, nocase))
      || (n > 4 && gtm_same(c, "Alt ", 4, nocase)
            && gtm_digits(c + 4, n - 4));
}

static bool gtm_store(const char *c, size_t n, bool nocase)
{
   return gtm_contains(c, n, "Virtual Console", nocase)
      || (n == 13 && gtm_same(c, "Switch Online", 13, nocase))
      || (n == 2 && gtm_same(c, "NP", 2, nocase))
      || gtm_has_suffix(c, n, " Collection", nocase);
}

/* Rev 1 and Rev A are 1; v1.1 is 1, v2.00 is 100; PRG1 and REV01 are 1. */
static bool gtm_revision(const char *c, size_t n, unsigned *v, bool nocase)
{
   const char *d;
   size_t k;
   if (gtm_has_prefix(c, n, "Rev ", nocase))
   {
      if (n == 5 && (*v = gtm_letter(c[4], nocase)))
         return true;
      if (!gtm_digits(c + 4, n - 4))
         return false;
      *v = gtm_number(c + 4, n - 4);
      return true;
   }
   if (     gtm_has_prefix(c, n, "REV", nocase)
         || gtm_has_prefix(c, n, "PRG", nocase))
   {
      k = (n > 3 && c[3] == ' ') ? 4 : 3;
      if (!gtm_digits(c + k, n - k))
         return false;
      *v = gtm_number(c + k, n - k);
      return true;
   }
   if (n < 4 || (c[0] != 'v' && c[0] != 'V'))
      return false;
   if (!(d = (const char*)memchr(c, '.', n)))
      return false;
   k = (size_t)(d - c);
   if (!gtm_digits(c + 1, k - 1) || !gtm_digits(d + 1, n - k - 1))
      return false;
   *v = gtm_number(c + 1, k - 1) * 100 + gtm_number(d + 1, n - k - 1);
   if (*v >= 100)
      *v -= 100;
   if (*v > 0xFFFF)
      *v = 0xFFFF;
   return true;
}

/* Disc 1, Disc A, Disk 1 of 2, Side B, Tape 2: A is 1. */
static bool gtm_disc(const char *c, size_t n, unsigned *v, bool nocase)
{
   static const char *const words[] = { "Disc ", "Disk ", "Side ", "Tape " };
   size_t i, k;
   for (i = 0; i < GTM_COUNT(words); i++)
      if (n > 5 && gtm_same(c, words[i], 5, nocase))
         break;
   if (i == GTM_COUNT(words))
      return false;
   c += 5;
   n -= 5;
   if ((*v = gtm_letter(c[0], nocase)))
      k = 1;
   else
   {
      if (!(k = gtm_span_digits(c, n)))
         return false;
      *v = gtm_number(c, k);
   }
   if (*v > 255)
      *v = 255;
   if (k == n)
      return true;
   return gtm_has_prefix(c + k, n - k, " of ", nocase)
      && gtm_digits(c + k + 4, n - k - 4);
}

/* tag: a tag that folds to nothing, TOSEC's unknown publisher "(-)", is
 * kept as written, so at T3 it never equals no tag. */
static void gtm_identity(struct gtm_parse *p, const char *s, const char *e,
      bool tag)
{
   struct gtm_out o;
   if (     p->nident >= GTM_MAX_TAGS
         || sizeof(p->tagbuf) - p->tag_len < 2)
   {
      p->flags |= GTM_NOTAGS;
      return;
   }
   o.s    = p->tagbuf + p->tag_len;
   o.len  = 0;
   o.size = sizeof(p->tagbuf) - p->tag_len;
   o.s[0] = '\0';
   gtm_fold_segment(&o, s, e, false, false);
   if (!o.len && tag)
      gtm_put(&o, s, (size_t)(e - s));
   if (!o.len)
      return;
   p->tags[p->nident++]  = p->tag_len;
   p->tag_len           += o.len + 1;
}

/* tosec: a '(' follows directly. Only a TOSEC date, before its publisher,
 * is dropped; a bare year keeps same-titled games apart.
 * false: an identity tag. */
static bool gtm_paren_known(struct gtm_parse *p, const char *c, size_t n,
      bool tosec, bool nocase)
{
   unsigned v    = 0;
   unsigned fam  = 0;
   uint32_t bits = gtm_region(c, n, &fam, nocase);
   if (bits)
   {
      p->region |= bits;
      p->flags  |= fam;
   }
   else if (gtm_language(c, n, nocase) || (tosec && gtm_date(c, n))
         || gtm_alt(c, n, nocase)
         || gtm_has_suffix(c, n, " Enhanced", nocase)
         || gtm_has_suffix(c, n, " Compatible", nocase))
      p->ntags++;
   else if (gtm_store(c, n, nocase))
   {
      p->flags |= GTM_STORE;
      p->ntags++;
   }
   else if (gtm_revision(c, n, &v, nocase))
      p->rev = v;
   else if (!p->disc && gtm_disc(c, n, &v, nocase))
      p->disc = v;
   else
      return false;
   return true;
}

/* A tag unknown as written is tried again ignoring case. "PT-BR" in any
 * case marks a Brazilian translation, not two TOSEC countries. */
static void gtm_paren(struct gtm_parse *p, const char *c, size_t n,
      bool tosec)
{
   if (     (n == 5 && gtm_equal_nocase(c, "pt-br", 5))
         || (     !gtm_paren_known(p, c, n, tosec, false)
               && !gtm_paren_known(p, c, n, tosec, true)))
      gtm_identity(p, c, c + n, true);
}

/* [!] [a1] [b] [o] [f] [u] [cr Name] are dump flags. */
static void gtm_bracket(struct gtm_parse *p, const char *c, size_t n)
{
   size_t k = 0;
   if (n == 1 && c[0] == '!')
      k = 1;
   else if (n >= 2 && c[0] == 'c' && c[1] == 'r')
      k = 2;
   else if (n >= 1 && (c[0] == 'a' || c[0] == 'b' || c[0] == 'o'
            || c[0] == 'f' || c[0] == 'u'))
      k = 1;
   if (k && (k == n || c[k] == ' ' || gtm_digits(c + k, n - k)))
      p->ntags++;
   else
      gtm_identity(p, c, c + n, true);
}

static void gtm_parse_tags(struct gtm_parse *p, const char *s, const char *e)
{
   while (s < e)
   {
      const char *q;
      char open = *s;
      if (open == ' ')
         s++;
      else if (open == '(' || open == '[')
      {
         char close     = (open == '(') ? ')' : ']';
         unsigned depth = 0;
         for (q = s; q < e; q++)
         {
            if (*q == open)
               depth++;
            else if (*q == close && --depth == 0)
               break;
         }
         if (open == '(')
            gtm_paren(p, s + 1, (size_t)(q - s - 1),
                  q + 1 < e && q[1] == '(');
         else
            gtm_bracket(p, s + 1, (size_t)(q - s - 1));
         s = (q < e) ? q + 1 : e;
      }
      else
      {
         q = s;
         while (q < e && *q != '(' && *q != '[')
            q++;
         gtm_identity(p, s, q, false);
         s = q;
      }
   }
}

static void gtm_join_identity(struct gtm_parse *p)
{
   struct gtm_out o;
   unsigned i, j;
   for (i = 1; i < p->nident; i++)
   {
      size_t t = p->tags[i];
      for (j = i; j > 0
            && strcmp(p->tagbuf + p->tags[j - 1], p->tagbuf + t) > 0; j--)
         p->tags[j] = p->tags[j - 1];
      p->tags[j] = t;
   }
   o.s         = p->ident;
   o.len       = 0;
   o.size      = sizeof(p->ident);
   p->ident[0] = '\0';
   for (i = 0; i < p->nident; i++)
   {
      if (i)
         gtm_put(&o, "\001", 1);
      gtm_put(&o, p->tagbuf + p->tags[i], strlen(p->tagbuf + p->tags[i]));
   }
   p->ident_len = o.len;
}

static void gtm_parse_stem(struct gtm_parse *p, const char *s, size_t len)
{
   struct gtm_out o;
   const char *e = s + len;
   p->region     = 0;
   p->rev        = 0;
   p->disc       = 0;
   p->ntags      = 0;
   p->nident     = 0;
   p->flags      = 0;
   p->tag_len    = 0;
   o.s           = p->key;
   o.len         = 0;
   o.size        = sizeof(p->key);
   p->key[0]     = '\0';
   p->key_len    = gtm_fold_name(&o, s, e, false, &p->tlen);
   o.s           = p->loose;
   o.len         = 0;
   o.size        = sizeof(p->loose);
   p->loose[0]   = '\0';
   p->loose_len  = gtm_fold_name(&o, s, e, true, &p->ltlen);
   gtm_parse_tags(p, gtm_title_end(s, e), e);
   gtm_join_identity(p);
}

static const char *gtm_pool_add(gfx_thumbnail_match_index_t *idx,
      const char *s, size_t len)
{
   struct gtm_pool *b = idx->pool;
   char *dst;
   if (!b || b->size - b->used < len + 1)
   {
      size_t size = (len + 1 > GTM_POOL_BLOCK) ? len + 1 : GTM_POOL_BLOCK;
      if (!(b = (struct gtm_pool*)malloc(sizeof(*b) + size)))
         return NULL;
      b->next   = idx->pool;
      b->used   = 0;
      b->size   = size;
      idx->pool = b;
   }
   dst       = (char*)(b + 1) + b->used;
   memcpy(dst, s, len);
   dst[len]  = '\0';
   b->used  += len + 1;
   return dst;
}

/* Rank of the extension in gtm_exts, -1 when it is not a thumbnail. */
static int gtm_ext(const char *name, size_t len, size_t *stem_len)
{
   int i;
   for (i = 0; gtm_exts[i]; i++)
   {
      size_t n = strlen(gtm_exts[i]);
      if (len > n && gtm_equal_nocase(name + len - n, gtm_exts[i], n))
      {
         *stem_len = len - n;
         return i;
      }
   }
   return -1;
}

/* What the menu scrubs before it looks for a file. */
static void gtm_scrub(char *dst, const char *src, size_t len)
{
   size_t i;
   for (i = 0; i < len; i++)
   {
      char c = src[i];
      if (c && strchr("&*/:`\"<>?\\|", c))
         c = '_';
      dst[i] = c;
   }
   dst[len] = '\0';
}

static int gtm_cmp_span(const char *a, size_t an, const char *b, size_t bn)
{
   int r = memcmp(a, b, an < bn ? an : bn);
   if (r)
      return r;
   if (an == bn)
      return 0;
   return (an < bn) ? -1 : 1;
}

static int gtm_cmp_name(const struct gtm_rec *a, const struct gtm_rec *b)
{
   int r = gtm_cmp_span(a->name, a->stem_len, b->name, b->stem_len);
   if (r)
      return r;
   if (a->ext != b->ext)
      return (int)a->ext - (int)b->ext;
   return strcmp(a->name, b->name);
}

static int gtm_cmp_title(const void *pa, const void *pb)
{
   const struct gtm_rec *a = (const struct gtm_rec*)pa;
   const struct gtm_rec *b = (const struct gtm_rec*)pb;
   int r = gtm_cmp_span(a->key, a->tlen, b->key, b->tlen);
   return r ? r : gtm_cmp_name(a, b);
}

static int gtm_cmp_loose(const void *pa, const void *pb)
{
   const struct gtm_rec *a = *(const struct gtm_rec *const*)pa;
   const struct gtm_rec *b = *(const struct gtm_rec *const*)pb;
   int r = gtm_cmp_span(a->loose, a->ltlen, b->loose, b->ltlen);
   return r ? r : gtm_cmp_name(a, b);
}

gfx_thumbnail_match_index_t *gfx_thumbnail_match_index_new(void)
{
   gfx_thumbnail_match_index_t *idx = (gfx_thumbnail_match_index_t*)
      calloc(1, sizeof(*idx));
   if (!idx)
      return NULL;
   if (!(idx->parse = (struct gtm_parse*)malloc(sizeof(*idx->parse))))
   {
      free(idx);
      return NULL;
   }
   return idx;
}

bool gfx_thumbnail_match_index_add(gfx_thumbnail_match_index_t *idx,
      const char *name)
{
   struct gtm_parse *p;
   struct gtm_rec *r;
   size_t len;
   size_t stem_len = 0;
   int ext;
   if (!idx || !name || !idx->parse)
      return false;
   len = strlen(name);
   ext = gtm_ext(name, len, &stem_len);
   if (ext < 0 || !stem_len || stem_len >= GTM_NAME_MAX)
      return true;
   if (idx->count == idx->cap)
   {
      size_t cap           = idx->cap ? idx->cap * 2 : 256;
      struct gtm_rec *recs = (struct gtm_rec*)realloc(idx->recs,
            cap * sizeof(*recs));
      if (!recs)
         return false;
      idx->recs = recs;
      idx->cap  = cap;
   }
   p = idx->parse;
   r = &idx->recs[idx->count];
   gtm_parse_stem(p, name, stem_len);
   if (     !(r->name  = gtm_pool_add(idx, name, len))
         || !(r->key   = gtm_pool_add(idx, p->key, p->key_len))
         || !(r->loose = gtm_pool_add(idx, p->loose, p->loose_len))
         || !(r->ident = p->ident_len
            ? gtm_pool_add(idx, p->ident, p->ident_len) : ""))
      return false;
   r->region   = p->region;
   r->stem_len = (uint16_t)stem_len;
   r->tlen     = (uint16_t)p->tlen;
   r->ltlen    = (uint16_t)p->ltlen;
   r->rev      = (uint16_t)p->rev;
   r->disc     = (uint8_t)p->disc;
   r->ext      = (uint8_t)ext;
   r->flags    = (uint8_t)p->flags;
   r->ntags    = (uint8_t)(p->ntags > 255 ? 255 : p->ntags);
   idx->count++;
   return true;
}

void gfx_thumbnail_match_sort_begin(gfx_thumbnail_match_sort_t *s,
      void *base, void *scratch, size_t count, size_t size,
      int (*cmp)(const void *a, const void *b))
{
   s->cmp   = cmp;
   s->base  = (char*)base;
   s->src   = (char*)base;
   s->dst   = (char*)scratch;
   s->count = count;
   s->size  = size;
   s->width = 1;
   s->i     = 0;
   s->mid   = 0;
   s->j     = 0;
   s->hi    = 0;
   s->k     = 0;
   s->moved = 0;
}

/* Each pass merges runs of width from src into dst, then the two swap.
 * Once one run is left, a last pass copies it home if it is not. */
bool gfx_thumbnail_match_sort_step(gfx_thumbnail_match_sort_t *s)
{
   size_t left = GFX_THUMBNAIL_MATCH_SORT_STEP;
   size_t size = s->size;
   if (s->width >= s->count && s->src == s->base)
      return false;
   for (;;)
   {
      size_t n;
      if (s->k == s->hi)
      {
         if (s->hi == s->count)
         {
            char *t = s->src;
            s->src  = s->dst;
            s->dst  = t;
            if (s->width < s->count)
               s->width *= 2;
            if (s->width >= s->count && s->src == s->base)
               return true;
            s->k    = 0;
         }
         s->i   = s->k;
         s->mid = (s->count - s->i > s->width) ? s->i + s->width : s->count;
         s->j   = s->mid;
         s->hi  = (s->count - s->j > s->width) ? s->j + s->width : s->count;
         continue;
      }
      if (!left)
         return true;
      if (s->i < s->mid && s->j < s->hi)
      {
         size_t from = (s->cmp(s->src + s->i * size,
               s->src + s->j * size) <= 0) ? s->i++ : s->j++;
         memcpy(s->dst + s->k * size, s->src + from * size, size);
         n = 1;
      }
      else
      {
         /* One run is spent: the rest of the other goes as a block. */
         size_t *from = (s->i < s->mid) ? &s->i : &s->j;
         size_t end   = (s->i < s->mid) ? s->mid : s->hi;
         n            = (end - *from < left) ? end - *from : left;
         memcpy(s->dst + s->k * size, s->src + *from * size, n * size);
         *from       += n;
      }
      s->k     += n;
      s->moved += n;
      left     -= n;
   }
}

bool gfx_thumbnail_match_index_finalize_step(gfx_thumbnail_match_index_t *idx)
{
   if (!idx || idx->finalized)
      return false;
   if (idx->parse)
   {
      free(idx->parse);
      idx->parse = NULL;
      if (     idx->count
            && (  !(idx->scratch  = malloc(idx->count * sizeof(*idx->recs)))
               || !(idx->by_loose = (const struct gtm_rec**)malloc(
                     idx->count * sizeof(*idx->by_loose)))))
         idx->count = 0;
      if (!idx->count)
      {
         free(idx->scratch);
         idx->scratch   = NULL;
         idx->finalized = true;
         return false;
      }
      gfx_thumbnail_match_sort_begin(&idx->sort, idx->recs, idx->scratch,
            idx->count, sizeof(*idx->recs), gtm_cmp_title);
   }
   if (gfx_thumbnail_match_sort_step(&idx->sort))
      return true;
   /* The titles are sorted; the loose order points at them. */
   if (idx->filled < idx->count)
   {
      size_t end = (idx->count - idx->filled > GFX_THUMBNAIL_MATCH_SORT_STEP)
         ? idx->filled + GFX_THUMBNAIL_MATCH_SORT_STEP : idx->count;
      for (; idx->filled < end; idx->filled++)
         idx->by_loose[idx->filled] = &idx->recs[idx->filled];
      if (idx->filled == idx->count)
         gfx_thumbnail_match_sort_begin(&idx->sort, idx->by_loose,
               idx->scratch, idx->count, sizeof(*idx->by_loose),
               gtm_cmp_loose);
      return true;
   }
   free(idx->scratch);
   idx->scratch   = NULL;
   idx->finalized = true;
   return false;
}

void gfx_thumbnail_match_index_finalize(gfx_thumbnail_match_index_t *idx)
{
   while (gfx_thumbnail_match_index_finalize_step(idx));
}

size_t gfx_thumbnail_match_index_count(const gfx_thumbnail_match_index_t *idx)
{
   return idx ? idx->count : 0;
}

void gfx_thumbnail_match_index_free(gfx_thumbnail_match_index_t *idx)
{
   struct gtm_pool *b;
   if (!idx)
      return;
   b = idx->pool;
   while (b)
   {
      struct gtm_pool *next = b->next;
      free(b);
      b = next;
   }
   free(idx->by_loose);
   free(idx->recs);
   free(idx->parse);
   free(idx->scratch);
   free(idx);
}

static size_t gtm_first_title(const gfx_thumbnail_match_index_t *idx,
      const char *t, size_t n)
{
   size_t lo = 0;
   size_t hi = idx->count;
   while (lo < hi)
   {
      size_t mid = lo + (hi - lo) / 2;
      if (gtm_cmp_span(idx->recs[mid].key, idx->recs[mid].tlen, t, n) < 0)
         lo = mid + 1;
      else
         hi = mid;
   }
   return lo;
}

static size_t gtm_first_loose(const gfx_thumbnail_match_index_t *idx,
      const char *t, size_t n)
{
   size_t lo = 0;
   size_t hi = idx->count;
   while (lo < hi)
   {
      size_t mid = lo + (hi - lo) / 2;
      const struct gtm_rec *r = idx->by_loose[mid];
      if (gtm_cmp_span(r->loose, r->ltlen, t, n) < 0)
         lo = mid + 1;
      else
         hi = mid;
   }
   return lo;
}

/* T0 (the scrubbed query is a stem) and T2 (same T2 key, non-empty title). */
static const struct gtm_rec *gtm_find_exact(
      const gfx_thumbnail_match_index_t *idx, const struct gtm_parse *q,
      size_t len, const struct gtm_rec *skip, unsigned *tier)
{
   const struct gtm_rec *key_hit  = NULL;
   const struct gtm_rec *case_hit = NULL;
   size_t i;
   for (i = gtm_first_title(idx, q->key, q->tlen); i < idx->count; i++)
   {
      const struct gtm_rec *r = &idx->recs[i];
      if (gtm_cmp_span(r->key, r->tlen, q->key, q->tlen))
         break;
      if (r == skip || strcmp(r->key, q->key))
         continue;
      if (r->stem_len == len && !memcmp(r->name, q->stem, len))
      {
         *tier = GTM_T0;
         return r;
      }
      if (!key_hit)
         key_hit = r;
      if (     !case_hit && r->stem_len == len
            && gtm_equal_nocase(r->name, q->stem, len))
         case_hit = r;
   }
   *tier = GTM_T2;
   if (!q->tlen)
      return NULL;
   return case_hit ? case_hit : key_hit;
}

/* T2b: same loose key; refused when the hits are different T2 titles. */
static enum gfx_thumbnail_match_status gtm_find_loose(
      const gfx_thumbnail_match_index_t *idx, const struct gtm_parse *q,
      const struct gtm_rec *skip, const struct gtm_rec **hit)
{
   size_t i;
   *hit = NULL;
   if (!idx->by_loose || !q->ltlen)
      return GFX_THUMBNAIL_MATCH_NONE;
   for (i = gtm_first_loose(idx, q->loose, q->ltlen); i < idx->count; i++)
   {
      const struct gtm_rec *r = idx->by_loose[i];
      if (gtm_cmp_span(r->loose, r->ltlen, q->loose, q->ltlen))
         break;
      if (r == skip || strcmp(r->loose, q->loose))
         continue;
      if (!*hit)
         *hit = r;
      else if (gtm_cmp_span(r->key, r->tlen, (*hit)->key, (*hit)->tlen))
      {
         *hit = NULL;
         return GFX_THUMBNAIL_MATCH_REFUSED;
      }
   }
   return *hit ? GFX_THUMBNAIL_MATCH_FOUND : GFX_THUMBNAIL_MATCH_NONE;
}

static unsigned gtm_region_rel(uint32_t c, uint32_t q)
{
   if (c == q)
      return 0;
   if ((c & q) == q)
      return 1;
   return (c & q) ? 2 : 3;
}

/* World (or no region), USA, Europe, Japan, the rest. */
static unsigned gtm_region_order(uint32_t c)
{
   if (!c || (c & GTM_WORLD))
      return 0;
   if (c & GTM_USA)
      return 1;
   if (c & GTM_EUROPE)
      return 2;
   return (c & GTM_JAPAN) ? 3 : 4;
}

static unsigned gtm_rev_rank(unsigned c, unsigned q)
{
   if (c == q)
      return 0;
   if (!c)
      return 1;
   return 2 + ((c > q) ? c - q : q - c);
}

/* Both have a region family and share none. */
static unsigned gtm_crosses(unsigned c, unsigned q)
{
   c &= GTM_WEST | GTM_EAST;
   q &= GTM_WEST | GTM_EAST;
   return (unsigned)(c && q && !(c & q));
}

/* First difference wins: region family, store tags, region, revision,
 * disc, extra tags, length, bytes. */
static bool gtm_better(const struct gtm_rec *a, const struct gtm_rec *b,
      const struct gtm_parse *q)
{
   unsigned qs = q->flags & GTM_STORE;
   unsigned x  = gtm_crosses(a->flags, q->flags);
   unsigned y  = gtm_crosses(b->flags, q->flags);
   if (x != y)
      return x < y;
   x = (unsigned)((a->flags & GTM_STORE) != qs);
   y = (unsigned)((b->flags & GTM_STORE) != qs);
   if (x != y)
      return x < y;
   x = gtm_region_rel(a->region, q->region);
   y = gtm_region_rel(b->region, q->region);
   if (x != y)
      return x < y;
   x = gtm_region_order(a->region);
   y = gtm_region_order(b->region);
   if (x != y)
      return x < y;
   x = gtm_rev_rank(a->rev, q->rev);
   y = gtm_rev_rank(b->rev, q->rev);
   if (x != y)
      return x < y;
   x = (unsigned)(a->disc != q->disc);
   y = (unsigned)(b->disc != q->disc);
   if (x != y)
      return x < y;
   if (a->ntags != b->ntags)
      return a->ntags < b->ntags;
   if (a->stem_len != b->stem_len)
      return a->stem_len < b->stem_len;
   return gtm_cmp_name(a, b) < 0;
}

/* Equal identity tags; the same disc, or a candidate without one.
 * A query without a disc (an m3u label) also takes disc 1. */
static bool gtm_same_game(const struct gtm_rec *r, const struct gtm_parse *q)
{
   return !(r->flags & GTM_NOTAGS)
      && (r->disc == q->disc || !r->disc || (!q->disc && r->disc == 1))
      && !strcmp(r->ident, q->ident);
}

/* T3: same title key, non-empty. */
static const struct gtm_rec *gtm_find_tagged(
      const gfx_thumbnail_match_index_t *idx, const struct gtm_parse *q,
      const struct gtm_rec *skip)
{
   const struct gtm_rec *best = NULL;
   size_t i;
   if (!q->tlen)
      return NULL;
   for (i = gtm_first_title(idx, q->key, q->tlen); i < idx->count; i++)
   {
      const struct gtm_rec *r = &idx->recs[i];
      if (gtm_cmp_span(r->key, r->tlen, q->key, q->tlen))
         break;
      if (r != skip && gtm_same_game(r, q) && (!best || gtm_better(r, best, q)))
         best = r;
   }
   return best;
}

/* T3b: same loose title; refused when the hits are different T2 titles. */
static enum gfx_thumbnail_match_status gtm_find_tagged_loose(
      const gfx_thumbnail_match_index_t *idx, const struct gtm_parse *q,
      const struct gtm_rec *skip, const struct gtm_rec **hit)
{
   const struct gtm_rec *first = NULL;
   size_t i;
   *hit = NULL;
   if (!idx->by_loose || !q->ltlen)
      return GFX_THUMBNAIL_MATCH_NONE;
   for (i = gtm_first_loose(idx, q->loose, q->ltlen); i < idx->count; i++)
   {
      const struct gtm_rec *r = idx->by_loose[i];
      if (gtm_cmp_span(r->loose, r->ltlen, q->loose, q->ltlen))
         break;
      if (r == skip || !gtm_same_game(r, q))
         continue;
      if (!first)
         first = r;
      else if (gtm_cmp_span(r->key, r->tlen, first->key, first->tlen))
      {
         *hit = NULL;
         return GFX_THUMBNAIL_MATCH_REFUSED;
      }
      if (!*hit || gtm_better(r, *hit, q))
         *hit = r;
   }
   return *hit ? GFX_THUMBNAIL_MATCH_FOUND : GFX_THUMBNAIL_MATCH_NONE;
}

/* skip: a record to leave out (the measurement's held-out name). */
static enum gfx_thumbnail_match_status gtm_find(
      const gfx_thumbnail_match_index_t *idx, const char *query,
      const struct gtm_rec **hit, unsigned *tier, const struct gtm_rec *skip)
{
   struct gtm_parse *q;
   enum gfx_thumbnail_match_status st = GFX_THUMBNAIL_MATCH_NONE;
   unsigned t                         = GTM_T0;
   size_t len;
   *hit = NULL;
   if (!idx || !idx->finalized || !idx->count || !query)
      return st;
   len = strlen(query);
   if (!len || len >= GTM_NAME_MAX)
      return st;
   if (!(q = (struct gtm_parse*)malloc(sizeof(*q))))
      return st;
   gtm_scrub(q->stem, query, len);
   gtm_parse_stem(q, q->stem, len);
   if (!(*hit = gtm_find_exact(idx, q, len, skip, &t)))
   {
      t  = GTM_T2B;
      st = gtm_find_loose(idx, q, skip, hit);
   }
   if (     !*hit && st != GFX_THUMBNAIL_MATCH_REFUSED
         && !(q->flags & GTM_NOTAGS))
   {
      t = GTM_T3;
      if (!(*hit = gtm_find_tagged(idx, q, skip)))
      {
         t  = GTM_T3B;
         st = gtm_find_tagged_loose(idx, q, skip, hit);
      }
   }
   if (*hit)
      st = GFX_THUMBNAIL_MATCH_FOUND;
   if (tier)
      *tier = t;
   free(q);
   return st;
}

enum gfx_thumbnail_match_status gfx_thumbnail_match_find(
      const gfx_thumbnail_match_index_t *idx, const char *query,
      const char **out)
{
   const struct gtm_rec *hit;
   enum gfx_thumbnail_match_status st = gtm_find(idx, query, &hit, NULL, NULL);
   if (out)
      *out = hit ? hit->name : NULL;
   return st;
}

enum gfx_thumbnail_match_status gfx_thumbnail_match_find_first(
      const gfx_thumbnail_match_index_t *idx, const char * const *queries,
      size_t num_queries, const char **out)
{
   enum gfx_thumbnail_match_status st = GFX_THUMBNAIL_MATCH_NONE;
   size_t i;
   if (out)
      *out = NULL;
   for (i = 0; queries && i < num_queries; i++)
   {
      if (!queries[i])
         continue;
      if ((st = gfx_thumbnail_match_find(idx, queries[i], out))
            == GFX_THUMBNAIL_MATCH_FOUND)
         break;
   }
   return st;
}
