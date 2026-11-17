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

/* input/input_pad_tag.h: a controller's tag is the end of its own
 * serial or Bluetooth address, else 6 hex digits of its identity hash,
 * which is pinned to FNV-1a's published values and to the identities
 * and tags samples/menu/pad_tag_rows expects (a pad keeps its tag only
 * while they stay as they are). */

#include <stdio.h>
#include <string.h>

#include "input_pad_tag.h"

static unsigned failures;

static void check(int ok, const char *what, const char *a, const char *b)
{
   if (ok)
      return;
   fprintf(stderr, "FAIL: %s%s%s%s%s\n", what,
         a ? ": " : "", a ? a : "", b ? " " : "", b ? b : "");
   failures++;
}

static void check_hash(void)
{
   static const unsigned char foobar[] = "foobar";
   check(input_pad_tag_fnv1a(0x811c9dc5UL, foobar, 0) == 0x811c9dc5UL,
         "FNV-1a of nothing", NULL, NULL);
   check(input_pad_tag_fnv1a(0x811c9dc5UL, (const unsigned char*)"a", 1)
         == 0xe40c292cUL, "FNV-1a of \"a\"", NULL, NULL);
   check(input_pad_tag_fnv1a(0x811c9dc5UL, foobar, 6) == 0xbf9cf968UL,
         "FNV-1a of \"foobar\"", NULL, NULL);
}

/* 6 lowercase hex digits */
static int is_hash_form(const char *s)
{
   size_t i;
   if (strlen(s) != INPUT_PAD_TAG_LEN)
      return 0;
   for (i = 0; s[i]; i++)
      if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
         return 0;
   return 1;
}

/* Serial-less pads on ports a last character apart: their tags differ
 * in at least 4 of 6 digits, or in the first */
static void check_apart(const char *phys_a, const char *phys_b)
{
   size_t i;
   unsigned differ = 0;
   char a[INPUT_PAD_TAG_LEN + 1];
   char b[INPUT_PAD_TAG_LEN + 1];
   input_pad_tag_text(input_pad_tag_ident(0x045e, 0x028e, "", phys_a), "", a);
   input_pad_tag_text(input_pad_tag_ident(0x045e, 0x028e, "", phys_b), "", b);
   for (i = 0; i < INPUT_PAD_TAG_LEN; i++)
      if (a[i] != b[i])
         differ++;
   check(differ >= 4 || a[0] != b[0], "tags alike", a, b);
}

/* @uniq and @phys's identity and tag; @hash_form: whether the tag must
 * be the hash's */
static void check_ident(unsigned vid, unsigned pid, const char *uniq,
      const char *phys, unsigned long want, const char *tag, int hash_form)
{
   char s[INPUT_PAD_TAG_LEN + 1];
   uint32_t got = input_pad_tag_ident(vid, pid, uniq, phys);
   check(got == want, "identity", uniq, phys);
   input_pad_tag_text(got, uniq, s);
   check(!strcmp(s, tag), "tag", tag, s);
   if (hash_form)
      check(is_hash_form(s), "not 6 lowercase hex digits", uniq, s);
}

int main(void)
{
   char a[INPUT_PAD_TAG_LEN + 1];
   char b[INPUT_PAD_TAG_LEN + 1];

   check_hash();
   /* the port path when the pad has no serial: the hash's form */
   check_ident(0x045e, 0x028e, "", "port-1",   0x8c6c6606UL, "e6675c", 1);
   check_ident(0x045e, 0x028e, "", "port-2",   0x8b6c6473UL, "9c309a", 1);
   check_ident(0x045e, 0x028e, "", "port-11",  0x45a4e895UL, "7f8bc8", 1);
   check_ident(0x045e, 0x028e, "", "port-22",  0xbca1d253UL, "e129e5", 1);
   check_ident(0x045e, 0x028e, "", "usb-0000:00:14.0-1/input0",
         0x82d1f4b8UL, "4112c4", 1);
   /* a serial wins, whichever port: its last 6 letters and digits */
   check_ident(0x045e, 0x028e, "serial-9", "port-1", 0xa1e19035UL,
         "erial9", 0);
   check_ident(0x045e, 0x028e, "serial-9", "port-2", 0xa1e19035UL,
         "erial9", 0);
   /* a Bluetooth address: the device's own half */
   check_ident(0x054c, 0x0ce6, "a0:5a:5c:12:34:56", "f4:4e:fc:00:00:01",
         0xb57c2e0eUL, "123456", 0);
   check_ident(0x054c, 0x0ce6, "a4:c1:38:9f:2b:7e", "f4:4e:fc:00:00:01",
         0xfb10a967UL, "9f2b7e", 0);
   /* capitals, spaces and separators */
   check_ident(0x045e, 0x028e, "A4:C1:38:9F:2B:7E", "port-1",
         0x3541d9beUL, "9f2b7e", 0);
   check_ident(0x045e, 0x028e, "SN 04-7A:B3 9F2C", "port-1",
         0x15cde816UL, "b39f2c", 0);
   /* exactly 6 */
   check_ident(0x045e, 0x028e, "A1-B2-C3", "port-1", 0xa10f0981UL,
         "a1b2c3", 0);
   /* 5, all zeros, all one letter, only separators: as no serial, the
    * port decides */
   check_ident(0x045e, 0x028e, "AB-123", "port-1", 0x8c6c6606UL,
         "e6675c", 1);
   check_ident(0x045e, 0x028e, "00:00:00:00:00:00", "port-1",
         0x8c6c6606UL, "e6675c", 1);
   check_ident(0x045e, 0x028e, "00:00:00:00:00:00", "port-2",
         0x8b6c6473UL, "9c309a", 1);
   check_ident(0x045e, 0x028e, "ffffff", "port-1", 0x8c6c6606UL,
         "e6675c", 1);
   check_ident(0x045e, 0x028e, "FFFFFF", "port-2", 0x8b6c6473UL,
         "9c309a", 1);
   check_ident(0x045e, 0x028e, ":::::::::", "port-1", 0x8c6c6606UL,
         "e6675c", 1);
   /* bytes past ASCII are skipped */
   check_ident(0x045e, 0x028e, "\xc3\xa9" "AB12\xc3\xa9" "34\xc3\xa9",
         "port-1", 0x618fea00UL, "ab1234", 0);
   /* a long one: its end */
   check_ident(0x045e, 0x028e,
         "0123456789abcdefghijklmnopqrstuvwxyz"
         "0123456789abcdefghijklmnopqrstuvwxyz"
         "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ",
         "port-1", 0x4494ecd3UL, "uvwxyz", 0);
   check_apart("port-1", "port-2");
   check_apart("port-2", "port-3");
   check_apart("usb-0000:00:14.0-3", "usb-0000:00:14.0-4");
   /* two clones with one placeholder serial, on two ports */
   input_pad_tag_text(input_pad_tag_ident(0x045e, 0x028e,
         "00:00:00:00:00:00", "usb-0000:1a:00.0-12/input0"),
         "00:00:00:00:00:00", a);
   input_pad_tag_text(input_pad_tag_ident(0x045e, 0x028e,
         "00:00:00:00:00:00", "usb-0000:1c:00.0-1/input0"),
         "00:00:00:00:00:00", b);
   check(strcmp(a, b) != 0, "one placeholder serial, one tag", a, b);
   /* no identity */
   check_ident(0x045e, 0x028e, NULL, "port-1", 0, "", 0);
   check_ident(0x045e, 0x028e, "", NULL,       0, "", 0);
   check_ident(0x045e, 0x028e, "", "",         0, "", 0);

   if (failures)
   {
      fprintf(stderr, "pad_tag_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("pad_tag_test: serials' ends and hashes' tags pinned\n");
   return 0;
}
