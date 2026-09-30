/* Stand-in for libsmb2's <smb2/libsmb2.h>: the entry points
 * network/cloud_sync/smb.c calls, each of which fake_smb.c defines
 * over its in-memory share. Prototypes follow the library's. */
#ifndef SMB_BACKUP_STANDIN_LIBSMB2_H
#define SMB_BACKUP_STANDIN_LIBSMB2_H

#include <stdint.h>
#include <smb2/smb2.h>

struct smb2_context;
struct smb2fh;

struct smb2_context *smb2_init_context(void);
void smb2_destroy_context(struct smb2_context *smb2);
const char *smb2_get_error(struct smb2_context *smb2);
void smb2_set_user(struct smb2_context *smb2, const char *user);
void smb2_set_password(struct smb2_context *smb2, const char *password);
void smb2_set_domain(struct smb2_context *smb2, const char *domain);
void smb2_set_authentication(struct smb2_context *smb2, int val);
void smb2_set_security_mode(struct smb2_context *smb2, uint16_t security_mode);
void smb2_set_timeout(struct smb2_context *smb2, int seconds);
int smb2_connect_share(struct smb2_context *smb2, const char *server,
      const char *share, const char *user);
int smb2_disconnect_share(struct smb2_context *smb2);
uint32_t smb2_get_max_read_size(struct smb2_context *smb2);
uint32_t smb2_get_max_write_size(struct smb2_context *smb2);
struct smb2fh *smb2_open(struct smb2_context *smb2, const char *path, int flags);
int smb2_close(struct smb2_context *smb2, struct smb2fh *fh);
int smb2_read(struct smb2_context *smb2, struct smb2fh *fh, uint8_t *buf, uint32_t count);
int smb2_write(struct smb2_context *smb2, struct smb2fh *fh, const uint8_t *buf, uint32_t count);
int smb2_ftruncate(struct smb2_context *smb2, struct smb2fh *fh, uint64_t length);
int smb2_stat(struct smb2_context *smb2, const char *path, struct smb2_stat_64 *st);
int smb2_rename(struct smb2_context *smb2, const char *oldpath, const char *newpath);
int smb2_unlink(struct smb2_context *smb2, const char *path);
int smb2_mkdir(struct smb2_context *smb2, const char *path);

#endif
