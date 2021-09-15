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
#include <windows.storage.streams.h>
#include <robuffer.h>
#include <collection.h>
#include <functional>
#include <fileapifromapp.h>
#include <AclAPI.h>

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::Storage::FileProperties;

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
#include <uwp/uwp_file_handle_access.h>
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

namespace
{
   /* Damn you, UWP, why no functions for that either */
   template<typename T>
   concurrency::task<T^> GetItemFromPathAsync(Platform::String^ path)
   {
      static_assert(false, "StorageFile and StorageFolder only");
   }
   template<>
   concurrency::task<StorageFile^> GetItemFromPathAsync(Platform::String^ path)
   {
      return concurrency::create_task(StorageFile::GetFileFromPathAsync(path));
   }
   template<>
   concurrency::task<StorageFolder^> GetItemFromPathAsync(Platform::String^ path)
   {
      return concurrency::create_task(StorageFolder::GetFolderFromPathAsync(path));
   }

   template<typename T>
   concurrency::task<T^> GetItemInFolderFromPathAsync(StorageFolder^ folder, Platform::String^ path)
   {
      static_assert(false, "StorageFile and StorageFolder only");
   }
   template<>
   concurrency::task<StorageFile^> GetItemInFolderFromPathAsync(StorageFolder^ folder, Platform::String^ path)
   {
      if (path->IsEmpty())
         retro_assert(false); /* Attempt to read a folder as a file - this really should have been caught earlier */
      return concurrency::create_task(folder->GetFileAsync(path));
   }
   template<>
   concurrency::task<StorageFolder^> GetItemInFolderFromPathAsync(StorageFolder^ folder, Platform::String^ path)
   {
      if (path->IsEmpty())
         return concurrency::create_task(concurrency::create_async([folder]() { return folder; }));
      return concurrency::create_task(folder->GetFolderAsync(path));
   }
}

namespace
{
   /* A list of all StorageFolder objects returned using from the file picker */
   Platform::Collections::Vector<StorageFolder^> accessible_directories;

   concurrency::task<Platform::String^> TriggerPickerAddDialog()
   {
      auto folderPicker = ref new Windows::Storage::Pickers::FolderPicker();
      folderPicker->SuggestedStartLocation = Windows::Storage::Pickers::PickerLocationId::Desktop;
      folderPicker->FileTypeFilter->Append("*");

      return concurrency::create_task(folderPicker->PickSingleFolderAsync()).then([](StorageFolder^ folder) {
         if (folder == nullptr)
            throw ref new Platform::Exception(E_ABORT, L"Operation cancelled by user");

         /* TODO: check for duplicates */
         accessible_directories.Append(folder);
         return folder->Path;
      });
   }

   template<typename T>
   concurrency::task<T^> LocateStorageItem(Platform::String^ path)
   {
      /* Look for a matching directory we can use */
      for each (StorageFolder^ folder in accessible_directories)
      {
         std::wstring file_path;
         std::wstring folder_path = folder->Path->Data();
         size_t folder_path_size  = folder_path.size();
         /* Could be C:\ or C:\Users\somebody - remove the trailing slash to unify them */
         if (folder_path[folder_path_size - 1] == '\\')
            folder_path.erase(folder_path_size - 1);

         file_path = path->Data();

         if (file_path.find(folder_path) == 0)
         {
            /* Found a match */
            file_path = file_path.length() > folder_path.length() 
               ? file_path.substr(folder_path.length() + 1) 
               : L"";
            return concurrency::create_task(GetItemInFolderFromPathAsync<T>(folder, ref new Platform::String(file_path.data())));
         }
      }

      /* No matches - try accessing directly, and fallback to user prompt */
      return concurrency::create_task(GetItemFromPathAsync<T>(path)).then([&](concurrency::task<T^> item) {
         try
         {
            T^ storageItem = item.get();
            return concurrency::create_task(concurrency::create_async([storageItem]() { return storageItem; }));
         }
         catch (Platform::AccessDeniedException^ e)
         {
            //for some reason the path is inaccessible from within here???
            Windows::UI::Popups::MessageDialog^ dialog = ref new Windows::UI::Popups::MessageDialog("Path is not currently accessible. Please open any containing directory to access it.");
            dialog->Commands->Append(ref new Windows::UI::Popups::UICommand("Open file picker"));
            dialog->Commands->Append(ref new Windows::UI::Popups::UICommand("Cancel"));
            return concurrency::create_task(dialog->ShowAsync()).then([path](Windows::UI::Popups::IUICommand^ cmd) {
               if (cmd->Label == "Open file picker")
               {
                  return TriggerPickerAddDialog().then([path](Platform::String^ added_path) {
                     // Retry
                     return LocateStorageItem<T>(path);
                  });
               }
               else
               {
                  throw ref new Platform::Exception(E_ABORT, L"Operation cancelled by user");
               }
            });
         }
      });
   }

   IStorageItem^ LocateStorageFileOrFolder(Platform::String^ path)
   {
      if (!path || path->IsEmpty())
         return nullptr;

      if (*(path->End() - 1) == '\\')
      {
         /* Ends with a slash, so it's definitely a directory */
         return RunAsyncAndCatchErrors<StorageFolder^>([&]() {
            return concurrency::create_task(LocateStorageItem<StorageFolder>(path));
         }, nullptr);
      }
      else
      {
         /* No final slash - probably a file (since RetroArch usually slash-terminates dirs), but there is still a chance it's a directory */
         IStorageItem^ item;
         item = RunAsyncAndCatchErrors<StorageFile^>([&]() {
            return concurrency::create_task(LocateStorageItem<StorageFile>(path));
         }, nullptr);
         if (!item)
         {
            item = RunAsyncAndCatchErrors<StorageFolder^>([&]() {
               return concurrency::create_task(LocateStorageItem<StorageFolder>(path));
            }, nullptr);
         }
         return item;
      }
   }
}


/* This is some pure magic and I have absolutely no idea how it works */
/* Wraps a raw buffer into a WinRT object */
/* https://stackoverflow.com/questions/10520335/how-to-wrap-a-char-buffer-in-a-winrt-ibuffer-in-c */
class NativeBuffer :
   public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
   ABI::Windows::Storage::Streams::IBuffer,
   Windows::Storage::Streams::IBufferByteAccess>
{
public:
   virtual ~NativeBuffer()
   {
   }

   HRESULT __stdcall RuntimeClassInitialize(
         byte *buffer, uint32_t capacity, uint32_t length)
   {
      m_buffer = buffer;
      m_capacity = capacity;
      m_length = length;
      return S_OK;
   }

   HRESULT __stdcall Buffer(byte **value)
   {
      if (m_buffer == nullptr)
         return E_INVALIDARG;
      *value = m_buffer;
      return S_OK;
   }

   HRESULT __stdcall get_Capacity(uint32_t *value)
   {
      *value = m_capacity;
      return S_OK;
   }

   HRESULT __stdcall get_Length(uint32_t *value)
   {
      *value = m_length;
      return S_OK;
   }

   HRESULT __stdcall put_Length(uint32_t value)
   {
      if (value > m_capacity)
         return E_INVALIDARG;
      m_length = value;
      return S_OK;
   }

private:
   byte *m_buffer;
   uint32_t m_capacity;
   uint32_t m_length;
};

IBuffer^ CreateNativeBuffer(void* buf, uint32_t capacity, uint32_t length)
{
   Microsoft::WRL::ComPtr<NativeBuffer> nativeBuffer;
   Microsoft::WRL::Details::MakeAndInitialize<NativeBuffer>(&nativeBuffer, (byte *)buf, capacity, length);
   auto iinspectable = (IInspectable *)reinterpret_cast<IInspectable *>(nativeBuffer.Get());
   IBuffer ^buffer = reinterpret_cast<IBuffer ^>(iinspectable);
   return buffer;
}

/* Get a Win32 file handle out of IStorageFile */
/* https://stackoverflow.com/questions/42799235/how-can-i-get-a-win32-handle-for-a-storagefile-or-storagefolder-in-uwp */
HRESULT GetHandleFromStorageFile(Windows::Storage::StorageFile^ file, HANDLE* handle, HANDLE_ACCESS_OPTIONS accessMode)
{
   Microsoft::WRL::ComPtr<IUnknown> abiPointer(reinterpret_cast<IUnknown*>(file));
   Microsoft::WRL::ComPtr<IStorageItemHandleAccess> handleAccess;
   if (SUCCEEDED(abiPointer.As(&handleAccess)))
   {
      HANDLE hFile = INVALID_HANDLE_VALUE;

      if (SUCCEEDED(handleAccess->Create(accessMode,
                  HANDLE_SHARING_OPTIONS::HSO_SHARE_READ,
                  HANDLE_OPTIONS::HO_NONE,
                  nullptr,
                  &hFile)))
      {
         *handle = hFile;
         return S_OK;
      }
   }

   return E_FAIL;
}

#ifdef VFS_FRONTEND
struct retro_vfs_file_handle
#else
struct libretro_vfs_implementation_file
#endif
{
   IRandomAccessStream^ fp;
   IBuffer^ bufferp;
   HANDLE file_handle;
   char* buffer;
   char* orig_path;
   size_t buffer_size;
   int buffer_left;
   size_t buffer_fill;
};

libretro_vfs_implementation_file *retro_vfs_file_open_impl(
      const char *path, unsigned mode, unsigned hints)
{
   char dirpath[PATH_MAX_LENGTH];
   char filename[PATH_MAX_LENGTH];
   wchar_t *path_wide;
   wchar_t *dirpath_wide;
   wchar_t *filename_wide;
   Platform::String^ path_str;
   Platform::String^ filename_str;
   Platform::String^ dirpath_str;
   HANDLE file_handle = INVALID_HANDLE_VALUE;
   DWORD desireAccess;
   DWORD creationDisposition;

   if (!path || !*path)
      return NULL;

   /* Something tried to access files from current directory. 
    * This is not allowed on UWP. */
   if (!path_is_absolute(path))
      return NULL;

   /* Trying to open a directory as file?! */
   if (PATH_CHAR_IS_SLASH(path[strlen(path) - 1]))
      return NULL;

   dirpath[0] = filename[0] = '\0';

   path_wide             = utf8_to_utf16_string_alloc(path);
   windowsize_path(path_wide);
   std::wstring temp_path = path_wide;
   while (true) {
       size_t p = temp_path.find(L"\\\\");
       if (p == std::wstring::npos) break;
       temp_path.replace(p, 2, L"\\");
   }
   path_wide = _wcsdup(temp_path.c_str());
   path_str              = ref new Platform::String(path_wide);
   free(path_wide);

   fill_pathname_basedir(dirpath, path, sizeof(dirpath));
   dirpath_wide          = utf8_to_utf16_string_alloc(dirpath);
   windowsize_path(dirpath_wide);
   dirpath_str           = ref new Platform::String(dirpath_wide);
   free(dirpath_wide);

   fill_pathname_base(filename, path, sizeof(filename));
   filename_wide         = utf8_to_utf16_string_alloc(filename);
   filename_str          = ref new Platform::String(filename_wide);
   free(filename_wide);

   retro_assert(!dirpath_str->IsEmpty() && !filename_str->IsEmpty());

   /* Try Win32 first, this should work in AppData */
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
   path_str = "\\\\?\\" + path_str;
   file_handle = CreateFile2FromAppW(path_str->Data(), desireAccess, FILE_SHARE_READ, creationDisposition, NULL);

   if (file_handle != INVALID_HANDLE_VALUE)
   {
      libretro_vfs_implementation_file* stream = (libretro_vfs_implementation_file*)calloc(1, sizeof(*stream));
      if (!stream)
         return (libretro_vfs_implementation_file*)NULL;

      stream->orig_path = strdup(path);
      stream->fp = nullptr;
      stream->file_handle = file_handle;
      stream->buffer_left = 0;
      stream->buffer_fill = 0;
      return stream;
   }
   return NULL;
}

int retro_vfs_file_close_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE))
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
      CloseHandle(stream->file_handle);
   else
   {
      /* Apparently, this is how you close a file in WinRT */
      /* Yes, really */
      stream->fp = nullptr;
      free(stream->buffer);
   }

   return 0;
}

int retro_vfs_file_error_impl(libretro_vfs_implementation_file *stream)
{
   return false; /* TODO */
}

int64_t retro_vfs_file_size_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE))
      return 0;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      LARGE_INTEGER sz;
      if (GetFileSizeEx(stream->file_handle, &sz))
         return sz.QuadPart;
      return 0;
   }

   return stream->fp->Size;
}

int64_t retro_vfs_file_truncate_impl(libretro_vfs_implementation_file *stream, int64_t length)
{
   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE))
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      int64_t p = retro_vfs_file_tell_impl(stream);
      retro_vfs_file_seek_impl(stream, length, RETRO_VFS_SEEK_POSITION_START);
      SetEndOfFile(stream->file_handle);

      if (p < length)
         retro_vfs_file_seek_impl(stream, p, RETRO_VFS_SEEK_POSITION_START);
   }
   else
      stream->fp->Size = length;
   
   return 0;
}

int64_t retro_vfs_file_tell_impl(libretro_vfs_implementation_file *stream)
{
   LARGE_INTEGER _offset;
   LARGE_INTEGER out;
   _offset.QuadPart = 0;

   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE))
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      SetFilePointerEx(stream->file_handle, _offset, &out, FILE_CURRENT);
      return out.QuadPart;
   }

   return stream->fp->Position - stream->buffer_left;
}

int64_t retro_vfs_file_seek_impl(
      libretro_vfs_implementation_file *stream,
      int64_t offset, int seek_position)
{
   LARGE_INTEGER _offset;
   _offset.QuadPart = offset;

   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE))
      return -1;

   switch (seek_position)
   {
      case RETRO_VFS_SEEK_POSITION_START:
         if (stream->file_handle != INVALID_HANDLE_VALUE)
            SetFilePointerEx(stream->file_handle, _offset, NULL, FILE_BEGIN);
         else
            stream->fp->Seek(offset);
         break;

      case RETRO_VFS_SEEK_POSITION_CURRENT:
         if (stream->file_handle != INVALID_HANDLE_VALUE)
            SetFilePointerEx(stream->file_handle, _offset, NULL, FILE_CURRENT);
         else
            stream->fp->Seek(retro_vfs_file_tell_impl(stream) + offset);
         break;

      case RETRO_VFS_SEEK_POSITION_END:
         if (stream->file_handle != INVALID_HANDLE_VALUE)
            SetFilePointerEx(stream->file_handle, _offset, NULL, FILE_END);
         else
            stream->fp->Seek(stream->fp->Size - offset);
         break;
   }

   /* For simplicity always flush the buffer on seek */
   stream->buffer_left = 0;

   return 0;
}

int64_t retro_vfs_file_read_impl(
      libretro_vfs_implementation_file *stream, void *s, uint64_t len)
{
   int64_t ret;
   int64_t bytes_read = 0;
   IBuffer^ buffer;

   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE) || !s)
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      DWORD _bytes_read;
      ReadFile(stream->file_handle, (char*)s, len, &_bytes_read, NULL);
      return (int64_t)_bytes_read;
   }

   if (len <= stream->buffer_size)
   {
      /* Small read, use manually buffered I/O */
      if (stream->buffer_left < len)
      {
         /* Exhaust the buffer */
         memcpy(s,
               &stream->buffer[stream->buffer_fill - stream->buffer_left],
               stream->buffer_left);
         len                 -= stream->buffer_left;
         bytes_read          += stream->buffer_left;
         stream->buffer_left = 0;

         /* Fill buffer */
         stream->buffer_left = RunAsyncAndCatchErrors<int64_t>([&]() {
               return concurrency::create_task(stream->fp->ReadAsync(stream->bufferp, stream->bufferp->Capacity, InputStreamOptions::None)).then([&](IBuffer^ buf) {
                     retro_assert(stream->bufferp == buf);
                     return (int64_t)stream->bufferp->Length;
                     });
               }, -1);
         stream->buffer_fill = stream->buffer_left;

         if (stream->buffer_left == -1)
         {
            stream->buffer_left = 0;
            stream->buffer_fill = 0;
            return -1;
         }

         if (stream->buffer_left < len)
         {
            /* EOF */
            memcpy(&((char*)s)[bytes_read],
                  stream->buffer, stream->buffer_left);
            bytes_read += stream->buffer_left;
            stream->buffer_left = 0;
            return bytes_read;
         }

         memcpy(&((char*)s)[bytes_read], stream->buffer, len);
         bytes_read += len;
         stream->buffer_left -= len;
         return bytes_read;
      }

      /* Internal buffer already contains requested amount */
      memcpy(s,
            &stream->buffer[stream->buffer_fill - stream->buffer_left],
            len);
      stream->buffer_left -= len;
      return len;
   }

   /* Big read exceeding buffer size,
    * exhaust small buffer and read rest in one go */
   memcpy(s, &stream->buffer[stream->buffer_fill - stream->buffer_left], stream->buffer_left);
   len                 -= stream->buffer_left;
   bytes_read          += stream->buffer_left;
   stream->buffer_left  = 0;

   buffer               = CreateNativeBuffer(&((char*)s)[bytes_read], len, 0);
   ret                  = RunAsyncAndCatchErrors<int64_t>([&]() {
         return concurrency::create_task(stream->fp->ReadAsync(buffer, buffer->Capacity - bytes_read, InputStreamOptions::None)).then([&](IBuffer^ buf) {
               retro_assert(buf == buffer);
               return (int64_t)buffer->Length;
               });
         }, -1);

   if (ret == -1)
      return -1;
   return bytes_read + ret;
}

int64_t retro_vfs_file_write_impl(
      libretro_vfs_implementation_file *stream, const void *s, uint64_t len)
{
   IBuffer^ buffer;
   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE) || !s)
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      DWORD bytes_written;
      WriteFile(stream->file_handle, s, len, &bytes_written, NULL);
      return (int64_t)bytes_written;
   }

   /* const_cast to remove const modifier is undefined behaviour, but the buffer is only read, should be safe */
   buffer = CreateNativeBuffer(const_cast<void*>(s), len, len);
   return RunAsyncAndCatchErrors<int64_t>([&]() {
         return concurrency::create_task(stream->fp->WriteAsync(buffer)).then([&](unsigned int written) {
               return (int64_t)written;
               });
         }, -1);
}

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE) || !stream->fp)
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      FlushFileBuffers(stream->file_handle);
      return 0;
   }

   return RunAsyncAndCatchErrors<int>([&]() {
         return concurrency::create_task(stream->fp->FlushAsync()).then([&](bool this_value_is_not_even_documented_wtf) {
               /* The bool value may be reporting an error or something, but just leave it alone for now */
               /* https://github.com/MicrosoftDocs/winrt-api/issues/841 */
               return 0;
               });
         }, -1);
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

/* TODO: this may not work if trying to move a directory */
/*int retro_vfs_file_rename_impl(const char* old_path, const char* new_path)
{
   char new_file_name[PATH_MAX_LENGTH];
   char new_dir_path[PATH_MAX_LENGTH];
   wchar_t *new_file_name_wide;
   wchar_t *old_path_wide, *new_dir_path_wide;
   Platform::String^ old_path_str;
   Platform::String^ new_dir_path_str;
   Platform::String^ new_file_name_str;

   if (!old_path || !*old_path || !new_path || !*new_path)
      return -1;

   new_file_name[0] = '\0';
   new_dir_path [0] = '\0';

   old_path_wide = utf8_to_utf16_string_alloc(old_path);
   old_path_str  = ref new Platform::String(old_path_wide);
   free(old_path_wide);

   fill_pathname_basedir(new_dir_path, new_path, sizeof(new_dir_path));
   new_dir_path_wide = utf8_to_utf16_string_alloc(new_dir_path);
   windowsize_path(new_dir_path_wide);
   new_dir_path_str  = ref new Platform::String(new_dir_path_wide);
   free(new_dir_path_wide);

   fill_pathname_base(new_file_name, new_path, sizeof(new_file_name));
   new_file_name_wide = utf8_to_utf16_string_alloc(new_file_name);
   new_file_name_str  = ref new Platform::String(new_file_name_wide);
   free(new_file_name_wide);

   retro_assert(!old_path_str->IsEmpty() && !new_dir_path_str->IsEmpty() && !new_file_name_str->IsEmpty());

   return RunAsyncAndCatchErrors<int>([&]() {
      concurrency::task<StorageFile^> old_file_task = concurrency::create_task(LocateStorageItem<StorageFile>(old_path_str));
      concurrency::task<StorageFolder^> new_dir_task = concurrency::create_task(LocateStorageItem<StorageFolder>(new_dir_path_str));
      return concurrency::create_task([&] {
         // Run these two tasks in parallel
         // TODO: There may be some cleaner way to express this
         concurrency::task_group group;
         group.run([&] { return old_file_task; });
         group.run([&] { return new_dir_task; });
         group.wait();
      }).then([&]() {
         return old_file_task.get()->MoveAsync(new_dir_task.get(), new_file_name_str, NameCollisionOption::ReplaceExisting);
      }).then([&]() {
         return 0;
      });
   }, -1);
}*/

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

char* uwp_trigger_picker(void)
{
   return RunAsyncAndCatchErrors<char*>([&]() {
      return TriggerPickerAddDialog().then([](Platform::String^ path) {
         return utf16_to_utf8_string_alloc(path->Data());
      });
   }, NULL);
}
