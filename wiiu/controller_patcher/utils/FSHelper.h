#ifndef __FSHELPER_H_
#define __FSHELPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <wiiu/types.h>

int FS_Helper_MountFS(void *pClient, void *pCmd, char **mount_path);
int FS_Helper_GetFile(void * pClient,void * pCmd,const char *, char **result);

#ifdef __cplusplus
}
#endif

#endif // __FSHELPER_H_
