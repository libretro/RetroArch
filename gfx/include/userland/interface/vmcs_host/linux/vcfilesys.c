/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define VCOS_LOG_CATEGORY (&hostfs_log_cat)

#ifndef  _LARGEFILE_SOURCE
#define  _LARGEFILE_SOURCE
#endif
#ifndef  _LARGEFILE64_SOURCE
#define  _LARGEFILE64_SOURCE
#endif
#define  _FILE_OFFSET_BITS 64    /* So we get lseek and lseek64 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>

#if defined(__GLIBC__) && !defined( __USE_FILE_OFFSET64 )
#error   "__USE_FILE_OFFSET64 isn't defined"
#endif

#include "interface/vcos/vcos.h"

/* Some hackery to prevent a clash with the Linux type of the same name */
#define dirent fs_dirent
#include "vcfilesys_defs.h"
#include "vchost.h"
#undef dirent

#include <dirent.h>

#include "vc_fileservice_defs.h"

VCOS_LOG_CAT_T hostfs_log_cat;

/******************************************************************************
Global data.
******************************************************************************/

/******************************************************************************
Local types and defines.
******************************************************************************/

//#define DEBUG_LEVEL 1
#define DEBUG_MINOR(...) vcos_log_info(__VA_ARGS__)
#define DEBUG_MAJOR(...) vcos_log_warn(__VA_ARGS__)

/* Define a wrapper for the native directory handle which includes the path
 * to that directory (needed to retrieve size and attributes via stat()).
 */

struct fs_dir
{
   DIR *dhandle;
   int pathlen;
   char pathbuf[PATH_MAX];
};

/*
 *  The media player on the Videocore may be asked to open a file on the Host that
 *  is in fact a FIFO.  We need to note when a FIFO has been opened so that we
 *  can fake out some FIFO seeks that the Videocore may perform, hence the following
 *  types and variables.
 */

typedef struct
{
   int is_fifo;            // non-zero if file is a FIFO
   uint64_t read_offset;   // read offset into file
} file_info_t;

#define FILE_INFO_TABLE_CHUNK_LEN   20

/******************************************************************************
Static data.
******************************************************************************/

static file_info_t *p_file_info_table = NULL;
static int file_info_table_len = 0;

/******************************************************************************
Static functions.
******************************************************************************/

static void backslash_to_slash( char *s );

/******************************************************************************
Global functions.
******************************************************************************/

/******************************************************************************
NAME
   vc_hostfs_init

SYNOPSIS
   void vc_hostfs_init(void)

FUNCTION
   Initialises the host to accept requests from Videocore

RETURNS
   void
******************************************************************************/

void vc_hostfs_init(void)
{
   // This hostfs module is not thread safe - it allocaes a block
   // of memory and uses it without any kind of locking.
   //
   // It offers no advantage of stdio, and so most clients should
   // not use it. Arguably FILESYS should use it in order to get
   // the FIFO support.

   const char *thread_name = vcos_thread_get_name(vcos_thread_current());
   if (strcmp(thread_name, "FILESYS") != 0 && strcmp(thread_name, "HFilesys") != 0)
   {
      fprintf(stderr,"%s: vc_hostfs is deprecated. Please use stdio\n",
              vcos_thread_get_name(vcos_thread_current()));
   }

   vcos_log_register("hostfs", &hostfs_log_cat);
   DEBUG_MINOR("init");
   // Allocate memory for the file info table
   p_file_info_table = (file_info_t *)calloc( FILE_INFO_TABLE_CHUNK_LEN, sizeof( file_info_t ) );
   assert( p_file_info_table != NULL );
   if (p_file_info_table)
   {
      file_info_table_len = FILE_INFO_TABLE_CHUNK_LEN;
   }
}

/** Terminate this library. Clean up resources.
  */

void vc_hostfs_exit(void)
{
   vcos_log_unregister(&hostfs_log_cat);
   if (p_file_info_table)
   {
      free(p_file_info_table);
      p_file_info_table = NULL;
   }
}

/******************************************************************************
NAME
   vc_hostfs_close

SYNOPSIS
   int vc_hostfs_close(int fildes)

FUNCTION
   Deallocates the file descriptor to a file.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_hostfs_close(int fildes)
{
   DEBUG_MINOR("vc_hostfs_close(%d)", fildes);
   return close(fildes);
}

/******************************************************************************
NAME
   vc_hostfs_lseek

SYNOPSIS
   long vc_hostfs_lseek(int fildes, long offset, int whence)

FUNCTION
   Sets the file pointer associated with the open file specified by fildes.  If
   the file is a FIFO (Linux does not support seeking on a FIFO) then, for the
   benefit of the Videocore streaming file handlers which do a number of null seeks,
   that is, seeks to the current position, the return value is faked without an
   actual seek being done.

RETURNS
   Successful completion: offset
   Otherwise: -1
******************************************************************************/

long vc_hostfs_lseek(int fildes, long offset, int whence)
{
   return (long) vc_hostfs_lseek64( fildes, (int64_t) offset, whence);
}

/******************************************************************************
NAME
   vc_hostfs_lseek64

SYNOPSIS
   int64_t vc_hostfs_lseek64(int fildes, int64_t offset, int whence)

FUNCTION
   Sets the file pointer associated with the open file specified by fildes.  If
   the file is a FIFO (Linux does not support seeking on a FIFO) then, for the
   benefit of the Videocore streaming file handlers which do a number of null seeks,
   that is, seeks to the current position, the return value is faked without an
   actual seek being done.

RETURNS
   Successful completion: offset
   Otherwise: -1
******************************************************************************/

int64_t vc_hostfs_lseek64(int fildes, int64_t offset, int whence)
{
   DEBUG_MINOR("vc_hostfs_lseek(%d,%" PRId64 ",%d)", fildes, offset, whence);
   if (fildes >= file_info_table_len)
   {
      // File descriptor not in table, so this is an error
      DEBUG_MAJOR("vc_hostfs_lseek: invalid fildes %d", fildes);
      return -1;
   }
   else
   {
      // There is entry in the file info table for this file descriptor, so go
      // ahead and handle the seek
      int64_t read_offset = p_file_info_table[fildes].read_offset;

      if (p_file_info_table[fildes].is_fifo)
      {
         // The Videocore is attempting to seek on a FIFO.  FIFOs don't support seeking
         // but, for the benefit of certain Videocore "streaming" file handlers, we
         // will fake limited FIFO seek functionality by computing where a seek
         // would take us to
         if (whence == SEEK_SET)
         {
            read_offset = offset;
         }
         else if (whence == SEEK_CUR)
         {
            read_offset += offset;
         }
         else
         {
            // seeking to the end of FIFO makes no sense, so this is an error
            DEBUG_MAJOR("vc_hostfs_lseek(%d,%lld,%d): SEEK_END not supported on FIFO", fildes, (long long)offset, whence);
            return -1;
         }
      }
      else
      {
         // File is not a FIFO, so do the seek
         read_offset = lseek64(fildes, offset, whence);
      }
      p_file_info_table[fildes].read_offset = read_offset;
      DEBUG_MINOR("vc_hostfs_lseek returning %" PRId64 ")", read_offset);
      return read_offset;
   }
}

/******************************************************************************
NAME
   vc_hostfs_open

SYNOPSIS
   int vc_hostfs_open(const char *path, int vc_oflag)

FUNCTION
   Establishes a connection between a file and a file descriptor.  For the benefit
   of faking out seeks on a FIFO, we will need to keep track of the read offset for
   all reads, and to facilitate this each opened file is given an entry in a local
   file info table.

RETURNS
   Successful completion: file descriptor
   Otherwise: -1
******************************************************************************/

int vc_hostfs_open(const char *inPath, int vc_oflag)
{
   char *path = strdup( inPath );
   //char *s;
   int flags = 0, ret=errno;
   struct stat fileStat;

   // Replace all '\' with '/'
   backslash_to_slash( path );

#if 0
   s = path + strlen( path );
   if (( s - path ) >= 4 )
   {
      if ( strcasecmp( &s[ -4 ], ".vll" ) == 0 )
      {
         // The Videocore is asking for a .vll file. Since it isn't consistent with
         // the case, we convert .vll files to all lowercase.
          "vc_hostfs_open: '%s'", path ;

         s--;	 // backup to the last character (*s is on the '\0')
         while (( s >= path ) && ( *s != '/' ))
         {
            *s = tolower( *s );
            s--;
         }
      }
   }
#endif
   DEBUG_MINOR("vc_hostfs_open: '%s'", path);

   flags = O_RDONLY;
   if (vc_oflag & VC_O_WRONLY)  flags =  O_WRONLY;
   if (vc_oflag & VC_O_RDWR)    flags =  O_RDWR;
   if (vc_oflag & VC_O_APPEND)  flags |= O_APPEND;
   if (vc_oflag & VC_O_CREAT)   flags |= O_CREAT;
   if (vc_oflag & VC_O_TRUNC)   flags |= O_TRUNC;
   if (vc_oflag & VC_O_EXCL)    flags |= O_EXCL;

   //while (*path == '\\') path++; // do not want initial '\'
   if (flags & O_CREAT)
      ret = open(path, flags, S_IRUSR | S_IWUSR );
   else
      ret = open(path, flags );

   if (ret < 0 )
   {
      DEBUG_MINOR("vc_hostfs_open(%s,%d) = %d", path, vc_oflag, ret);
   }
   else
   {
      DEBUG_MINOR("vc_hostfs_open(%s,%d) = %d", path, vc_oflag, ret);
   }

   // If the file was successfully open then initialize its entry in
   // the file info table.  If necessary, we expand the size of the table
   if (ret >= 0)
   {
      // File was successfully opened
      if (ret >= file_info_table_len)
      {
         file_info_t *p_new_file_info_table = p_file_info_table;
         int new_file_info_table_len = file_info_table_len;

         // try and allocate a bigger buffer for the file info table
         new_file_info_table_len += FILE_INFO_TABLE_CHUNK_LEN;
         p_new_file_info_table = calloc( (size_t)new_file_info_table_len, sizeof( file_info_t ) );
         if (p_new_file_info_table == NULL)
         {
            // calloc failed
            DEBUG_MAJOR("vc_hostfs_open: file_info_table calloc failed");
            assert( 0 );
         }
         else
         {
            // calloc successful, so copy data from previous buffer to new buffer,
            // free previous buffer and update ptr and len info
            memcpy( p_new_file_info_table, p_file_info_table, sizeof( file_info_t ) * file_info_table_len );
            free( p_file_info_table );
            p_file_info_table = p_new_file_info_table;
            file_info_table_len = new_file_info_table_len;
         }
      }
      assert( ret < file_info_table_len );
      {
         // initialize this file's entry in the file info table
         p_file_info_table[ret].is_fifo = 0;
         p_file_info_table[ret].read_offset = 0;
      }

      // Check whether the file is a FIFO.  A FIFO does not support seeking
      // but we will fake, to the extent supported by the buffered file system
      // on the Videocore, limited FIFO seek functionality.  This is for the benefit
      // of certain Videocore "streaming" file handlers.
      if (fstat( ret, &fileStat ) != 0)
      {
         DEBUG_MINOR("vc_hostfs_open: fstat failed: %s", strerror(errno));
      }
      else if (S_ISFIFO( fileStat.st_mode ))
      {
         // file is a FIFO, so note its fildes for future reference
         p_file_info_table[ret].is_fifo = 1;
         DEBUG_MINOR("vc_hostfs_open: file with fildes %d is a FIFO", ret);
      }
   }

   free( path );

   return ret;
}

/******************************************************************************
NAME
   vc_hostfs_read

SYNOPSIS
   int vc_hostfs_read(int fildes, void *buf, unsigned int nbyte)

FUNCTION
   Attempts to read nbyte bytes from the file associated with the file
   descriptor, fildes, into the buffer pointed to by buf.  For the benefit
   of faking out seeks on a FIFO, we keep track of the read offset for all
   reads.

RETURNS
   Successful completion: number of bytes read
   Otherwise: -1
******************************************************************************/

int vc_hostfs_read(int fildes, void *buf, unsigned int nbyte)
{
   if (fildes >= file_info_table_len)
   {
      // File descriptor not in table, so this is an error
      DEBUG_MAJOR("vc_hostfs_read(%d,%p,%u): invalid fildes", fildes, buf, nbyte);
      return -1;
   }
   else
   {
      // There is entry in the file info table for this file descriptor, so go
      // ahead and handle the read
      int ret = (int) read(fildes, buf, nbyte);
      DEBUG_MINOR("vc_hostfs_read(%d,%p,%u) = %d", fildes, buf, nbyte, ret);
      if (ret > 0)
      {
         p_file_info_table[fildes].read_offset += (long) ret;
      }
      return ret;
   }
}

/******************************************************************************
NAME
   vc_hostfs_write

SYNOPSIS
   int vc_hostfs_write(int fildes, const void *buf, unsigned int nbyte)

FUNCTION
   Attempts to write nbyte bytes from the buffer pointed to by buf to file
   associated with the file descriptor, fildes.

RETURNS
   Successful completion: number of bytes written
   Otherwise: -1
******************************************************************************/

int vc_hostfs_write(int fildes, const void *buf, unsigned int nbyte)
{
   int ret = (int) write(fildes, buf, nbyte);
   DEBUG_MINOR("vc_hostfs_write(%d,%p,%u) = %d", fildes, buf, nbyte, ret);
   return ret;
}

/******************************************************************************
NAME
   vc_hostfs_closedir

SYNOPSIS
   int vc_hostfs_closedir(void *dhandle)

FUNCTION
   Ends a directory list iteration.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_hostfs_closedir(void *dhandle)
{
   struct fs_dir *fsdir = (struct fs_dir *)dhandle;
   int ret = -1;

   DEBUG_MINOR( "vc_hostfs_closedir(%p)", dhandle );

   if (dhandle && fsdir->dhandle)
   {
      (void)closedir(fsdir->dhandle);
      fsdir->dhandle = NULL;
      free(fsdir);
      ret = 0;
   }

   return ret;
}

/******************************************************************************
NAME
   vc_hostfs_format

SYNOPSIS
   int vc_hostfs_format(const char *path)

FUNCTION
   Formats the physical file system that contains path.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_hostfs_format(const char *path)
{
   DEBUG_MINOR("vc_hostfs_format: '%s' not implemented", path);
   return -1;
}

/******************************************************************************
NAME
   vc_hostfs_freespace

SYNOPSIS
   int vc_hostfs_freespace(const char *path)

FUNCTION
   Returns the amount of free space on the physical file system that contains
   path.

RETURNS
   Successful completion: free space
   Otherwise: -1
******************************************************************************/

int vc_hostfs_freespace(const char *inPath)
{
   int ret;

   int64_t freeSpace =  vc_hostfs_freespace64( inPath );

   // Saturate return value (need this in case we have a large file system)
   if (freeSpace > (int64_t) INT_MAX)
   {
      ret = INT_MAX;
   }
   else
   {
      ret = (int) freeSpace;
   }

   return ret;
}

/******************************************************************************
NAME
   vc_hostfs_freespace

SYNOPSIS
   int vc_hostfs_freespace(const char *path)

FUNCTION
   Returns the amount of free space on the physical file system that contains
   path.

RETURNS
   Successful completion: free space
   Otherwise: -1
******************************************************************************/
int64_t vc_hostfs_freespace64(const char *inPath)
{
   char *path = strdup( inPath );
   int64_t ret;
   struct statfs fsStat;

   // Replace all '\' with '/'
   backslash_to_slash( path );

   ret = (int64_t) statfs( path, &fsStat );

   if (ret == 0)
   {
      ret = fsStat.f_bsize * fsStat.f_bavail;
   }
   else
   {
      ret = -1;
   }

   DEBUG_MINOR( "vc_hostfs_freespace64 for '%s' returning %" PRId64 "", path, ret );

   free( path );
   return ret;
}

/******************************************************************************
NAME
   vc_hostfs_get_attr

SYNOPSIS
   int vc_hostfs_get_attr(const char *path, fattributes_t *attr)

FUNCTION
   Gets the file/directory attributes.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_hostfs_get_attr(const char *path, fattributes_t *attr)
{
    struct stat sb;

    DEBUG_MINOR("vc_hostfs_get_attr: '%s'", path );

    *attr = 0;

    if ( stat( path, &sb ) == 0 )
    {
        if ( S_ISDIR( sb.st_mode ))
        {
            *attr |= ATTR_DIRENT;
        }

        if (( sb.st_mode & S_IWUSR  ) == 0 )
        {
            *attr |= ATTR_RDONLY;
        }

        return 0;
    }
    return -1;
}

/******************************************************************************
NAME
   vc_hostfs_mkdir

SYNOPSIS
   int vc_hostfs_mkdir(const char *path)

FUNCTION
   Creates a new directory named by the pathname pointed to by path.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_hostfs_mkdir(const char *path)
{
    DEBUG_MINOR( "vc_hostfs_mkdir: '%s'",  path );
    if ( mkdir( path, 0777 ) == 0 )
    {
        return 0;
    }
    return -1;
}

/******************************************************************************
NAME
   vc_hostfs_opendir

SYNOPSIS
   void *vc_hostfs_opendir(const char *dirname)

FUNCTION
   Starts a directory list iteration of sub-directories.

RETURNS
   Successful completion: dhandle (pointer)
   Otherwise: NULL
******************************************************************************/

void *vc_hostfs_opendir(const char *dirname)
{
   struct fs_dir *fsdir = NULL;

   DEBUG_MINOR( "vc_hostfs_opendir: '%s'", dirname );

   if (dirname && dirname[0])
   {
      fsdir = (struct fs_dir *)malloc(sizeof(struct fs_dir));

      if (fsdir)
      {
         DIR *dhandle;
         int len = strlen(dirname);

         memcpy(fsdir->pathbuf, dirname, len);

         backslash_to_slash(fsdir->pathbuf);

         /* Remove any trailing slashes */
         while (fsdir->pathbuf[len - 1] == '/')
            len--;

         fsdir->pathbuf[len] = '\0';

         dhandle = opendir(fsdir->pathbuf);
         DEBUG_MINOR( "opendir: '%s' = %p", fsdir->pathbuf, dhandle );

         if (dhandle)
         {
            fsdir->pathlen = len;
            fsdir->dhandle = dhandle;
         }
         else
         {
            free(fsdir);
            fsdir = NULL;
         }
      }
   }

   return fsdir;
}

/******************************************************************************
NAME
   vc_hostfs_readdir_r

SYNOPSIS
   struct dirent *vc_hostfs_readdir_r(void *dhandle, struct dirent *result)

FUNCTION
   Fills in the passed result structure with details of the directory entry
   at the current psition in the directory stream specified by the argument
   dhandle, and positions the directory stream at the next entry. If the last
   sub-directory has been reached it ends the iteration and begins a new one
   for files in the directory.

RETURNS
   Successful completion: result
   End of directory stream: NULL
******************************************************************************/

struct fs_dirent *vc_hostfs_readdir_r(void *dhandle, struct fs_dirent *result)
{
   struct fs_dir *fsdir = (struct fs_dir *)dhandle;

   DEBUG_MINOR( "vc_hostfs_readdir_r(%p)", fsdir );

   if (fsdir && result)
   {
      struct dirent *dent;

      while ((dent = readdir(fsdir->dhandle)) != NULL)
      {
         struct stat statbuf;
         int ret;

         /* Append the filename, and stat the resulting path */
         fsdir->pathbuf[fsdir->pathlen] = '/';
         vcos_safe_strcpy(fsdir->pathbuf, dent->d_name, sizeof(fsdir->pathbuf), fsdir->pathlen + 1);
         ret = stat(fsdir->pathbuf, &statbuf);
         fsdir->pathbuf[fsdir->pathlen] = '\0';

         if (ret == 0)
         {
            vcos_safe_strcpy(result->d_name, dent->d_name, sizeof(result->d_name), 0);
            result->d_size = (statbuf.st_size <= 0xffffffff) ? (unsigned int)statbuf.st_size : 0xffffffff;
            result->d_attrib = ATTR_NORMAL;
            if ((statbuf.st_mode & S_IWUSR) == 0)
               result->d_attrib |= ATTR_RDONLY;
            if (statbuf.st_mode & S_IFDIR)
               result->d_attrib |= ATTR_DIRENT;
            result->d_creatime = statbuf.st_ctime;
            result->d_modtime = statbuf.st_mtime;
            DEBUG_MINOR( "vc_hostfs_readdir_r() = '%s', %x, %x", result->d_name, result->d_size, result->d_attrib );
            break;
         }
      }

      if (!dent)
      {
         DEBUG_MINOR( "vc_hostfs_readdir_r() = NULL" );
         rewinddir(fsdir->dhandle);
         result = NULL;
      }
   }
   else
   {
      result = NULL;
   }

   return result;
}

/******************************************************************************
NAME
   vc_hostfs_remove

SYNOPSIS
   int vc_hostfs_remove(const char *path)

FUNCTION
   Removes a file or a directory. A directory must be empty before it can be
   deleted.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_hostfs_remove(const char *path)
{
    char *pathbuf = strdup(path);
    int ret = -1;

    DEBUG_MINOR( "vc_hostfs_remove: '%s'", path );

    if (pathbuf)
    {
       backslash_to_slash(pathbuf);

       if ( unlink( pathbuf ) == 0 )
          ret = 0;
    }

    free(pathbuf);

    return ret;
}

/******************************************************************************
NAME
   vc_hostfs_rename

SYNOPSIS
   int vc_hostfs_rename(const char *old, const char *new)

FUNCTION
   Changes the name of a file. The old and new pathnames must be on the same
   physical file system.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_hostfs_rename(const char *old, const char *new)
{
    char *oldbuf = strdup(old);
    char *newbuf = strdup(new);
    int ret = -1;

    DEBUG_MINOR( "vc_hostfs_rename: '%s' to '%s'", old, new );

    if (oldbuf && newbuf)
    {
       backslash_to_slash(oldbuf);
       backslash_to_slash(newbuf);

       if ( rename( oldbuf, newbuf ) == 0 )
          ret = 0;
    }

    if (oldbuf)
       free(oldbuf);

    if (newbuf)
       free(newbuf);

    return ret;
}

/******************************************************************************
NAME
   vc_hostfs_set_attr

SYNOPSIS
   int vc_hostfs_set_attr(const char *path, fattributes_t attr)

FUNCTION
   Sets file/directory attributes.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_hostfs_set_attr(const char *path, fattributes_t attr)
{
   char *pathbuf = strdup(path);
   int ret = -1;

   DEBUG_MINOR( "vc_hostfs_set_attr: '%s', %x", path, attr );

   if (pathbuf)
   {
      mode_t mode = 0;
      struct stat sb;

      backslash_to_slash(pathbuf);

      if ( stat( path, &sb ) == 0 )
      {
         mode = sb.st_mode;

         if ( attr & ATTR_RDONLY )
         {
            mode &= ~S_IWUSR;
         }
         else
         {
            mode |= S_IWUSR;
         }

         /* coverity[toctou] Not doing anything security-relevant here,
          * so the race condition is harmless */
         if ( chmod( path, mode ) == 0 )
            ret = 0;
      }
   }

   if (pathbuf)
      free(pathbuf);

   return ret;
}

/******************************************************************************
NAME
   vc_hostfs_setend

SYNOPSIS
   int vc_hostfs_setend(int fildes)

FUNCTION
   Truncates file at current position.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_hostfs_setend(int filedes)
{
    off_t   currPosn;

    if (( currPosn = lseek( filedes, 0, SEEK_CUR )) != (off_t)-1 )
    {
        if ( ftruncate( filedes, currPosn ) == 0 )
        {
            return 0;
        }
    }
   return -1;
}

/******************************************************************************
NAME
   vc_hostfs_totalspace64

SYNOPSIS
   int64_t vc_hostfs_totalspace64(const char *path)

FUNCTION
   Returns the total amount of space on the physical file system that contains
   path.

RETURNS
   Successful completion: total space
   Otherwise: -1
******************************************************************************/

int64_t vc_hostfs_totalspace64(const char *inPath)
{
   char *path = strdup( inPath );
   int64_t ret = -1;
   struct statfs fsStat;

   // Replace all '\' with '/'
   if (path)
   {
      backslash_to_slash( path );

      ret = statfs( path, &fsStat );

      if (ret == 0)
      {
         ret = fsStat.f_bsize * fsStat.f_blocks;
      }
      else
      {
         ret = -1;
      }
   }

   DEBUG_MINOR( "vc_hostfs_totalspace for '%s' returning %" PRId64 "", path, ret );

   if (path)
      free( path );
   return ret;
}

/******************************************************************************
NAME
   vc_hostfs_totalspace

SYNOPSIS
   int vc_hostfs_totalspace(const char *path)

FUNCTION
   Returns the total amount of space on the physical file system that contains
   path.

RETURNS
   Successful completion: total space
   Otherwise: -1
******************************************************************************/

int vc_hostfs_totalspace(const char *inPath)
{
   int ret;
   int64_t totalSpace = vc_hostfs_totalspace64(inPath);

   // Saturate return value (need this in case we have a large file system)
   if (totalSpace > (int64_t) INT_MAX)
   {
      ret = INT_MAX;
   }
   else
   {
      ret = (int) totalSpace;
   }
   return ret;
}

/******************************************************************************
NAME
   backslash_to_slash

SYNOPSIS
   void backslash_to_slash( char *s )

FUNCTION
   Convert all '\' in a string to '/'.

RETURNS
   None.
******************************************************************************/

static void backslash_to_slash( char *s )
{
   while ( *s != '\0' )
   {
       if ( *s == '\\' )
       {
           *s = '/';
       }
       s++;
   }
}

/******************************************************************************
NAME
   vc_hostfs_scandisk

SYNOPSIS
   void vc_hostfs_scandisk(const char *path)

FUNCTION
   Invalidates any cluster chains in the FAT that are not referenced
   in any directory structures

RETURNS
   Void
******************************************************************************/

void vc_hostfs_scandisk(const char *path)
{
   (void)path;

   // not yet implemented
}

/******************************************************************************
NAME
   vc_hostfs_chkdsk

SYNOPSIS
   int vc_hostfs_chkdsk(const char *path, int fix_errors)

FUNCTION
   Checks whether or not a FAT filesystem is corrupt or not. If fix_errors
   is TRUE behaves exactly as vc_filesys_scandisk.

RETURNS
   Successful completion: 0
   Otherwise: indicates failure
******************************************************************************/

int vc_hostfs_chkdsk(const char *path, int fix_errors)
{
   (void)path;
   (void)fix_errors;
   return 0;
}
