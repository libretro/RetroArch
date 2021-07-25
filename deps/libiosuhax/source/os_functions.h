#ifndef __OS_FUNCTIONS_H_
#define __OS_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define OS_MUTEX_SIZE                   44

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Mutex functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern void OSInitMutex(void* mutex);
extern void OSLockMutex(void* mutex);
extern void OSUnlockMutex(void* mutex);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! IOS function
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern int IOS_Ioctl(int fd, unsigned int request, void *input_buffer,unsigned int input_buffer_len, void *output_buffer, unsigned int output_buffer_len);
extern int IOS_Open(char *path, unsigned int mode);
extern int IOS_Close(int fd);

#ifdef __cplusplus
}
#endif

#endif // __OS_FUNCTIONS_H_
