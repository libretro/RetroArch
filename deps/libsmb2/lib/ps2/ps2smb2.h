/**
 * @file
 * PS2SMB2 definitions.
 */

#ifndef __PS2SMB2_H__
#define __PS2SMB2_H__

#define SMB2_PATH_MAX 1024
/*
 * DEVCTL commands
 */
#define SMB2_DEVCTL_CONNECT		0xC0DE0001
#define SMB2_DEVCTL_DISCONNECT_ALL	0xC0DE0002

/*
 * I/O structures for DEVCTL commands
 */
#define SMB2_MAX_NAME_LEN 32
typedef struct {
	char name[SMB2_MAX_NAME_LEN];
	char username[32];
	char password[32];
	char url[256];
} smb2Connect_in_t;

typedef struct {
	void *ctx;
} smb2Connect_out_t;

typedef struct {
	void *ctx;
} smb2Disconnect_in_t;

#endif /* __PS2SMB2_H__ */
