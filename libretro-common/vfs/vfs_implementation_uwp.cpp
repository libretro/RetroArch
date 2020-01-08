/* Copyright  (C) 2018-2019 The RetroArch team
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

namespace
{
	void windowsize_path(wchar_t* path)
	{
		/* UWP deals with paths containing / instead of \ way worse than normal Windows */
		/* and RetroArch may sometimes mix them (e.g. on archive extraction) */
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
			std::wstring folder_path = folder->Path->Data();
			/* Could be C:\ or C:\Users\somebody - remove the trailing slash to unify them */
			if (folder_path[folder_path.size() - 1] == '\\')
				folder_path.erase(folder_path.size() - 1);
			std::wstring file_path = path->Data();
			if (file_path.find(folder_path) == 0)
			{
				/* Found a match */
				file_path = file_path.length() > folder_path.length() ? file_path.substr(folder_path.length() + 1) : L"";
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
				Windows::UI::Popups::MessageDialog^ dialog =
					ref new Windows::UI::Popups::MessageDialog("Path \"" + path + "\" is not currently accessible. Please open any containing directory to access it.");
				dialog->Commands->Append(ref new Windows::UI::Popups::UICommand("Open file picker"));
				dialog->Commands->Append(ref new Windows::UI::Popups::UICommand("Cancel"));
				return concurrency::create_task(dialog->ShowAsync()).then([path](Windows::UI::Popups::IUICommand^ cmd) {
					if (cmd->Label == "Open file picker")
					{
						return TriggerPickerAddDialog().then([path](Platform::String^ added_path) {
							/* Retry */
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

	HRESULT __stdcall RuntimeClassInitialize(byte *buffer, uint32_t capacity, uint32_t length)
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

#ifdef VFS_FRONTEND
struct retro_vfs_file_handle
#else
struct libretro_vfs_implementation_file
#endif
{
	IRandomAccessStream^ fp;
	IBuffer^ bufferp;
	char* buffer;
	char* orig_path;
	size_t buffer_size;
	int buffer_left;
	size_t buffer_fill;
};

libretro_vfs_implementation_file *retro_vfs_file_open_impl(const char *path, unsigned mode, unsigned hints)
{
	if (!path || !*path)
		return NULL;

	if (!path_is_absolute(path))
	{
      /* Something tried to access files from current directory. This is not allowed on UWP. */
		return NULL;
	}

   /* Trying to open a directory as file?! */
	if (path_char_is_slash(path[strlen(path) - 1]))
		return NULL;

	char* dirpath = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
	fill_pathname_basedir(dirpath, path, PATH_MAX_LENGTH);
	wchar_t *dirpath_wide = utf8_to_utf16_string_alloc(dirpath);
	windowsize_path(dirpath_wide);
	Platform::String^ dirpath_str = ref new Platform::String(dirpath_wide);
	free(dirpath_wide);
	free(dirpath);

	char* filename = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
	fill_pathname_base(filename, path, PATH_MAX_LENGTH);
	wchar_t *filename_wide = utf8_to_utf16_string_alloc(filename);
	Platform::String^ filename_str = ref new Platform::String(filename_wide);
	free(filename_wide);
	free(filename);

	retro_assert(!dirpath_str->IsEmpty() && !filename_str->IsEmpty());

	return RunAsyncAndCatchErrors<libretro_vfs_implementation_file*>([&]() {
		return concurrency::create_task(LocateStorageItem<StorageFolder>(dirpath_str)).then([&](StorageFolder^ dir) {
			if (mode == RETRO_VFS_FILE_ACCESS_READ)
				return dir->GetFileAsync(filename_str);
			else
				return dir->CreateFileAsync(filename_str, (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) != 0 ?
					CreationCollisionOption::OpenIfExists : CreationCollisionOption::ReplaceExisting);
		}).then([&](StorageFile^ file) {
			FileAccessMode accessMode = mode == RETRO_VFS_FILE_ACCESS_READ ?
				FileAccessMode::Read : FileAccessMode::ReadWrite;
			return file->OpenAsync(accessMode);
		}).then([&](IRandomAccessStream^ fpstream) {
			libretro_vfs_implementation_file *stream = (libretro_vfs_implementation_file*)calloc(1, sizeof(*stream));
			if (!stream)
				return (libretro_vfs_implementation_file*)NULL;

			stream->orig_path = strdup(path);
			stream->fp = fpstream;
			stream->fp->Seek(0);
			// Preallocate a small buffer for manually buffered IO, makes short read faster
			int buf_size = 8 * 1024;
			stream->buffer = (char*)malloc(buf_size);
			stream->bufferp = CreateNativeBuffer(stream->buffer, buf_size, 0);
			stream->buffer_left = 0;
			stream->buffer_fill = 0;
			stream->buffer_size = buf_size;
			return stream;
		});
	}, NULL);
}

int retro_vfs_file_close_impl(libretro_vfs_implementation_file *stream)
{
	if (!stream || !stream->fp)
		return -1;

	/* Apparently, this is how you close a file in WinRT */
	/* Yes, really */
	stream->fp = nullptr;
	free(stream->buffer);

	return 0;
}

int retro_vfs_file_error_impl(libretro_vfs_implementation_file *stream)
{
	return false; /* TODO */
}

int64_t retro_vfs_file_size_impl(libretro_vfs_implementation_file *stream)
{
	if (!stream || !stream->fp)
		return 0;
	return stream->fp->Size;
}

int64_t retro_vfs_file_truncate_impl(libretro_vfs_implementation_file *stream, int64_t length)
{
	if (!stream || !stream->fp)
		return -1;
	stream->fp->Size = length;
	return 0;
}

int64_t retro_vfs_file_tell_impl(libretro_vfs_implementation_file *stream)
{
	if (!stream || !stream->fp)
		return -1;
	return stream->fp->Position - stream->buffer_left;
}

int64_t retro_vfs_file_seek_impl(libretro_vfs_implementation_file *stream, int64_t offset, int seek_position)
{
	if (!stream || !stream->fp)
		return -1;

	switch (seek_position)
	{
	case RETRO_VFS_SEEK_POSITION_START:
		stream->fp->Seek(offset);
		break;

	case RETRO_VFS_SEEK_POSITION_CURRENT:
		stream->fp->Seek(retro_vfs_file_tell_impl(stream) + offset);
		break;

	case RETRO_VFS_SEEK_POSITION_END:
		stream->fp->Seek(stream->fp->Size - offset);
		break;
	}

	// For simplicity always flush the buffer on seek
	stream->buffer_left = 0;

	return 0;
}

int64_t retro_vfs_file_read_impl(libretro_vfs_implementation_file *stream, void *s, uint64_t len)
{
	if (!stream || !stream->fp || !s)
		return -1;

	int64_t bytes_read = 0;

	if (len <= stream->buffer_size) {
		// Small read, use manually buffered IO
		if (stream->buffer_left < len) {
			// Exhaust the buffer
			memcpy(s, &stream->buffer[stream->buffer_fill - stream->buffer_left], stream->buffer_left);
			len -= stream->buffer_left;
			bytes_read += stream->buffer_left;
			stream->buffer_left = 0;

			// Fill buffer
			stream->buffer_left = RunAsyncAndCatchErrors<int64_t>([&]() {
				return concurrency::create_task(stream->fp->ReadAsync(stream->bufferp, stream->bufferp->Capacity, InputStreamOptions::None)).then([&](IBuffer^ buf) {
					retro_assert(stream->bufferp == buf);
					return (int64_t)stream->bufferp->Length;
				});
			}, -1);
			stream->buffer_fill = stream->buffer_left;

			if (stream->buffer_left == -1) {
				stream->buffer_left = 0;
				stream->buffer_fill = 0;
				return -1;
			}

			if (stream->buffer_left < len) {
				// EOF
				memcpy(&((char*)s)[bytes_read], stream->buffer, stream->buffer_left);
				bytes_read += stream->buffer_left;
				stream->buffer_left = 0;
				return bytes_read;
			}

			memcpy(&((char*)s)[bytes_read], stream->buffer, len);
			bytes_read += len;
			stream->buffer_left -= len;
			return bytes_read;
		}

		// Internal buffer already contains requested amount
		memcpy(s, &stream->buffer[stream->buffer_fill - stream->buffer_left], len);
		stream->buffer_left -= len;
		return len;
	}

	// Big read exceeding buffer size, exhaust small buffer and read rest in one go
	memcpy(s, &stream->buffer[stream->buffer_fill - stream->buffer_left], stream->buffer_left);
	len -= stream->buffer_left;
	bytes_read += stream->buffer_left;
	stream->buffer_left = 0;

	IBuffer^ buffer = CreateNativeBuffer(&((char*)s)[bytes_read], len, 0);

	int64_t ret = RunAsyncAndCatchErrors<int64_t>([&]() {
		return concurrency::create_task(stream->fp->ReadAsync(buffer, buffer->Capacity - bytes_read, InputStreamOptions::None)).then([&](IBuffer^ buf) {
			retro_assert(buf == buffer);
			return (int64_t)buffer->Length;
		});
	}, -1);

	if (ret == -1) {
		return -1;
	}
	return bytes_read + ret;
}

int64_t retro_vfs_file_write_impl(libretro_vfs_implementation_file *stream, const void *s, uint64_t len)
{
	if (!stream || !stream->fp || !s)
		return -1;

	// const_cast to remove const modifier is undefined behaviour, but the buffer is only read, should be safe
	IBuffer^ buffer = CreateNativeBuffer(const_cast<void*>(s), len, len);
	return RunAsyncAndCatchErrors<int64_t>([&]() {
		return concurrency::create_task(stream->fp->WriteAsync(buffer)).then([&](unsigned int written) {
			return (int64_t)written;
		});
	}, -1);
}

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file *stream)
{
	if (!stream || !stream->fp)
		return -1;

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
	if (!path || !*path)
		return -1;

	wchar_t *path_wide = utf8_to_utf16_string_alloc(path);
	windowsize_path(path_wide);
	Platform::String^ path_str = ref new Platform::String(path_wide);
	free(path_wide);

	return RunAsyncAndCatchErrors<int>([&]() {
		return concurrency::create_task(LocateStorageItem<StorageFile>(path_str)).then([&](StorageFile^ file) {
			return file->DeleteAsync(StorageDeleteOption::PermanentDelete);
		}).then([&]() {
			return 0;
		});
	}, -1);
}

/* TODO: this may not work if trying to move a directory */
int retro_vfs_file_rename_impl(const char *old_path, const char *new_path)
{
	if (!old_path || !*old_path || !new_path || !*new_path)
		return -1;

	wchar_t* old_path_wide = utf8_to_utf16_string_alloc(old_path);
	Platform::String^ old_path_str = ref new Platform::String(old_path_wide);
	free(old_path_wide);

	char* new_dir_path = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
	fill_pathname_basedir(new_dir_path, new_path, PATH_MAX_LENGTH);
	wchar_t *new_dir_path_wide = utf8_to_utf16_string_alloc(new_dir_path);
	windowsize_path(new_dir_path_wide);
	Platform::String^ new_dir_path_str = ref new Platform::String(new_dir_path_wide);
	free(new_dir_path_wide);
	free(new_dir_path);

	char* new_file_name = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
	fill_pathname_base(new_file_name, new_path, PATH_MAX_LENGTH);
	wchar_t *new_file_name_wide = utf8_to_utf16_string_alloc(new_file_name);
	Platform::String^ new_file_name_str = ref new Platform::String(new_file_name_wide);
	free(new_file_name_wide);
	free(new_file_name);

	retro_assert(!old_path_str->IsEmpty() && !new_dir_path_str->IsEmpty() && !new_file_name_str->IsEmpty());

	return RunAsyncAndCatchErrors<int>([&]() {
		concurrency::task<StorageFile^> old_file_task = concurrency::create_task(LocateStorageItem<StorageFile>(old_path_str));
		concurrency::task<StorageFolder^> new_dir_task = concurrency::create_task(LocateStorageItem<StorageFolder>(new_dir_path_str));
		return concurrency::create_task([&] {
			/* Run these two tasks in parallel */
			/* TODO: There may be some cleaner way to express this */
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
	if (!path || !*path)
		return 0;

	wchar_t *path_wide = utf8_to_utf16_string_alloc(path);
	windowsize_path(path_wide);
	Platform::String^ path_str = ref new Platform::String(path_wide);
	free(path_wide);

	IStorageItem^ item = LocateStorageFileOrFolder(path_str);
	if (!item)
		return 0;

	return RunAsyncAndCatchErrors<int>([&]() {
		return concurrency::create_task(item->GetBasicPropertiesAsync()).then([&](BasicProperties^ properties) {
			if (size)
				*size = properties->Size;
			return item->IsOfType(StorageItemTypes::Folder) ? RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY : RETRO_VFS_STAT_IS_VALID;
		});
	}, 0);
}

int retro_vfs_mkdir_impl(const char *dir)
{
	if (!dir || !*dir)
		return -1;

	char* dir_local = strdup(dir);
	/* If the path ends with a slash, we have to remove it for basename to work */
	char* tmp = dir_local + strlen(dir_local) - 1;
	if (path_char_is_slash(*tmp))
		*tmp = 0;

	char* dir_name = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
	fill_pathname_base(dir_name, dir_local, PATH_MAX_LENGTH);
	wchar_t *dir_name_wide = utf8_to_utf16_string_alloc(dir_name);
	Platform::String^ dir_name_str = ref new Platform::String(dir_name_wide);
	free(dir_name_wide);
	free(dir_name);

	char* parent_path = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
	fill_pathname_parent_dir(parent_path, dir_local, PATH_MAX_LENGTH);
	wchar_t *parent_path_wide = utf8_to_utf16_string_alloc(parent_path);
	windowsize_path(parent_path_wide);
	Platform::String^ parent_path_str = ref new Platform::String(parent_path_wide);
	free(parent_path_wide);
	free(parent_path);

	retro_assert(!dir_name_str->IsEmpty() && !parent_path_str->IsEmpty());

	free(dir_local);

	return RunAsyncAndCatchErrors<int>([&]() {
		return concurrency::create_task(LocateStorageItem<StorageFolder>(parent_path_str)).then([&](StorageFolder^ parent) {
			return parent->CreateFolderAsync(dir_name_str);
		}).then([&](concurrency::task<StorageFolder^> new_dir) {
			try
			{
				new_dir.get();
			}
			catch (Platform::COMException^ e)
			{
				if (e->HResult == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))
					return -2;
				throw;
			}
			return 0;
		});
	}, -1);
}

#ifdef VFS_FRONTEND
struct retro_vfs_dir_handle
#else
struct libretro_vfs_implementation_dir
#endif
{
	IVectorView<IStorageItem^>^ directory;
	IIterator<IStorageItem^>^ entry;
	char *entry_name;
};

libretro_vfs_implementation_dir *retro_vfs_opendir_impl(const char *name, bool include_hidden)
{
	libretro_vfs_implementation_dir *rdir;

	if (!name || !*name)
		return NULL;

	rdir = (libretro_vfs_implementation_dir*)calloc(1, sizeof(*rdir));
	if (!rdir)
		return NULL;

	wchar_t *name_wide = utf8_to_utf16_string_alloc(name);
	windowsize_path(name_wide);
	Platform::String^ name_str = ref new Platform::String(name_wide);
	free(name_wide);

	rdir->directory = RunAsyncAndCatchErrors<IVectorView<IStorageItem^>^>([&]() {
		return concurrency::create_task(LocateStorageItem<StorageFolder>(name_str)).then([&](StorageFolder^ folder) {
			return folder->GetItemsAsync();
		});
	}, nullptr);

	if (rdir->directory)
		return rdir;

	free(rdir);
	return NULL;
}

bool retro_vfs_readdir_impl(libretro_vfs_implementation_dir *rdir)
{
	if (!rdir->entry)
	{
		rdir->entry = rdir->directory->First();
		return rdir->entry->HasCurrent;
	}
	else
	{
		return rdir->entry->MoveNext();
	}
}

const char *retro_vfs_dirent_get_name_impl(libretro_vfs_implementation_dir *rdir)
{
	if (rdir->entry_name)
		free(rdir->entry_name);
	rdir->entry_name = utf16_to_utf8_string_alloc(rdir->entry->Current->Name->Data());
	return rdir->entry_name;
}

bool retro_vfs_dirent_is_dir_impl(libretro_vfs_implementation_dir *rdir)
{
	return rdir->entry->Current->IsOfType(StorageItemTypes::Folder);
}

int retro_vfs_closedir_impl(libretro_vfs_implementation_dir *rdir)
{
	if (!rdir)
		return -1;

	if (rdir->entry_name)
		free(rdir->entry_name);
	rdir->entry = nullptr;
	rdir->directory = nullptr;

	free(rdir);
	return 0;
}

bool uwp_drive_exists(const char *path)
{
	if (!path || !*path)
		return 0;

	wchar_t *path_wide = utf8_to_utf16_string_alloc(path);
	Platform::String^ path_str = ref new Platform::String(path_wide);
	free(path_wide);

	return RunAsyncAndCatchErrors<bool>([&]() {
		return concurrency::create_task(StorageFolder::GetFolderFromPathAsync(path_str)).then([](StorageFolder^ properties) {
			return true;
		});
	}, false);
}

char* uwp_trigger_picker(void)
{
	return RunAsyncAndCatchErrors<char*>([&]() {
		return TriggerPickerAddDialog().then([](Platform::String^ path) {
			return utf16_to_utf8_string_alloc(path->Data());
		});
	}, NULL);
}
