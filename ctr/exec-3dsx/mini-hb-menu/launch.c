#include "common.h"

Handle launchOpenFile(const char* path)
{
	if (strncmp(path, "sdmc:/", 6) == 0)
		path += 5;
	else if (*path != '/')
		return 0;

	// Convert the executable path to UTF-16
	static uint16_t __utf16path[PATH_MAX+1];
	ssize_t units = utf8_to_utf16(__utf16path, (const uint8_t*)path, PATH_MAX);
	if (units < 0 || units >= PATH_MAX) return 0;
	__utf16path[units] = 0;

	// Open the file directly
	FS_Path apath = { PATH_EMPTY, 1, (u8*)"" };
	FS_Path fpath = { PATH_UTF16, (units+1)*2, (u8*)__utf16path };
	Handle file;
	Result res = FSUSER_OpenFileDirectly(&file, ARCHIVE_SDMC, apath, fpath, FS_OPEN_READ, 0);
	return R_SUCCEEDED(res) ? file : 0;
}
