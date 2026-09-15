/* Hashes a disc image through rcheevos, using the same cdfs-backed
 * reader cheevos registers, so the hash can be compared between CHD
 * readers.
 *
 * This is the last thing above chd_stream that a CHD reader can break.
 * cdfs agreeing on file contents and on sector reads makes a hash
 * difference unlikely, but rcheevos hashes specific byte ranges chosen
 * per console, and "unlikely" is what the pregap bug looked like
 * before anything compared bytes.
 *
 * Build once per backend and diff the output. Usage:
 *   rc_hash_backend_compare <console_id> <image.chd>...
 * Console ids: 12 PlayStation, 21 PlayStation 2, 40 Dreamcast.
 */
/* Hash a disc image exactly as cheevos does: register the cdfs-backed
 * reader, then ask rcheevos for the hash. Built once per CHD reader,
 * the two hashes must match. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/cdfs.h>
#include <streams/chd_stream.h>
#include <rc_hash.h>

static void* chd_open_track(const char* path, uint32_t track)
{
   cdfs_track_t* t;
   switch (track)
   {
      case RC_HASH_CDTRACK_FIRST_DATA: t = cdfs_open_data_track(path); break;
      case RC_HASH_CDTRACK_LAST:       t = cdfs_open_track(path, CHDSTREAM_TRACK_LAST); break;
      case RC_HASH_CDTRACK_LARGEST:    t = cdfs_open_track(path, CHDSTREAM_TRACK_PRIMARY); break;
      default:                         t = cdfs_open_track(path, (int32_t)track); break;
   }
   if (t)
   {
      cdfs_file_t* f = (cdfs_file_t*)calloc(1, sizeof(*f));
      if (f && cdfs_open_file(f, t, NULL)) return f;
      free(f); cdfs_close_track(t);
   }
   return NULL;
}
static size_t chd_read_sector(void* h, uint32_t sector, void* buf, size_t want)
{
   cdfs_file_t* f = (cdfs_file_t*)h;
   uint32_t n = cdfs_get_num_sectors(f);
   sector -= cdfs_get_first_sector(f);
   if (sector >= n) return 0;
   cdfs_seek_sector(f, sector);
   return cdfs_read_file(f, buf, want);
}
static void chd_close_track(void* h)
{
   cdfs_file_t* f = (cdfs_file_t*)h;
   if (f) { cdfs_close_track(f->track); cdfs_close_file(f); free(f); }
}
static uint32_t chd_first_sector(void* h)
{ return cdfs_get_first_sector((cdfs_file_t*)h); }

int main(int argc, char **argv)
{
   struct rc_hash_cdreader r;
   char hash[33];
   int i;
   memset(&r, 0, sizeof(r));
   r.open_track = chd_open_track;
   r.read_sector = chd_read_sector;
   r.close_track = chd_close_track;
   r.first_track_sector = chd_first_sector;
   rc_hash_init_custom_cdreader(&r);
   for (i = 2; i < argc; i++)
   {
      uint32_t cid = (uint32_t)atoi(argv[1]);
      hash[0] = '\0';
      if (rc_hash_generate_from_file(hash, cid, argv[i]))
         printf("  %-42s %s\n", argv[i] + 7, hash);
      else
         printf("  %-42s (no hash)\n", argv[i] + 7);
   }
   return 0;
}
int path_is_directory(const char *p){(void)p;return 0;}
int path_mkdir(const char *d){(void)d;return 0;}
