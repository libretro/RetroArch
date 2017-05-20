#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <wiiu/fs.h>
#define FS_MAX_MOUNTPATH_SIZE 128
int FS_Helper_MountFS(void *pClient, void *pCmd, char **mount_path){
    int result = -1;

    void *mountSrc = malloc(sizeof(FSMountSource));
    if(!mountSrc)
        return -3;

    char* mountPath = (char*) malloc(FS_MAX_MOUNTPATH_SIZE);
    if(!mountPath) {
        free(mountSrc);
        return -4;
    }

    memset(mountSrc, 0, sizeof(FSMountSource));
    memset(mountPath, 0, FS_MAX_MOUNTPATH_SIZE);

    // Mount sdcard
    if (FSGetMountSource(pClient, pCmd, FS_MOUNT_SOURCE_SD, mountSrc, -1) == 0)
    {
        result = FSMount(pClient, pCmd, mountSrc, mountPath, FS_MAX_MOUNTPATH_SIZE, -1);
        if((result == 0) && mount_path) {
            *mount_path = (char*)malloc(strlen(mountPath) + 1);
            if(*mount_path)
                strcpy(*mount_path, mountPath);
        }
    }

    free(mountPath);
    free(mountSrc);
    return result;
}

int FS_Helper_GetFile(void * pClient,void * pCmd,const char * path, char *(*result)){
    if(pClient == NULL || pCmd == NULL || path == NULL || result == NULL) return -2;
    FSStat stats;
    s32 status = -1;
    s32 handle = 0;
    if((status = FSGetStat(pClient,pCmd,path,&stats,-1)) == FS_STATUS_OK){
        (*result)  = (uint8_t *)  memalign(0x40, (sizeof(uint8_t)*stats.size)+1);
		if(!(*result)){
			printf("FS_Helper_GetFile(line %d): error: Failed to allocate space for reading the file\n",__LINE__);
			return -1;
		}
        (*result)[stats.size] = '\0';
        if((status = FSOpenFile(pClient,pCmd,path,"r",&handle,-1)) == FS_STATUS_OK){
            s32 total_read = 0;
			s32 ret2 = 0;

			char * cur_result_pointer = *result;
			s32 sizeToRead = stats.size;

			while ((ret2 = FSReadFile(pClient,pCmd, cur_result_pointer, 0x01, sizeToRead, handle, 0, -1)) > 0){
				total_read += ret2;
				cur_result_pointer += ret2;
				sizeToRead -= ret2;
            }

        }else{
            printf("FS_Helper_GetFile(line %d): error: (FSOpenFile) Couldn't open file (%s), error: %d",__LINE__,path,status);
            free((*result));
            (*result)=NULL;
            return -1;
        }

        FSCloseFile(pClient,pCmd,handle,-1);
        return 0;
    }else{
        printf("FS_Helper_GetFile(line %d): error: (GetStat) Couldn't open file (%s), error: %d",__LINE__,path,status);
    }
    return -1;
}
