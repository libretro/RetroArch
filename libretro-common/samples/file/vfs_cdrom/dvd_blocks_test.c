/* Exercise the real optical commands and VFS with a simulated drive.
 * Only the OS boundary is replaced; no physical disc is required. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <compat/fopen_utf8.h>
#include <vfs/vfs_implementation.h>
#if defined(_WIN32)
#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#else
#include <sys/ioctl.h>
#include <scsi/sg.h>
#endif

static unsigned calls, opens, failures;
#if defined(_WIN32)
static unsigned closes;
#endif
static uint32_t last_lba = 3000000, sector_size = 2048;
static int fail_at, short_transfer;
static int toc_bad;
static uint16_t media_profile = 0x10;
static void *direct_buffer;
static int saw_direct;
#define CHECK(x) do { if (!(x)) { \
   fprintf(stderr, "line %d: %s\n", __LINE__, #x); failures++; } } while (0)

static int drive_command(unsigned char *cmd, void *buf, size_t len)
{
   unsigned char *dst = (unsigned char*)buf;
   uint32_t lba, count;
   size_t i;
   calls++;
   if (cmd[0] == 0x25 || cmd[0] == 0x28 || cmd[0] == 0x46)
      CHECK(cmd[1] == 0 && cmd[6] == 0 && cmd[9] == 0);
   if (cmd[0] != 0xB9)
      CHECK(((uintptr_t)buf & 4095) == 0);
   if (fail_at && (int)calls == fail_at)
      return 1;
   if (cmd[0] == 0x46)
   {
      CHECK(len == 8);
      memset(dst, 0, len);
      dst[3] = 4;
      dst[6] = (unsigned char)(media_profile >> 8);
      dst[7] = (unsigned char)media_profile;
      return 0;
   }
   if (cmd[0] == 0xBB)
      return 0;
   if (cmd[0] == 0x43)
   {
      CHECK(len == 2352);
      memset(dst, 0, len);
      if (toc_bad == 1)
      {
         dst[0] = 0xff;
         dst[1] = 0xff;
      }
      else
      {
         dst[1] = 24;
         dst[5] = 0x10;
         dst[7] = 0xa1;
         dst[12] = 1;
         dst[16] = 0x14;
         dst[18] = 1;
         dst[24] = 2;
         if (toc_bad == 2)
         {
            dst[1] = 35;
            dst[27] = 0x14;
            dst[29] = 1;
            dst[35] = 2;
         }
      }
      return 0;
   }
   if (cmd[0] == 0x52)
   {
      CHECK(len == 384);
      memset(dst, 0, len);
      dst[1] = 34;
      dst[6] = 1;
      dst[26] = 3;
      dst[27] = 0xe8; /* 1000 sectors, device LBA zero */
      return 0;
   }
   if (cmd[0] == 0xB9)
   {
      CHECK(len == 2352);
      memset(dst, cmd[5], len);
      return 0;
   }
   if (cmd[0] == 0x55)
   {
      CHECK(len == 8 && dst[0] == 0x5a && dst[7] == 0x7f);
      return 0;
   }
   if (cmd[0] == 0x25)
   {
      CHECK(len == 8);
      dst[0] = (unsigned char)(last_lba >> 24);
      dst[1] = (unsigned char)(last_lba >> 16);
      dst[2] = (unsigned char)(last_lba >> 8);
      dst[3] = (unsigned char)last_lba;
      dst[4] = (unsigned char)(sector_size >> 24);
      dst[5] = (unsigned char)(sector_size >> 16);
      dst[6] = (unsigned char)(sector_size >> 8);
      dst[7] = (unsigned char)sector_size;
      return 0;
   }
   CHECK(cmd[0] == 0x28);
   lba = ((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16)
       | ((uint32_t)cmd[4] << 8) | cmd[5];
   count = ((uint32_t)cmd[7] << 8) | cmd[8];
   CHECK(count > 0 && count <= 32 && len == (size_t)count * 2048);
   if (buf == direct_buffer)
      saw_direct = 1;
   for (i = 0; i < len; i++)
      dst[i] = (unsigned char)((uint64_t)lba * 2048 + i);
   return 0;
}

#if defined(_WIN32)
static BOOL WINAPI drive_ioctl(HANDLE h, DWORD code, LPVOID in,
      DWORD insize, LPVOID out, DWORD outsize, LPDWORD bytes,
      LPOVERLAPPED overlapped)
{
   SCSI_PASS_THROUGH_DIRECT *s = (SCSI_PASS_THROUGH_DIRECT*)in;
   int rv;
   (void)h; (void)insize; (void)out; (void)outsize; (void)overlapped;
   CHECK(code == IOCTL_SCSI_PASS_THROUGH_DIRECT && (s->CdbLength == 10 || s->CdbLength == 12));
   CHECK(s->DataIn == SCSI_IOCTL_DATA_IN
         || s->DataIn == SCSI_IOCTL_DATA_OUT || s->Cdb[0] == 0xBB);
   rv = drive_command(s->Cdb, s->DataBuffer, s->DataTransferLength);
   s->ScsiStatus = rv ? 2 : 0;
   if (rv)
      ((unsigned char*)in)[s->SenseInfoOffset + 2] = 5;
   if (short_transfer)
      s->DataTransferLength--;
   *bytes = insize;
   return TRUE;
}
static HANDLE WINAPI drive_open(LPCSTR path, DWORD access, DWORD share,
      LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE templ)
{
   (void)access; (void)share; (void)security; (void)creation;
   (void)flags; (void)templ;
   CHECK(strcmp(path, "\\\\.\\D:") == 0);
   opens++;
   return (HANDLE)(uintptr_t)1;
}
static BOOL WINAPI drive_close(HANDLE h)
{
   (void)h;
   closes++;
   return TRUE;
}
#define DeviceIoControl drive_ioctl
#undef CreateFile
#define CreateFile drive_open
#define CloseHandle drive_close
#define IMAGE_PATH "D:/drive.iso"
#else
#include <stdarg.h>
static int drive_ioctl(int fd, unsigned long request, ...)
{
   va_list ap;
   sg_io_hdr_t *s;
   int rv;
   (void)fd;
   va_start(ap, request);
   s = va_arg(ap, sg_io_hdr_t*);
   va_end(ap);
   CHECK(request == SG_IO && (s->cmd_len == 10 || s->cmd_len == 12));
   CHECK(s->dxfer_direction == SG_DXFER_FROM_DEV
         || s->dxfer_direction == SG_DXFER_TO_DEV || s->cmdp[0] == 0xBB);
   rv = drive_command(s->cmdp, s->dxferp, s->dxfer_len);
   s->info = rv ? SG_INFO_CHECK : 0;
   if (rv) s->sbp[2] = 5;
   s->resid = short_transfer ? 1 : 0;
   return 0;
}
static FILE *drive_open(const char *path, const char *mode)
{
   CHECK(strcmp(path, "/dev/sg1") == 0);
   (void)mode;
   opens++;
   return tmpfile();
}
#define ioctl drive_ioctl
#undef fopen_utf8
#define fopen_utf8 drive_open
#define IMAGE_PATH "drive1.iso"
#endif

#include "../../../cdrom/cdrom.c"
#include "../../../vfs/vfs_implementation_cdrom.c"

int main(void)
{
   libretro_vfs_implementation_file stream;
   static vfs_cdrom_t disc;
   uint64_t blocks;
   uint32_t size;
   uint16_t profile;
   cdrom_toc_t *toc;
   char *cue = NULL;
   size_t cue_len = 0;
   unsigned char tracks = 0;
   unsigned char *buf;
   unsigned before;
   size_t i;
   libretro_vfs_implementation_file *opened;
   memset(&stream, 0, sizeof(stream));
   memset(&disc, 0, sizeof(disc));
   stream.cdrom = &disc;
   stream.orig_path = IMAGE_PATH;
   retro_vfs_file_open_cdrom(&stream, IMAGE_PATH,
         RETRO_VFS_FILE_ACCESS_READ, 0);
   CHECK(opens == 1 && disc.logical_blocks);
   CHECK(disc.disc_bytes == UINT64_C(6144002048));
   CHECK(cdrom_get_capacity(&stream, &blocks, &size) == 0);
   CHECK(blocks == 3000001 && size == 2048);
   CHECK(cdrom_get_media_profile(&stream, &profile) == 0 && profile == 0x10);
   short_transfer = 1;
   CHECK(cdrom_get_media_profile(&stream, &profile) == 1 && profile == 0);
   short_transfer = 0;
   last_lba = UINT32_MAX;
   CHECK(cdrom_get_capacity(&stream, &blocks, &size) == 1);
   CHECK(blocks == 0 && size == 0);
   last_lba = 3000000;
   short_transfer = 1;
   CHECK(cdrom_get_capacity(&stream, &blocks, &size) == 1);
   CHECK(blocks == 0 && size == 0);
   short_transfer = 0;
   buf = (unsigned char*)memalign_alloc(4096, 70 * 2048 + 16);
   CHECK(buf != NULL);
   if (!buf) return 1;
   direct_buffer = buf;
   before = calls;
   CHECK(cdrom_read_blocks(&stream, 1234567, 70, buf) == 0);
   CHECK(calls - before == 3 && saw_direct);
   for (i = 0; i < 70 * 2048; i++)
      CHECK(buf[i] == (unsigned char)(UINT64_C(1234567) * 2048 + i));
   CHECK(cdrom_read_blocks(&stream, UINT32_MAX, 1, buf + 1) == 0);
   before = calls;
   CHECK(cdrom_read_blocks(&stream, UINT32_MAX, 2, buf) == 1);
   CHECK(cdrom_read_blocks(&stream, 0, 1, NULL) == 1);
   CHECK(cdrom_read_blocks(&stream, 0, 0, NULL) == 0);
   CHECK(cdrom_read(&stream, NULL, 0, 2, 0, buf, SIZE_MAX, 1) == 1);
   CHECK(cdrom_read(&stream, NULL, 0, 60, 0, buf, 2352, 0) == 1);
#if SIZE_MAX == UINT32_MAX
   CHECK(cdrom_read_blocks(&stream, 0, UINT32_MAX, buf) == 1);
#endif
   CHECK(calls == before);
   CHECK(retro_vfs_file_seek_cdrom(&stream, 4095, SEEK_SET) == 0);
   CHECK(retro_vfs_file_read_cdrom(&stream, buf + 1, 10000) == 10000);
   for (i = 0; i < 10000; i++)
      CHECK(buf[i + 1] == (unsigned char)(4095 + i));
   CHECK(retro_vfs_file_tell_cdrom(&stream) == 14095);
   CHECK(retro_vfs_file_seek_cdrom(&stream, -1, SEEK_SET) == -1);
   CHECK(retro_vfs_file_seek_cdrom(&stream, INT64_MAX, SEEK_CUR) == -1);
   CHECK(retro_vfs_file_seek_cdrom(&stream, 0, 99) == -1);
   CHECK(retro_vfs_file_tell_cdrom(&stream) == 14095);
   CHECK(retro_vfs_file_seek_cdrom(&stream, -13, SEEK_END) == 0);
   CHECK(retro_vfs_file_read_cdrom(&stream, buf, UINT64_MAX) == 13);
   CHECK(retro_vfs_file_read_cdrom(&stream, buf, 1) == 0);
   CHECK(retro_vfs_file_seek_cdrom(&stream, 0, SEEK_SET) == 0);
   fail_at = (int)calls + 2;
   CHECK(retro_vfs_file_read_cdrom(&stream, buf, 40 * 2048) == 32 * 2048);
   CHECK(retro_vfs_file_tell_cdrom(&stream) == 32 * 2048);
   CHECK(retro_vfs_file_error_cdrom(&stream) == 1);
   fail_at = (int)calls + 1;
   CHECK(retro_vfs_file_read_cdrom(&stream, buf, 2048) == -1);
   CHECK(retro_vfs_file_tell_cdrom(&stream) == 32 * 2048);
   fail_at = 0;
   disc.last_frame_valid = false;
   memset(buf, 0xa5, 2352 * 2);
   fail_at = (int)calls + 1;
   before = calls;
   CHECK(cdrom_read(&stream, NULL, 0, 2, 0, buf, 2352 * 2, 0) == 1);
   CHECK(calls == before + 1 && buf[0] == 0xa5 && buf[2352] == 0xa5);
   fail_at = 0;
   disc.last_frame_valid = false;
   before = calls;
   CHECK(cdrom_read(&stream, NULL, 0, 2, 0, buf, 2352, 0) == 0);
   CHECK(cdrom_read(&stream, NULL, 0, 2, 0, buf, 2352, 0) == 0);
   CHECK(cdrom_read(&stream, NULL, 0, 2, 0, buf, 2352, 0) == 0);
   CHECK(calls == before + 1);
   {
      unsigned char cdb[10] = {0x55, 0, 0, 0, 0, 0, 0, 0, 8, 0};
      unsigned char payload[8] = {0x5a, 0, 0, 0, 0, 0, 0, 0x7f};
      CHECK(cdrom_send_command(&stream, DIRECTION_OUT, payload, 8,
               cdb, sizeof(cdb), 0) == 0);
      CHECK(payload[0] == 0x5a && payload[7] == 0x7f);
   }
   toc = (cdrom_toc_t*)calloc(1, sizeof(*toc));
   CHECK(toc != NULL);
   if (toc)
   {
      toc_bad = 1;
      CHECK(cdrom_write_cue(&stream, &cue, &cue_len, '1', &tracks, toc) == 1);
      CHECK(!cue && !cue_len && !tracks);
      toc_bad = 2;
      CHECK(cdrom_write_cue(&stream, &cue, &cue_len, '1', &tracks, toc) == 1);
      CHECK(!cue && !cue_len && !tracks);
      toc_bad = 0;
      short_transfer = 1;
      CHECK(cdrom_write_cue(&stream, &cue, &cue_len, '1', &tracks, toc) == 0);
      short_transfer = 0;
      CHECK(tracks == 1 && toc->track[0].lba_start == 150);
      CHECK(toc->track[0].track_bytes == 2352000);
      disc.logical_blocks = false;
      disc.cur_track = 1;
      stream.orig_path = "drive1-track01.bin";
      vfs_cdrom_toc = *toc;
      CHECK(retro_vfs_file_seek_cdrom(&stream, -2352, SEEK_END) == 0);
      CHECK(retro_vfs_file_tell_cdrom(&stream) == 2349648);
      CHECK(disc.cur_lba == 1149);
      CHECK(retro_vfs_file_seek_cdrom(&stream, -2352001, SEEK_END) == -1);
      CHECK(retro_vfs_file_tell_cdrom(&stream) == 2349648);
      fail_at = (int)calls + 1;
      CHECK(retro_vfs_file_read_cdrom(&stream, buf, 2352) == -1);
      CHECK(retro_vfs_file_tell_cdrom(&stream) == 2349648);
      CHECK(retro_vfs_file_error_cdrom(&stream) == 1);
      fail_at = 0;
      vfs_cdrom_toc.track[0].audio = true;
      vfs_cdrom_toc.track[0].lba = 200;
      CHECK(retro_vfs_file_seek_cdrom(&stream, 0, SEEK_SET) == 0);
      CHECK(disc.cur_lba == 150);
      stream.orig_path = IMAGE_PATH;
      free(cue);
      free(toc);
   }
   CHECK(retro_vfs_file_close_cdrom(&stream) == 0);
   memset(&stream, 0, sizeof(stream));
   memset(&disc, 0, sizeof(disc));
   stream.cdrom = &disc;
   stream.orig_path = IMAGE_PATH;
   before = opens;
   retro_vfs_file_open_cdrom(&stream, IMAGE_PATH,
         RETRO_VFS_FILE_ACCESS_WRITE, 0);
   retro_vfs_file_open_cdrom(&stream, "nonsense.iso",
         RETRO_VFS_FILE_ACCESS_READ, 0);
   CHECK(opens == before);
   sector_size = 2352;
   retro_vfs_file_open_cdrom(&stream, IMAGE_PATH,
         RETRO_VFS_FILE_ACCESS_READ, 0);
   CHECK(!disc.logical_blocks && !stream.fp);
#if defined(_WIN32)
   CHECK(!stream.fh && closes == 2);
#endif
   sector_size = 2048;
   opened = retro_vfs_file_open_impl("cdrom://" IMAGE_PATH,
         RETRO_VFS_FILE_ACCESS_READ, 0);
   CHECK(opened != NULL);
   if (opened)
   {
      CHECK(retro_vfs_file_size_impl(opened) == INT64_C(6144002048));
      CHECK(retro_vfs_file_read_impl(opened, buf, 2048) == 2048);
      CHECK(retro_vfs_file_close_impl(opened) == 0);
   }
   sector_size = 2352;
   CHECK(retro_vfs_file_open_impl("cdrom://" IMAGE_PATH,
         RETRO_VFS_FILE_ACCESS_READ, 0) == NULL);
   memalign_free(buf);
   printf("DVD block/VFS tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
