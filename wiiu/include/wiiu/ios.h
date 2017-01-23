#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int IOS_Open(char *path, u32 mode);
int IOS_Close(int fd);
int IOS_Ioctl(int fd, u32 request, void *input_buffer, u32 input_buffer_len, void *output_buffer, u32 output_buffer_len);
int IOS_IoctlAsync(int fd, u32 request, void *input_buffer, u32 input_buffer_len, void *output_buffer, u32 output_buffer_len, void *cb, void *cbarg);

#ifdef __cplusplus
}
#endif
