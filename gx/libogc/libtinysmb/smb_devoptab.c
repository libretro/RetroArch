/****************************************************************************
 * TinySMB
 * Nintendo Wii/GameCube SMB implementation
 *
 * SMB devoptab
 ****************************************************************************/
#include <malloc.h>
#include <sys/iosupport.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include <ogc/lwp.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/mutex.h>

#include "smb.h"

#define MAX_SMB_MOUNTED 10

static lwp_t cache_thread = LWP_THREAD_NULL;
static SMBDIRENTRY last_dentry;
static int last_env=-1;
static char last_path[SMB_MAXPATH];


typedef struct
{
	SMBFILE handle;
	off_t offset;
	off_t len;
	char filename[SMB_MAXPATH];
	unsigned short access;
	int env;
	u32 attributes;
} SMBFILESTRUCT;

typedef struct
{
	SMBDIRENTRY smbdir;
	int env;
	char dir[SMB_MAXPATH];
} SMBDIRSTATESTRUCT;

static bool smbInited = false;
static unsigned short smbFlags = SMB_SRCH_DIRECTORY | SMB_SRCH_READONLY;

///////////////////////////////////////////
//      CACHE FUNCTION DEFINITIONS       //
///////////////////////////////////////////
#define SMB_READ_BUFFERSIZE				65472
#define SMB_WRITE_BUFFERSIZE			(60*1024)

typedef struct
{
	off_t offset;
	u64 last_used;
	SMBFILESTRUCT *file;
	void *ptr;
} smb_cache_page;

typedef struct
{
	u64 used;
	size_t len;
	SMBFILESTRUCT *file;
	void *ptr;
} smb_write_cache;

static void DestroySMBReadAheadCache(const char *name);
static void SMBEnableReadAhead(const char *name, u32 pages);
static int ReadSMBFromCache(void *buf, size_t len, SMBFILESTRUCT *file);

///////////////////////////////////////////
//    END CACHE FUNCTION DEFINITIONS     //
///////////////////////////////////////////

// SMB Enviroment
typedef struct
{
	char *name;
	int pos;
	devoptab_t *devoptab;

	SMBCONN smbconn;
	u8 SMBCONNECTED;

	char currentpath[SMB_MAXPATH];
	bool first_item_dir;
	bool diropen_root;

	smb_write_cache SMBWriteCache;
	smb_cache_page *SMBReadAheadCache;
	int SMB_RA_pages;

	mutex_t _SMB_mutex;
} smb_env;

static smb_env SMBEnv[MAX_SMB_MOUNTED];

static inline void _SMB_lock(int i)
{
	if(SMBEnv[i]._SMB_mutex!=LWP_MUTEX_NULL) LWP_MutexLock(SMBEnv[i]._SMB_mutex);
}

static inline void _SMB_unlock(int i)
{
	if(SMBEnv[i]._SMB_mutex!=LWP_MUTEX_NULL) LWP_MutexUnlock(SMBEnv[i]._SMB_mutex);
}

///////////////////////////////////////////
//         CACHE FUNCTIONS              //
///////////////////////////////////////////

static smb_env* FindSMBEnv(const char *name)
{
	int i;
	char *aux;

	aux=strdup(name);
	i=strlen(aux);
	if(aux[i-1]==':')aux[i-1]='\0';
	for(i=0;i<MAX_SMB_MOUNTED ;i++)
	{
		if(SMBEnv[i].SMBCONNECTED && strcmp(aux,SMBEnv[i].name)==0)
		{
			free(aux);
			return &SMBEnv[i];
		}
	}
	free(aux);
	return NULL;
}

static int FlushWriteSMBCache(char *name)
{
	smb_env *env;
	env=FindSMBEnv(name);
	if(env==NULL) return -1;

	if (env->SMBWriteCache.file == NULL || env->SMBWriteCache.len == 0)
	{
		return 0;
	}

	int ret;
	int written = 0;

	while(env->SMBWriteCache.len > 0)
	{
		ret = SMB_WriteFile(env->SMBWriteCache.ptr+written, env->SMBWriteCache.len,
				env->SMBWriteCache.file->offset, env->SMBWriteCache.file->handle);

		if (ret <= 0)
			return -1;

		written += ret;
		env->SMBWriteCache.file->offset += ret;
		if (env->SMBWriteCache.file->offset > env->SMBWriteCache.file->len)
			env->SMBWriteCache.file->len = env->SMBWriteCache.file->offset;

		env->SMBWriteCache.len-=ret;
		if(env->SMBWriteCache.len==0) break;
	}
	env->SMBWriteCache.used = 0;
	env->SMBWriteCache.file = NULL;

	return 0;
}

static void DestroySMBReadAheadCache(const char *name)
{
	smb_env *env;
	env=FindSMBEnv(name);
	if(env==NULL) return ;

	int i;
	if (env->SMBReadAheadCache != NULL)
	{
		for (i = 0; i < env->SMB_RA_pages; i++)
		{
			if(env->SMBReadAheadCache[i].ptr)
				free(env->SMBReadAheadCache[i].ptr);
		}
		free(env->SMBReadAheadCache);
		env->SMBReadAheadCache = NULL;
		env->SMB_RA_pages = 0;
	}
	FlushWriteSMBCache(env->name);

	if(env->SMBWriteCache.ptr)
		free(env->SMBWriteCache.ptr);

	env->SMBWriteCache.used = 0;
	env->SMBWriteCache.len = 0;
	env->SMBWriteCache.file = NULL;
	env->SMBWriteCache.ptr = NULL;
}

static void *process_cache_thread(void *ptr)
{
	int i;
	while (1)
	{
		for(i=0;i<MAX_SMB_MOUNTED ;i++)
		{
			if(SMBEnv[i].SMBCONNECTED)
			{
				if (SMBEnv[i].SMBWriteCache.used > 0)
				{
					if (ticks_to_millisecs(gettime())-ticks_to_millisecs(SMBEnv[i].SMBWriteCache.used) > 500)
					{
						_SMB_lock(i);
						FlushWriteSMBCache(SMBEnv[i].name);
						_SMB_unlock(i);
					}
				}
			}
		}
		usleep(10000);
	}
	return NULL;
}

static void SMBEnableReadAhead(const char *name, u32 pages)
{
	s32 i, j;

	smb_env *env;
	env=FindSMBEnv(name);
	if(env==NULL) return;

	DestroySMBReadAheadCache(name);

	if (pages == 0)
		return;

	//only 1 page for write
	env->SMBWriteCache.ptr = memalign(32, SMB_WRITE_BUFFERSIZE);
	env->SMBWriteCache.used = 0;
	env->SMBWriteCache.len = 0;
	env->SMBWriteCache.file = NULL;

	env->SMB_RA_pages = pages;
	env->SMBReadAheadCache = (smb_cache_page *) malloc(sizeof(smb_cache_page) * env->SMB_RA_pages);
	if (env->SMBReadAheadCache == NULL)
		return;
	for (i = 0; i < env->SMB_RA_pages; i++)
	{
		env->SMBReadAheadCache[i].offset = 0;
		env->SMBReadAheadCache[i].last_used = 0;
		env->SMBReadAheadCache[i].file = NULL;
		env->SMBReadAheadCache[i].ptr = memalign(32, SMB_READ_BUFFERSIZE);
		if (env->SMBReadAheadCache[i].ptr == NULL)
		{
			for (j = i - 1; j >= 0; j--)
				if (env->SMBReadAheadCache[j].ptr)
					free(env->SMBReadAheadCache[j].ptr);
			free(env->SMBReadAheadCache);
			env->SMBReadAheadCache = NULL;
			free(env->SMBWriteCache.ptr);
			return;
		}
		memset(env->SMBReadAheadCache[i].ptr, 0, SMB_READ_BUFFERSIZE);
	}
}

// clear cache from file
// called when file is closed
static void ClearSMBFileCache(SMBFILESTRUCT *file)
{
	int i,j;
	j=file->env;

	for (i = 0; i < SMBEnv[j].SMB_RA_pages; i++)
	{
		if (SMBEnv[j].SMBReadAheadCache[i].file == file)
		{
			SMBEnv[j].SMBReadAheadCache[i].offset = 0;
			SMBEnv[j].SMBReadAheadCache[i].last_used = 0;
			SMBEnv[j].SMBReadAheadCache[i].file = NULL;
			memset(SMBEnv[j].SMBReadAheadCache[i].ptr, 0, SMB_READ_BUFFERSIZE);
		}
	}
}

static int ReadSMBFromCache(void *buf, size_t len, SMBFILESTRUCT *file)
{
	int i,j, leastUsed;
	off_t new_offset, rest;
	j=file->env;

	if ( len == 0 ) return 0;

	if (SMBEnv[j].SMBReadAheadCache == NULL)
	{
		if (SMB_ReadFile(buf, len, file->offset, file->handle) <= 0)
		{
			return -1;
		}
		return 0;
	}

	new_offset = file->offset;
	rest = len;

continue_read:

	for (i = 0; i < SMBEnv[j].SMB_RA_pages; i++)
	{
		if (SMBEnv[j].SMBReadAheadCache[i].file == file)
		{
			if ((new_offset >= SMBEnv[j].SMBReadAheadCache[i].offset) &&
				(new_offset < (SMBEnv[j].SMBReadAheadCache[i].offset + SMB_READ_BUFFERSIZE)))
			{
				//we hit the page
				//copy as much as we can
				SMBEnv[j].SMBReadAheadCache[i].last_used = gettime();

				off_t buffer_used = (SMBEnv[j].SMBReadAheadCache[i].offset + SMB_READ_BUFFERSIZE) - new_offset;
				if (buffer_used > rest) buffer_used = rest;
				memcpy(buf, SMBEnv[j].SMBReadAheadCache[i].ptr + (new_offset - SMBEnv[j].SMBReadAheadCache[i].offset), buffer_used);
				buf += buffer_used;
				rest -= buffer_used;
				new_offset += buffer_used;

				if (rest == 0)
				{
					return 0;
				}
				goto continue_read;
			}
		}
	}

	leastUsed = 0;
	for ( i = 1; i < SMBEnv[j].SMB_RA_pages; i++)
	{
		if ((SMBEnv[j].SMBReadAheadCache[i].last_used < SMBEnv[j].SMBReadAheadCache[leastUsed].last_used))
			leastUsed = i;
	}

	//fill least used page with new data

	off_t cache_offset = new_offset;

	//do not interset with existing pages
	for (i = 0; i < SMBEnv[j].SMB_RA_pages; i++)
	{
		if ( i == leastUsed ) continue;
		if ( SMBEnv[j].SMBReadAheadCache[i].file != file ) continue;

		if ( (cache_offset < SMBEnv[j].SMBReadAheadCache[i].offset ) && (cache_offset + SMB_READ_BUFFERSIZE > SMBEnv[j].SMBReadAheadCache[i].offset) )
		{
			//tail of new page intersects with some cache block, clear page
			SMBEnv[j].SMBReadAheadCache[i].file = NULL;
		}

		if ( (cache_offset >= SMBEnv[j].SMBReadAheadCache[i].offset ) && (cache_offset < SMBEnv[j].SMBReadAheadCache[i].offset + SMB_READ_BUFFERSIZE ) )
		{
			//head of new page intersects with some cache block, clear page
			SMBEnv[j].SMBReadAheadCache[i].file = NULL;
		}
	}

	off_t cache_to_read = file->len - cache_offset;
	if ( cache_to_read > SMB_READ_BUFFERSIZE )
	{
		cache_to_read = SMB_READ_BUFFERSIZE;
	}

	int read=0, readed;
	while(read<cache_to_read)
	{
		readed = SMB_ReadFile(SMBEnv[j].SMBReadAheadCache[leastUsed].ptr+read, cache_to_read-read, cache_offset+read, file->handle);
		if ( readed <=0 )
		{
			SMBEnv[j].SMBReadAheadCache[leastUsed].file = NULL;
			return -1;
		}
		read += readed;
	}
	SMBEnv[j].SMBReadAheadCache[leastUsed].last_used = gettime();

	SMBEnv[j].SMBReadAheadCache[leastUsed].offset = cache_offset;
	SMBEnv[j].SMBReadAheadCache[leastUsed].file = file;

	goto continue_read;
}

static int WriteSMBUsingCache(const char *buf, size_t len, SMBFILESTRUCT *file)
{
	size_t size=len;
	if (file == NULL || buf == NULL)
		return -1;
	int j;
	j=file->env;
	if (SMBEnv[j].SMBWriteCache.file != NULL)
	{
		if (strcmp(SMBEnv[j].SMBWriteCache.file->filename, file->filename) != 0)
		{
			//Flush current buffer
			if (FlushWriteSMBCache(SMBEnv[j].name) < 0)
			{
				goto failed;
			}
			SMBEnv[j].SMBWriteCache.file = file;
			SMBEnv[j].SMBWriteCache.len = 0;
		}
	}
	else
	{
		SMBEnv[j].SMBWriteCache.file = file;
		SMBEnv[j].SMBWriteCache.len = 0;
	}
	int rest;
	s32 written;
	while(len > 0)
	{
		if(SMBEnv[j].SMBWriteCache.len+len >= SMB_WRITE_BUFFERSIZE)
		{
			rest = SMB_WRITE_BUFFERSIZE - SMBEnv[j].SMBWriteCache.len;
			memcpy(SMBEnv[j].SMBWriteCache.ptr + SMBEnv[j].SMBWriteCache.len, buf, rest);

			written = SMB_WriteFile(SMBEnv[j].SMBWriteCache.ptr, SMB_WRITE_BUFFERSIZE, file->offset, file->handle);
			if(written<0)
			{
				goto failed;
			}
			file->offset += written;
			if (file->offset > file->len)
				file->len = file->offset;

			buf += rest;
			len -= rest;
			SMBEnv[j].SMBWriteCache.used = gettime();
			SMBEnv[j].SMBWriteCache.len = 0;
		}
		else
		{
			memcpy(SMBEnv[j].SMBWriteCache.ptr + SMBEnv[j].SMBWriteCache.len, buf, len);
			SMBEnv[j].SMBWriteCache.len += len;
			SMBEnv[j].SMBWriteCache.used = gettime();
			break;
		}
	}
	return size;

failed:
	return -1;
}

///////////////////////////////////////////
//         END CACHE FUNCTIONS           //
///////////////////////////////////////////

static char *smb_absolute_path_no_device(const char *srcpath, char *destpath, int env)
{
	char temp[SMB_MAXPATH];
	int i=0,j=0;

	if (strchr(srcpath, ':') != NULL)
	{
		srcpath = strchr(srcpath, ':') + 1;
	}
	if (strchr(srcpath, ':') != NULL)
	{
		return NULL;
	}

	memset(temp,0,SMB_MAXPATH);

	if (srcpath[0] != '\\' && srcpath[0] != '/')
	{
		strcpy(temp, SMBEnv[env].currentpath);
	}
	if(srcpath[0]!='\0') //strlen(srcpath) > 0
	{
		if(srcpath[0]=='.' && (srcpath[1]=='\\' || srcpath[1]=='\0')) // to fix opendir(".") or chdir(".")
			strcat(temp, &srcpath[1]);
		else
			strcat(temp, srcpath);
	}

	while(temp[i]!='\0' && i < SMB_MAXPATH)
	{
		if(temp[i]=='/')
		{
			destpath[j++]='\\';
			while(temp[i]!='\0' && i < SMB_MAXPATH && (temp[i]=='/' || temp[i]=='\\'))i++;
		}
		else if(srcpath[i]=='\\')
		{
			destpath[j++]=temp[i++];
			while(temp[i]!='\0' && i < SMB_MAXPATH && (temp[i]=='/' || temp[i]=='\\'))i++;
		}
		else
		{
			destpath[j++]=temp[i++];
		}
	}
	destpath[j]='\0';

	return destpath;
}

static char *ExtractDevice(const char *path, char *device)
{
	int i,l;
	l=strlen(path);

	for(i=0;i<l && path[i]!='\0' && path[i]!=':' && i < 20;i++)
		device[i]=path[i];
	if(path[i]!=':')device[0]='\0';
	else device[i]='\0';
	return device;
}

//FILE IO
static int __smb_open(struct _reent *r, void *fileStruct, const char *path, int flags, int mode)
{
	SMBFILESTRUCT *file = (SMBFILESTRUCT*) fileStruct;
	char fixedpath[SMB_MAXPATH];
	smb_env *env;

	ExtractDevice(path,fixedpath);
	if(fixedpath[0]=='\0')
	{
		getcwd(fixedpath,SMB_MAXPATH);
		ExtractDevice(fixedpath,fixedpath);
	}
	env=FindSMBEnv(fixedpath);
	if(env==NULL)
	{
		r->_errno = ENODEV;
		return -1;
	}
	file->env=env->pos;

	if (!env->SMBCONNECTED)
	{
		r->_errno = ENODEV;
		return -1;
	}

	if (smb_absolute_path_no_device(path, fixedpath, file->env) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	if(!smbCheckConnection(env->name))
	{
		r->_errno = ENODEV;
		return -1;
	}

	SMBDIRENTRY dentry;
	bool fileExists = true;
	_SMB_lock(env->pos);
	if (SMB_PathInfo(fixedpath, &dentry, env->smbconn) != SMB_SUCCESS)
		fileExists = false;

	
	// Determine which mode the file is open for
	u8 smb_mode;
	unsigned short access;
	if ((flags & 0x03) == O_RDONLY)
	{
		// Open the file for read-only access
		smb_mode = SMB_OF_OPEN;
		access = SMB_OPEN_READING;
	}
	else if ((flags & 0x03) == O_WRONLY)
	{
		// Open file for write only access
		if (fileExists)
			smb_mode = SMB_OF_OPEN;
		else
			smb_mode = SMB_OF_CREATE;
		access = SMB_OPEN_WRITING;
	}
	else if ((flags & 0x03) == O_RDWR)
	{
		// Open file for read/write access
		access = SMB_OPEN_READWRITE;
		if (fileExists)
			smb_mode = SMB_OF_OPEN;
		else
			smb_mode = SMB_OF_CREATE;
	}
	else
	{
		r->_errno = EACCES;
		_SMB_unlock(env->pos);
		return -1;
	}

	if ((flags & O_CREAT) && !fileExists)
		smb_mode = SMB_OF_CREATE;
	if (!(flags & O_APPEND) && fileExists && ((flags & 0x03) != O_RDONLY))
		smb_mode = SMB_OF_TRUNCATE;
	file->handle = SMB_OpenFile(fixedpath, access, smb_mode, env->smbconn);
	if (!file->handle)
	{
		r->_errno = ENOENT;
		_SMB_unlock(env->pos);
		return -1;
	}

	file->len = 0;
	file->attributes = 0;
	if (fileExists)
	{
		file->len = dentry.size;
		file->attributes = dentry.attributes;
	}
	
	if (flags & O_APPEND)
		file->offset = file->len;
	else
		file->offset = 0;

	file->access=access;

	strcpy(file->filename, fixedpath);
	_SMB_unlock(env->pos);
	return 0;
}

static off_t __smb_seek(struct _reent *r, int fd, off_t pos, int dir)
{
	SMBFILESTRUCT *file = (SMBFILESTRUCT*) fd;
	off_t position;
	
	if (file == NULL)
	{
		r->_errno = EBADF;
		return -1;
	}

	//have to flush because SMBWriteCache.file->offset holds offset of cached block not yet written
	_SMB_lock(file->env);
	if (SMBEnv[file->env].SMBWriteCache.file == file)
	{
		FlushWriteSMBCache(SMBEnv[file->env].name);
	}

	switch (dir)
	{
	case SEEK_SET:
		position = pos;
		break;
	case SEEK_CUR:
		position = file->offset + pos;
		break;
	case SEEK_END:
		position = file->len + pos;
		break;
	default:
		r->_errno = EINVAL;
		_SMB_unlock(file->env);
		return -1;
	}

	if (pos > 0 && position < 0)
	{
		r->_errno = EOVERFLOW;
		_SMB_unlock(file->env);
		return -1;
	}
	if (position < 0)
	{
		r->_errno = EINVAL;
		_SMB_unlock(file->env);
		return -1;
	}

	// Save position
	file->offset = position;
	_SMB_unlock(file->env);
	return position;
}

static ssize_t __smb_read(struct _reent *r, int fd, char *ptr, size_t len)
{
	size_t offset = 0;
	size_t readsize;
	int ret = 0;
	int cnt_retry=2;
	SMBFILESTRUCT *file = (SMBFILESTRUCT*) fd;

	if (file == NULL)
	{
		r->_errno = EBADF;
		return -1;
	} 

	if (len <= 0)
	{
        return 0;
	}
	//have to flush because SMBWriteCache.file->offset holds offset of cached block not yet writeln
	//and file->len also may not have been updated yet
	_SMB_lock(file->env);
	if (SMBEnv[file->env].SMBWriteCache.file == file)
	{
		FlushWriteSMBCache(SMBEnv[file->env].name);
	}

	// Don't try to read if the read pointer is past the end of file
	if (file->offset >= file->len)
	{
		r->_errno = EOVERFLOW;
		_SMB_unlock(file->env);
		return 0;
	}

	// Don't read past end of file
	if (len + file->offset > file->len)
	{
		r->_errno = EOVERFLOW;
		len = file->len - file->offset;
	}

	// Short circuit cases where len is 0 (or less)
	if (len <= 0)
	{
		_SMB_unlock(file->env);
        return 0;
	}
retry_read:
	while(offset < len)
	{
		readsize = len - offset;
		if(readsize > SMB_READ_BUFFERSIZE) readsize = SMB_READ_BUFFERSIZE;
		ret = ReadSMBFromCache(ptr+offset, readsize, file);
		if(ret < 0)	break;
		offset += readsize;
		file->offset += readsize;
	}

	if (ret < 0)
	{
		retry_reconnect:
		cnt_retry--;
		if(cnt_retry>=0)
		{
			if(smbCheckConnection(SMBEnv[file->env].name))
			{
				ClearSMBFileCache(file);
				file->handle = SMB_OpenFile(file->filename, file->access, SMB_OF_OPEN, SMBEnv[file->env].smbconn);
				if (!file->handle)
				{
					r->_errno = ENOENT;
					_SMB_unlock(file->env);
					return -1;
				}
				goto retry_read;
			}
			usleep(10000);
			goto retry_reconnect;
		}
		r->_errno = EIO;
		_SMB_unlock(file->env);
		return -1;
	}
	_SMB_unlock(file->env);
	return len;
}

static ssize_t __smb_write(struct _reent *r, int fd, const char *ptr, size_t len)
{
	SMBFILESTRUCT *file = (SMBFILESTRUCT*) fd;
	int written;
	if (file == NULL)
	{
		r->_errno = EBADF;
		return -1;
	}

	// Don't try to write if the pointer is past the end of file
	if (file->offset > file->len)
	{
		r->_errno = EOVERFLOW;
		return -1;
	}

	// Short circuit cases where len is 0 (or less)
	if (len == 0)
        return 0;

	_SMB_lock(file->env);
	written = WriteSMBUsingCache(ptr, len, file);
    _SMB_unlock(file->env);

	if (written <= 0)
	{
		r->_errno = EIO;
		return -1;
	}

	return written;
}

static int __smb_close(struct _reent *r, int fd)
{
	SMBFILESTRUCT *file = (SMBFILESTRUCT*) fd;
	int j;
	j=file->env;
	_SMB_lock(j);
	if (SMBEnv[j].SMBWriteCache.file == file)
	{
		FlushWriteSMBCache(SMBEnv[j].name);
	}
	ClearSMBFileCache(file);
	SMB_CloseFile(file->handle);
	file->len = 0;
	file->offset = 0;
	file->filename[0] = '\0';
	_SMB_unlock(j);

	return 0;
}

static int __smb_chdir(struct _reent *r, const char *path)
{
	char path_absolute[SMB_MAXPATH];

	SMBDIRENTRY dentry;
	int found;

	ExtractDevice(path,path_absolute);
	if(path_absolute[0]=='\0')
	{
		getcwd(path_absolute,SMB_MAXPATH);
		ExtractDevice(path_absolute,path_absolute);
	}

	smb_env* env;
	env=FindSMBEnv(path_absolute);

	if(env == NULL)
	{
		r->_errno = ENODEV;
		return -1;
	}

	if (!env->SMBCONNECTED)
	{
		r->_errno = ENODEV;
		return -1;
	}

	if (smb_absolute_path_no_device(path, path_absolute,env->pos) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	if(!smbCheckConnection(env->name))
	{
		r->_errno = ENODEV;
		return -1;
	}

	memset(&dentry, 0, sizeof(SMBDIRENTRY));

	_SMB_lock(env->pos);
	found = SMB_PathInfo(path_absolute, &dentry, env->smbconn);

	if (found != SMB_SUCCESS)
	{
		r->_errno = ENOENT;
		_SMB_unlock(env->pos);
		return -1;
	}

	if (!(dentry.attributes & SMB_SRCH_DIRECTORY))
	{
		r->_errno = ENOTDIR;
		_SMB_unlock(env->pos);
		return -1;
	}

	strcpy(env->currentpath, path_absolute);
	if (env->currentpath[0] != 0)
	{
		if (env->currentpath[strlen(env->currentpath) - 1] != '\\')
			strcat(env->currentpath, "\\");
	}
	_SMB_unlock(env->pos);
	return 0;
}

static int __smb_dirreset(struct _reent *r, DIR_ITER *dirState)
{
	char path_abs[SMB_MAXPATH];
	SMBDIRSTATESTRUCT* state = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);
	SMBDIRENTRY dentry;

	memset(&dentry, 0, sizeof(SMBDIRENTRY));

	_SMB_lock(state->env);
	SMB_FindClose(&state->smbdir, SMBEnv[state->env].smbconn);

	strcpy(path_abs,SMBEnv[state->env].currentpath);
	strcat(path_abs,"*");
	int found = SMB_FindFirst(path_abs, smbFlags, &dentry, SMBEnv[state->env].smbconn);

	if (found != SMB_SUCCESS)
	{
		r->_errno = ENOENT;
		_SMB_unlock(state->env);
		return -1;
	}

	if (!(dentry.attributes & SMB_SRCH_DIRECTORY))
	{
		r->_errno = ENOTDIR;
		_SMB_unlock(state->env);
		return -1;
	}

	state->smbdir.size = dentry.size;
	state->smbdir.ctime = dentry.ctime;
	state->smbdir.atime = dentry.atime;
	state->smbdir.mtime = dentry.mtime;
	state->smbdir.attributes = dentry.attributes;
	state->smbdir.sid = dentry.sid;
	strcpy(state->smbdir.name, dentry.name);

	SMBEnv[state->env].first_item_dir = true;
	_SMB_unlock(state->env);
	return 0;
}

static DIR_ITER* __smb_diropen(struct _reent *r, DIR_ITER *dirState, const char *path)
{
	char path_absolute[SMB_MAXPATH];
	int found;
	SMBDIRSTATESTRUCT* state = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);
	SMBDIRENTRY dentry;

	ExtractDevice(path,path_absolute);
	if(path_absolute[0]=='\0')
	{
		getcwd(path_absolute,SMB_MAXPATH);
		ExtractDevice(path_absolute,path_absolute);
	}

	smb_env* env;
	env=FindSMBEnv(path_absolute);

	if(env == NULL)
	{
		r->_errno = ENODEV;
		return NULL;
	}

	if (smb_absolute_path_no_device(path, path_absolute, env->pos) == NULL)
	{
		r->_errno = EINVAL;
		return NULL;
	}
	if (strlen(path_absolute) > 0 && path_absolute[strlen(path_absolute) - 1] != '\\')
		strcat(path_absolute, "\\");

	if(!strcmp(path_absolute,"\\"))
		env->diropen_root=true;
	else
		env->diropen_root=false;

	if(!smbCheckConnection(env->name))	
	{
		r->_errno = ENODEV;
				return NULL;
	}

	_SMB_lock(env->pos);
	if(!env->diropen_root) // root must be valid - we don't need check it
	{
		memset(&dentry, 0, sizeof(SMBDIRENTRY));
		found = SMB_PathInfo(path_absolute, &dentry, env->smbconn);
		if (found != SMB_SUCCESS)
		{
			r->_errno = ENOENT;
			_SMB_unlock(env->pos);
			return NULL;
		}

		if (!(dentry.attributes & SMB_SRCH_DIRECTORY))
		{
			r->_errno = ENOTDIR;
			_SMB_unlock(env->pos);
			return NULL;
		}
	}

	strcpy(state->dir,path_absolute);

	strcat(path_absolute, "*");
	memset(&dentry, 0, sizeof(SMBDIRENTRY));
	found = SMB_FindFirst(path_absolute, smbFlags, &dentry, env->smbconn);

	if (found != SMB_SUCCESS)
	{
		r->_errno = ENOENT;
		_SMB_unlock(env->pos);
		return NULL;
	}

	state->env=env->pos;
	state->smbdir.size = dentry.size;
	state->smbdir.ctime = dentry.ctime;
	state->smbdir.atime = dentry.atime;
	state->smbdir.mtime = dentry.mtime;
	state->smbdir.attributes = dentry.attributes;
	state->smbdir.sid = dentry.sid;
	strcpy(state->smbdir.name, dentry.name);
	env->first_item_dir = true;
	_SMB_unlock(env->pos);
	return dirState;
}

static int dentry_to_stat(SMBDIRENTRY *dentry, struct stat *st)
{
	if (!st)
		return -1;
	if (!dentry)
		return -1;

	st->st_dev = 0;
	st->st_ino = 0;

	st->st_mode = ((dentry->attributes & SMB_SRCH_DIRECTORY) ? S_IFDIR : S_IFREG);
	st->st_nlink = 1;
	st->st_uid = 1; // Faked
	st->st_rdev = st->st_dev;
	st->st_gid = 2; // Faked
	st->st_size = dentry->size;
	st->st_atime = dentry->atime/10000000.0 - 11644473600LL;
	st->st_spare1 = 0;
	st->st_mtime = dentry->mtime/10000000.0 - 11644473600LL;
	st->st_spare2 = 0;
	st->st_ctime = dentry->ctime/10000000.0 - 11644473600LL;
	st->st_spare3 = 0;
	st->st_blksize = 1024;
	st->st_blocks = (st->st_size + st->st_blksize - 1) / st->st_blksize; // File size in blocks
	st->st_spare4[0] = 0;
	st->st_spare4[1] = 0;

	return 0;
}

static int cpy_dentry(SMBDIRENTRY *dest, SMBDIRENTRY *source)
{
	if (!dest || !source)
		return -1;

	dest->attributes=source->attributes;
	dest->size=source->size;
	dest->atime=source->atime;
	dest->mtime=source->mtime;
	dest->ctime=source->ctime;
	strcpy(dest->name,source->name);

	return 0;
}

static int __smb_dirnext(struct _reent *r, DIR_ITER *dirState, char *filename,
		struct stat *filestat)
{
	int ret;
	SMBDIRSTATESTRUCT* state = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);
	SMBDIRENTRY dentry;

	if (SMBEnv[state->env].currentpath[0] == '\0' || filestat == NULL)
	{
		r->_errno = ENOENT;
		return -1;
	}

	memset(&dentry, 0, sizeof(SMBDIRENTRY));
	_SMB_lock(state->env);
	if (SMBEnv[state->env].first_item_dir)
	{
		SMBEnv[state->env].first_item_dir = false;
		dentry.size = state->smbdir.size;
		dentry.ctime = state->smbdir.ctime;
		dentry.atime = state->smbdir.atime;
		dentry.mtime = state->smbdir.mtime;
		dentry.attributes = state->smbdir.attributes;
		strcpy(dentry.name, state->smbdir.name);
		strcpy(filename, dentry.name);
		dentry_to_stat(&dentry, filestat);
		cpy_dentry(&last_dentry,&dentry);
		last_env=state->env;
		strcpy(last_path,state->dir);
		_SMB_unlock(state->env);
		return 0;
	}

	dentry.sid = state->smbdir.sid;

	ret = SMB_FindNext(&dentry, SMBEnv[state->env].smbconn);
	if(ret==SMB_SUCCESS && SMBEnv[state->env].diropen_root)
	{
		if(strlen(dentry.name) == 2 && strcmp(dentry.name,"..") == 0)
			ret = SMB_FindNext(&dentry, SMBEnv[state->env].smbconn);
	}
	if (ret == SMB_SUCCESS)
	{
		state->smbdir.size = dentry.size;
		state->smbdir.ctime = dentry.ctime;
		state->smbdir.atime = dentry.atime;
		state->smbdir.mtime = dentry.mtime;
		state->smbdir.attributes = dentry.attributes;
		strcpy(state->smbdir.name, dentry.name);
		strcpy(filename, dentry.name);
	}
	else
	{
		r->_errno = ENOENT;
		_SMB_unlock(state->env);
		return -1;
	}

	dentry_to_stat(&dentry, filestat);
	cpy_dentry(&last_dentry,&dentry);
	last_env=state->env;
	strcpy(last_path,state->dir);
	_SMB_unlock(state->env);
	return 0;
}

static int __smb_dirclose(struct _reent *r, DIR_ITER *dirState)
{
	SMBDIRSTATESTRUCT* state = (SMBDIRSTATESTRUCT*) (dirState->dirStruct);

	int j = state->env;
	_SMB_lock(j);
	SMB_FindClose(&state->smbdir, SMBEnv[j].smbconn);
	memset(state, 0, sizeof(SMBDIRSTATESTRUCT));
	_SMB_unlock(j);
	return 0;
}

static int __smb_stat(struct _reent *r, const char *path, struct stat *st)
{
	char path_absolute[SMB_MAXPATH];
	SMBDIRENTRY dentry;

	if(path == NULL)
	{
		r->_errno = ENODEV;
		return -1;
	}

	if(strcmp(path,".")==0 || strcmp(path,"..")==0)
	{
		memset(st,0,sizeof(struct stat));
		st->st_mode = S_IFDIR;
		return 0;
	}

	ExtractDevice(path,path_absolute);
	if(path_absolute[0]=='\0')
	{
		getcwd(path_absolute,SMB_MAXPATH);
		ExtractDevice(path_absolute,path_absolute);
	}

	smb_env* env;
	env=FindSMBEnv(path_absolute);

	if(env == NULL)
	{
		r->_errno = ENODEV;
		return -1;
	}

	if (smb_absolute_path_no_device(path, path_absolute, env->pos) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	if(env->pos==last_env) //optimization, usually after a dirnext we do stat
	{
		char file[SMB_MAXPATH];
		strcpy(file,last_path);
		strcat(file,last_dentry.name);
		if(strcmp(file,path_absolute)==0)
		{
			dentry_to_stat(&last_dentry, st);
			return 0;
		}
	}

	_SMB_lock(env->pos);
	if (SMB_PathInfo(path_absolute, &dentry, env->smbconn) != SMB_SUCCESS)
	{
		r->_errno = ENOENT;
		_SMB_unlock(env->pos);
		return -1;
	}

	if (dentry.name[0] == '\0')
	{
		
		r->_errno = ENOENT;
		_SMB_unlock(env->pos);
		return -1;
	}

	dentry_to_stat(&dentry, st);
	_SMB_unlock(env->pos);

	return 0;
}

static int __smb_fstat(struct _reent *r, int fd, struct stat *st)
{
	SMBFILESTRUCT *filestate = (SMBFILESTRUCT *) fd;

	if (!filestate)
	{
		r->_errno = EBADF;
		return -1;
	}

	st->st_size = filestate->len;
	st->st_mode = ((filestate->attributes & SMB_SRCH_DIRECTORY) ? S_IFDIR : S_IFREG);

	return 0;
}

static int __smb_mkdir(struct _reent *r, const char *name, int mode)
{
	char fixedName[SMB_MAXPATH];
	smb_env *env;

	ExtractDevice(name,fixedName);
	if(fixedName[0]=='\0')
	{
		getcwd(fixedName,SMB_MAXPATH);
		ExtractDevice(fixedName,fixedName);
	}

	env=FindSMBEnv(fixedName);
	if(env==NULL)
	{
		r->_errno = ENODEV;
		return -1;
	}

	if (smb_absolute_path_no_device(name, fixedName, env->pos) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	s32 ret = 0;
	_SMB_lock(env->pos);
        if(SMB_CreateDirectory(fixedName, env->smbconn) != SMB_SUCCESS)
            ret = -1;
	_SMB_unlock(env->pos);

	return ret;
}

static int __smb_unlink(struct _reent *r, const char *name)
{
	char fixedName[SMB_MAXPATH];
	smb_env *env;
	bool isDir = false;

	DIR *dir = NULL;
	dir = opendir(name);
	if(dir)
    {
        closedir(dir);
        isDir = true;
	}

	ExtractDevice(name,fixedName);
	if(fixedName[0]=='\0')
	{
		getcwd(fixedName,SMB_MAXPATH);
		ExtractDevice(fixedName,fixedName);
	}

	env=FindSMBEnv(fixedName);
	if(env==NULL)
	{
		r->_errno = ENODEV;
		return -1;
	}

	if (smb_absolute_path_no_device(name, fixedName, env->pos) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	s32 ret = 0;
	_SMB_lock(env->pos);
        if(isDir)
            ret = SMB_DeleteDirectory(fixedName, env->smbconn);
        else
            ret = SMB_DeleteFile(fixedName, env->smbconn);
	_SMB_unlock(env->pos);

	if(ret != SMB_SUCCESS)
        return -1;

	return 0;
}

static int __smb_rename(struct _reent *r, const char *oldName, const char *newName)
{
	char fixedOldName[SMB_MAXPATH];
	char fixedNewName[SMB_MAXPATH];
	smb_env *env;

	ExtractDevice(oldName,fixedOldName);
	if(fixedOldName[0]=='\0')
	{
		getcwd(fixedOldName,SMB_MAXPATH);
		ExtractDevice(fixedOldName,fixedOldName);
	}

	env=FindSMBEnv(fixedOldName);
	if(env==NULL)
	{
		r->_errno = ENODEV;
		return -1;
	}

	if (smb_absolute_path_no_device(oldName, fixedOldName, env->pos) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	ExtractDevice(newName,fixedNewName);
	if(fixedNewName[0]=='\0')
	{
		getcwd(fixedNewName,SMB_MAXPATH);
		ExtractDevice(fixedNewName,fixedNewName);
	}

	if (smb_absolute_path_no_device(newName, fixedNewName, env->pos) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	s32 ret = 0;
	_SMB_lock(env->pos);
	if (SMB_Rename(fixedOldName, fixedNewName, env->smbconn) != SMB_SUCCESS)
		ret = -1;
	_SMB_unlock(env->pos);

	return ret;
}

static int __smb_statvfs_r(struct _reent *r, const char *name, struct statvfs *buf)
{
	char fixedName[SMB_MAXPATH];
	smb_env *env;

	ExtractDevice(name,fixedName);
	if(fixedName[0]=='\0')
	{
		getcwd(fixedName,SMB_MAXPATH);
		ExtractDevice(fixedName,fixedName);
	}

	env=FindSMBEnv(fixedName);
	if(env==NULL)
	{
		r->_errno = ENODEV;
		return -1;
	}

	if (smb_absolute_path_no_device(name, fixedName, env->pos) == NULL)
	{
		r->_errno = EINVAL;
		return -1;
	}

	s32 ret = 0;
	_SMB_lock(env->pos);
        if(SMB_DiskInformation(buf, env->smbconn) != SMB_SUCCESS)
            ret = -1;
	_SMB_unlock(env->pos);

	return ret;
}

static void MountDevice(const char *name,SMBCONN smbconn, int env)
{
	devoptab_t *dotab_smb;
	char *aux;
	int l;

	aux=strdup(name);
	l=strlen(aux);
	if(aux[l-1]==':')aux[l-1]='\0';

	dotab_smb=(devoptab_t*)malloc(sizeof(devoptab_t));

	dotab_smb->name=strdup(aux);
	dotab_smb->structSize=sizeof(SMBFILESTRUCT); // size of file structure
	dotab_smb->open_r=__smb_open; // device open
	dotab_smb->close_r=__smb_close; // device close
	dotab_smb->write_r=__smb_write; // device write
	dotab_smb->read_r=__smb_read; // device read
	dotab_smb->seek_r=__smb_seek; // device seek
	dotab_smb->fstat_r=__smb_fstat; // device fstat
	dotab_smb->stat_r=__smb_stat; // device stat
	dotab_smb->link_r=NULL; // device link
	dotab_smb->unlink_r=__smb_unlink; // device unlink
	dotab_smb->chdir_r=__smb_chdir; // device chdir
	dotab_smb->rename_r=__smb_rename; // device rename
	dotab_smb->mkdir_r=__smb_mkdir; // device mkdir

	dotab_smb->dirStateSize=sizeof(SMBDIRSTATESTRUCT); // dirStateSize
	dotab_smb->diropen_r=__smb_diropen; // device diropen_r
	dotab_smb->dirreset_r=__smb_dirreset; // device dirreset_r
	dotab_smb->dirnext_r=__smb_dirnext; // device dirnext_r
	dotab_smb->dirclose_r=__smb_dirclose; // device dirclose_r
	dotab_smb->statvfs_r=__smb_statvfs_r;			// device statvfs_r
	dotab_smb->ftruncate_r=NULL;               // device ftruncate_r
	dotab_smb->fsync_r=NULL;           // device fsync_r
	dotab_smb->deviceData=NULL;       	/* Device data */

	AddDevice(dotab_smb);

	SMBEnv[env].pos=env;
	SMBEnv[env].smbconn=smbconn;
	SMBEnv[env].first_item_dir=false;
	SMBEnv[env].diropen_root=false;
	SMBEnv[env].devoptab=dotab_smb;
	SMBEnv[env].SMBCONNECTED=true;
	SMBEnv[env].name=strdup(aux);

	SMBEnableReadAhead(aux,8);

	free(aux);
}

bool smbInitDevice(const char* name, const char *user, const char *password, const char *share, const char *ip)
{
	int i;

	if(!name || strlen(name) > 9)
		return false;

	char devname[10];
	sprintf(devname, "%s:", name);
	if(FindDevice(devname) >= 0)
		return false;

	if(!smbInited)
	{
		for(i=0;i<MAX_SMB_MOUNTED;i++)
		{
			SMBEnv[i].SMBCONNECTED=false;
			SMBEnv[i].currentpath[0]='\\';
			SMBEnv[i].currentpath[1]='\0';
			SMBEnv[i].first_item_dir=false;
			SMBEnv[i].pos=i;
			SMBEnv[i].SMBReadAheadCache=NULL;
			LWP_MutexInit(&SMBEnv[i]._SMB_mutex, false);
		}

		if(cache_thread == LWP_THREAD_NULL)
			if(LWP_CreateThread(&cache_thread, process_cache_thread, NULL, NULL, 0, 64) != 0)
				return false;

		smbInited = true;
	}

	//root connect
	bool ret = true;
	SMBCONN smbconn;
	if(SMB_Connect(&smbconn, user, password, share, ip) != SMB_SUCCESS)
		return false;

	for(i=0;i<MAX_SMB_MOUNTED && SMBEnv[i].SMBCONNECTED;i++);

	if(i==MAX_SMB_MOUNTED)
	{
		SMB_Close(smbconn);
		return false; // all samba connections in use
	}

	SMBEnv[i].SMBCONNECTED=true; // reserved
	MountDevice(name,smbconn,i);
	return ret;
}

bool smbInit(const char *user, const char *password, const char *share, const char *ip)
{
	return smbInitDevice("smb", user, password, share, ip);
}

void smbClose(const char* name)
{
	smb_env *env = FindSMBEnv(name);
	if(env==NULL) return;

	_SMB_lock(env->pos);
	if(env->SMBCONNECTED)
		SMB_Close(env->smbconn);

	char device[11];
	sprintf(device, "%s:", env->name);
	RemoveDevice(device);
	env->SMBCONNECTED=false;
	_SMB_unlock(env->pos);
}

bool smbCheckConnection(const char* name)
{
	char device[10];
	int i;
	bool ret;
	smb_env *env;

	for(i=0; i < 10 && name[i]!='\0' && name[i]!=':'; i++) device[i]=name[i];
	device[i]='\0';

	env=FindSMBEnv(device);
	if(env==NULL) return false;
	_SMB_lock(env->pos);
	ret=(SMB_Reconnect(&env->smbconn,true)==SMB_SUCCESS);
	_SMB_unlock(env->pos);
	return ret;
}

void smbSetSearchFlags(unsigned short flags)
{
	smbFlags = flags;
}
