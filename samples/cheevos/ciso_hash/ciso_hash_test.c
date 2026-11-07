/* ciso_hash_test: rc_hash reads a CISO file through
 * cheevos/cheevos_ciso.c as the disc image it stands for, so a
 * GameCube disc hashes the same as a CISO and as an ISO.
 *
 * The disc made up here has its headers, apploader and main.dol
 * where rc_hash looks for them, segments that cross block edges, and
 * runs of blocks left out of the CISO between them. A Wii header and
 * a file that is not CISO are told apart by the console check.
 *
 * Usage: ciso_hash_test [dir], the files written there (default .). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/file_path.h>
#include <streams/file_stream.h>

#include "../../../deps/rcheevos/include/rc_consoles.h"
#include "../../../deps/rcheevos/include/rc_hash.h"
#include "../../../cheevos/cheevos_ciso.h"

#define BLOCK   0x8000
#define BLOCKS  16
#define HEAD    0x8000

static int failures = 0;
static uint32_t rng_state = 0x9E3779B9u;

static uint32_t rnd(void)
{
   rng_state ^= rng_state << 13;
   rng_state ^= rng_state >> 17;
   rng_state ^= rng_state << 5;
   return rng_state;
}

static void check(int ok, const char *what)
{
   printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
   if (!ok)
      failures++;
}

static void put32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)v;
}

static int write_file(const char *path, const uint8_t *d, size_t n)
{
   FILE *f = fopen(path, "wb");
   int   ok;
   if (!f)
      return 0;
   ok = fwrite(d, 1, n, f) == n;
   return fclose(f) == 0 && ok;
}

/* The disc as an ISO and as a CISO of the blocks in @used */
static int write_disc(const char *iso, const char *ciso,
      const uint8_t *used, const uint8_t *id, const uint8_t *magic,
      unsigned magic_at)
{
   uint8_t *disc = (uint8_t*)calloc(1, (size_t)BLOCKS * BLOCK);
   uint8_t *file = (uint8_t*)calloc(1, HEAD + (size_t)BLOCKS * BLOCK);
   size_t   at   = HEAD;
   size_t   i;
   int      ok;

   if (!disc || !file)
   {
      free(disc);
      free(file);
      return 0;
   }
   for (i = 0; i < (size_t)BLOCKS * BLOCK; i++)
      if (used[i / BLOCK])
         disc[i] = (uint8_t)rnd();

   memcpy(disc, id, 6);
   memcpy(disc + magic_at, magic, 4);
   /* apploader: 0x1000 octets of body, no trailer */
   put32(disc + 0x2440 + 0x14, 0x1000);
   put32(disc + 0x2440 + 0x18, 0);
   /* main.dol at block 8: a code segment over blocks 9 and 10, a data
    * segment from block 14 into 15 */
   put32(disc + 0x420, 0x40000);
   memset(disc + 0x40000, 0, 0xD8);
   put32(disc + 0x40000 + 0x00, 0x48000);
   put32(disc + 0x40000 + 0x90, 0x9000);
   put32(disc + 0x40000 + 7 * 4, 0x70000);
   put32(disc + 0x40000 + 0x90 + 7 * 4, 0x8100);

   memcpy(file, "CISO", 4);
   file[5] = (uint8_t)(BLOCK >> 8);
   for (i = 0; i < BLOCKS; i++)
   {
      file[8 + i] = used[i];
      if (!used[i])
         continue;
      memcpy(file + at, disc + i * BLOCK, BLOCK);
      at += BLOCK;
   }

   ok =     write_file(iso, disc, (size_t)BLOCKS * BLOCK)
         && write_file(ciso, file, at);
   free(disc);
   free(file);
   return ok;
}

int main(int argc, char *argv[])
{
   static const uint8_t used[BLOCKS] =
      { 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1 };
   static const uint8_t gc_magic[4]  = { 0xC2, 0x33, 0x9F, 0x3D };
   static const uint8_t wii_magic[4] = { 0x5D, 0x1C, 0x9E, 0xA3 };
   const char *dir = argc > 1 ? argv[1] : ".";
   char iso[1024], ciso[1024], wii_iso[1024], wii_ciso[1024];
   char h_iso[33], h_ciso[33], h_raw[33];
   int  ok_iso, ok_ciso, ok_raw;

   fill_pathname_join_special(iso,      dir, "ciso_hash_gc.iso",   sizeof(iso));
   fill_pathname_join_special(ciso,     dir, "ciso_hash_gc.ciso",  sizeof(ciso));
   fill_pathname_join_special(wii_iso,  dir, "ciso_hash_wii.iso",  sizeof(wii_iso));
   fill_pathname_join_special(wii_ciso, dir, "ciso_hash_wii.ciso", sizeof(wii_ciso));

   if (     !write_disc(iso, ciso, used, (const uint8_t*)"GALE01",
               gc_magic, 0x1c)
         || !write_disc(wii_iso, wii_ciso, used, (const uint8_t*)"RMCE01",
               wii_magic, 0x18))
   {
      printf("FAIL  could not write the images in %s\n", dir);
      return 1;
   }

   check(rcheevos_ciso_get_console_id(ciso) == RC_CONSOLE_GAMECUBE,
         "GameCube CISO identified");
   check(rcheevos_ciso_get_console_id(wii_ciso) == RC_CONSOLE_WII,
         "Wii CISO identified");
   check(rcheevos_ciso_get_console_id(iso) == RC_CONSOLE_UNKNOWN,
         "an ISO is not taken as CISO");

   memset(h_iso, 0, sizeof(h_iso));
   memset(h_ciso, 0, sizeof(h_ciso));
   memset(h_raw, 0, sizeof(h_raw));

   rc_hash_init_custom_filereader(NULL);
   ok_iso = rc_hash_generate_from_file(h_iso, RC_CONSOLE_GAMECUBE, iso);
   ok_raw = rc_hash_generate_from_file(h_raw, RC_CONSOLE_GAMECUBE, ciso);
   rcheevos_ciso_filereader_init();
   ok_ciso = rc_hash_generate_from_file(h_ciso, RC_CONSOLE_GAMECUBE, ciso);
   rc_hash_init_custom_filereader(NULL);

   printf("      iso  %s\n      ciso %s\n", h_iso, h_ciso);
   check(ok_iso, "ISO hashes");
   check(!ok_raw, "CISO read as it is stored does not hash");
   check(ok_ciso && !strcmp(h_iso, h_ciso),
         "CISO hashes as its disc image");

   filestream_delete(iso);
   filestream_delete(ciso);
   filestream_delete(wii_iso);
   filestream_delete(wii_ciso);

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("ciso_hash_test: all passed\n");
   return 0;
}
