#ifndef EXEC_3DSX_H
#define EXEC_3DSX_H

//since 3dsx programs are not guaranteed access to the OS, the 3dsx bootloader run by the exploit must run the next program
//your program must call this then exit gracefully to work, exit() also doesnt work
int exec_3dsx(const char* path, const char* args);

#endif