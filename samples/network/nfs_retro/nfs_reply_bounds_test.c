/* net_nfs3.c's reply parsers against short and hostile replies, with
 * no server: the file is included so its static decoders are called
 * on crafted buffers. Each reply sits at the very end of its own
 * allocation, so a read past it is caught under ASan, and the walks
 * over server-sent counts are held to a CPU-time bound. */
#include "../../../libretro-common/net/net_nfs3.c"

#include <stdio.h>
#include <time.h>

static int failures;

#define CHECK(cond, what) do { if (!(cond)) { \
   printf("[FAIL] %s (line %d)\n", what, __LINE__); failures++; } } while (0)

/* A reply filled into a buffer of exactly its size, so the octet
 * after its end is outside the allocation. */
struct reply
{
   uint8_t *mem;
   size_t   len;
   size_t   fill;
};

static void reply_init(struct reply *r, size_t len)
{
   r->mem  = (uint8_t*)malloc(len ? len : 1);
   r->len  = len;
   r->fill = 0;
}

static void reply_u32(struct reply *r, uint32_t v)
{
   if (r->len - r->fill < 4)
      return;
   r->mem[r->fill++] = (uint8_t)(v >> 24);
   r->mem[r->fill++] = (uint8_t)(v >> 16);
   r->mem[r->fill++] = (uint8_t)(v >> 8);
   r->mem[r->fill++] = (uint8_t)v;
}

static void reply_bytes(struct reply *r, uint8_t v, size_t n)
{
   while (n-- && r->fill < r->len)
      r->mem[r->fill++] = v;
}

static void reply_xdr(struct reply *r, struct xdr *x)
{
   x->p    = r->mem;
   x->end  = r->mem + r->len;
   x->fail = 0;
}

/* a walk bounded by the bytes received finishes at once; one that
 * spins on a count of 2^32 takes seconds */
#define CPU_BOUND (CLOCKS_PER_SEC / 4)

static void test_nfs4_res(void)
{
   struct reply r;
   struct xdr   x;
   reply_init(&r, 4);
   reply_u32(&r, OP_GETATTR);
   reply_xdr(&r, &x);
   CHECK(nfs4_res(&x) != NFS3_OK, "result cut after its opnum is not NFS4_OK");
   CHECK(nfs4_res(&x) != NFS3_OK, "result past the end is not NFS4_OK");
   free(r.mem);
}

static void test_fattr(void)
{
   struct reply    r;
   struct xdr      x;
   struct rnfs_stat st;
   clock_t         t;
   int             ret;

   /* bitmap count of 2^32 - 1 in a reply of three words */
   reply_init(&r, 12);
   reply_u32(&r, 0xFFFFFFFFu);
   reply_u32(&r, FATTR4_W0_TYPE);
   reply_u32(&r, 0);
   reply_xdr(&r, &x);
   t   = clock();
   ret = nfs4_get_fattr(&x, &st);
   t   = clock() - t;
   CHECK(ret == -1, "fattr4 with a bitmap count past the reply fails");
   CHECK(t < CPU_BOUND, "fattr4 bitmap walk stops at the end of the reply");
   free(r.mem);

   /* a whole one: dir, size, mtime */
   reply_init(&r, 4 * 3 + 4 + 24);
   reply_u32(&r, 2);
   reply_u32(&r, FATTR4_W0_TYPE | FATTR4_W0_SIZE);
   reply_u32(&r, FATTR4_W1_TIME_MODIFY);
   reply_u32(&r, 24);
   reply_u32(&r, NF4DIR);
   reply_u32(&r, 0); reply_u32(&r, 0x1234);
   reply_u32(&r, 0); reply_u32(&r, 77); reply_u32(&r, 0);
   reply_xdr(&r, &x);
   CHECK(nfs4_get_fattr(&x, &st) == 0, "whole fattr4 decodes");
   CHECK(st.is_dir && st.size == 0x1234 && st.mtime == 77, "fattr4 values");
   free(r.mem);
}

/* OPEN4resok: stateid, change_info, rflags, attrset, delegation */
static void open_head(struct reply *r)
{
   reply_bytes(r, 0x5A, 16);
   reply_u32(r, 1); reply_u32(r, 0); reply_u32(r, 1); reply_u32(r, 0); reply_u32(r, 2);
   reply_u32(r, 2);                     /* rflags: CONFIRM */
}

static void test_open_res(void)
{
   struct reply r;
   struct xdr   x;
   uint8_t      sid[16], keep[16];
   uint32_t     rflags = 0;
   clock_t      t;
   int          ret;
   memset(keep, 0xAA, sizeof(keep));

   /* attrset count of 2^32 - 1, reply ends after it */
   reply_init(&r, 16 + 24 + 4);
   open_head(&r);
   reply_u32(&r, 0xFFFFFFFFu);
   reply_xdr(&r, &x);
   memcpy(sid, keep, 16);
   t   = clock();
   ret = nfs4_open_res(&x, sid, &rflags);
   t   = clock() - t;
   CHECK(ret == -1, "OPEN with an attrset count past the reply fails");
   CHECK(t < CPU_BOUND, "OPEN attrset walk stops at the end of the reply");
   CHECK(!memcmp(sid, keep, 16), "failed OPEN leaves the stateid alone");
   free(r.mem);

   /* reply ends where the delegation type should be */
   reply_init(&r, 16 + 24 + 8);
   open_head(&r);
   reply_u32(&r, 1); reply_u32(&r, 0);
   reply_xdr(&r, &x);
   memcpy(sid, keep, 16);
   CHECK(nfs4_open_res(&x, sid, &rflags) == -1, "OPEN cut before its delegation fails");
   CHECK(!memcmp(sid, keep, 16), "OPEN cut before its delegation leaves the stateid alone");
   free(r.mem);

   /* reply ends inside the stateid */
   reply_init(&r, 8);
   reply_bytes(&r, 0x5A, 8);
   reply_xdr(&r, &x);
   memcpy(sid, keep, 16);
   CHECK(nfs4_open_res(&x, sid, &rflags) == -1, "OPEN cut inside its stateid fails");
   CHECK(!memcmp(sid, keep, 16), "OPEN cut inside its stateid leaves the stateid alone");
   free(r.mem);

   /* a delegation granted though none was asked for */
   reply_init(&r, 16 + 24 + 8);
   open_head(&r);
   reply_u32(&r, 0); reply_u32(&r, 1);
   reply_xdr(&r, &x);
   CHECK(nfs4_open_res(&x, sid, &rflags) == -1, "OPEN with a delegation is refused");
   free(r.mem);

   /* whole */
   reply_init(&r, 16 + 24 + 12);
   open_head(&r);
   reply_u32(&r, 1); reply_u32(&r, 0); reply_u32(&r, 0);
   reply_xdr(&r, &x);
   memcpy(sid, keep, 16);
   CHECK(nfs4_open_res(&x, sid, &rflags) == 0, "whole OPEN result decodes");
   CHECK(sid[0] == 0x5A && sid[15] == 0x5A && rflags == 2, "OPEN stateid and rflags");
   CHECK(x.p == x.end, "OPEN result consumed exactly");
   free(r.mem);

   /* OPEN_CONFIRM's stateid, cut short */
   reply_init(&r, 12);
   reply_bytes(&r, 0x33, 12);
   reply_xdr(&r, &x);
   memcpy(sid, keep, 16);
   CHECK(nfs4_get_stateid(&x, sid) == -1, "stateid cut short fails");
   CHECK(!memcmp(sid, keep, 16), "stateid cut short leaves the stateid alone");
   free(r.mem);
}

static void test_readdir_take(void)
{
   struct reply    r;
   struct xdr      x;
   struct rnfs_dir d;
   memset(&d, 0, sizeof(d));
   d.buf     = (uint8_t*)malloc(NFS3_MAX_IO);
   d.buf_len = 99;
   memset(d.cookieverf, 0x77, sizeof(d.cookieverf));

   /* reply ends inside the cookie verifier */
   reply_init(&r, 4);
   reply_bytes(&r, 0x11, 4);
   reply_xdr(&r, &x);
   CHECK(rnfs_readdir_take(&d, &x) == -1, "READDIR cut inside its verifier fails");
   CHECK(d.buf_len == 99 && d.cookieverf[0] == 0x77,
         "failed READDIR leaves the page and verifier alone");
   free(r.mem);

   /* reply already failed before the verifier (status past the end) */
   reply_init(&r, 0);
   reply_xdr(&r, &x);
   xdr_get_u32(&x);
   CHECK(rnfs_readdir_take(&d, &x) == -1, "READDIR after a failed cursor fails");
   free(r.mem);

   /* verifier and an entry list */
   reply_init(&r, 8 + 12);
   reply_bytes(&r, 0x22, 8);
   reply_u32(&r, 1); reply_u32(&r, 2); reply_u32(&r, 3);
   reply_xdr(&r, &x);
   CHECK(rnfs_readdir_take(&d, &x) == 0, "whole READDIR page is taken");
   CHECK(d.cookieverf[0] == 0x22 && d.cookieverf[7] == 0x22, "READDIR verifier");
   CHECK(d.buf_len == 12 && d.buf_off == 0 && d.buf[11] == 3, "READDIR entry list");
   free(r.mem);
   free(d.buf);
}

static void test_opaque_and_sequence(void)
{
   static struct rnfs_ctx c;
   struct reply r;
   struct xdr   x;
   size_t       len = 1;

   /* a length near 2^32 whose pad would wrap a 32-bit sum */
   reply_init(&r, 8);
   reply_u32(&r, 0xFFFFFFFDu);
   reply_u32(&r, 0);
   reply_xdr(&r, &x);
   CHECK(xdr_get_opaque(&x, &len) == NULL && x.fail && len == 0,
         "opaque longer than the reply fails");
   free(r.mem);

   /* SEQUENCE answered OK, reply ends inside its session id */
   reply_init(&r, 8 + 8);
   reply_u32(&r, OP_SEQUENCE);
   reply_u32(&r, NFS3_OK);
   reply_bytes(&r, 0, 8);
   reply_xdr(&r, &x);
   CHECK(nfs4_sequence_res(&x, &c, 0) != NFS3_OK, "SEQUENCE cut inside its session id fails");
   CHECK(x.p <= x.end, "cursor stays inside the reply");
   free(r.mem);
}

int main(void)
{
   test_nfs4_res();
   test_fattr();
   test_open_res();
   test_readdir_take();
   test_opaque_and_sequence();
   if (failures)
   {
      printf("nfs_reply_bounds_test: %d failure(s)\n", failures);
      return 1;
   }
   printf("[pass] nfs_reply_bounds_test\n");
   return 0;
}
