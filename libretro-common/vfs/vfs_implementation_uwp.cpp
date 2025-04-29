/* Copyright  (C) 2018-2020 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (vfs_implementation_uwp.cpp).
* ---------------------------------------------------------------------------------------
*
* Permission is hereby granted, free of charge,
* to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <retro_environment.h>

#include <ppl.h>
#include <ppltasks.h>
#include <stdio.h>
#include <wrl.h>
#include <wrl/implements.h>
#include <robuffer.h>
#include <collection.h>
#include <functional>
#include <fileapifromapp.h>
#include <AclAPI.h>
#include <sddl.h>
#include <io.h>
#include <fcntl.h>

#ifdef RARCH_INTERNAL
#ifndef VFS_FRONTEND
#define VFS_FRONTEND
#endif
#endif

#include <vfs/vfs.h>
#include <vfs/vfs_implementation.h>
#include <libretro.h>
#include <encodings/utf.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <retro_environment.h>
#include <uwp/uwp_async.h>
#include <uwp/std_filesystem_compat.h>

namespace
{
   /* UWP deals with paths containing / instead of
    * \ way worse than normal Windows */
   /* and RetroArch may sometimes mix them
    * (e.g. on archive extraction) */
   static void windowsize_path(wchar_t* path)
   {
      if (path)
      {
         while (*path)
         {
            if (*path == '/')
               *path = '\\';
            ++path;
         }
      }
   }
}

#ifdef VFS_FRONTEND
struct retro_vfs_file_handle
#else
struct libretro_vfs_implementation_file
#endif
{
    int64_t size;
    uint64_t mappos;
    uint64_t mapsize;
    FILE* fp;
    HANDLE fh;
    char* buf;
    char* orig_path;
    uint8_t* mapped;
    int fd;
    unsigned hints;
    enum vfs_scheme scheme;
};

#define RFILE_HINT_UNBUFFERED (1 << 8)

int retro_vfs_file_close_impl(libretro_vfs_implementation_file* stream)
{
    if (!stream)
        return -1;

    if (stream->fp)
        fclose(stream->fp);

    if (stream->buf != NULL)
    {
        free(stream->buf);
        stream->buf = NULL;
    }
    if (stream->orig_path)
        free(stream->orig_path);

    free(stream);

    return 0;
}

int retro_vfs_file_error_impl(libretro_vfs_implementation_file* stream)
{
    return ferror(stream->fp);
}

int64_t retro_vfs_file_size_impl(libretro_vfs_implementation_file* stream)
{
    if (stream)
        return stream->size;
    return 0;
}

int64_t retro_vfs_file_truncate_impl(libretro_vfs_implementation_file* stream, int64_t length)
{
   if (stream && _chsize(_fileno(stream->fp), length) == 0)
      return 0;
   return -1;
}

int64_t retro_vfs_file_tell_impl(libretro_vfs_implementation_file* stream)
{
    if (!stream)
        return -1;

    if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
        return _ftelli64(stream->fp);
    if (lseek(stream->fd, 0, SEEK_CUR) < 0)
        return -1;

    return 0;
}

int64_t retro_vfs_file_seek_internal(
    libretro_vfs_implementation_file* stream,
    int64_t offset, int whence)
{
    if (!stream)
        return -1;

    if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
        return _fseeki64(stream->fp, offset, whence);
    if (lseek(stream->fd, (off_t)offset, whence) < 0)
        return -1;

    return 0;
}

int64_t retro_vfs_file_seek_impl(libretro_vfs_implementation_file* stream,
    int64_t offset, int seek_position)
{
    return retro_vfs_file_seek_internal(stream, offset, seek_position);
}

int64_t retro_vfs_file_read_impl(libretro_vfs_implementation_file* stream,
    void* s, uint64_t len)
{
    if (!stream || (!stream->fp && stream->fh == INVALID_HANDLE_VALUE) || !s)
      return -1;

   if (stream->fh != INVALID_HANDLE_VALUE)
   {
      DWORD _bytes_read;
      ReadFile(stream->fh, (char*)s, len, &_bytes_read, NULL);
      return (int64_t)_bytes_read;
   }

    if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
       return fread(s, 1, (size_t)len, stream->fp);
    return read(stream->fd, s, (size_t)len);
}


int64_t retro_vfs_file_write_impl(libretro_vfs_implementation_file* stream, const void* s, uint64_t len)
{
    if (!stream || (!stream->fp && stream->fh == INVALID_HANDLE_VALUE) || !s)
        return -1;

    if (stream->fh != INVALID_HANDLE_VALUE)
    {
        DWORD bytes_written;
        WriteFile(stream->fh, s, len, &bytes_written, NULL);
        return (int64_t)bytes_written;
    }

    if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
        return fwrite(s, 1, (size_t)len, stream->fp);

    return write(stream->fd, s, (size_t)len);
}

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file* stream)
{
    if (stream && fflush(stream->fp) == 0)
       return 0;
    return -1;
}

int retro_vfs_file_remove_impl(const char *path)
{
   BOOL result;
   wchar_t *path_wide;

   if (!path || !*path)
      return -1;

   path_wide = utf8_to_utf16_string_alloc(path);
   windowsize_path(path_wide);

   /* Try Win32 first, this should work in AppData */
   result = DeleteFileFromAppW(path_wide);
   free(path_wide);
   if (result)
      return 0;

   return -1;
}

libretro_vfs_implementation_file* retro_vfs_file_open_impl(
    const char* path, unsigned mode, unsigned hints)
{
    HANDLE file_handle;
    std::wstring path_wstring;
    DWORD desireAccess;
    DWORD creationDisposition;
    wchar_t                       *path_wide = NULL;
    int                                flags = 0;
    const char                     *mode_str = NULL;
    libretro_vfs_implementation_file* stream =
        (libretro_vfs_implementation_file*)
        malloc(sizeof(*stream));

    if (!stream)
        return NULL;

    stream->fd        = 0;
    stream->hints     = hints;
    stream->size      = 0;
    stream->buf       = NULL;
    stream->fp        = NULL;
    stream->fh        = 0;
    stream->orig_path = NULL;
    stream->mappos    = 0;
    stream->mapsize   = 0;
    stream->mapped    = NULL;
    stream->scheme    = VFS_SCHEME_NONE;

#ifdef VFS_FRONTEND
   if (     path
         && path[0] == 'v'
         && path[1] == 'f'
         && path[2] == 's'
         && path[3] == 'o'
         && path[4] == 'n'
         && path[5] == 'l'
         && path[6] == 'y'
         && path[7] == ':'
         && path[8] == '/'
         && path[9] == '/')
         path             += sizeof("vfsonly://")-1;
#endif

    path_wide    = utf8_to_utf16_string_alloc(path);
    windowsize_path(path_wide);
    path_wstring = path_wide;
    free(path_wide);

    for (;;)
    {
       size_t p = path_wstring.find(L"\\\\");
       if (p == std::wstring::npos)
          break;
       path_wstring.replace(p, 2, L"\\");
    }

    path_wstring      = L"\\\\?\\" + path_wstring;
    stream->orig_path = strdup(path);

    stream->hints    &= ~RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS;

    switch (mode)
    {
       case RETRO_VFS_FILE_ACCESS_READ:
          mode_str = "rb";
          flags    = O_RDONLY | O_BINARY;
          break;

       case RETRO_VFS_FILE_ACCESS_WRITE:
          mode_str = "wb";
          flags    = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
          break;

       case RETRO_VFS_FILE_ACCESS_READ_WRITE:
          mode_str = "w+b";
          flags    = O_RDWR | O_CREAT | O_TRUNC | O_BINARY;
          break;

       case RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
       case RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
          mode_str = "r+b";
          flags    = O_RDWR | O_BINARY;
          break;

       default:
          goto error;
    }

    switch (mode)
    {
       case RETRO_VFS_FILE_ACCESS_READ_WRITE:
          desireAccess = GENERIC_READ | GENERIC_WRITE;
          break;
       case RETRO_VFS_FILE_ACCESS_WRITE:
          desireAccess = GENERIC_WRITE;
          break;
       case RETRO_VFS_FILE_ACCESS_READ:
          desireAccess = GENERIC_READ;
          break;
    }
    if (mode == RETRO_VFS_FILE_ACCESS_READ)
        creationDisposition = OPEN_EXISTING;
    else
        creationDisposition = (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) != 0
           ? OPEN_ALWAYS
           : CREATE_ALWAYS;

    if ((file_handle = CreateFile2FromAppW(path_wstring.data(), desireAccess,
                FILE_SHARE_READ, creationDisposition, NULL)) == INVALID_HANDLE_VALUE)
       goto error;

    stream->fh      = file_handle;
    if ((stream->fd = _open_osfhandle((uint64)stream->fh, flags)) == -1)
        goto error;

    {
        FILE *fp = _fdopen(stream->fd, mode_str);

        if (!fp)
            goto error;
        stream->fp = fp;
    }

    /* Regarding setvbuf:
        *
        * https://www.freebsd.org/cgi/man.cgi?query=setvbuf&apropos=0&sektion=0&manpath=FreeBSD+11.1-RELEASE&arch=default&format=html
        *
        * If the size argument is not zero but buf is NULL,
        * a buffer of the given size will be allocated immediately, and
        * released on close. This is an extension to ANSI C.
        *
        * Since C89 does not support specifying a NULL buffer
        * with a non-zero size, we create and track our own buffer for it.
        */
        /* TODO: this is only useful for a few platforms,
        * find which and add ifdef */
    if (stream->scheme != VFS_SCHEME_CDROM)
    {
        stream->buf = (char*)calloc(1, 0x4000);
        if (stream->fp)
            setvbuf(stream->fp, stream->buf, _IOFBF, 0x4000);
    }

    retro_vfs_file_seek_internal(stream, 0, SEEK_SET);
    retro_vfs_file_seek_internal(stream, 0, SEEK_END);

    stream->size = retro_vfs_file_tell_impl(stream);

    retro_vfs_file_seek_internal(stream, 0, SEEK_SET);

    return stream;

error:
    retro_vfs_file_close_impl(stream);
    return NULL;
}

static int uwp_mkdir_impl(std::experimental::filesystem::path dir)
{
    /*I feel like this should create the directory recursively but the existing implementation does not so this update won't
     *I put in the work but I just commented out the stuff you would need */
    WIN32_FILE_ATTRIBUTE_DATA lpFileInfo;
    bool parent_dir_exists = false;

    if (dir.empty())
        return -1;

    /* Check if file attributes can be gotten successfully  */
    if (GetFileAttributesExFromAppW(dir.parent_path().wstring().c_str(), GetFileExInfoStandard, &lpFileInfo))
    {
        /* Check that the files attributes are not null or empty */
        if (lpFileInfo.dwFileAttributes != INVALID_FILE_ATTRIBUTES && lpFileInfo.dwFileAttributes != 0)
        {
            if (lpFileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                parent_dir_exists = true;
        }
    }
    if (!parent_dir_exists)
    {
        /* Try to create parent dir */
        int success = uwp_mkdir_impl(dir.parent_path());
        if (success != 0 && success != -2)
            return success;
    }


    /* Try Win32 first, this should work in AppData */
    if (CreateDirectoryFromAppW(dir.wstring().c_str(), NULL))
        return 0;

    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return -2;

    return -1;
}

int retro_vfs_mkdir_impl(const char* dir)
{
    return uwp_mkdir_impl(std::filesystem::path(dir));
}

/* The first run parameter is used to avoid error checking
 * when doing recursion.
 * Unlike the initial implementation, this can move folders
 * even empty ones when you want to move a directory structure.
 *
 * This will fail even if a single file cannot be moved.
 */
static int uwp_move_path(
      std::filesystem::path old_path,
      std::filesystem::path new_path,
      bool firstrun)
{
    if (old_path.empty() || new_path.empty())
        return -1;

    if (firstrun)
    {
        WIN32_FILE_ATTRIBUTE_DATA lpFileInfo, targetfileinfo;
        bool parent_dir_exists = false;

        /* Make sure that parent path exists */
        if (GetFileAttributesExFromAppW(
                 new_path.parent_path().wstring().c_str(),
                 GetFileExInfoStandard, &lpFileInfo))
        {
            /* Check that the files attributes are not null or empty */
            if (     lpFileInfo.dwFileAttributes
                  != INVALID_FILE_ATTRIBUTES
                  && lpFileInfo.dwFileAttributes != 0)
            {
               /* Parent path doesn't exist, so we gotta create it  */
                if (!(lpFileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    uwp_mkdir_impl(new_path.parent_path());
            }
        }

        /* Make sure that source path exists */
        if (GetFileAttributesExFromAppW(old_path.wstring().c_str(), GetFileExInfoStandard, &lpFileInfo))
        {
            /* Check that the files attributes are not null or empty */
            if (     lpFileInfo.dwFileAttributes != INVALID_FILE_ATTRIBUTES
                  && lpFileInfo.dwFileAttributes != 0)
            {
                /* Check if source path is a dir */
                if (lpFileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                   int result;
                   /* create the target dir */
                   CreateDirectoryFromAppW(new_path.wstring().c_str(), NULL);
                   /* Call move function again but with first run disabled in
                    * order to move the folder */
                   if ((result = uwp_move_path(old_path, new_path, false)) != 0)
                      return result;
                }
                else
                {
                    /* The file that we want to move exists so we can copy it now
                     * check if target file already exists. */
                   if (GetFileAttributesExFromAppW(
                            new_path.wstring().c_str(),
                            GetFileExInfoStandard,
                            &targetfileinfo))
                    {
                        if (
                                 targetfileinfo.dwFileAttributes != INVALID_FILE_ATTRIBUTES
                              && targetfileinfo.dwFileAttributes != 0
                              && (!(targetfileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)))
                        {
                            if (DeleteFileFromAppW(new_path.wstring().c_str()))
                                return -1;
                        }
                    }

                    if (!MoveFileFromAppW(old_path.wstring().c_str(),
                             new_path.wstring().c_str()))
                        return -1;
                    /* Set ACL */
                    uwp_set_acl(new_path.wstring().c_str(), L"S-1-15-2-1");
                }
            }
        }

    }
    else
    {
       HANDLE searchResults;
       WIN32_FIND_DATA findDataResult;
       /* We are bypassing error checking and moving a dir.
        * First we have to get a list of files in the dir. */
       wchar_t* filteredPath = wcsdup(old_path.wstring().c_str());
       wcscat_s(filteredPath, sizeof(L"\\*.*"), L"\\*.*");
       searchResults         = FindFirstFileExFromAppW(
             filteredPath, FindExInfoBasic, &findDataResult,
             FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);

       if (searchResults != INVALID_HANDLE_VALUE)
       {
          bool fail = false;
          do
          {
             if (     wcscmp(findDataResult.cFileName, L".")  != 0
                   && wcscmp(findDataResult.cFileName, L"..") != 0)
             {
                std::filesystem::path temp_old = old_path;
                std::filesystem::path temp_new = new_path;
                temp_old /= findDataResult.cFileName;
                temp_new /= findDataResult.cFileName;
                if (    findDataResult.dwFileAttributes
                      & FILE_ATTRIBUTE_DIRECTORY)
                {
                   CreateDirectoryFromAppW(temp_new.wstring().c_str(), NULL);
                   if (uwp_move_path(temp_old, temp_new, false) != 0)
                      fail = true;
                }
                else
                {
                   WIN32_FILE_ATTRIBUTE_DATA targetfileinfo;
                   /* The file that we want to move exists so we can copy
                    * it now.
                    * Check if target file already exists. */
                   if (GetFileAttributesExFromAppW(temp_new.wstring().c_str(),
                            GetFileExInfoStandard, &targetfileinfo))
                   {
                      if (
                               (targetfileinfo.dwFileAttributes !=
                                INVALID_FILE_ATTRIBUTES)
                            && (targetfileinfo.dwFileAttributes != 0)
                            && (!(targetfileinfo.dwFileAttributes
                                  & FILE_ATTRIBUTE_DIRECTORY)))
                      {
                         if (DeleteFileFromAppW(temp_new.wstring().c_str()))
                            fail = true;
                      }
                   }

                   if (!MoveFileFromAppW(temp_old.wstring().c_str(),
                            temp_new.wstring().c_str()))
                      fail = true;
                   /* Set ACL - this step sucks or at least used to
                    * before I made a whole function
                    * Don't know if we actually "need" to set the ACL
                    * though */
                   uwp_set_acl(temp_new.wstring().c_str(), L"S-1-15-2-1");
                }
             }
          } while (FindNextFile(searchResults, &findDataResult));
          FindClose(searchResults);
          if (fail)
             return -1;
       }
       free(filteredPath);
    }
    return 0;
}

/* C doesn't support default arguments so we wrap it up in a shell to enable
 * us to use default arguments.
 * Default arguments mean that we can do better recursion */
int retro_vfs_file_rename_impl(const char* old_path, const char* new_path)
{
    return uwp_move_path(std::filesystem::path(old_path),
          std::filesystem::path(old_path), true);
}

const char *retro_vfs_file_get_path_impl(libretro_vfs_implementation_file *stream)
{
   /* Should never happen, do something noisy so caller can be fixed */
   if (!stream)
      abort();
   return stream->orig_path;
}

int retro_vfs_stat_impl(const char *path, int32_t *size)
{
   wchar_t *path_wide;
   _WIN32_FILE_ATTRIBUTE_DATA attribdata;

   if (!path || !*path)
      return 0;

   path_wide = utf8_to_utf16_string_alloc(path);
   windowsize_path(path_wide);

   /* Try Win32 first, this should work in AppData */
   if (GetFileAttributesExFromAppW(path_wide,
            GetFileExInfoStandard, &attribdata))
   {
       if (attribdata.dwFileAttributes != INVALID_FILE_ATTRIBUTES)
       {
           if (!(attribdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
           {
               LARGE_INTEGER sz;
               if (size)
               {
                   sz.HighPart = attribdata.nFileSizeHigh;
                   sz.LowPart = attribdata.nFileSizeLow;
                   *size = sz.QuadPart;
               }
           }
           free(path_wide);
           return (attribdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
              ? RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY
              : RETRO_VFS_STAT_IS_VALID;
       }
   }
   free(path_wide);
   return 0;
}

#ifdef VFS_FRONTEND
struct retro_vfs_dir_handle
#else
struct libretro_vfs_implementation_dir
#endif
{
    char* orig_path;
    WIN32_FIND_DATAW entry;
    HANDLE directory;
    bool next;
    char path[PATH_MAX_LENGTH];
};

libretro_vfs_implementation_dir* retro_vfs_opendir_impl(
    const char* name, bool include_hidden)
{
    size_t _len;
    char path_buf[1024];
    wchar_t* path_wide = NULL;
    libretro_vfs_implementation_dir* rdir;

    /* Reject NULL or empty string paths*/
    if (!name || (*name == 0))
        return NULL;

    /*Allocate RDIR struct. Tidied later with retro_closedir*/
    if (!(rdir = (libretro_vfs_implementation_dir*)calloc(1, sizeof(*rdir))))
        return NULL;

    rdir->orig_path = strdup(name);

    _len = strlcpy(path_buf, name, sizeof(path_buf));

    /* Non-NT platforms don't like extra slashes in the path */
    if (path_buf[_len - 1] != '\\')
        path_buf[_len++]   = '\\';

    path_buf[_len]         = '*';
    path_buf[_len + 1]     = '\0';

    path_wide              = utf8_to_utf16_string_alloc(path_buf);
    rdir->directory        = FindFirstFileExFromAppW(
          path_wide, FindExInfoStandard, &rdir->entry,
          FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);

    if (path_wide)
        free(path_wide);

    if (include_hidden)
        rdir->entry.dwFileAttributes |= FILE_ATTRIBUTE_HIDDEN;
    else
        rdir->entry.dwFileAttributes &= ~FILE_ATTRIBUTE_HIDDEN;

    if (rdir->directory && rdir != INVALID_HANDLE_VALUE)
        return rdir;

    retro_vfs_closedir_impl(rdir);
    return NULL;
}

bool retro_vfs_readdir_impl(libretro_vfs_implementation_dir* rdir)
{
    if (rdir->next)
        return (FindNextFileW(rdir->directory, &rdir->entry) != 0);

    rdir->next = true;
    return (rdir->directory != INVALID_HANDLE_VALUE);
}

const char* retro_vfs_dirent_get_name_impl(libretro_vfs_implementation_dir* rdir)
{
    char* name = utf16_to_utf8_string_alloc(rdir->entry.cFileName);
    memset(rdir->entry.cFileName, 0, sizeof(rdir->entry.cFileName));
    strlcpy((char*)rdir->entry.cFileName, name, sizeof(rdir->entry.cFileName));
    if (name)
        free(name);
    return (char*)rdir->entry.cFileName;
}

bool retro_vfs_dirent_is_dir_impl(libretro_vfs_implementation_dir* rdir)
{
    const WIN32_FIND_DATA* entry = (const WIN32_FIND_DATA*)&rdir->entry;
    return entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
}

int retro_vfs_closedir_impl(libretro_vfs_implementation_dir* rdir)
{
    if (!rdir)
        return -1;

    if (rdir->directory != INVALID_HANDLE_VALUE)
        FindClose(rdir->directory);

    if (rdir->orig_path)
        free(rdir->orig_path);
    free(rdir);
    return 0;
}

void uwp_set_acl(const wchar_t* path, const wchar_t* AccessString)
{
    PSECURITY_DESCRIPTOR SecurityDescriptor = NULL;
    EXPLICIT_ACCESSW ExplicitAccess         = { 0 };
    ACL* AccessControlCurrent               = NULL;
    ACL* AccessControlNew                   = NULL;
    SECURITY_INFORMATION SecurityInfo       = DACL_SECURITY_INFORMATION;
    PSID SecurityIdentifier                 = NULL;
    HANDLE original_file                    = CreateFileFromAppW(path,
          GENERIC_READ    | GENERIC_WRITE | WRITE_DAC,
          FILE_SHARE_READ | FILE_SHARE_WRITE,
          NULL, OPEN_EXISTING, 0, NULL);

    if (original_file != INVALID_HANDLE_VALUE)
    {
       if (
             GetSecurityInfo(
                original_file,
                SE_FILE_OBJECT,
                DACL_SECURITY_INFORMATION,
                NULL,
                NULL,
                &AccessControlCurrent,
                NULL,
                &SecurityDescriptor
                ) == ERROR_SUCCESS
          )
       {
          ConvertStringSidToSidW(AccessString, &SecurityIdentifier);
          if (SecurityIdentifier)
          {
             ExplicitAccess.grfAccessPermissions= GENERIC_READ | GENERIC_EXECUTE | GENERIC_WRITE;
             ExplicitAccess.grfAccessMode       = SET_ACCESS;
             ExplicitAccess.grfInheritance      = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
             ExplicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
             ExplicitAccess.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
             ExplicitAccess.Trustee.ptstrName   = reinterpret_cast<wchar_t*>(SecurityIdentifier);

             if (
                   SetEntriesInAclW(
                      1,
                      &ExplicitAccess,
                      AccessControlCurrent,
                      &AccessControlNew
                      ) == ERROR_SUCCESS
                )
                SetSecurityInfo(
                      original_file,
                      SE_FILE_OBJECT,
                      SecurityInfo,
                      NULL,
                      NULL,
                      AccessControlNew,
                      NULL
                      );
          }
       }
       if (SecurityDescriptor)
          LocalFree(reinterpret_cast<HLOCAL>(SecurityDescriptor));
       if (AccessControlNew)
          LocalFree(reinterpret_cast<HLOCAL>(AccessControlNew));
       CloseHandle(original_file);
    }
}
