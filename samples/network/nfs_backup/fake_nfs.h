#ifndef FAKE_NFS_H
#define FAKE_NFS_H

#include <stdint.h>
#include <stddef.h>
#include <retro_miscellaneous.h>

/* An in-memory export standing in for net_nfs3.c, keeping its status
 * contract: each call leaves the server's answer in rnfs_get_status(),
 * or RNFS_STATUS_NONE when a call is made to fail unanswered. */

#define FAKE_NFS_MAX_NODES 64

enum fake_nfs_op
{
   FAKE_OP_OPEN = 0,
   FAKE_OP_READ,
   FAKE_OP_WRITE,
   FAKE_OP_CLOSE,
   FAKE_OP_STAT,
   FAKE_OP_UNLINK,
   FAKE_OP_MKDIR,
   FAKE_OP_RENAME,
   FAKE_OP_COUNT
};

typedef struct
{
   int      used;
   int      dir;
   char     path[PATH_MAX_LENGTH + 64];
   uint8_t *data;
   size_t   size;
} fake_node_t;

typedef struct
{
   fake_node_t nodes[FAKE_NFS_MAX_NODES];
   unsigned    calls[FAKE_OP_COUNT];
   /* the fail_nth call of fail_op fails with fail_status */
   int         fail_op;
   unsigned    fail_nth;
   uint32_t    fail_status;
   /* with fail_path set, every fail_op call on that path fails instead */
   char        fail_path[PATH_MAX_LENGTH + 64];
   /* MKDIR finds the directory made by someone else first */
   int         mkdir_race;
} fake_nfs_t;

extern fake_nfs_t fake_nfs;

void         fake_nfs_reset(void);
void         fake_nfs_fail(int op, unsigned nth, uint32_t status);
void         fake_nfs_reset_fail(void);
void         fake_nfs_fail_path(int op, const char *path, uint32_t status);
fake_node_t *fake_nfs_find(const char *path);
fake_node_t *fake_nfs_put(const char *path, const char *text);
fake_node_t *fake_nfs_mkdir_node(const char *path);
unsigned     fake_nfs_count(void);

#endif
