#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <wiiu/fs.h>

/* FS defines and types */
#define FS_MAX_LOCALPATH_SIZE           511
#define FS_MAX_MOUNTPATH_SIZE           128
#define FS_MAX_FULLPATH_SIZE            (FS_MAX_LOCALPATH_SIZE + FS_MAX_MOUNTPATH_SIZE)
#define FS_MAX_ARGPATH_SIZE             FS_MAX_FULLPATH_SIZE

#define FS_STATUS_OK                    0
#define FS_RET_UNSUPPORTED_CMD          0x0400
#define FS_RET_NO_ERROR                 0x0000
#define FS_RET_ALL_ERROR                (unsigned int)(-1)

#define FS_STAT_FLAG_IS_DIRECTORY       0x80000000

/* max length of file/dir name */
#define FS_MAX_ENTNAME_SIZE             256

#define FS_SOURCETYPE_EXTERNAL          0
#define FS_SOURCETYPE_HFIO              1
#define FS_SOURCETYPE_HFIO              1

#define FS_MOUNT_SOURCE_SIZE            0x300
#define FS_CLIENT_SIZE                  0x1700
#define FS_CMD_BLOCK_SIZE               0xA80

int MountFS(void *pClient, void *pCmd, char **mount_path)
{
    int result = -1;

    void *mountSrc = malloc(FS_MOUNT_SOURCE_SIZE);
    if(!mountSrc)
        return -3;

    char* mountPath = (char*) malloc(FS_MAX_MOUNTPATH_SIZE);
    if(!mountPath) {
        free(mountSrc);
        return -4;
    }

    memset(mountSrc, 0, FS_MOUNT_SOURCE_SIZE);
    memset(mountPath, 0, FS_MAX_MOUNTPATH_SIZE);

    // Mount sdcard
    if (FSGetMountSource(pClient, pCmd, FS_SOURCETYPE_EXTERNAL, mountSrc, -1) == 0)
    {
        result = FSMount(pClient, pCmd, mountSrc, mountPath, FS_MAX_MOUNTPATH_SIZE, -1);
        if((result == 0) && mount_path) {
            *mount_path = (char*)malloc(strlen(mountPath) + 1);
            if(*mount_path)
                strcpy(*mount_path, mountPath);
        }
    }

    free(mountPath);
    free(mountSrc);
    return result;
}

int UmountFS(void *pClient, void *pCmd, const char *mountPath)
{
    int result = -1;
    result = FSUnmount(pClient, pCmd, mountPath, -1);

    return result;
}

int LoadFileToMem(const char *filepath, u8 **inbuffer, u32 *size)
{
    //! always initialze input
	*inbuffer = NULL;
    if(size)
        *size = 0;

    int iFd = open(filepath, O_RDONLY);
	if (iFd < 0)
		return -1;

	u32 filesize = lseek(iFd, 0, SEEK_END);
    lseek(iFd, 0, SEEK_SET);

	u8 *buffer = (u8 *) malloc(filesize);
	if (buffer == NULL)
	{
        close(iFd);
		return -2;
	}

    u32 blocksize = 0x4000;
    u32 done = 0;
    int readBytes = 0;

	while(done < filesize)
    {
        if(done + blocksize > filesize) {
            blocksize = filesize - done;
        }
        readBytes = read(iFd, buffer + done, blocksize);
        if(readBytes <= 0)
            break;
        done += readBytes;
    }

    close(iFd);

	if (done != filesize)
	{
		free(buffer);
		return -3;
	}

	*inbuffer = buffer;

    //! sign is optional input
    if(size)
        *size = filesize;

	return filesize;
}

int CheckFile(const char * filepath)
{
	if(!filepath)
		return 0;

	struct stat filestat;

	char dirnoslash[strlen(filepath)+2];
	snprintf(dirnoslash, sizeof(dirnoslash), "%s", filepath);

	while(dirnoslash[strlen(dirnoslash)-1] == '/')
		dirnoslash[strlen(dirnoslash)-1] = '\0';

	char * notRoot = strrchr(dirnoslash, '/');
	if(!notRoot)
	{
		strcat(dirnoslash, "/");
	}

	if (stat(dirnoslash, &filestat) == 0)
		return 1;

	return 0;
}

int CreateSubfolder(const char * fullpath)
{
	if(!fullpath)
		return 0;

	int result = 0;

	char dirnoslash[strlen(fullpath)+1];
	strcpy(dirnoslash, fullpath);

	int pos = strlen(dirnoslash)-1;
	while(dirnoslash[pos] == '/')
	{
		dirnoslash[pos] = '\0';
		pos--;
	}

	if(CheckFile(dirnoslash))
	{
		return 1;
	}
	else
	{
		char parentpath[strlen(dirnoslash)+2];
		strcpy(parentpath, dirnoslash);
		char * ptr = strrchr(parentpath, '/');

		if(!ptr)
		{
			//!Device root directory (must be with '/')
			strcat(parentpath, "/");
			struct stat filestat;
			if (stat(parentpath, &filestat) == 0)
				return 1;

			return 0;
		}

		ptr++;
		ptr[0] = '\0';

		result = CreateSubfolder(parentpath);
	}

	if(!result)
		return 0;

	if (mkdir(dirnoslash, 0777) == -1)
	{
		return 0;
	}

	return 1;
}
