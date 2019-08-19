#include <stdio.h>
#include <tamtypes.h>
#include <kernel.h>
#include <string.h>
#include <time.h>
#include <sifrpc.h>
#include <sys/fcntl.h>
#include <cdvd_rpc.h>
#include <fileXio_cdvd.h>
#include <libcdvd-common.h>
#include "ps2_devices.h"
#include "ps2_descriptor.h"

/* I dont know why but this line is totally needed */
static SifRpcClientData_t clientInit __attribute__ ((aligned(64)));

static int comp_entries_by_filename(const void *elem1, const void *elem2)
{
   return strcmp(((entries*)elem1)->filename, ((entries*)elem2)->filename);
}

static int ps2_cdDiscValid(void)  //returns 1 if disc valid, else returns 0
{
	int cdmode = sceCdGetDiskType();

	switch (cdmode) {
		case SCECdPSCD:
		case SCECdPSCDDA:
		case SCECdPS2CD:
		case SCECdPS2CDDA:
		case SCECdPS2DVD:
		case SCECdCDDA:
		case SCECdDVDV:
			return 1;
		case SCECdNODISC:
		case SCECdDETCT:
		case SCECdDETCTCD:
		case SCECdDETCTDVDS:
		case SCECdDETCTDVDD:
		case SCECdUNKNOWN:
		case SCECdIllegalMedia:
		default:
			return 0;
	}
}

static u64 cd_Timer(void)
{
   return (clock() / (CLOCKS_PER_SEC / 1000));
}

static void ps2_cdStop(void)
{
   CDVD_Stop();
   sceCdSync(0);
}

static int prepareCDVD(void)
{
   u64 wait_start;
   int cdmode = sceCdGetDiskType();

   if (sceCdGetDiskType() <= SCECdUNKNOWN) {
      wait_start = cd_Timer();
      while ((cd_Timer() < wait_start + 500) && !ps2_cdDiscValid()) {
         if (cdmode == SCECdNODISC)
            return 0;
      }
      if (cdmode == SCECdNODISC)
         return 0;
      if ((cdmode < SCECdPSCD) || (cdmode > SCECdPS2DVD)) {
         ps2_cdStop();
         return 0;
      }
   }

   return 1;
}

static int listcdvd(const char *path, entries *FileEntry)
{
   static struct TocEntry TocEntryList[FILEENTRY_SIZE];
   char dir[1025];
   int i, n;
   int t = 0;
   int first_file_index;

   strcpy(dir, &path[5]);
   // Directories first...

   CDVD_FlushCache();
   n = CDVD_GetDir(dir, NULL, CDVD_GET_DIRS_ONLY, TocEntryList, FILEENTRY_SIZE, dir);

   for (i = 0; i < n; i++) {
      if (TocEntryList[i].fileProperties & 0x02 && (!strcmp(
               TocEntryList[i].filename, ".") || !strcmp(
                     TocEntryList[i].filename, "..")))
         continue; // Skip pseudopaths "." and ".."

      FileEntry[t].dircheck = 1;
      strcpy(FileEntry[t].filename, TocEntryList[i].filename);
      t++;

      if (t >= FILEENTRY_SIZE - 2) {
         break;
      }
   }

   qsort(FileEntry, t, sizeof(entries), comp_entries_by_filename);
   first_file_index = t;

   // Now files only

   CDVD_FlushCache();
   n = CDVD_GetDir(dir, NULL, CDVD_GET_FILES_ONLY, TocEntryList, FILEENTRY_SIZE, dir);

   for (i = 0; i < n; i++) {
      if (TocEntryList[i].fileProperties & 0x02 && (!strcmp(
               TocEntryList[i].filename, ".") || !strcmp(
                     TocEntryList[i].filename, "..")))
         continue; // Skip pseudopaths "." and ".."

      FileEntry[t].dircheck = 0;
      strcpy(FileEntry[t].filename, TocEntryList[i].filename);
      t++;

      if (t >= FILEENTRY_SIZE - 2) {
         break;
      }
   }

   qsort(FileEntry + first_file_index, t - first_file_index, sizeof(entries), comp_entries_by_filename);

   return t;
}

static int fileXioCDDread(int fd, iox_dirent_t *dirent)
{
   DescriptorTranslation *descriptor = __ps2_fd_grab(fd);

   if (descriptor && descriptor->current_folder_position < descriptor->items) {
      strcpy(dirent->name, descriptor->FileEntry[descriptor->current_folder_position].filename);
      if (descriptor->FileEntry[descriptor->current_folder_position].dircheck) {
         dirent->stat.mode = FIO_S_IFDIR;
      } else {
         dirent->stat.mode = FIO_S_IFREG;
      }
      descriptor->current_folder_position++;
   } else {
      descriptor->current_folder_position = 0;
      return 0;
   }

   return 1;
}

static int fileXioCDDopen(const char *name)
{
   int fd = -1;
   if (prepareCDVD()){
      fd = __ps2_acquire_descriptor();
      DescriptorTranslation *descriptor = __ps2_fd_grab(fd);
      descriptor->current_folder_position = 0;
      descriptor->items = listcdvd(name, descriptor->FileEntry);
   }
   return fd;
}


int ps2fileXioDopen(const char *name)
{
   enum BootDeviceIDs deviceID = getBootDeviceID((char *)name);
   int fd = -1;
   if (deviceID == BOOT_DEVICE_CDFS) {
      fd = fileXioCDDopen(name);
   } else {
      fd = fileXioDopen(name);
   }

   return fd;
}

int ps2fileXioDread(int fd, iox_dirent_t *dirent)
{
   if (is_fd_valid(fd)) {
      return fileXioCDDread(fd, dirent);
   } else {
      return fileXioDread(fd, dirent);
   }
}

int ps2fileXioDclose(int fd)
{
   int ret = -19;
   if (is_fd_valid(fd)) {
      ret = __ps2_release_descriptor(fd);
   } else if (fd > 0) {
      ret = fileXioDclose(fd);
   }

   return ret;
}
