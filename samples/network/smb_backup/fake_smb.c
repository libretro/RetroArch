/* An in-memory stand-in for the libsmb2 calls network/cloud_sync/smb.c
 * makes: a flat table of files and directories, with switches the test
 * flips to make a write, rename or unlink fail.  Enough of libsmb2's
 * contract to exercise the driver as shipped: negative errno returns,
 * ENOENT from stat on a missing path, rename refusing an existing
 * destination (as SMB2 does without ReplaceIfExists). */

#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include "fake_smb.h"

fake_smb_t fake_smb;

struct smb2_context { int unused; };
struct smb2fh       { fake_node_t *node; };

static struct smb2_context the_ctx;

void fake_smb_reset(void)
{
   int i;
   for (i = 0; i < FAKE_SMB_MAX_NODES; i++)
      free(fake_smb.nodes[i].data);
   memset(&fake_smb, 0, sizeof(fake_smb));
   fake_smb.fail_write_after = -1;
}

fake_node_t *fake_smb_find(const char *path)
{
   int i;
   for (i = 0; i < FAKE_SMB_MAX_NODES; i++)
      if (fake_smb.nodes[i].used && !strcmp(fake_smb.nodes[i].path, path))
         return &fake_smb.nodes[i];
   return NULL;
}

static fake_node_t *fake_smb_add(const char *path, bool dir)
{
   int i;
   for (i = 0; i < FAKE_SMB_MAX_NODES; i++)
      if (!fake_smb.nodes[i].used)
      {
         fake_node_t *n = &fake_smb.nodes[i];
         memset(n, 0, sizeof(*n));
         n->used = true;
         n->dir  = dir;
         snprintf(n->path, sizeof(n->path), "%s", path);
         return n;
      }
   return NULL;
}

void fake_smb_put(const char *path, const char *text)
{
   fake_node_t *n = fake_smb_find(path);
   if (!n)
      n = fake_smb_add(path, false);
   free(n->data);
   n->len  = strlen(text);
   n->data = (uint8_t*)malloc(n->len + 1);
   memcpy(n->data, text, n->len + 1);
}

bool fake_smb_is(const char *path, const char *text)
{
   fake_node_t *n = fake_smb_find(path);
   return n && !n->dir && n->len == strlen(text)
       && (!n->len || !memcmp(n->data, text, n->len));
}

struct smb2_context *smb2_init_context(void) { return &the_ctx; }
void smb2_destroy_context(struct smb2_context *smb2) { (void)smb2; }
int smb2_connect_share(struct smb2_context *smb2, const char *server,
      const char *share, const char *user)
{ (void)smb2; (void)server; (void)share; (void)user; return 0; }
int smb2_disconnect_share(struct smb2_context *smb2) { (void)smb2; return 0; }
void smb2_set_user(struct smb2_context *smb2, const char *user) { (void)smb2; (void)user; }
void smb2_set_password(struct smb2_context *smb2, const char *password) { (void)smb2; (void)password; }
void smb2_set_domain(struct smb2_context *smb2, const char *domain) { (void)smb2; (void)domain; }
void smb2_set_security_mode(struct smb2_context *smb2, uint16_t mode) { (void)smb2; (void)mode; }
void smb2_set_timeout(struct smb2_context *smb2, int seconds) { (void)smb2; (void)seconds; }
void smb2_set_authentication(struct smb2_context *smb2, int val) { (void)smb2; (void)val; }
const char *smb2_get_error(struct smb2_context *smb2) { (void)smb2; return "fake error"; }
uint32_t smb2_get_max_read_size(struct smb2_context *smb2) { (void)smb2; return 4; }
uint32_t smb2_get_max_write_size(struct smb2_context *smb2) { (void)smb2; return 4; }

struct smb2_context *fake_smb_ctx(void) { return &the_ctx; }

int smb2_stat(struct smb2_context *smb2, const char *path,
      struct smb2_stat_64 *st)
{
   fake_node_t *n = fake_smb_find(path);
   (void)smb2;
   if (!n)
      return -ENOENT;
   memset(st, 0, sizeof(*st));
   st->smb2_type = n->dir ? SMB2_TYPE_DIRECTORY : SMB2_TYPE_FILE;
   st->smb2_size = n->len;
   return 0;
}

int smb2_mkdir(struct smb2_context *smb2, const char *path)
{
   (void)smb2;
   if (fake_smb_find(path))
      return -EEXIST;
   return fake_smb_add(path, true) ? 0 : -ENOSPC;
}

struct smb2fh *smb2_open(struct smb2_context *smb2, const char *path, int flags)
{
   fake_node_t   *n = fake_smb_find(path);
   struct smb2fh *fh;
   (void)smb2;

   if (!n)
   {
      if (!(flags & O_CREAT))
         return NULL;
      if (!(n = fake_smb_add(path, false)))
         return NULL;
   }
   if (n->dir)
      return NULL;
   if (flags & O_TRUNC)
   {
      free(n->data);
      n->data = NULL;
      n->len  = 0;
   }
   fh       = (struct smb2fh*)calloc(1, sizeof(*fh));
   fh->node = n;
   n->pos   = 0;
   return fh;
}

int smb2_close(struct smb2_context *smb2, struct smb2fh *fh)
{
   (void)smb2;
   free(fh);
   return 0;
}

int smb2_ftruncate(struct smb2_context *smb2, struct smb2fh *fh,
      uint64_t length)
{
   (void)smb2;
   if (length < fh->node->len)
      fh->node->len = (size_t)length;
   if (fh->node->pos > fh->node->len)
      fh->node->pos = fh->node->len;
   return 0;
}

int smb2_read(struct smb2_context *smb2, struct smb2fh *fh,
      uint8_t *buf, uint32_t count)
{
   fake_node_t *n = fh->node;
   size_t left    = n->len - n->pos;
   (void)smb2;
   if (count > left)
      count = (uint32_t)left;
   memcpy(buf, n->data + n->pos, count);
   n->pos += count;
   return (int)count;
}

int smb2_write(struct smb2_context *smb2, struct smb2fh *fh,
      const uint8_t *buf, uint32_t count)
{
   fake_node_t *n = fh->node;
   (void)smb2;

   /* Fail once this many bytes have been written in total. */
   if (fake_smb.fail_write_after >= 0
         && fake_smb.written + count > (size_t)fake_smb.fail_write_after)
      return -EIO;

   if (n->pos + count > n->len)
   {
      n->data = (uint8_t*)realloc(n->data, n->pos + count + 1);
      n->len  = n->pos + count;
   }
   memcpy(n->data + n->pos, buf, count);
   n->pos           += count;
   fake_smb.written += count;
   return (int)count;
}

int smb2_unlink(struct smb2_context *smb2, const char *path)
{
   fake_node_t *n = fake_smb_find(path);
   (void)smb2;
   if (fake_smb.fail_unlink && !strstr(path, ".rauploading"))
      return -EACCES;
   if (!n || n->dir)
      return -ENOENT;
   free(n->data);
   memset(n, 0, sizeof(*n));
   return 0;
}

int smb2_rename(struct smb2_context *smb2, const char *oldpath,
      const char *newpath)
{
   fake_node_t *n = fake_smb_find(oldpath);
   (void)smb2;
   fake_smb.renames++;
   if (fake_smb.fail_rename_number
         && fake_smb.renames == fake_smb.fail_rename_number)
      return -EACCES;
   if (!n)
      return -ENOENT;
   if (fake_smb_find(newpath))
      return -EEXIST;
   snprintf(n->path, sizeof(n->path), "%s", newpath);
   return 0;
}
