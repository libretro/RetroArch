
#ifndef _IOS_H_
#define _IOS_H_

int IOS_Open(char *path, unsigned int mode);
int IOS_Close(int fd);
int IOS_Ioctl(int fd, unsigned int request, void *input_buffer, unsigned int input_buffer_len, void *output_buffer, unsigned int output_buffer_len);
int IOS_IoctlAsync(int fd, unsigned int request, void *input_buffer, unsigned int input_buffer_len, void *output_buffer, unsigned int output_buffer_len, void *cb, void *cbarg);

#endif
