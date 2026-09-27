#ifndef FAKE_SMB_H
#define FAKE_SMB_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>

#define FAKE_SMB_MAX_NODES 64

typedef struct
{
   bool     used;
   bool     dir;
   char     path[512];
   uint8_t *data;
   size_t   len;
   size_t   pos;
} fake_node_t;

typedef struct
{
   fake_node_t nodes[FAKE_SMB_MAX_NODES];
   /* Failure switches: -1 / 0 mean "never". */
   long     fail_write_after;   /* total bytes after which writes fail */
   unsigned fail_rename_number; /* 1-based: which rename call fails */
   bool     fail_unlink;        /* unlinks of real files fail */
   size_t   written;
   unsigned renames;
} fake_smb_t;

extern fake_smb_t fake_smb;

struct smb2_context *fake_smb_ctx(void);
void         fake_smb_reset(void);
fake_node_t *fake_smb_find(const char *path);
void         fake_smb_put(const char *path, const char *text);
bool         fake_smb_is(const char *path, const char *text);

#endif
