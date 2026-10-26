#ifndef __FS_UTILS_H_
#define __FS_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <wiiu/types.h>

int MountFS(void *pClient, void *pCmd, char **mount_path);

#ifdef __cplusplus
}
#endif

#endif /* __FS_UTILS_H_ */
