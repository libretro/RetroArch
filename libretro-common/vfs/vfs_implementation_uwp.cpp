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
#include <retro_assert.h>
#include <string/stdstring.h>
#include <retro_environment.h>
#include <uwp/uwp_async.h>
#include <uwp/std_filesystem_compat.h>

// define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING to silence warnings
// idk why this warning happens considering we can't use the non experimental version but whatever ig

namespace
{
   void windowsize_path(wchar_t* path)
   {
      /* UWP deals with paths containing / instead of 
       * \ way worse than normal Windows */
      /* and RetroArch may sometimes mix them 
       * (e.g. on archive extraction) */
      if (!path)
         return;

      while (*path)
      {
         if (*path == '/')
            *path = '\\';
         ++path;
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

    /*if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
    {
        if (stream->fp)
            fclose(stream->fp);
    }*/

    if (stream->fp)
        fclose(stream->fp);

    /*if (stream->fd > 0)
    {
        fclose(stream->fd);
    }*/
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
    if (!stream)
        return -1;

    if (_chsize(_fileno(stream->fp), length) != 0)
        return -1;

    return 0;
}


int64_t retro_vfs_file_tell_impl(libretro_vfs_implementation_file* stream)
{
    if (!stream || (!stream->fp && stream->fh == INVALID_HANDLE_VALUE))
        return -1;

    if (stream->fh != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER sz;
        if (GetFileSizeEx(stream->fh, &sz))
            return sz.QuadPart;
        return 0;
    }
    if ((stream->hints & RFILE_HINT_UNBUFFERED) == 0)
    {
        return ftell(stream->fp);
    }
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
    {
        return _fseeki64(stream->fp, offset, whence);
    }


    if (lseek(stream->fd, (off_t)offset, whence) < 0)
        return -1;

    return 0;
}

int64_t retro_vfs_file_seek_impl(libretro_vfs_implementation_file* stream,
    int64_t offset, int seek_position)
{
    int whence = -1;
    switch (seek_position)
    {
    case RETRO_VFS_SEEK_POSITION_START:
        whence = SEEK_SET;
        break;
    case RETRO_VFS_SEEK_POSITION_CURRENT:
        whence = SEEK_CUR;
        break;
    case RETRO_VFS_SEEK_POSITION_END:
        whence = SEEK_END;
        break;
    }

    return retro_vfs_file_seek_internal(stream, offset, whence);
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
    {
        return fread(s, 1, (size_t)len, stream->fp);
    }
    DWORD BytesRead;
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
    {
        return fwrite(s, 1, (size_t)len, stream->fp);
    }

    return write(stream->fd, s, (size_t)len);
    //return write(stream->fd, s, (size_t)len);
}

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file* stream)
{
    if (!stream)
        return -1;
    return fflush(stream->fp) == 0 ? 0 : -1;
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
#if defined(VFS_FRONTEND) || defined(HAVE_CDROM)
    int                             path_len = (int)strlen(path);
#endif
#ifdef VFS_FRONTEND
    const char* dumb_prefix = "vfsonly://";
    size_t                   dumb_prefix_siz = STRLEN_CONST("vfsonly://");
    int                      dumb_prefix_len = (int)dumb_prefix_siz;
#endif
    wchar_t* path_wide;
    int                                flags = 0;
    const char* mode_str = NULL;
    libretro_vfs_implementation_file* stream =
        (libretro_vfs_implementation_file*)
        malloc(sizeof(*stream));

    if (!stream)
        return NULL;

    stream->fd = 0;
    stream->hints = hints;
    stream->size = 0;
    stream->buf = NULL;
    stream->fp = NULL;
    stream->fh = 0;
    stream->orig_path = NULL;
    stream->mappos = 0;
    stream->mapsize = 0;
    stream->mapped = NULL;
    stream->scheme = VFS_SCHEME_NONE;

#ifdef VFS_FRONTEND
    if (path_len >= dumb_prefix_len)
        if (!memcmp(path, dumb_prefix, dumb_prefix_len))
            path += dumb_prefix_siz;
#endif

    path_wide = utf8_to_utf16_string_alloc(path);
    windowsize_path(path_wide);
    std::wstring path_wstring = path_wide;
    free(path_wide);
    while (true) {
        size_t p = path_wstring.find(L"\\\\");
        if (p == std::wstring::npos) break;
        path_wstring.replace(p, 2, L"\\");
    }

    path_wstring = L"\\\\?\\" + path_wstring;
    stream->orig_path = strdup(path);

    stream->hints &= ~RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS;

    DWORD desireAccess;
    DWORD creationDisposition;

    switch (mode)
    {
    case RETRO_VFS_FILE_ACCESS_READ:
        mode_str = "rb";
        flags = O_RDONLY | O_BINARY;
        break;

    case RETRO_VFS_FILE_ACCESS_WRITE:
        mode_str = "wb";
        flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
        break;

    case RETRO_VFS_FILE_ACCESS_READ_WRITE:
        mode_str = "w+b";
        flags = O_RDWR | O_CREAT | O_TRUNC | O_BINARY;
        break;

    case RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
    case RETRO_VFS_FILE_ACCESS_READ_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
        mode_str = "r+b";
        flags = O_RDWR | O_BINARY;
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
    {
        creationDisposition = OPEN_EXISTING;
    }
    else
    {
        creationDisposition = (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) != 0 ?
            OPEN_ALWAYS : CREATE_ALWAYS;
    }
    HANDLE file_handle = CreateFile2FromAppW(path_wstring.data(), desireAccess, FILE_SHARE_READ, creationDisposition, NULL);
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        stream->fh = file_handle;
    }
    else
    {
        goto error;
    }
    stream->fd = _open_osfhandle((uint64)stream->fh, flags);
    if (stream->fd == -1)
        goto error;
    else
    {
        FILE* fp;
        fp = _fdopen(stream->fd, mode_str);

        if (!fp)
        {
            int gamingerror = errno;
            goto error;
        }
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


    {
        retro_vfs_file_seek_internal(stream, 0, SEEK_SET);
        retro_vfs_file_seek_internal(stream, 0, SEEK_END);

        stream->size = retro_vfs_file_tell_impl(stream);

        retro_vfs_file_seek_internal(stream, 0, SEEK_SET);
    }
    return stream;

error:
    retro_vfs_file_close_impl(stream);
    return NULL;
}

//this is enables you to copy access permissions from one file/folder to another
//however depending on the target and where the file is being transferred to and from it may not be needed.
//(use disgression)
int uwp_copy_acl(const wchar_t* source, const wchar_t* target)
{
    PSECURITY_DESCRIPTOR sidOwnerDescriptor = nullptr;
    PSECURITY_DESCRIPTOR sidGroupDescriptor = nullptr;
    PSECURITY_DESCRIPTOR daclDescriptor = nullptr;
    PSID sidOwner;
    PSID sidGroup;
    PACL dacl;
    PACL sacl;
    DWORD result;
    HANDLE original_file = CreateFileFromAppW(source, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (original_file != INVALID_HANDLE_VALUE)
    {
        result = GetSecurityInfo(original_file, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, &sidOwner, &sidGroup, &dacl, &sacl, &daclDescriptor);
        if (result != 0)
        {
            LocalFree(daclDescriptor);
            CloseHandle(original_file);
            return result;
        }

        result = GetSecurityInfo(original_file, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &sidOwner, &sidGroup, &dacl, &sacl, &sidOwnerDescriptor);
        if (result != 0)
        {
            LocalFree(sidOwnerDescriptor);
            LocalFree(daclDescriptor);
            CloseHandle(original_file);
            return result;
        }

        result = GetSecurityInfo(original_file, SE_FILE_OBJECT, GROUP_SECURITY_INFORMATION, &sidOwner, &sidGroup, &dacl, &sacl, &sidGroupDescriptor);

        //close file handle regardless of result
        CloseHandle(original_file);

        if (result != 0)
        {
            LocalFree(sidOwnerDescriptor);
            LocalFree(sidGroupDescriptor);
            LocalFree(daclDescriptor);
            CloseHandle(original_file);
            return result;
        }
        CloseHandle(original_file);
    }
    else
    {
        result = GetNamedSecurityInfoW(source, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, &sidOwner, &sidGroup, &dacl, &sacl, &daclDescriptor);
        if (result != 0)
        {
            LocalFree(daclDescriptor);
            return result;
        }
        result = GetNamedSecurityInfoW(source, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &sidOwner, &sidGroup, &dacl, &sacl, &sidOwnerDescriptor);
        if (result != 0)
        {
            LocalFree(sidOwnerDescriptor);
            LocalFree(daclDescriptor);
            return result;
        }
        result = GetNamedSecurityInfoW(source, SE_FILE_OBJECT, GROUP_SECURITY_INFORMATION, &sidOwner, &sidGroup, &dacl, &sacl, &sidGroupDescriptor);
        if (result != 0)
        {
            LocalFree(sidOwnerDescriptor);
            LocalFree(sidGroupDescriptor);
            LocalFree(daclDescriptor);
            return result;
        }
    }
    SECURITY_INFORMATION info = DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION;
    HANDLE target_file = CreateFileFromAppW(target, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (target_file != INVALID_HANDLE_VALUE)
    {
        result = SetSecurityInfo(target_file, SE_FILE_OBJECT, info, sidOwner, sidGroup, dacl, sacl);
        CloseHandle(target_file);
    }
    else
    {
        wchar_t* temp = wcsdup(target);
        result = SetNamedSecurityInfoW(temp, SE_FILE_OBJECT, info, sidOwner, sidGroup, dacl, sacl);
        free(temp);
    }

    if (result != 0)
    {
        LocalFree(sidOwnerDescriptor);
        LocalFree(sidGroupDescriptor);
        LocalFree(daclDescriptor);
        return result;
    }

    if ((sidOwnerDescriptor != nullptr && LocalFree(sidOwnerDescriptor) != nullptr) || (daclDescriptor != nullptr && LocalFree(daclDescriptor) != nullptr) || (daclDescriptor != nullptr && LocalFree(daclDescriptor) != nullptr))
    {
        //an error occured but idk what error code is right so we just return -1
        return -1;
    }

    //woo we made it all the way to the end so we can return success
    return 0;
}

int uwp_mkdir_impl(std::experimental::filesystem::path dir)
{
    //I feel like this should create the directory recursively but the existing implementation does not so this update won't
    //I put in the work but I just commented out the stuff you would need
    WIN32_FILE_ATTRIBUTE_DATA lpFileInfo;
    bool parent_dir_exists = false;

    if (dir.empty())
        return -1;

    //check if file attributes can be gotten successfully 
    if (GetFileAttributesExFromAppW(dir.parent_path().wstring().c_str(), GetFileExInfoStandard, &lpFileInfo))
    {
        //check that the files attributes are not null or empty
        if (lpFileInfo.dwFileAttributes != INVALID_FILE_ATTRIBUTES && lpFileInfo.dwFileAttributes != 0)
        {
            if (lpFileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                parent_dir_exists = true;
            }
        }
    }
    if (!parent_dir_exists)
    {
        //try to create parent dir
        int success = uwp_mkdir_impl(dir.parent_path());
        if (success != 0 && success != -2)
            return success;
    }


    /* Try Win32 first, this should work in AppData */
    bool create_dir = CreateDirectoryFromAppW(dir.wstring().c_str(), NULL);

    if (create_dir)
        return 0;

    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return -2;

    return -1;
}

int retro_vfs_mkdir_impl(const char* dir)
{
    return uwp_mkdir_impl(std::filesystem::path(dir));
}

//the first run paramater is used to avoid error checking when doing recursion
//unlike the initial implementation this can move folders even empty ones when you want to move a directory structure
//this will fail even if a single file cannot be moved
int uwp_move_path(std::filesystem::path old_path, std::filesystem::path new_path,  bool firstrun = true)
{
    if (old_path.empty() || new_path.empty())
        return -1;

    if (firstrun)
    {
        WIN32_FILE_ATTRIBUTE_DATA lpFileInfo, targetfileinfo;
        bool parent_dir_exists = false;


        //make sure that parent path exists
        if (GetFileAttributesExFromAppW(new_path.parent_path().wstring().c_str(), GetFileExInfoStandard, &lpFileInfo))
        {
            //check that the files attributes are not null or empty
            if (lpFileInfo.dwFileAttributes != INVALID_FILE_ATTRIBUTES && lpFileInfo.dwFileAttributes != 0)
            {
                if (!(lpFileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    //parent path doesn't exist ;-; so we gotta create it 
                    uwp_mkdir_impl(new_path.parent_path());
                }
            }
        }

        //make sure that source path exists
        if (GetFileAttributesExFromAppW(old_path.wstring().c_str(), GetFileExInfoStandard, &lpFileInfo))
        {
            //check that the files attributes are not null or empty
            if (lpFileInfo.dwFileAttributes != INVALID_FILE_ATTRIBUTES && lpFileInfo.dwFileAttributes != 0)
            {
                //check if source path is a dir
                if (lpFileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    //create the target dir
                    CreateDirectoryFromAppW(new_path.wstring().c_str(), NULL);
                    //call move function again but with first run disabled in order to move the folder
                    int result = uwp_move_path(old_path, new_path, false);
                    if (result != 0)
                    {
                        //return the error 
                        return result;
                    }
                }
                else
                {
                    //the file that we want to move exists so we can copy it now
                    //check if target file already exists
                    if (GetFileAttributesExFromAppW(new_path.wstring().c_str(), GetFileExInfoStandard, &targetfileinfo))
                    {
                        if (targetfileinfo.dwFileAttributes != INVALID_FILE_ATTRIBUTES && targetfileinfo.dwFileAttributes != 0 && (!(targetfileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)))
                        {
                            //delete target file
                            if (DeleteFileFromAppW(new_path.wstring().c_str()))
                            {
                                //return an error if we can't successfully delete the target file 
                                return -1;
                            }
                        }
                    }

                    //move the file
                    if (!MoveFileFromAppW(old_path.wstring().c_str(), new_path.wstring().c_str()))
                    {
                        //failed to move the file
                        return -1;
                    }
                    //set acl - this step fucking sucks or at least to before I made a whole ass function
                    //idk if we actually "need" to set the acl though
                    if (uwp_copy_acl(new_path.parent_path().wstring().c_str(), new_path.wstring().c_str()) != 0)
                    {
                        //setting acl failed
                        return -1;
                    }
                }
            }
        }

    }
    else
    {
        //we are bypassing error checking and moving a dir
        //first we gotta get a list of files in the dir
        wchar_t* filteredPath = wcsdup(old_path.wstring().c_str());
        wcscat_s(filteredPath, sizeof(L"\\*.*"), L"\\*.*");
        WIN32_FIND_DATA findDataResult;
        HANDLE searchResults = FindFirstFileExFromAppW(filteredPath, FindExInfoBasic, &findDataResult, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (searchResults != INVALID_HANDLE_VALUE)
        {
            bool fail = false;
            do
            {
                if (wcscmp(findDataResult.cFileName, L".") != 0 && wcscmp(findDataResult.cFileName, L"..") != 0)
                {
                    std::filesystem::path temp_old = old_path;
                    std::filesystem::path temp_new = new_path;
                    temp_old /= findDataResult.cFileName;
                    temp_new /= findDataResult.cFileName;
                    if (findDataResult.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        CreateDirectoryFromAppW(temp_new.wstring().c_str(), NULL);
                        int result = uwp_move_path(temp_old, temp_new, false);
                        if (result != 0)
                            fail = true;
                        
                    }
                    else
                    {
                        WIN32_FILE_ATTRIBUTE_DATA targetfileinfo;
                        //the file that we want to move exists so we can copy it now
                        //check if target file already exists
                        if (GetFileAttributesExFromAppW(temp_new.wstring().c_str(), GetFileExInfoStandard, &targetfileinfo))
                        {
                            if (targetfileinfo.dwFileAttributes != INVALID_FILE_ATTRIBUTES && targetfileinfo.dwFileAttributes != 0 && (!(targetfileinfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)))
                            {
                                //delete target file
                                if (DeleteFileFromAppW(temp_new.wstring().c_str()))
                                {
                                    //return an error if we can't successfully delete the target file 
                                    fail = true;
                                }
                            }
                        }

                        //move the file
                        if (!MoveFileFromAppW(temp_old.wstring().c_str(), temp_new.wstring().c_str()))
                        {
                            //failed to move the file
                            fail = true;
                        }
                        //set acl - this step fucking sucks or at least to before I made a whole ass function
                        //idk if we actually "need" to set the acl though
                        if (uwp_copy_acl(new_path.wstring().c_str(), temp_new.wstring().c_str()) != 0)
                        {
                            //setting acl failed
                            fail = true;
                        }
                    }
                }
            } while (FindNextFile(searchResults, &findDataResult));
            FindClose(searchResults);
            if (fail)
                return -1;
        }
        free(filteredPath);
    }
    //yooooooo we finally made it all the way to the end
    //we can now return success
    return 0;
}

//c doesn't support default arguments so we wrap it up in a shell to enable us to use default arguments
//default arguments mean that we can do better recursion
int retro_vfs_file_rename_impl(const char* old_path, const char* new_path)
{
    return uwp_move_path(std::filesystem::path(old_path), std::filesystem::path(old_path));
}

const char *retro_vfs_file_get_path_impl(libretro_vfs_implementation_file *stream)
{
   /* should never happen, do something noisy so caller can be fixed */
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
   if (GetFileAttributesExFromAppW(path_wide, GetFileExInfoStandard, &attribdata))
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
           return (attribdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY : RETRO_VFS_STAT_IS_VALID;
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
    unsigned path_len;
    char path_buf[1024];
    size_t copied = 0;
    wchar_t* path_wide = NULL;
    libretro_vfs_implementation_dir* rdir;

    /*Reject null or empty string paths*/
    if (!name || (*name == 0))
        return NULL;

    /*Allocate RDIR struct. Tidied later with retro_closedir*/
    rdir = (libretro_vfs_implementation_dir*)calloc(1, sizeof(*rdir));
    if (!rdir)
        return NULL;

    rdir->orig_path = strdup(name);

    path_buf[0] = '\0';
    path_len = strlen(name);

    copied = strlcpy(path_buf, name, sizeof(path_buf));

    /* Non-NT platforms don't like extra slashes in the path */
    if (name[path_len - 1] != '\\')
        path_buf[copied++] = '\\';

    path_buf[copied] = '*';
    path_buf[copied + 1] = '\0';

    path_wide = utf8_to_utf16_string_alloc(path_buf);
    rdir->directory = FindFirstFileExFromAppW(path_wide, FindExInfoStandard, &rdir->entry, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

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
