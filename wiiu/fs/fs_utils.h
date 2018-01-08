#ifndef __FS_UTILS_H_
#define __FS_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <wiiu/types.h>

int MountFS(void *pClient, void *pCmd, char **mount_path);
int UmountFS(void *pClient, void *pCmd, const char *mountPath);

int LoadFileToMem(const char *filepath, u8 **inbuffer, u32 *size);

/* TODO/FIXME: C++ class */
int CreateSubfolder(const char * fullpath);
int CheckFile(const char * filepath);

#ifdef __cplusplus
}
#endif

#endif /* __FS_UTILS_H_ */
