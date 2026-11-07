/* zar_test: writes ZArchives from file trees it makes up and checks that
 * formats/zar looks files up in them and reads them back, octet for
 * octet.
 *
 * The archives hold blocks compressed with Zstandard beside blocks
 * kept as they are (data that does not compress), a file crossing
 * them, a directory with more entries than the reader asks for at once
 * and a nested one. Ranges are supplied short now and then. Every
 * request is checked: a lookup asks for a directory's entries a run at
 * a time and for each name only as far as it can match, exactly that
 * many octets; a read of a block kept as it is lands in the read's own
 * destination; a compressed block is fetched once for the partial
 * reads that follow it. A directory is listed in the archive's order,
 * each name read whole. A footer that is not one, a size that
 * disagrees and a block that does not decode must fail. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/zar.h>
#include <encodings/rzstd.h>

#define BLOCK   65536
#define FILLERS 38
#define XEX_SIZE (3 * BLOCK + 1234)

static int failures = 0;
static uint32_t rng_state = 0xC0FFEE11u;

static uint32_t rnd(void)
{
   rng_state ^= rng_state << 13;
   rng_state ^= rng_state >> 17;
   rng_state ^= rng_state << 5;
   return rng_state;
}

static void check(int ok, const char *what)
{
   if (!ok)
   {
      failures++;
      printf("FAIL  %s\n", what);
   }
}

static void be32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)v;
}

static void be64(uint8_t *p, uint64_t v)
{
   be32(p, (uint32_t)(v >> 32));
   be32(p + 4, (uint32_t)v);
}

/* an archive, and how the reader asked for it */
typedef struct
{
   uint8_t *data;
   size_t   size;
   bool     shorten;
   unsigned requests;
   size_t   asked;
} file_t;

static size_t pull(file_t *f, const zar_request_t *req)
{
   size_t n = req->length;
   f->requests++;
   f->asked += req->length;
   if (req->offset >= f->size)
      return 0;
   if (n > f->size - req->offset)
      n = f->size - (size_t)req->offset;
   if (f->shorten && n > 1 && (rnd() & 3) == 0)
      n = 1 + rnd() % (n - 1);
   memcpy(req->dst, f->data + req->offset, n);
   return n;
}

/* the tree: the root holds FILLERS files, media/ and default.xex last;
 * media/ holds a.bin and intro.wmv */
#define N_NODES (1 + FILLERS + 2 + 2)

typedef struct
{
   char     name[32];
   uint64_t offset;
   uint64_t size;
   uint32_t first;
   uint32_t count;
   bool     file;
} node_t;

static uint8_t *stream;      /* the files, laid end to end */
static size_t   stream_size;

/* The archive of the tree, with @bad_block's compressed octets broken
 * when it is not -1 */
static void make_archive(file_t *f, int bad_block)
{
   node_t   nodes[N_NODES];
   uint8_t *out    = (uint8_t*)calloc(1, 16 * BLOCK + 65536);
   uint8_t *blocks = out;
   size_t   bsize  = 0;
   size_t   n_blocks;
   uint8_t  rec[4 * 40];
   uint8_t  names[2048];
   size_t   nsize  = 0;
   uint8_t  tree[N_NODES * 16];
   uint8_t *enc;
   size_t   at, i, b, pos;
   uint32_t name_at[N_NODES];

   memset(nodes, 0, sizeof(nodes));
   memset(rec, 0, sizeof(rec));
   /* root */
   nodes[0].first = 1;
   nodes[0].count = FILLERS + 2;
   for (i = 0; i < FILLERS; i++)
   {
      sprintf(nodes[1 + i].name, "file%02u.bin", (unsigned)i);
      nodes[1 + i].file = true;
   }
   strcpy(nodes[1 + FILLERS].name, "media");
   nodes[1 + FILLERS].first = 1 + FILLERS + 2;
   nodes[1 + FILLERS].count = 2;
   strcpy(nodes[2 + FILLERS].name, "default.xex");
   nodes[2 + FILLERS].file = true;
   nodes[2 + FILLERS].size = XEX_SIZE;
   strcpy(nodes[3 + FILLERS].name, "a.bin");
   nodes[3 + FILLERS].file = true;
   nodes[3 + FILLERS].size = 5;
   strcpy(nodes[4 + FILLERS].name, "intro.wmv");
   nodes[4 + FILLERS].file = true;
   nodes[4 + FILLERS].size = 100;
   for (i = 0; i < FILLERS; i++)
      nodes[1 + i].size = 10;

   /* the stream: default.xex first - block 0 compresses, block 1 does
    * not, block 2 compresses - then the rest */
   free(stream);
   stream = (uint8_t*)calloc(1, 5 * BLOCK);
   pos    = 0;
   for (i = 0; i < XEX_SIZE; i++)
      stream[i] = (i / BLOCK == 1) ? (uint8_t)rnd() : (uint8_t)(i * 7 / 64);
   nodes[2 + FILLERS].offset = 0;
   pos = XEX_SIZE;
   for (i = 0; i < N_NODES; i++)
   {
      size_t k;
      if (!nodes[i].file || i == 2 + FILLERS)
         continue;
      nodes[i].offset = pos;
      for (k = 0; k < nodes[i].size; k++)
         stream[pos + k] = (uint8_t)(i * 31 + k);
      pos += (size_t)nodes[i].size;
   }
   stream_size = (pos + BLOCK - 1) / BLOCK * BLOCK;
   n_blocks    = stream_size / BLOCK;

   /* the blocks and their record */
   enc = (uint8_t*)malloc(rzstd_compress_bound(BLOCK));
   be64(rec, 0);
   for (b = 0; b < n_blocks; b++)
   {
      size_t wrote = 0;
      size_t len;
      rzstd_encode(enc, rzstd_compress_bound(BLOCK), stream + b * BLOCK,
            BLOCK, 3, &wrote);
      if (wrote && wrote < BLOCK)
      {
         memcpy(blocks + bsize, enc, wrote);
         if ((int)b == bad_block)
            blocks[bsize + wrote / 2] ^= 0x5A;
         len = wrote;
      }
      else
      {
         memcpy(blocks + bsize, stream + b * BLOCK, BLOCK);
         len = BLOCK;
      }
      rec[8 + b * 2]     = (uint8_t)((len - 1) >> 8);
      rec[8 + b * 2 + 1] = (uint8_t)(len - 1);
      bsize += len;
   }
   free(enc);

   /* names and nodes */
   for (i = 0; i < N_NODES; i++)
   {
      size_t len = strlen(nodes[i].name);
      if (!i)
      {
         name_at[i] = 0x7FFFFFFFu;
         continue;
      }
      name_at[i]      = (uint32_t)nsize;
      names[nsize++]  = (uint8_t)len;
      memcpy(names + nsize, nodes[i].name, len);
      nsize          += len;
   }
   for (i = 0; i < N_NODES; i++)
   {
      uint8_t *n = tree + i * 16;
      be32(n, name_at[i] | (nodes[i].file ? 0x80000000u : 0));
      if (nodes[i].file)
      {
         be32(n + 4, (uint32_t)nodes[i].offset);
         be32(n + 8, (uint32_t)nodes[i].size);
         be32(n + 12, 0);
      }
      else
      {
         be32(n + 4, nodes[i].first);
         be32(n + 8, nodes[i].count);
         be32(n + 12, 0);
      }
   }

   /* blocks, records, names, tree, two empty sections, the footer */
   at = bsize;
   {
      size_t   rec_at   = at;
      size_t   names_at = rec_at + 40;
      size_t   tree_at  = names_at + nsize;
      size_t   end      = tree_at + sizeof(tree);
      uint8_t *ft;

      memcpy(out + rec_at, rec, 40);
      memcpy(out + names_at, names, nsize);
      memcpy(out + tree_at, tree, sizeof(tree));
      ft = out + end;
      memset(ft, 0, 144);
      be64(ft + 0,  0);         be64(ft + 8,  bsize);
      be64(ft + 16, rec_at);    be64(ft + 24, 40);
      be64(ft + 32, names_at);  be64(ft + 40, nsize);
      be64(ft + 48, tree_at);   be64(ft + 56, sizeof(tree));
      be64(ft + 64, end);       be64(ft + 72, 0);
      be64(ft + 80, end);       be64(ft + 88, 0);
      be64(ft + 128, end + 144);
      be32(ft + 136, 0x61BF3A01u);
      be32(ft + 140, 0x169F52D6u);
      f->data     = out;
      f->size     = end + 144;
   }
   f->shorten  = false;
   f->requests = 0;
   f->asked    = 0;
}

static int open_zar(file_t *f, zar_t **out)
{
   zar_request_t req;
   int err;
   *out = zar_new(f->size);
   while ((err = zar_open_step(*out, &req)) == ZAR_PENDING)
      zar_feed(*out, pull(f, &req));
   return err;
}

static int lookup(file_t *f, zar_t *z, const char *path, zar_file_t *out)
{
   zar_request_t req;
   int err;
   if ((err = zar_lookup_begin(z, path)) != ZAR_OK)
      return err;
   while ((err = zar_lookup_step(z, &req, out)) == ZAR_PENDING)
      zar_feed(z, pull(f, &req));
   return err;
}

/* counts the requests a read makes for octets of compressed blocks,
 * and those of blocks kept as they are that land outside @dst */
static int read_file(file_t *f, zar_t *z, const zar_file_t *zf,
      uint64_t off, uint8_t *dst, size_t len, unsigned *outside)
{
   zar_request_t req;
   int err;
   if ((err = zar_read_begin(z, zf, off, dst, len)) != ZAR_OK)
      return err;
   while ((err = zar_read_step(z, &req)) == ZAR_PENDING)
   {
      /* anything but a record, outside the destination */
      if (     req.length != 40
            && (req.dst < dst || req.dst + req.length > dst + len))
         (*outside)++;
      zar_feed(z, pull(f, &req));
   }
   return err;
}

static void lookup_lane(void)
{
   file_t     f;
   zar_t     *z;
   zar_file_t zf;

   make_archive(&f, -1);
   check(open_zar(&f, &z) == ZAR_OK, "opens");
   check(f.asked == 144, "open reads the footer alone");

   f.asked = 0;
   check(   lookup(&f, z, "\\DEFAULT.XEX", &zf) == ZAR_OK
         && zf.offset == 0 && zf.size == XEX_SIZE, "lookup by name");
   /* the root's node, its entries in two runs, each name as far as an
    * eleven-octet name can match */
   check(f.asked == 16 + 32 * 16 + 8 * 16 + (FILLERS + 2) * 13,
         "lookup reads only what it needs");

   check(   lookup(&f, z, "media/Intro.WMV", &zf) == ZAR_OK
         && zf.size == 100, "nested lookup");
   check(   lookup(&f, z, "media/nope", &zf) == ZAR_ERROR_NOT_FOUND
         && lookup(&f, z, "default.xex/x", &zf) == ZAR_ERROR_NOT_FOUND
         && lookup(&f, z, "media", &zf) == ZAR_ERROR_NOT_FOUND
         && lookup(&f, z, "nope", &zf) == ZAR_ERROR_NOT_FOUND,
         "missing files");
   zar_free(z);
   free(f.data);
}

/* Lists @path, checking each entry against @names; returns how many
 * matched in order, or -1 when the listing fails */
static int list(file_t *f, zar_t *z, const char *path,
      const char *const *names, const int *dirs, int n)
{
   zar_request_t req;
   zar_dirent_t  ent;
   int           err, i = 0;
   if (zar_dir_begin(z, path) != ZAR_OK)
      return -1;
   for (;;)
   {
      while ((err = zar_dir_step(z, &req, &ent)) == ZAR_PENDING)
         zar_feed(z, pull(f, &req));
      if (err != ZAR_OK)
         break;
      if (     i >= n || strcmp(ent.name, names[i])
            || ent.len != strlen(names[i]) || !ent.is_dir != !dirs[i])
         return -1;
      i++;
   }
   return err == ZAR_END ? i : -1;
}

static void list_lane(void)
{
   static const char *const media[]  = { "a.bin", "intro.wmv" };
   static const int         mdirs[]  = { 0, 0 };
   const char               *root[FILLERS + 2];
   int                       rdirs[FILLERS + 2];
   char                      fill[FILLERS][16];
   file_t     f;
   zar_t     *z;
   zar_file_t zf;
   int        i, pass;

   for (i = 0; i < FILLERS; i++)
   {
      sprintf(fill[i], "file%02u.bin", (unsigned)i);
      root[i]  = fill[i];
      rdirs[i] = 0;
   }
   root[FILLERS]      = "media";
   rdirs[FILLERS]     = 1;
   root[FILLERS + 1]  = "default.xex";
   rdirs[FILLERS + 1] = 0;

   for (pass = 0; pass < 2; pass++)
   {
      make_archive(&f, -1);
      f.shorten = pass == 1;
      open_zar(&f, &z);
      check(list(&f, z, "", root, rdirs, FILLERS + 2) == FILLERS + 2,
            "the root, in order");
      check(list(&f, z, "\\MEDIA\\", media, mdirs, 2) == 2,
            "a directory, in order");
      check(   list(&f, z, "nope", media, mdirs, 2) == -1
            && list(&f, z, "default.xex", media, mdirs, 2) == -1,
            "a missing directory and a file are not listed");
      /* a lookup after a listing */
      check(   lookup(&f, z, "media/a.bin", &zf) == ZAR_OK
            && zf.size == 5, "lookup after a listing");
      zar_free(z);
      free(f.data);
   }
}

static void read_lane(void)
{
   file_t     f;
   zar_t     *z;
   zar_file_t zf;
   uint8_t   *got = (uint8_t*)malloc(XEX_SIZE + 16);
   unsigned   outside = 0;
   unsigned   i;
   size_t     before;
   int        pass;

   for (pass = 0; pass < 2; pass++)
   {
      make_archive(&f, -1);
      f.shorten = pass == 1;
      open_zar(&f, &z);
      lookup(&f, z, "default.xex", &zf);

      check(   read_file(&f, z, &zf, 0, got, XEX_SIZE + 16, &outside) == ZAR_OK
            && zar_read_progress(z) == XEX_SIZE
            && !memcmp(got, stream, XEX_SIZE), "whole file");

      for (i = 0; i < 300; i++)
      {
         uint64_t off = rnd() % XEX_SIZE;
         size_t   len = 1 + rnd() % (2 * BLOCK);
         size_t   want = len > XEX_SIZE - off ? (size_t)(XEX_SIZE - off) : len;
         if (     read_file(&f, z, &zf, off, got, len, &outside) != ZAR_OK
               || zar_read_progress(z) != want
               || memcmp(got, stream + off, want))
         {
            check(0, "random reads");
            break;
         }
      }
      /* inside the block kept as it is */
      outside = 0;
      check(   read_file(&f, z, &zf, BLOCK + 100, got, 5000, &outside) == ZAR_OK
            && !memcmp(got, stream + BLOCK + 100, 5000)
            && !outside, "a block kept as it is lands in place");
      zar_free(z);
      free(f.data);
   }

   /* an XEX's head, its optional headers and its execution info: one
    * fetch of the compressed block, the rest from it */
   make_archive(&f, -1);
   open_zar(&f, &z);
   lookup(&f, z, "default.xex", &zf);
   f.requests = 0;
   read_file(&f, z, &zf, 0, got, 24, &outside);
   before = f.requests;
   read_file(&f, z, &zf, 24, got, 320, &outside);
   read_file(&f, z, &zf, 1000, got, 24, &outside);
   check(before == 2 && f.requests == 2,
         "partial reads of a block fetch it once");
   check(!memcmp(got, stream + 1000, 24), "and read it right");

   /* the third block whole: decoded where the read goes */
   check(   read_file(&f, z, &zf, 2 * BLOCK, got, BLOCK, &outside) == ZAR_OK
         && !memcmp(got, stream + 2 * BLOCK, BLOCK), "whole block");
   zar_free(z);
   free(f.data);
   free(got);
}

static void broken_lanes(void)
{
   file_t        f;
   zar_t        *z;
   zar_file_t    zf;
   zar_request_t req;
   uint8_t       buf[64];
   unsigned      outside = 0;

   make_archive(&f, -1);
   f.data[f.size - 1] ^= 1;
   check(open_zar(&f, &z) == ZAR_ERROR_DATA, "bad magic");
   check(zar_lookup_begin(z, "x") == ZAR_ERROR_STATE, "lookup before open");
   zar_free(z);
   f.data[f.size - 1] ^= 1;
   f.size--;
   check(open_zar(&f, &z) == ZAR_ERROR_DATA, "size disagrees");
   zar_free(z);
   free(f.data);

   make_archive(&f, 0);
   open_zar(&f, &z);
   lookup(&f, z, "default.xex", &zf);
   check(read_file(&f, z, &zf, 0, buf, 24, &outside) == ZAR_ERROR_DATA,
         "block that does not decode");
   zar_free(z);
   free(f.data);

   make_archive(&f, -1);
   z = zar_new(f.size);
   check(zar_feed(z, 1) == ZAR_ERROR_STATE, "feed with nothing asked");
   check(zar_open_step(z, &req) == ZAR_PENDING, "open asks");
   check(zar_feed(z, 0) == ZAR_ERROR_DATA
         && zar_open_step(z, &req) == ZAR_ERROR_DATA,
         "nothing supplied fails");
   zar_free(z);
   free(f.data);
}

int main(void)
{
   lookup_lane();
   list_lane();
   read_lane();
   broken_lanes();
   free(stream);

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("zar_test: all passed\n");
   return 0;
}
