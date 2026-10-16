/* Exercise the real optical commands and VFS with a simulated drive.
 * Only the OS boundary is replaced; no physical disc is required. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <memalign.h>
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

static unsigned calls, opens, failures, toc_reads, inquiries;
#if defined(_WIN32)
static unsigned closes;
#endif
static uint32_t last_lba = 3000000, sector_size = 2048;
static int fail_at, short_transfer;
static int close_error;
static unsigned char error_key = 5, error_asc, error_response = 0x70;
static int toc_bad;
static uint16_t media_profile = 0x10;
static unsigned active_drive = 1;
#ifndef _WIN32
static unsigned drive_map[128];
#endif
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
   if (cmd[0] == 0x43 || cmd[0] == 0x52)
      toc_reads++;
   if (cmd[0] == 0x12)
   {
      inquiries++;
      memset(dst, 0, len);
      dst[0] = 5; /* MMC device */
      memcpy(dst + 8, "LIBRETRO", 8);
      memcpy(dst + 16, "VIRTUAL DRIVE   ", 16);
      memcpy(dst + 32, "1.00", 4);
      return 0;
   }
   if (cmd[0] == 0x46)
   {
      CHECK(len == 8);
      memset(dst, 0, len);
      dst[3] = 4;
      dst[6] = (unsigned char)(media_profile >> 8);
      dst[7] = (unsigned char)media_profile;
      return 0;
   }
   if (cmd[0] == 0xBB || cmd[0] == 0x00)
      return 0;
   if (cmd[0] == 0x5A)
   {
      memset(dst, 0, len);
      return 0;
   }
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
         dst[24] = active_drive == 2 ? 3 : 2;
         dst[25] = active_drive == 2 ? 25 : 0;
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
      if (active_drive == 2)
      {
         dst[11] = 100;
         dst[26] = 1;
         dst[27] = 0xf4; /* 500 sectors at device LBA 100 */
      }
      else
      {
         dst[26] = 3;
         dst[27] = 0xe8; /* 1000 sectors at device LBA zero */
      }
      return 0;
   }
   if (cmd[0] == 0xB9)
   {
      CHECK(((uintptr_t)buf & 4095) == 0);
      if (buf == direct_buffer) saw_direct = 1;
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

static void drive_sense(unsigned char *sense)
{
   sense[0] = error_response;
   if (error_response == 0x72 || error_response == 0x73)
   {
      sense[1] = error_key;
      sense[2] = error_asc;
   }
   else
   {
      sense[2] = error_key;
      sense[7] = 10;
      sense[12] = error_asc;
   }
}

#if defined(_WIN32)
static BOOL WINAPI drive_ioctl(HANDLE h, DWORD code, LPVOID in,
      DWORD insize, LPVOID out, DWORD outsize, LPDWORD bytes,
      LPOVERLAPPED overlapped)
{
   SCSI_PASS_THROUGH_DIRECT *s = (SCSI_PASS_THROUGH_DIRECT*)in;
   int rv;
   active_drive = (unsigned)(uintptr_t)h;
   (void)insize; (void)out; (void)outsize; (void)overlapped;
   CHECK(code == IOCTL_SCSI_PASS_THROUGH_DIRECT && (s->CdbLength == 9 || s->CdbLength == 10 || s->CdbLength == 12));
   CHECK(s->DataIn == SCSI_IOCTL_DATA_IN
         || s->DataIn == SCSI_IOCTL_DATA_OUT || s->Cdb[0] == 0xBB || s->Cdb[0] == 0x00);
   rv = drive_command(s->Cdb, s->DataBuffer, s->DataTransferLength);
   s->ScsiStatus = rv ? 2 : 0;
   if (rv)
      drive_sense((unsigned char*)in + s->SenseInfoOffset);
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
   CHECK(strcmp(path, "\\\\.\\D:") == 0 || strcmp(path, "\\\\.\\E:") == 0);
   opens++;
   return (HANDLE)(uintptr_t)(path[4] == 'E' ? 2 : 1);
}
static BOOL WINAPI drive_close(HANDLE h)
{
   (void)h;
   closes++;
   return close_error ? FALSE : TRUE;
}
static DWORD WINAPI mock_logical_drives(void)
{
   return (1u << 2) | (1u << 3) | (1u << 4); /* C: fixed, D: and E: optical */
}
static UINT WINAPI mock_drive_type(LPCSTR root)
{
   return (root[0] == 'd' || root[0] == 'e') ? DRIVE_CDROM : DRIVE_FIXED;
}
#define GetLogicalDrives mock_logical_drives
#undef GetDriveType
#define GetDriveType mock_drive_type
#define DeviceIoControl drive_ioctl
#undef CreateFile
#define CreateFile drive_open
#define CloseHandle drive_close
#define IMAGE_PATH "D:/drive.iso"
#define BIN_A "cdrom://D:/drive-track01.bin"
#define BIN_B "cdrom://E:/drive-track01.bin"
#define CUE_A "cdrom://D:/drive.cue"
#define DEVICE_A "cdrom://D:/drive.device"
#define DRIVE_A 'D'
#define BAD_TRACK "cdrom://D:/drive-track00.bin"
#define OUT_TRACK "cdrom://D:/drive-track02.bin"
#else
#include <stdarg.h>
static int drive_ioctl(int fd, unsigned long request, ...)
{
   va_list ap;
   sg_io_hdr_t *s;
   int rv;
   active_drive = fd >= 0 && fd < 128 ? drive_map[fd] : 1;
   va_start(ap, request);
   s = va_arg(ap, sg_io_hdr_t*);
   va_end(ap);
   CHECK(request == SG_IO && (s->cmd_len == 9 || s->cmd_len == 10 || s->cmd_len == 12));
   CHECK(s->dxfer_direction == SG_DXFER_FROM_DEV
         || s->dxfer_direction == SG_DXFER_TO_DEV || s->cmdp[0] == 0xBB || s->cmdp[0] == 0x00);
   rv = drive_command(s->cmdp, s->dxferp, s->dxfer_len);
   s->info = rv ? SG_INFO_CHECK : 0;
   if (rv) drive_sense(s->sbp);
   s->resid = short_transfer ? 1 : 0;
   return 0;
}
static FILE *drive_open(const char *path, const char *mode)
{
   FILE *file;
   int fd;
   CHECK(strcmp(path, "/dev/sg1") == 0 || strcmp(path, "/dev/sg2") == 0);
   (void)mode;
   opens++;
   file = tmpfile();
   if (file)
   {
      fd = fileno(file);
      CHECK(fd >= 0 && fd < 128);
      if (fd >= 0 && fd < 128) drive_map[fd] = path[7] == '2' ? 2 : 1;
   }
   return file;
}
#define ioctl drive_ioctl
#undef fopen_utf8
#define fopen_utf8 drive_open
static int drive_fclose(FILE *file)
{
   int result = fclose(file);
   return close_error ? EOF : result;
}
#define fclose drive_fclose
#define IMAGE_PATH "drive1.iso"
#define BIN_A "cdrom://drive1-track01.bin"
#define BIN_B "cdrom://drive2-track01.bin"
#define CUE_A "cdrom://drive1.cue"
#define DEVICE_A "cdrom://drive1.device"
#define DRIVE_A '1'
#define BAD_TRACK "cdrom://drive1-track00.bin"
#define OUT_TRACK "cdrom://drive1-track02.bin"
#endif

static size_t staging_limit, staging_max;
static unsigned staging_allocations;
static void *tracked_alloc(size_t align, size_t bytes)
{
   staging_allocations++;
   if (bytes > staging_max) staging_max = bytes;
   if (staging_limit && bytes > staging_limit) return NULL;
   return memalign_alloc(align, bytes);
}
#define memalign_alloc tracked_alloc
#include "../../../cdrom/cdrom.c"
#include "../../../vfs/vfs_implementation_cdrom.c"
#undef memalign_alloc

static void test_bounded_staging(unsigned char *buf)
{
   libretro_vfs_implementation_file *file = retro_vfs_file_open_impl(
         BIN_A, RETRO_VFS_FILE_ACCESS_READ, 0);
   unsigned before, i;
   unsigned char command[12] = {0xb9};
   CHECK(file != NULL);
   if (!file) return;
   staging_limit = 1; staging_max = 0; staging_allocations = 0;
   before = calls;
   CHECK(cdrom_send_command(file, DIRECTION_IN, buf + 1, (size_t)INT_MAX + 2352,
            command, 12, 0) == 1);
   CHECK(calls == before && staging_max == 2352 && staging_allocations == 1);
   staging_limit = 2352; staging_max = 0; staging_allocations = 0;
   before = calls;
   CHECK(cdrom_send_command(file, DIRECTION_OUT, buf, 2352, command, 12, 0) == 1);
   CHECK(cdrom_send_command(file, DIRECTION_IN, buf, 0, command, 12, 0) == 0);
   command[0] = 0x55;
   CHECK(cdrom_send_command(file, DIRECTION_OUT, buf, SIZE_MAX, command, 12, 1) == 1);
   CHECK(calls == before && staging_allocations == 0);
   CHECK(retro_vfs_file_read_impl(file, buf, 2352) == 2352 && staging_allocations == 0);
   before = calls;
   staging_limit = 1; /* A cache hit succeeds even when staging cannot allocate. */
   CHECK(retro_vfs_file_seek_impl(file, 511, RETRO_VFS_SEEK_POSITION_START) == 0);
   CHECK(retro_vfs_file_read_impl(file, buf + 1, 100) == 100);
   CHECK(calls == before && staging_allocations == 0);
   for (i = 0; i < 100; i++) CHECK(buf[i + 1] == 0);
   staging_limit = 2352;
   CHECK(retro_vfs_file_seek_impl(file, 511, RETRO_VFS_SEEK_POSITION_START) == 0);
   CHECK(retro_vfs_file_read_impl(file, buf + 1, 4704) == 4704);
   CHECK(staging_max == 2352 && staging_allocations == 1);
   for (i = 0; i < 4704; i++)
      CHECK(buf[i + 1] == (unsigned char)((i + 511) / 2352));
   CHECK(retro_vfs_file_seek_impl(file, 0, RETRO_VFS_SEEK_POSITION_START) == 0);
   before = calls; fail_at = (int)calls + 2;
   CHECK(retro_vfs_file_read_impl(file, buf + 1, 7000) == -1);
   CHECK(calls == before + 2 && retro_vfs_file_tell_impl(file) == 0);
   CHECK(!file->cdrom->last_frame_valid);
   fail_at = 0;
   CHECK(retro_vfs_file_read_impl(file, buf + 1, 7000) == 7000);
   for (i = 0; i < 7000; i++) CHECK(buf[i + 1] == (unsigned char)(i / 2352));
   CHECK(staging_max == 2352 && staging_allocations == 3);
   before = calls;
   CHECK(retro_vfs_file_seek_impl(file, 7034, RETRO_VFS_SEEK_POSITION_START) == 0);
   CHECK(retro_vfs_file_read_impl(file, buf + 1, 32) == 32);
   CHECK(calls == before + 1 && staging_allocations == 4);
   for (i = 0; i < 32; i++) CHECK(buf[i + 1] == (i < 22 ? 2 : 3));
   staging_limit = 1; staging_allocations = 0;
   before = calls;
   CHECK(retro_vfs_file_seek_impl(file, 3 * 2352 + 511, RETRO_VFS_SEEK_POSITION_START) == 0);
   CHECK(retro_vfs_file_read_impl(file, buf + 2255, 1841 + 2352) == 1841 + 2352);
   CHECK(calls == before + 1 && staging_allocations == 0);
   for (i = 0; i < 1841 + 2352; i++)
      CHECK(buf[2255 + i] == (i < 1841 ? 3 : 4));
   before = calls;
   CHECK(retro_vfs_file_seek_impl(file, 5 * 2352, RETRO_VFS_SEEK_POSITION_START) == 0);
   CHECK(retro_vfs_file_read_impl(file, buf, 4704) == -1);
   CHECK(calls == before + 1 && staging_allocations == 1);
   CHECK(retro_vfs_file_tell_impl(file) == 5 * 2352);
   staging_limit = 0;
   CHECK(retro_vfs_file_read_impl(file, buf, 4704) == 4704);
   for (i = 0; i < 4704; i++) CHECK(buf[i] == (i < 2352 ? 5 : 6));
   staging_limit = 0;
   CHECK(retro_vfs_file_close_impl(file) == 0);
}

static void test_media_change(unsigned char *buf)
{
   unsigned format, condition, dvd, before;
   libretro_vfs_implementation_file *file;
   sector_size = 2048;
   for (format = 0; format < 4; format++)
      for (condition = 0; condition < 3; condition++)
         for (dvd = 0; dvd < 2; dvd++)
         {
            unsigned bytes = dvd ? 2048 : 2352;
            const char *path = dvd ? "cdrom://" IMAGE_PATH : BIN_A;
            file = retro_vfs_file_open_impl(path, RETRO_VFS_FILE_ACCESS_READ, 0);
            CHECK(file != NULL);
            if (!file) continue;
            CHECK(retro_vfs_file_read_impl(file, buf, bytes) == bytes);
            error_response = (unsigned char)(0x70 + format);
            error_key = condition == 2 ? 2 : 6;
            error_asc = condition == 2 ? 0x3a : (unsigned char)(0x28 + condition);
            before = calls;
            fail_at = (int)calls + 1;
            CHECK(retro_vfs_file_read_impl(file, buf, bytes) == -1);
            CHECK(calls == before + 1); /* no retry onto another medium */
            CHECK(retro_vfs_file_tell_impl(file) == bytes);
            CHECK(file->cdrom->media_changed && !file->cdrom->last_frame_valid);
            CHECK(retro_vfs_file_error_impl(file));
            fail_at = 0;
            CHECK(retro_vfs_file_seek_impl(file, 0, RETRO_VFS_SEEK_POSITION_START) == 0);
            before = calls;
            CHECK(retro_vfs_file_read_impl(file, buf, bytes) == -1);
            CHECK(calls == before && retro_vfs_file_tell_impl(file) == 0);
            CHECK(retro_vfs_file_read_impl(file, buf, 0) == 0);
            if (dvd)
               CHECK(cdrom_read_blocks(file, 0, 1, buf) != 0);
            else
               CHECK(cdrom_read(file, NULL, 0, 2, 0, buf, 2352, 0) != 0);
            CHECK(calls == before); /* low-level readers also reject stale geometry */
            CHECK(cdrom_is_media_inserted(file)); /* readiness cannot revive geometry */
            CHECK(file->cdrom->media_changed);
            CHECK(retro_vfs_file_close_impl(file) == 0);
            file = retro_vfs_file_open_impl(path, RETRO_VFS_FILE_ACCESS_READ, 0);
            CHECK(file != NULL);
            if (file)
            {
               CHECK(!file->cdrom->media_changed);
               CHECK(retro_vfs_file_read_impl(file, buf, bytes) == bytes);
               CHECK(retro_vfs_file_close_impl(file) == 0);
            }
         }
   error_key = 5; error_asc = 0; error_response = 0x70;
   file = retro_vfs_file_open_impl(BIN_A, RETRO_VFS_FILE_ACCESS_READ, 0);
   CHECK(file != NULL);
   if (file)
   {
      fail_at = (int)calls + 1;
      CHECK(retro_vfs_file_read_impl(file, buf, 2352) == -1);
      CHECK(!file->cdrom->media_changed);
      fail_at = 0;
      CHECK(retro_vfs_file_read_impl(file, buf, 2352) == 2352);
      error_key = 6; error_asc = 0x28;
      fail_at = (int)calls + 1;
      CHECK(!cdrom_is_media_inserted(file));
      CHECK(file->cdrom->media_changed && !file->cdrom->last_frame_valid);
      fail_at = 0;
      CHECK(retro_vfs_file_close_impl(file) == 0);
   }
   file = retro_vfs_file_open_impl(DEVICE_A, RETRO_VFS_FILE_ACCESS_READ, 0);
   CHECK(file != NULL);
   if (file)
   {
      fail_at = (int)calls + 1;
      CHECK(!cdrom_is_media_inserted(file));
      CHECK(!file->cdrom->media_changed);
      fail_at = 0;
      CHECK(cdrom_is_media_inserted(file));
      CHECK(retro_vfs_file_close_impl(file) == 0);
   }
   error_key = 5; error_asc = 0; error_response = 0x70;
}

static void test_handle_geometry(unsigned char *buf)
{
   libretro_vfs_implementation_file *a, *b, *probe;
   const cdrom_toc_t *a_toc, *b_toc;
   uint16_t profile;
   unsigned before;
   toc_bad = 0;
   a = retro_vfs_file_open_impl(BIN_A, RETRO_VFS_FILE_ACCESS_READ, 0);
   b = retro_vfs_file_open_impl(BIN_B, RETRO_VFS_FILE_ACCESS_READ, 0);
   CHECK(a && b);
   if (a && b)
   {
      a_toc = retro_vfs_file_get_cdrom_toc(a);
      b_toc = retro_vfs_file_get_cdrom_toc(b);
      CHECK(a_toc && b_toc && a_toc != b_toc);
      CHECK(a_toc->track[0].lba == 150 && b_toc->track[0].lba == 250);
      CHECK(retro_vfs_file_size_impl(a) == 2352000);
      CHECK(retro_vfs_file_size_impl(b) == 1176000);
      CHECK(retro_vfs_file_seek_impl(a, 0, RETRO_VFS_SEEK_POSITION_START) == 0);
      CHECK(retro_vfs_file_read_impl(a, buf, 2352) == 2352 && buf[0] == 0);
      CHECK(retro_vfs_file_read_impl(b, buf, 2352) == 2352 && buf[0] == 25);
      CHECK(retro_vfs_file_close_impl(b) == 0);
      b = NULL;
      CHECK(retro_vfs_file_get_cdrom_toc(a) == a_toc);
      CHECK(retro_vfs_file_seek_impl(a, -2352, RETRO_VFS_SEEK_POSITION_END) == 0);
      CHECK(retro_vfs_file_tell_impl(a) == 2349648);
      CHECK(a->cdrom->cur_lba == 1149);
      CHECK(retro_vfs_file_close_impl(a) == 0);
      a = NULL;
   }
   if (a) retro_vfs_file_close_impl(a);
   if (b) retro_vfs_file_close_impl(b);
   before = opens;
   CHECK(retro_vfs_file_open_impl(BAD_TRACK, RETRO_VFS_FILE_ACCESS_READ, 0) == NULL);
   CHECK(retro_vfs_file_open_impl("cdrom://not-a-drive.cue", RETRO_VFS_FILE_ACCESS_READ, 0) == NULL);
   CHECK(opens == before);
   CHECK(retro_vfs_file_open_impl(OUT_TRACK, RETRO_VFS_FILE_ACCESS_READ, 0) == NULL);
   toc_bad = 1;
   CHECK(retro_vfs_file_open_impl(CUE_A, RETRO_VFS_FILE_ACCESS_READ, 0) == NULL);
   CHECK(retro_vfs_file_open_impl(BIN_A, RETRO_VFS_FILE_ACCESS_READ, 0) == NULL);
   probe = retro_vfs_file_open_impl(DEVICE_A, RETRO_VFS_FILE_ACCESS_READ, 0);
   CHECK(probe != NULL);
   if (probe)
   {
      CHECK(retro_vfs_file_get_cdrom_toc(probe) == NULL);
      CHECK(retro_vfs_file_size_impl(probe) == 0);
      CHECK(cdrom_get_media_profile(probe, &profile) == 0 && profile == 0x10);
      CHECK(retro_vfs_file_read_impl(probe, buf, 1) == -1);
      CHECK(retro_vfs_file_error_impl(probe) != 0);
      CHECK(retro_vfs_file_close_impl(probe) == 0);
   }
   CHECK(cdrom_drive_has_media(DRIVE_A));
   toc_bad = 0;
   a = retro_vfs_file_open_impl(CUE_A, RETRO_VFS_FILE_ACCESS_READ, 0);
   CHECK(a != NULL);
   if (a)
   {
      CHECK(retro_vfs_file_get_cdrom_toc(a) != NULL);
      CHECK(retro_vfs_file_read_impl(a, buf, 100) > 0);
      CHECK(retro_vfs_file_close_impl(a) == 0);
   }
}

#if defined(_WIN32)
/* Drive enumeration identifies drives by INQUIRY alone: it must open
 * metadata-only device handles and never read a TOC, which would spin
 * up every drive (and drop empty ones) each time the menu is built. */
static void test_enumeration(void)
{
   unsigned before_toc = toc_reads, before_inquiry = inquiries;
   unsigned before_opens = opens;
   struct string_list *list = cdrom_get_available_drives();
   CHECK(list != NULL);
   if (!list)
      return;
   CHECK(list->size == 2);
   if (list->size == 2)
   {
      CHECK(list->elems[0].attr.i == 'd' && list->elems[1].attr.i == 'e');
      CHECK(strstr(list->elems[0].data, "LIBRETRO") != NULL);
   }
   CHECK(opens == before_opens + 2);
   CHECK(inquiries == before_inquiry + 2);
   CHECK(toc_reads == before_toc);
   string_list_free(list);
}
#endif

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
   /* The final partial read cached sector six. */
   before = calls;
   CHECK(retro_vfs_file_seek_cdrom(&stream, 6 * 2048, SEEK_SET) == 0);
   CHECK(retro_vfs_file_read_cdrom(&stream, buf, 2048) == 2048);
   CHECK(calls == before);
   for (i = 0; i < 2048; i++)
      CHECK(buf[i] == (unsigned char)(6 * 2048 + i));
   CHECK(retro_vfs_file_seek_cdrom(&stream, 6 * 2048, SEEK_SET) == 0);
   CHECK(retro_vfs_file_read_cdrom(&stream, buf + 1, 2048 + 19) == 2048 + 19);
   CHECK(calls == before + 1);
   for (i = 0; i < 2048 + 19; i++)
      CHECK(buf[i + 1] == (unsigned char)(6 * 2048 + i));
   before = calls;
   fail_at = (int)calls + 1;
   CHECK(retro_vfs_file_seek_cdrom(&stream, 7 * 2048, SEEK_SET) == 0);
   CHECK(retro_vfs_file_read_cdrom(&stream, buf, 2 * 2048) == 2048);
   CHECK(calls == before + 1 && !disc.last_frame_valid);
   CHECK(retro_vfs_file_tell_cdrom(&stream) == 8 * 2048);
   fail_at = 0;
   CHECK(retro_vfs_file_seek_cdrom(&stream, 7 * 2048, SEEK_SET) == 0);
   CHECK(retro_vfs_file_read_cdrom(&stream, buf, 2048) == 2048);
   CHECK(calls == before + 2);
   CHECK(retro_vfs_file_seek_cdrom(&stream, 14095, SEEK_SET) == 0);

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
      disc.toc = toc;
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
      disc.toc->track[0].audio = true;
      disc.toc->track[0].lba = 200;
      CHECK(retro_vfs_file_seek_cdrom(&stream, 0, SEEK_SET) == 0);
      CHECK(disc.cur_lba == 150);
      stream.orig_path = IMAGE_PATH;
      free(cue);
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
   test_handle_geometry(buf);
   test_bounded_staging(buf);
   {
      libretro_vfs_implementation_file *file = retro_vfs_file_open_impl(
            BIN_A, RETRO_VFS_FILE_ACCESS_READ, 0);
      CHECK(file != NULL);
      if (file)
      {
         saw_direct = 0;
         CHECK(retro_vfs_file_read_impl(file, buf, 2352) == 2352 && saw_direct);
         CHECK(retro_vfs_file_seek_impl(file, 0, RETRO_VFS_SEEK_POSITION_START) == 0);
         before = calls;
         CHECK(retro_vfs_file_read_impl(file, buf, 2352) == 2352 && calls == before);
         CHECK(retro_vfs_file_seek_impl(file, 2352, RETRO_VFS_SEEK_POSITION_START) == 0);
         direct_buffer = buf + 1; saw_direct = 0;
         CHECK(retro_vfs_file_read_impl(file, buf + 1, 2352) == 2352 && !saw_direct);
         CHECK(retro_vfs_file_seek_impl(file, 4704, RETRO_VFS_SEEK_POSITION_START) == 0);
         direct_buffer = buf; saw_direct = 0;
         CHECK(retro_vfs_file_read_impl(file, buf, 100) == 100 && !saw_direct);
         CHECK(retro_vfs_file_seek_impl(file, 7056, RETRO_VFS_SEEK_POSITION_START) == 0);
         saw_direct = 0;
         CHECK(retro_vfs_file_read_impl(file, buf, 4704) == 4704 && saw_direct);
         for (i = 0; i < 4704; i++)
            CHECK(buf[i] == (i < 2352 ? 3 : 4));
         CHECK(retro_vfs_file_seek_impl(file, 5 * 2352, RETRO_VFS_SEEK_POSITION_START) == 0);
         direct_buffer = buf + 1; saw_direct = 0;
         CHECK(retro_vfs_file_read_impl(file, buf + 1, 4704) == 4704 && !saw_direct);
         for (i = 0; i < 4704; i++)
            CHECK(buf[i + 1] == (i < 2352 ? 5 : 6));
         CHECK(retro_vfs_file_seek_impl(file, 7 * 2352, RETRO_VFS_SEEK_POSITION_START) == 0);
         direct_buffer = buf; saw_direct = 0;
         CHECK(retro_vfs_file_read_impl(file, buf, 2352 + 100) == 2352 + 100 && saw_direct);
         for (i = 0; i < 2352 + 100; i++)
            CHECK(buf[i] == (i < 2352 ? 7 : 8));
         CHECK(retro_vfs_file_close_impl(file) == 0);
      }
   }
   test_media_change(buf);
#if defined(_WIN32)
   test_enumeration();
#endif
   {
      libretro_vfs_implementation_file *file = retro_vfs_file_open_impl(
            BIN_A, RETRO_VFS_FILE_ACCESS_READ, 0);
      CHECK(file != NULL);
      if (file)
      {
         close_error = 1;
         CHECK(retro_vfs_file_close_impl(file) == -1);
         close_error = 0;
      }
   }
   memalign_free(buf);
   printf("DVD block/VFS tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
