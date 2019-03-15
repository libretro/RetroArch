#include <stdio.h>
#include <tamtypes.h>
#include <kernel.h>
#include <string.h>
#include <sifrpc.h>
#include <sys/fcntl.h>
#include <cdvd_rpc.h>
#include <fileXio_cdvd.h>
#include "ps2_devices.h"
#include "ps2_descriptor.h"

#define CD_SERVER_INIT			0x80000592
#define CD_SERVER_SCMD			0x80000593
#define CD_SCMD_GETDISCTYPE		0x03

static SifRpcClientData_t clientInit __attribute__ ((aligned(64)));
static u32 initMode __attribute__ ((aligned(64)));
static int cdThreadId = 0;
static int bindSearchFile = -1;
static int bindDiskReady = -1;
static int bindInit = -1;
static int bindNCmd = -1;
static int bindSCmd = -1;
static int nCmdSemaId = -1;		// n-cmd semaphore id
static int sCmdSemaId = -1;		// s-cmd semaphore id
static int callbackSemaId = -1;	// callback semaphore id
static int cdDebug = 0;
static int sCmdNum = 0;
static SifRpcClientData_t clientSCmd __attribute__ ((aligned(64)));
static u8 sCmdRecvBuff[48] __attribute__ ((aligned(64)));
static volatile int cbSema = 0;
static ee_thread_status_t cdThreadParam;
static int callbackThreadId = 0;
volatile int cdCallbackNum __attribute__ ((aligned(64)));

static void cdSemaInit(void);
static int cdCheckSCmd(int cur_cmd);
static int cdSyncS(int mode);
static void cdSemaExit(void);

static int first_file_index;

int cdInit(int mode)
{
	int i;

	if (cdSyncS(1))
		return 0;
	SifInitRpc(0);
	cdThreadId = GetThreadId();
	bindSearchFile = -1;
	bindNCmd = -1;
	bindSCmd = -1;
	bindDiskReady = -1;
	bindInit = -1;

	while (1) {
		if (SifBindRpc(&clientInit, CD_SERVER_INIT, 0) >= 0)
			if (clientInit.server != 0) break;
		i = 0x10000;
		while (i--);
	}

	bindInit = 0;
	initMode = mode;
	SifWriteBackDCache(&initMode, 4);
	if (SifCallRpc(&clientInit, 0, 0, &initMode, 4, 0, 0, 0, 0) < 0)
		return 0;
	if (mode == CDVD_INIT_EXIT) {
		cdSemaExit();
		nCmdSemaId = -1;
		sCmdSemaId = -1;
		callbackSemaId = -1;
	} else {
		cdSemaInit();
	}
	return 1;
}

static void cdSemaExit(void)
{
	if (callbackThreadId) {
		cdCallbackNum = -1;
		SignalSema(callbackSemaId);
	}
	DeleteSema(nCmdSemaId);
	DeleteSema(sCmdSemaId);
	DeleteSema(callbackSemaId);
}

static void cdSemaInit(void)
{
	struct t_ee_sema semaParam;

	// return if both semaphores are already inited
	if (nCmdSemaId != -1 && sCmdSemaId != -1)
		return;

	semaParam.init_count = 1;
	semaParam.max_count = 1;
	semaParam.option = 0;
	nCmdSemaId = CreateSema(&semaParam);
	sCmdSemaId = CreateSema(&semaParam);

	semaParam.init_count = 0;
	callbackSemaId = CreateSema(&semaParam);

	cbSema = 0;
}

static int cdCheckSCmd(int cur_cmd)
{
	int i;
	cdSemaInit();
	if (PollSema(sCmdSemaId) != sCmdSemaId) {
		if (cdDebug > 0)
			printf("Scmd fail sema cur_cmd:%d keep_cmd:%d\n", cur_cmd, sCmdNum);
		return 0;
	}
	sCmdNum = cur_cmd;
	ReferThreadStatus(cdThreadId, &cdThreadParam);
	if (cdSyncS(1)) {
		SignalSema(sCmdSemaId);
		return 0;
	}

	SifInitRpc(0);
	if (bindSCmd >= 0)
		return 1;
	while (1) {
		if (SifBindRpc(&clientSCmd, CD_SERVER_SCMD, 0) < 0) {
			if (cdDebug > 0)
				printf("Libcdvd bind err S cmd\n");
		}
		if (clientSCmd.server != 0)
			break;

		i = 0x10000;
		while (i--)
			;
	}

	bindSCmd = 0;
	return 1;
}

static int cdSyncS(int mode)
{
	if (mode == 0) {
		if (cdDebug > 0)
			printf("S cmd wait\n");
		while (SifCheckStatRpc(&clientSCmd))
			;
		return 0;
	}
	return SifCheckStatRpc(&clientSCmd);
}

static int comp_entries_by_filename(const void *elem1, const void *elem2)
{
    return strcmp(((entries*)elem1)->filename, ((entries*)elem2)->filename);
}

static inline char* strzncpy(char *d, const char *s, size_t l) 
{ 
   d[0] = 0; return strncat(d, s, l); 
}

static int listcdvd(const char *path, entries *FileEntry)
{
   static struct TocEntry TocEntryList[FILEENTRY_SIZE];
   char dir[1025];
   int i, n;
   int t = 0;

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
      strzncpy(FileEntry[t].displayname, FileEntry[t].filename, 63);
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
      strzncpy(FileEntry[t].displayname, FileEntry[t].filename, 63);
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
   if (descriptor->current_folder_position == -1) {
      descriptor->current_folder_position = 0;
      descriptor->items = listcdvd(descriptor->path, descriptor->FileEntry);
   }

   if (descriptor->current_folder_position < descriptor->items) {
      strcpy(dirent->name, descriptor->FileEntry[descriptor->current_folder_position].filename);
      if (descriptor->FileEntry[descriptor->current_folder_position].dircheck) {
         dirent->stat.mode = FIO_S_IFDIR;
      }
      descriptor->current_folder_position++;
   } else {
      descriptor->current_folder_position = 0;
      return 0;
   }

   return 1;
}

int ps2fileXioDopen(const char *name)
{
   enum BootDeviceIDs deviceID = getBootDeviceID((char *)name);
   int fd;
   if (deviceID == BOOT_DEVICE_CDFS) {
      fd = __ps2_acquire_descriptor();
      DescriptorTranslation *descriptor = __ps2_fd_grab(fd);
      strcpy(descriptor->path, name);
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
   if (is_fd_valid(fd)) {
      return __ps2_release_descriptor(fd);
   } else {
      return fileXioDclose(fd);
   }
}
