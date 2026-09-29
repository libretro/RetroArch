/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_smb2.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <net/net_smb2.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <crypto/crypto.h>
#include <crypto/kdf.h>
#include <lrc_hash.h>
#include <encodings/utf.h>
#include <compat/strl.h>

/* ---- constants ---------------------------------------------------- */

#define SMB2_PORT 445

#define SMB2_NEGOTIATE        0x0000
#define SMB2_SESSION_SETUP    0x0001
#define SMB2_LOGOFF           0x0002
#define SMB2_TREE_CONNECT     0x0003
#define SMB2_TREE_DISCONNECT  0x0004
#define SMB2_CREATE           0x0005
#define SMB2_CLOSE            0x0006
#define SMB2_READ             0x0008
#define SMB2_WRITE            0x0009
#define SMB2_ECHO             0x000d
#define SMB2_QUERY_DIRECTORY  0x000e
#define SMB2_QUERY_INFO       0x0010
#define SMB2_SET_INFO         0x0011

#define SMB2_FLAGS_SIGNED     0x00000008

#define STATUS_SUCCESS                  0x00000000
#define STATUS_MORE_PROCESSING_REQUIRED 0xc0000016
#define STATUS_END_OF_FILE              0xc0000011
#define STATUS_NO_MORE_FILES            0x80000006
#define STATUS_OBJECT_NAME_NOT_FOUND    0xc0000034
#define STATUS_LOGON_FAILURE            0xc000006d

#define DIALECT_202 0x0202
#define DIALECT_210 0x0210
#define DIALECT_300 0x0300
#define DIALECT_302 0x0302
#define DIALECT_311 0x0311

#define CIPHER_AES128_CCM 0x0001
#define CIPHER_AES128_GCM 0x0002

#define SMB2_HDR_SIZE 64
#define SMB2_TRANSFORM_HDR_SIZE 52
/* One credit's worth of data, and the unit of the small buffers
 * (directory pages, the srvsvc pipe). Reads and writes go up to
 * SMB2_LARGE_IO once the server has granted the credits; the
 * connection's buffers are sized to what it negotiated. */
#define SMB2_MAX_IO   (64 * 1024)
#define SMB2_LARGE_IO (1024 * 1024)
#define SMB2_BUF_SIZE(io) (SMB2_TRANSFORM_HDR_SIZE + SMB2_HDR_SIZE + (io) + 4096)
#define SMB2_RX_SIZE  SMB2_BUF_SIZE(SMB2_MAX_IO)

/* FILE_* access, attributes and create options, prefixed so they
 * do not collide with the Win32 headers' own definitions. */
#define SMB2_FILE_READ_DATA        0x00000001
#define SMB2_FILE_WRITE_DATA       0x00000002
#define SMB2_FILE_APPEND_DATA      0x00000004
#define SMB2_FILE_READ_EA          0x00000008
#define SMB2_FILE_READ_ATTRIBUTES  0x00000080
#define SMB2_FILE_WRITE_ATTRIBUTES 0x00000100
#define SMB2_FILE_LIST_DIRECTORY   0x00000001
#define SMB2_SYNCHRONIZE           0x00100000
#define SMB2_FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define SMB2_FILE_SHARE_ALL        0x00000007
#define SMB2_FILE_OPEN             0x00000001
#define SMB2_FILE_CREATE           0x00000002
#define SMB2_FILE_OPEN_IF          0x00000003
#define SMB2_FILE_OVERWRITE_IF     0x00000005
#define SMB2_FILE_DIRECTORY_FILE     0x00000001
#define SMB2_FILE_NON_DIRECTORY_FILE 0x00000040
#define SMB2_FILE_DELETE_ON_CLOSE    0x00001000
#define SMB2_DELETE                  0x00010000

/* NTLMSSP */
#define NTLMSSP_NEGOTIATE_UNICODE        0x00000001
#define NTLMSSP_REQUEST_TARGET           0x00000004
#define NTLMSSP_NEGOTIATE_SIGN           0x00000010
#define NTLMSSP_NEGOTIATE_NTLM           0x00000200
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN    0x00008000
#define NTLMSSP_NEGOTIATE_EXT_SESSSEC    0x00080000
#define NTLMSSP_NEGOTIATE_TARGET_INFO    0x00800000
#define NTLMSSP_NEGOTIATE_128            0x20000000
#define NTLMSSP_NEGOTIATE_56             0x80000000

struct rsmb_file
{
   uint64_t offset;
   uint64_t size;
   uint8_t  fid[16];
};

struct rsmb_dir
{
   struct rsmb_dirent ent;
   uint8_t  *buf;       /* one QUERY_DIRECTORY response's entries */
   size_t    buf_len;
   size_t    buf_off;
   uint8_t   fid[16];
   uint8_t   done;
};

struct rsmb_ctx
{
   uint8_t *rx;                 /* SMB2_RX_SIZE */
   uint8_t *tx;                 /* SMB2_RX_SIZE */
   char    *user;
   char    *password;
   char    *domain;
   uint64_t session_id;
   uint64_t message_id;
   uint32_t tree_id;
   uint32_t max_read;
   uint32_t max_write;
   uint32_t io_size;            /* largest single read or write */
   size_t   buf_size;           /* rx and tx capacity */
   int      credits;            /* granted by the server, unspent */
   unsigned charge;             /* CreditCharge of the next message */
   uint32_t status;
   unsigned dialect;
   unsigned cipher;             /* 0, CIPHER_AES128_CCM or _GCM */
   unsigned timeout;
   uint16_t port;
   int      fd;
   uint8_t  session_key[16];
   uint8_t  signing_key[16];
   uint8_t  enc_key[16];        /* client -> server */
   uint8_t  dec_key[16];        /* server -> client */
   uint8_t  preauth[64];        /* 3.1.1 hash chain */
   uint64_t nonce_counter;
   uint8_t  signing;            /* sign requests once the session is up */
   uint8_t  session_ready;      /* keys derived: sign / verify from here */
   uint8_t  encrypt;            /* seal everything after setup */
   uint8_t  connected;
   char     server[256];
   char     error[128];
};

/* ---- little-endian helpers ---------------------------------------- */

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { crypto_store32_le(p, v); }
static void put64(uint8_t *p, uint64_t v) { crypto_store64_le(p, v); }
static unsigned get16(const uint8_t *p)   { return p[0] | ((unsigned)p[1] << 8); }
static uint32_t get32(const uint8_t *p)   { return crypto_load32_le(p); }
static uint64_t get64(const uint8_t *p)
{
   return (uint64_t)crypto_load32_le(p) | ((uint64_t)crypto_load32_le(p + 4) << 32);
}

static void rsmb_err(struct rsmb_ctx *c, const char *msg)
{
   strlcpy(c->error, msg, sizeof(c->error));
}

/* UTF-8 to UTF-16LE; returns octets written, or 0 for empty. Path
 * separators become backslashes when @path is set. */
static size_t rsmb_utf16(const char *s, uint8_t *out, size_t cap, int path)
{
   size_t n = 0;
   while (s && *s)
   {
      uint32_t cp = utf8_walk(&s);
      if (!cp)
         break;
      if (path && cp == '/')
         cp = '\\';
      if (cp >= 0x10000)
      {
         if (n + 4 > cap)
            break;
         cp -= 0x10000;
         put16(out + n,     0xd800 | (cp >> 10));
         put16(out + n + 2, 0xdc00 | (cp & 0x3ff));
         n += 4;
      }
      else
      {
         if (n + 2 > cap)
            break;
         put16(out + n, cp);
         n += 2;
      }
   }
   return n;
}

/* UTF-16LE to UTF-8 into a NUL-terminated buffer. */
static void rsmb_utf8(const uint8_t *in, size_t in_len, char *out, size_t cap)
{
   size_t i = 0, o = 0;
   while (i + 1 < in_len && o + 4 < cap)
   {
      uint32_t cp = get16(in + i);
      i += 2;
      if (cp >= 0xd800 && cp < 0xdc00 && i + 1 < in_len)
      {
         uint32_t lo = get16(in + i);
         i += 2;
         cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
      }
      if (cp < 0x80)
         out[o++] = (char)cp;
      else if (cp < 0x800)
      {
         out[o++] = (char)(0xc0 | (cp >> 6));
         out[o++] = (char)(0x80 | (cp & 0x3f));
      }
      else if (cp < 0x10000)
      {
         out[o++] = (char)(0xe0 | (cp >> 12));
         out[o++] = (char)(0x80 | ((cp >> 6) & 0x3f));
         out[o++] = (char)(0x80 | (cp & 0x3f));
      }
      else
      {
         out[o++] = (char)(0xf0 | (cp >> 18));
         out[o++] = (char)(0x80 | ((cp >> 12) & 0x3f));
         out[o++] = (char)(0x80 | ((cp >> 6) & 0x3f));
         out[o++] = (char)(0x80 | (cp & 0x3f));
      }
   }
   out[o] = '\0';
}

/* Windows FILETIME (100 ns since 1601) to Unix seconds. */
static uint64_t rsmb_filetime(uint64_t ft)
{
   const uint64_t epoch = (uint64_t)116444736u * 1000000000u;
   if (ft < epoch)
      return 0;
   return (ft - epoch) / 10000000u;
}

/* ---- transport ---------------------------------------------------- */

/* Sends one NetBIOS-framed message. @msg lives in c->tx, which keeps
 * four spare octets ahead of it for the length prefix, so frame and
 * prefix leave in a single write: as two, the frame sat in Nagle's
 * algorithm behind the unacknowledged prefix until the server's
 * delayed ACK, 40 ms on every request. */
static int rsmb_send_raw(struct rsmb_ctx *c, const uint8_t *msg, size_t len)
{
   uint8_t *nb = (uint8_t*)msg - 4;
   nb[0] = 0;
   nb[1] = (uint8_t)(len >> 16);
   nb[2] = (uint8_t)(len >> 8);
   nb[3] = (uint8_t)len;
   if (!socket_send_all_blocking_with_timeout(c->fd, nb, 4 + len, (int)c->timeout * 1000, true))
   {
      rsmb_err(c, "send failed or timed out");
      return -1;
   }
   return 0;
}

/* Reads one NetBIOS-framed message into c->rx; returns its length. */
static int rsmb_recv_raw(struct rsmb_ctx *c, size_t *len)
{
   uint8_t nb[4];
   size_t  n;
   if (socket_receive_all_blocking_with_timeout(c->fd, nb, 4, (int)c->timeout * 1000) <= 0)
   {
      rsmb_err(c, "receive failed or timed out");
      return -1;
   }
   n = ((size_t)nb[1] << 16) | ((size_t)nb[2] << 8) | nb[3];
   if (nb[0] != 0 || n < SMB2_HDR_SIZE || n > c->buf_size)
   {
      rsmb_err(c, "bad frame");
      return -1;
   }
   if (socket_receive_all_blocking_with_timeout(c->fd, c->rx, n, (int)c->timeout * 1000) <= 0)
   {
      rsmb_err(c, "receive failed or timed out");
      return -1;
   }
   *len = n;
   return 0;
}

/* ---- signing and sealing ------------------------------------------ */

static void rsmb_sign(struct rsmb_ctx *c, uint8_t *msg, size_t len)
{
   uint8_t mac[32];
   put32(msg + 16, get32(msg + 16) | SMB2_FLAGS_SIGNED);
   memset(msg + 48, 0, 16);
   if (c->dialect >= DIALECT_300)
   {
      struct aes_ctx a;
      aes_init(&a, c->signing_key, 16);
      aes_cmac(&a, msg, len, mac);
      crypto_memzero(&a, sizeof(a));
   }
   else
      hmac_sha256(c->signing_key, 16, msg, len, mac);
   memcpy(msg + 48, mac, 16);
}

static int rsmb_verify_sig(struct rsmb_ctx *c, uint8_t *msg, size_t len)
{
   uint8_t got[16], mac[32];
   if (!(get32(msg + 16) & SMB2_FLAGS_SIGNED))
      return -1;
   memcpy(got, msg + 48, 16);
   memset(msg + 48, 0, 16);
   if (c->dialect >= DIALECT_300)
   {
      struct aes_ctx a;
      aes_init(&a, c->signing_key, 16);
      aes_cmac(&a, msg, len, mac);
      crypto_memzero(&a, sizeof(a));
   }
   else
      hmac_sha256(c->signing_key, 16, msg, len, mac);
   memcpy(msg + 48, got, 16);
   return crypto_memeq_ct(got, mac, 16) ? 0 : -1;
}

/* Wraps msg (len octets, in c->tx after the transform header slot)
 * in a TRANSFORM_HEADER; the sealed message is written in place. */
static int rsmb_seal(struct rsmb_ctx *c, uint8_t *frame, size_t msg_len)
{
   uint8_t *th  = frame;
   uint8_t *msg = frame + SMB2_TRANSFORM_HDR_SIZE;
   struct aes_ctx a;
   uint8_t tag[16];
   int r;

   memset(th, 0, SMB2_TRANSFORM_HDR_SIZE);
   th[0] = 0xfd; th[1] = 'S'; th[2] = 'M'; th[3] = 'B';
   /* nonce: counter, 11 or 12 octets used */
   put64(th + 20, ++c->nonce_counter);
   put32(th + 36, (uint32_t)msg_len);
   put16(th + 42, 1);                      /* Flags / EncryptionAlgorithm */
   put64(th + 44, c->session_id);

   if (aes_init(&a, c->enc_key, 16) != 0)
      return -1;
   if (c->cipher == CIPHER_AES128_GCM)
   {
      struct aes_gcm_ctx g;
      aes_gcm_init(&g, c->enc_key, 16);
      r = aes_gcm_encrypt(&g, th + 20, 12, th + 20, 32, msg, msg_len, msg, tag);
      crypto_memzero(&g, sizeof(g));
   }
   else
      r = aes_ccm_encrypt(&a, th + 20, 11, th + 20, 32, msg, msg_len, msg, tag, 16);
   crypto_memzero(&a, sizeof(a));
   if (r != 0)
      return -1;
   memcpy(th + 4, tag, 16);
   return 0;
}

/* Unseals c->rx in place; returns the plain message length. */
static int rsmb_unseal(struct rsmb_ctx *c, size_t *len)
{
   uint8_t *th  = c->rx;
   uint8_t *msg = c->rx + SMB2_TRANSFORM_HDR_SIZE;
   size_t   mlen;
   struct aes_ctx a;
   int r;

   if (*len < SMB2_TRANSFORM_HDR_SIZE)
      return -1;
   mlen = get32(th + 36);
   if (mlen != *len - SMB2_TRANSFORM_HDR_SIZE || get64(th + 44) != c->session_id)
      return -1;
   if (aes_init(&a, c->dec_key, 16) != 0)
      return -1;
   if (c->cipher == CIPHER_AES128_GCM)
   {
      struct aes_gcm_ctx g;
      aes_gcm_init(&g, c->dec_key, 16);
      r = aes_gcm_decrypt(&g, th + 20, 12, th + 20, 32, msg, mlen, th + 4, msg);
      crypto_memzero(&g, sizeof(g));
   }
   else
      r = aes_ccm_decrypt(&a, th + 20, 11, th + 20, 32, msg, mlen, th + 4, 16, msg);
   crypto_memzero(&a, sizeof(a));
   if (r != 0)
      return -1;
   memmove(c->rx, msg, mlen);
   *len = mlen;
   return 0;
}

/* ---- request / response ------------------------------------------- */

/* Builds a header for @cmd at c->tx (after the transform slot) and
 * returns a pointer to the body. */
static uint8_t *rsmb_begin(struct rsmb_ctx *c, unsigned cmd)
{
   uint8_t *h = c->tx + SMB2_TRANSFORM_HDR_SIZE;
   memset(h, 0, SMB2_HDR_SIZE);
   h[0] = 0xfe; h[1] = 'S'; h[2] = 'M'; h[3] = 'B';
   put16(h + 4, 64);
   put16(h + 6, (unsigned)(c->charge ? c->charge : 1));   /* CreditCharge */
   put16(h + 12, cmd);
   put16(h + 14, 64);                      /* CreditRequest: keep a pool */
   put64(h + 24, c->message_id);
   put32(h + 32, 0xfeff);                  /* ProcessId */
   put32(h + 36, c->tree_id);
   put64(h + 40, c->session_id);
   return h + SMB2_HDR_SIZE;
}

/* Sends the message built by rsmb_begin() with @body_len body octets
 * and reads the reply into c->rx; *body / *body_len describe the
 * reply body, c->status the NT status. Signs and seals per session
 * state; the preauth chain is fed when @preauth is set. */
static int rsmb_call(struct rsmb_ctx *c, size_t body_len,
      uint8_t **body, size_t *body_len_out, int preauth)
{
   uint8_t *msg  = c->tx + SMB2_TRANSFORM_HDR_SIZE;
   size_t   len  = SMB2_HDR_SIZE + body_len;
   size_t   rlen;
   uint64_t mid  = c->message_id;

   if (c->signing && c->session_ready && !c->encrypt)
      rsmb_sign(c, msg, len);
   if (preauth && c->dialect == DIALECT_311)
   {
      struct sha512_state s;
      sha512_stream_init(&s, 0);
      sha512_stream_update(&s, c->preauth, 64);
      sha512_stream_update(&s, msg, len);
      sha512_stream_final(&s, c->preauth);
   }
   /* a multi-credit message consumes as many ids as its charge */
   c->message_id += c->charge ? c->charge : 1;

   if (c->encrypt)
   {
      if (rsmb_seal(c, c->tx, len) != 0)
      {
         rsmb_err(c, "seal failed");
         return -1;
      }
      if (rsmb_send_raw(c, c->tx, SMB2_TRANSFORM_HDR_SIZE + len) != 0)
         return -1;
   }
   else if (rsmb_send_raw(c, msg, len) != 0)
      return -1;

   for (;;)
   {
      if (rsmb_recv_raw(c, &rlen) != 0)
         return -1;
      if (c->rx[0] == 0xfd)
      {
         if (!c->encrypt || rsmb_unseal(c, &rlen) != 0)
         {
            rsmb_err(c, "unseal failed");
            return -1;
         }
      }
      if (rlen < SMB2_HDR_SIZE || c->rx[0] != 0xfe)
      {
         rsmb_err(c, "bad response");
         return -1;
      }
      /* STATUS_PENDING interim replies carry our id and 0x103. */
      if (get64(c->rx + 24) != mid)
         continue;
      if (get32(c->rx + 8) == 0x00000103 && (get32(c->rx + 16) & 0x2))
         continue;
      break;
   }

   c->status = get32(c->rx + 8);
   /* credit balance: what this reply grants, less what the message spent */
   c->credits += (int)get16(c->rx + 14) - (int)(c->charge ? c->charge : 1);
   c->charge   = 0;
   if (c->signing && c->session_ready && !c->encrypt
         && rsmb_verify_sig(c, c->rx, rlen) != 0)
   {
      rsmb_err(c, "bad signature");
      return -1;
   }
   if (preauth && c->dialect == DIALECT_311
         && c->status == STATUS_MORE_PROCESSING_REQUIRED)
   {
      struct sha512_state s;
      sha512_stream_init(&s, 0);
      sha512_stream_update(&s, c->preauth, 64);
      sha512_stream_update(&s, c->rx, rlen);
      sha512_stream_final(&s, c->preauth);
   }
   *body         = c->rx + SMB2_HDR_SIZE;
   *body_len_out = rlen - SMB2_HDR_SIZE;
   return 0;
}

/* ---- negotiate ---------------------------------------------------- */

static int rsmb_negotiate(struct rsmb_ctx *c)
{
   uint8_t *b = rsmb_begin(c, SMB2_NEGOTIATE);
   uint8_t *r;
   size_t   rlen, off;
   static const unsigned dialects[] = { DIALECT_202, DIALECT_210, DIALECT_300, DIALECT_302, DIALECT_311 };
   unsigned i;

   memset(b, 0, 36);
   put16(b, 36);
   put16(b + 2, 5);                        /* DialectCount */
   put16(b + 4, 1);                        /* SecurityMode: signing enabled */
   put32(b + 8, 0x40);                     /* Capabilities: ENCRYPTION */
   crypto_random_bytes(b + 12, 16);        /* ClientGuid */
   for (i = 0; i < 5; i++)
      put16(b + 36 + 2 * i, dialects[i]);
   off = 36 + 10;
   /* 3.1.1 contexts, 8-byte aligned from the header start */
   while ((SMB2_HDR_SIZE + off) & 7)
      b[off++] = 0;
   put32(b + 28, (uint32_t)(SMB2_HDR_SIZE + off));   /* NegotiateContextOffset */
   put16(b + 32, 2);                                 /* NegotiateContextCount */
   /* PREAUTH_INTEGRITY_CAPABILITIES */
   put16(b + off, 1); put16(b + off + 2, 38); put32(b + off + 4, 0);
   put16(b + off + 8, 1); put16(b + off + 10, 32); put16(b + off + 12, 1);
   crypto_random_bytes(b + off + 14, 32);
   off += 8 + 38;
   while ((SMB2_HDR_SIZE + off) & 7)
      b[off++] = 0;
   /* ENCRYPTION_CAPABILITIES: GCM preferred, CCM */
   put16(b + off, 2); put16(b + off + 2, 6); put32(b + off + 4, 0);
   put16(b + off + 8, 2); put16(b + off + 10, CIPHER_AES128_GCM); put16(b + off + 12, CIPHER_AES128_CCM);
   off += 8 + 6;

   memset(c->preauth, 0, 64);
   c->dialect = DIALECT_311;   /* so the chain is fed; corrected below */
   if (rsmb_call(c, off, &r, &rlen, 1) != 0)
      return -1;
   if (c->status != STATUS_SUCCESS || rlen < 65)
   {
      rsmb_err(c, "negotiate refused");
      return -1;
   }
   c->dialect   = get16(r + 4);
   c->max_read  = get32(r + 32);
   c->max_write = get32(r + 36);
   /* Large I/O needs multi-credit requests, which 2.0.2 does not have. */
   {
      uint32_t cap = c->dialect == DIALECT_202 ? SMB2_MAX_IO : SMB2_LARGE_IO;
      if (c->max_read > cap)  c->max_read  = cap;
      if (c->max_write > cap) c->max_write = cap;
      if (c->max_read < SMB2_MAX_IO)  c->max_read  = SMB2_MAX_IO;
      if (c->max_write < SMB2_MAX_IO) c->max_write = SMB2_MAX_IO;
      c->io_size = c->max_read > c->max_write ? c->max_read : c->max_write;
   }
   c->signing   = 1;
   c->cipher    = 0;
   if (c->dialect == DIALECT_300 || c->dialect == DIALECT_302)
      c->cipher = CIPHER_AES128_CCM;
   if (c->dialect == DIALECT_311)
   {
      /* preauth: the response is chained too (status was SUCCESS so
       * rsmb_call did not do it) */
      struct sha512_state s;
      unsigned n = get16(r + 6);
      size_t   coff = get32(r + 60);
      sha512_stream_init(&s, 0);
      sha512_stream_update(&s, c->preauth, 64);
      sha512_stream_update(&s, c->rx, rlen + SMB2_HDR_SIZE);
      sha512_stream_final(&s, c->preauth);
      for (i = 0; i < n && coff + 8 <= rlen + SMB2_HDR_SIZE; i++)
      {
         const uint8_t *ctx = c->rx + coff;
         unsigned type = get16(ctx), dlen = get16(ctx + 2);
         if (type == 2 && dlen >= 4)
            c->cipher = get16(ctx + 10);
         coff += 8 + dlen;
         while (coff & 7)
            coff++;
      }
   }
   if (c->dialect != DIALECT_202 && c->dialect != DIALECT_210
         && c->dialect != DIALECT_300 && c->dialect != DIALECT_302
         && c->dialect != DIALECT_311)
   {
      rsmb_err(c, "unsupported dialect");
      return -1;
   }
   c->io_size = c->max_read > c->max_write ? c->max_read : c->max_write;
   /* The reply in c->rx is done with: grow the buffers to the
    * negotiated size (r pointed into the old one). */
   if (c->io_size > SMB2_MAX_IO)
   {
      /* grow the buffers to the negotiated size */
      size_t   need = SMB2_BUF_SIZE(c->io_size);
      uint8_t *nrx  = (uint8_t*)realloc(c->rx, need);
      uint8_t *ntx  = (uint8_t*)realloc(c->tx - 4, need + 4);
      if (nrx)
         c->rx = nrx;
      if (ntx)
         c->tx = ntx + 4;
      if (!nrx || !ntx)
      {
         /* stay at one credit per message */
         c->max_read = c->max_write = c->io_size = SMB2_MAX_IO;
      }
      else
         c->buf_size = need;
   }
   return 0;
}

/* ---- SPNEGO + NTLMSSP --------------------------------------------- */

static const uint8_t oid_spnego[] = {0x06,0x06,0x2b,0x06,0x01,0x05,0x05,0x02};
static const uint8_t oid_ntlmssp[] = {0x06,0x0a,0x2b,0x06,0x01,0x04,0x01,0x82,0x37,0x02,0x02,0x0a};

/* DER length: short form or 0x82 nn nn (nothing here exceeds 64K). */
static size_t der_len(uint8_t *p, size_t n)
{
   if (n < 0x80) { p[0] = (uint8_t)n; return 1; }
   if (n < 0x100) { p[0] = 0x81; p[1] = (uint8_t)n; return 2; }
   p[0] = 0x82; p[1] = (uint8_t)(n >> 8); p[2] = (uint8_t)n; return 3;
}

/* NegTokenInit: [APPLICATION 0] { OID spnego, [0] { SEQ { [0] mechTypes
 * { OID ntlmssp }, [2] mechToken OCTET STRING } } } */
static size_t spnego_init(uint8_t *out, const uint8_t *tok, size_t tok_len)
{
   uint8_t  tmp[16];
   size_t   mech_types = 2 + sizeof(oid_ntlmssp);          /* 30 len OID */
   size_t   a0 = 2 + mech_types;                           /* a0 len SEQ */
   size_t   octet = 1 + der_len(tmp, tok_len) + tok_len;   /* 04 len tok */
   size_t   a2 = 1 + der_len(tmp, octet) + octet;
   size_t   seq = a0 + a2;
   size_t   inner = 1 + der_len(tmp, seq) + seq;           /* 30 len ... */
   size_t   ctx0 = 1 + der_len(tmp, inner) + inner;        /* a0 len 30 */
   size_t   app = sizeof(oid_spnego) + ctx0;
   uint8_t *p = out;

   *p++ = 0x60; p += der_len(p, app);
   memcpy(p, oid_spnego, sizeof(oid_spnego)); p += sizeof(oid_spnego);
   *p++ = 0xa0; p += der_len(p, inner);
   *p++ = 0x30; p += der_len(p, seq);
   *p++ = 0xa0; *p++ = (uint8_t)mech_types;
   *p++ = 0x30; *p++ = (uint8_t)sizeof(oid_ntlmssp);
   memcpy(p, oid_ntlmssp, sizeof(oid_ntlmssp)); p += sizeof(oid_ntlmssp);
   *p++ = 0xa2; p += der_len(p, octet);
   *p++ = 0x04; p += der_len(p, tok_len);
   memcpy(p, tok, tok_len); p += tok_len;
   return (size_t)(p - out);
}

/* NegTokenResp: [1] { SEQ { [2] responseToken OCTET STRING } } */
static size_t spnego_resp(uint8_t *out, const uint8_t *tok, size_t tok_len)
{
   uint8_t  tmp[16];
   size_t   octet = 1 + der_len(tmp, tok_len) + tok_len;
   size_t   a2 = 1 + der_len(tmp, octet) + octet;
   size_t   seq = 1 + der_len(tmp, a2) + a2;
   uint8_t *p = out;
   *p++ = 0xa1; p += der_len(p, seq);
   *p++ = 0x30; p += der_len(p, a2);
   *p++ = 0xa2; p += der_len(p, octet);
   *p++ = 0x04; p += der_len(p, tok_len);
   memcpy(p, tok, tok_len); p += tok_len;
   return (size_t)(p - out);
}

/* Finds the NTLMSSP token inside whatever SPNEGO wrapping the server
 * used, by its signature; the token runs to the end of the blob's
 * enclosing OCTET STRING, which is the end of the blob in practice. */
static const uint8_t *spnego_find_ntlm(const uint8_t *blob, size_t len, size_t *tok_len)
{
   size_t i;
   for (i = 0; i + 8 <= len; i++)
      if (memcmp(blob + i, "NTLMSSP", 8) == 0)
      {
         *tok_len = len - i;
         return blob + i;
      }
   return NULL;
}

static int rsmb_session_setup(struct rsmb_ctx *c)
{
   uint8_t *b, *r;
   size_t   rlen, blen;
   uint8_t  neg[40];
   uint8_t *chal = NULL;
   size_t   chal_len = 0;
   uint8_t  server_chal[8];
   uint32_t chal_flags;
   const uint8_t *tinfo = NULL;
   size_t   tinfo_len = 0;
   uint8_t  ntowf[16];
   uint8_t  ntproof[16];
   uint8_t  session_base[16];
   uint8_t  client_chal[8];
   uint8_t *auth, *blob, *nt_resp;
   size_t   auth_len, nt_len, user_len, dom_len, blob_len;
   uint8_t  mic[16];
   uint64_t ft;
   int      ret = -1;

   /* --- NEGOTIATE_MESSAGE --- */
   memset(neg, 0, sizeof(neg));
   memcpy(neg, "NTLMSSP", 8);
   put32(neg + 8, 1);
   /* No NTLMSSP_NEGOTIATE_SIGN: SMB2 signs with the session key
    * itself, and asking NTLMSSP for integrity makes SPNEGO demand a
    * third leg carrying a mechListMIC (NTLMSSP signatures, RC4). */
   put32(neg + 12, NTLMSSP_NEGOTIATE_UNICODE | NTLMSSP_REQUEST_TARGET
         | NTLMSSP_NEGOTIATE_NTLM | NTLMSSP_NEGOTIATE_ALWAYS_SIGN | NTLMSSP_NEGOTIATE_EXT_SESSSEC
         | NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_56);
   put32(neg + 20, 40); put32(neg + 28, 40);   /* empty domain / workstation at offset 40 */

   b = rsmb_begin(c, SMB2_SESSION_SETUP);
   memset(b, 0, 24);
   put16(b, 25);
   b[3] = 1;                                   /* SecurityMode: signing enabled */
   put16(b + 12, SMB2_HDR_SIZE + 24);
   blen = spnego_init(b + 24, neg, 32);
   put16(b + 14, (unsigned)blen);
   if (rsmb_call(c, 24 + blen, &r, &rlen, 1) != 0)
      return -1;
   if (c->status != STATUS_MORE_PROCESSING_REQUIRED || rlen < 8)
   {
      rsmb_err(c, "session setup refused");
      return -1;
   }
   c->session_id = get64(c->rx + 40);
   {
      size_t boff = get16(r + 4), bl = get16(r + 6);
      if (boff < SMB2_HDR_SIZE || boff + bl > rlen + SMB2_HDR_SIZE)
      {
         rsmb_err(c, "bad challenge");
         return -1;
      }
      chal = (uint8_t*)spnego_find_ntlm(c->rx + boff, bl, &chal_len);
   }
   if (!chal || chal_len < 56 || get32(chal + 8) != 2)
   {
      rsmb_err(c, "bad challenge");
      return -1;
   }
   /* --- CHALLENGE_MESSAGE --- */
   memcpy(server_chal, chal + 24, 8);
   chal_flags = get32(chal + 20);
   if (chal_flags & NTLMSSP_NEGOTIATE_TARGET_INFO)
   {
      tinfo_len = get16(chal + 40);
      {
         size_t toff = get32(chal + 44);
         if (toff + tinfo_len > chal_len)
         {
            rsmb_err(c, "bad challenge");
            return -1;
         }
         tinfo = chal + toff;
      }
   }
   /* keep a copy: the AUTHENTICATE build reuses c->tx */
   {
      uint8_t *keep = (uint8_t*)malloc(chal_len + tinfo_len + 1);
      if (!keep)
         return -1;
      memcpy(keep, chal, chal_len);
      chal = keep;
   }
   /* tinfo pointed into c->rx; point it at the copy */
   if (tinfo_len)
      tinfo = chal + get32(chal + 44);

   /* --- AUTHENTICATE_MESSAGE --- */
   if (ntlm_ntowf_v2(c->password ? c->password : "", c->user ? c->user : "",
            c->domain ? c->domain : "", ntowf) != 0)
   {
      free(chal);
      rsmb_err(c, "bad credentials");
      return -1;
   }
   crypto_random_bytes(client_chal, 8);
   ft = ((uint64_t)time(NULL) + (uint64_t)11644473600u) * 10000000u;

   /* NTLMv2 blob: 01 01 00000000 time(8) clientchal(8) 00000000 targetinfo [+ AV_FLAGS MIC] 00000000 */
   {
      size_t bl = 28 + tinfo_len + 8 + 4;   /* + MsvAvFlags(4+4) before EOL, + trailing 4 */
      auth = (uint8_t*)malloc(88 + 24 + 16 + bl + 1024);
      if (!auth)
      {
         free(chal);
         return -1;
      }
      blob = auth + 88 + 24 + 16;  /* NT response goes right after LM (24) and NTProof (16) */
      memset(blob, 0, bl);
      blob[0] = 1; blob[1] = 1;
      put64(blob + 8, ft);
      memcpy(blob + 16, client_chal, 8);
      /* target info with MsvAvFlags (0x0002 = MIC present) inserted before MsvAvEOL */
      if (tinfo_len >= 4)
      {
         memcpy(blob + 28, tinfo, tinfo_len - 4);        /* all but EOL */
         put16(blob + 28 + tinfo_len - 4, 6);              /* MsvAvFlags */
         put16(blob + 28 + tinfo_len - 2, 4);
         put32(blob + 28 + tinfo_len, 0x2);
         put32(blob + 28 + tinfo_len + 4, 0);              /* MsvAvEOL */
         blob_len = 28 + tinfo_len + 8 + 4;
      }
      else
      {
         put32(blob + 28, 0);
         blob_len = 28 + 4 + 4;
      }
      /* NTProofStr = HMAC-MD5(ntowf, serverchal || blob) */
      {
         uint8_t *tmp = (uint8_t*)malloc(8 + blob_len);
         if (!tmp)
         {
            free(auth); free(chal);
            return -1;
         }
         memcpy(tmp, server_chal, 8);
         memcpy(tmp + 8, blob, blob_len);
         hmac_md5(ntowf, 16, tmp, 8 + blob_len, ntproof);
         free(tmp);
      }
      hmac_md5(ntowf, 16, ntproof, 16, session_base);
      memcpy(c->session_key, session_base, 16);
   }

   /* message layout: header 88 (with MIC), LM 24 zeros, NT = proof+blob, domain, user, workstation */
   nt_resp = auth + 88 + 24;
   memcpy(nt_resp, ntproof, 16);
   nt_len = 16 + blob_len;
   {
      uint8_t *p = auth + 88 + 24 + nt_len;
      size_t  off = 88 + 24 + nt_len;
      dom_len  = rsmb_utf16(c->domain, p, 512, 0);  p += dom_len;  off += dom_len;
      user_len = rsmb_utf16(c->user, p, 512, 0);    p += user_len; off += user_len;
      auth_len = off;

      memset(auth, 0, 88);
      memcpy(auth, "NTLMSSP", 8);
      put32(auth + 8, 3);
      put16(auth + 12, 24); put16(auth + 14, 24); put32(auth + 16, 88);            /* LM */
      put16(auth + 20, (unsigned)nt_len); put16(auth + 22, (unsigned)nt_len); put32(auth + 24, 88 + 24);
      put16(auth + 28, (unsigned)dom_len); put16(auth + 30, (unsigned)dom_len); put32(auth + 32, (uint32_t)(88 + 24 + nt_len));
      put16(auth + 36, (unsigned)user_len); put16(auth + 38, (unsigned)user_len); put32(auth + 40, (uint32_t)(88 + 24 + nt_len + dom_len));
      put16(auth + 44, 0); put16(auth + 46, 0); put32(auth + 48, (uint32_t)auth_len);   /* workstation */
      put16(auth + 52, 0); put16(auth + 54, 0); put32(auth + 56, (uint32_t)auth_len);   /* session key */
      put32(auth + 60, (chal_flags & ~(uint32_t)0x40000000) | NTLMSSP_NEGOTIATE_UNICODE);
      memset(auth + 88, 0, 24);   /* LM response: zeros */
      /* MIC over NEGOTIATE || CHALLENGE || AUTHENTICATE(MIC = 0) */
      {
         uint8_t *tmp = (uint8_t*)malloc(32 + chal_len + auth_len);
         if (!tmp)
         {
            free(auth); free(chal);
            return -1;
         }
         memcpy(tmp, neg, 32);
         memcpy(tmp + 32, chal, chal_len);
         memcpy(tmp + 32 + chal_len, auth, auth_len);
         hmac_md5(c->session_key, 16, tmp, 32 + chal_len + auth_len, mic);
         free(tmp);
         memcpy(auth + 72, mic, 16);
      }
   }

   b = rsmb_begin(c, SMB2_SESSION_SETUP);
   memset(b, 0, 24);
   put16(b, 25);
   b[3] = 1;
   put16(b + 12, SMB2_HDR_SIZE + 24);
   blen = spnego_resp(b + 24, auth, auth_len);
   put16(b + 14, (unsigned)blen);
   free(auth);
   free(chal);
   crypto_memzero(ntowf, sizeof(ntowf));

   if (rsmb_call(c, 24 + blen, &r, &rlen, 1) != 0)
      goto done;
   if (c->status == STATUS_LOGON_FAILURE)
   {
      rsmb_err(c, "logon failure");
      goto done;
   }
   if (c->status != STATUS_SUCCESS || rlen < 8)
   {
      rsmb_err(c, "session setup failed");
      goto done;
   }

   /* keys */
   if (c->dialect == DIALECT_311)
   {
      kbkdf_hmac_sha256(c->session_key, 16, (const uint8_t*)"SMBSigningKey", 14, c->preauth, 64, c->signing_key, 16);
      kbkdf_hmac_sha256(c->session_key, 16, (const uint8_t*)"SMBC2SCipherKey", 16, c->preauth, 64, c->enc_key, 16);
      kbkdf_hmac_sha256(c->session_key, 16, (const uint8_t*)"SMBS2CCipherKey", 16, c->preauth, 64, c->dec_key, 16);
   }
   else if (c->dialect >= DIALECT_300)
   {
      kbkdf_hmac_sha256(c->session_key, 16, (const uint8_t*)"SMB2AESCMAC", 12, (const uint8_t*)"SmbSign", 8, c->signing_key, 16);
      kbkdf_hmac_sha256(c->session_key, 16, (const uint8_t*)"SMB2AESCCM", 11, (const uint8_t*)"ServerIn ", 10, c->enc_key, 16);
      kbkdf_hmac_sha256(c->session_key, 16, (const uint8_t*)"SMB2AESCCM", 11, (const uint8_t*)"ServerOut", 10, c->dec_key, 16);
   }
   else
      memcpy(c->signing_key, c->session_key, 16);

   c->session_ready = 1;

   /* SessionFlags. A guest (0x0001) or anonymous/null (0x0002) session
    * is not signed even when the server requires signing, so signing
    * is turned off for it and the final response is not checked - the
    * server returns SUCCESS on it unsigned, which would otherwise read
    * as a bad signature (MS-SMB2 3.2.5.3.1). */
   if (get16(r + 2) & 0x3)
      c->signing = 0;
   if ((get16(r + 2) & 0x4) && c->cipher)
      c->encrypt = 1;

   /* the final response is signed with the new key: check it now */
   if (c->signing && rsmb_verify_sig(c, c->rx, rlen + SMB2_HDR_SIZE) != 0)
   {
      rsmb_err(c, "bad signature");
      goto done;
   }
   ret = 0;
done:
   crypto_memzero(session_base, sizeof(session_base));
   return ret;
}

/* ---- tree connect ------------------------------------------------- */

static int rsmb_tree_connect(struct rsmb_ctx *c, const char *share)
{
   uint8_t *b = rsmb_begin(c, SMB2_TREE_CONNECT);
   uint8_t *r;
   size_t   rlen, plen;
   char     path[512];
   size_t   pl;

   /* \\server\share */
   pl = strlcpy(path, "\\\\", sizeof(path));
   pl = strlcat(path, c->server, sizeof(path));
   pl = strlcat(path, "\\", sizeof(path));
   pl = strlcat(path, share, sizeof(path));
   (void)pl;
   memset(b, 0, 8);
   put16(b, 9);
   put16(b + 4, SMB2_HDR_SIZE + 8);
   plen = rsmb_utf16(path, b + 8, 1024, 0);
   put16(b + 6, (unsigned)plen);
   if (rsmb_call(c, 8 + plen, &r, &rlen, 0) != 0)
      return -1;
   if (c->status != STATUS_SUCCESS || rlen < 16)
   {
      rsmb_err(c, "tree connect refused");
      return -1;
   }
   c->tree_id = get32(c->rx + 36);
   if ((get32(r + 4) & 0x8000) && c->cipher)   /* SMB2_SHAREFLAG_ENCRYPT_DATA */
      c->encrypt = 1;
   return 0;
}

/* ---- public: context and connection ------------------------------- */

struct rsmb_ctx *rsmb_new(void)
{
   struct rsmb_ctx *c = (struct rsmb_ctx*)calloc(1, sizeof(*c));
   if (!c)
      return NULL;
   c->buf_size = SMB2_RX_SIZE;
   c->credits  = 1;
   c->rx = (uint8_t*)malloc(SMB2_RX_SIZE);
   /* four octets ahead of the frame for the NetBIOS length prefix */
   c->tx = (uint8_t*)malloc(SMB2_RX_SIZE + 4);
   if (c->tx)
      c->tx += 4;
   if (!c->rx || !c->tx)
   {
      rsmb_free(c);
      return NULL;
   }
   c->fd      = -1;
   c->timeout = 10;
   return c;
}

static char *rsmb_dup(const char *s)
{
   size_t n;
   char  *d;
   if (!s)
      return NULL;
   n = strlen(s);
   d = (char*)malloc(n + 1);
   if (d)
      memcpy(d, s, n + 1);
   return d;
}

void rsmb_set_credentials(struct rsmb_ctx *c, const char *user,
      const char *password, const char *domain)
{
   free(c->user);
   if (c->password)
      crypto_memzero(c->password, strlen(c->password));
   free(c->password);
   free(c->domain);
   c->user     = rsmb_dup(user);
   c->password = rsmb_dup(password);
   c->domain   = rsmb_dup(domain);
}

static void rsmb_set_str(char **slot, const char *v, int secret)
{
   if (*slot && secret)
      crypto_memzero(*slot, strlen(*slot));
   free(*slot);
   *slot = rsmb_dup(v);
}

void rsmb_set_user(struct rsmb_ctx *c, const char *user)         { rsmb_set_str(&c->user, user, 0); }
void rsmb_set_password(struct rsmb_ctx *c, const char *password) { rsmb_set_str(&c->password, password, 1); }
void rsmb_set_domain(struct rsmb_ctx *c, const char *domain)     { rsmb_set_str(&c->domain, domain, 0); }

void rsmb_set_port(struct rsmb_ctx *c, uint16_t port) { c->port = port; }

void rsmb_set_timeout(struct rsmb_ctx *c, unsigned seconds)
{
   c->timeout = seconds ? seconds : 10;
}

int rsmb_connect(struct rsmb_ctx *c, const char *server, const char *share)
{
   struct addrinfo *addr = NULL;
   int fd;

   rsmb_disconnect(c);
   strlcpy(c->server, server, sizeof(c->server));
   fd = socket_init((void**)&addr, c->port ? c->port : SMB2_PORT, server, SOCKET_TYPE_STREAM, AF_UNSPEC);
   if (fd < 0 || !addr)
   {
      rsmb_err(c, "cannot resolve server");
      if (addr)
         freeaddrinfo_retro(addr);
      return -1;
   }
   if (!socket_connect_with_timeout(fd, addr, (int)c->timeout * 1000))
   {
      freeaddrinfo_retro(addr);
      socket_close(fd);
      rsmb_err(c, "connect failed");
      return -1;
   }
   freeaddrinfo_retro(addr);
   /* Non-blocking, so every send and receive is bounded by the
    * context timeout instead of waiting on a server forever. */
   socket_set_block(fd, false);
   c->fd            = fd;
   c->message_id    = 0;
   c->credits       = 1;
   c->charge        = 0;
   c->io_size       = SMB2_MAX_IO;
   c->session_id    = 0;
   c->tree_id       = 0;
   c->signing       = 0;
   c->session_ready = 0;
   c->encrypt       = 0;
   c->nonce_counter = 0;

   if (rsmb_negotiate(c) != 0 || rsmb_session_setup(c) != 0
         || rsmb_tree_connect(c, share) != 0)
   {
      socket_close(c->fd);
      c->fd = -1;
      return -1;
   }
   c->connected = 1;
   return 0;
}

void rsmb_disconnect(struct rsmb_ctx *c)
{
   uint8_t *r;
   size_t   rlen;
   if (c->fd < 0)
      return;
   if (c->connected)
   {
      uint8_t *b = rsmb_begin(c, SMB2_TREE_DISCONNECT);
      put16(b, 4); put16(b + 2, 0);
      rsmb_call(c, 4, &r, &rlen, 0);
      c->tree_id = 0;
      b = rsmb_begin(c, SMB2_LOGOFF);
      put16(b, 4); put16(b + 2, 0);
      rsmb_call(c, 4, &r, &rlen, 0);
   }
   socket_close(c->fd);
   c->fd        = -1;
   c->connected = 0;
   crypto_memzero(c->session_key, 16);
   crypto_memzero(c->signing_key, 16);
   crypto_memzero(c->enc_key, 16);
   crypto_memzero(c->dec_key, 16);
}

void rsmb_free(struct rsmb_ctx *c)
{
   if (!c)
      return;
   rsmb_disconnect(c);
   rsmb_set_credentials(c, NULL, NULL, NULL);
   free(c->rx);
   if (c->tx)
      free(c->tx - 4);
   free(c);
}

const char *rsmb_get_error(const struct rsmb_ctx *c) { return c->error; }
uint32_t    rsmb_get_status(const struct rsmb_ctx *c) { return c->status; }
int         rsmb_get_fd(const struct rsmb_ctx *c)     { return c->fd; }

/* ---- files -------------------------------------------------------- */

/* CREATE; on success fills @fid and @size / @is_dir when wanted. */
static int rsmb_create(struct rsmb_ctx *c, const char *path,
      uint32_t access, uint32_t disposition, uint32_t options,
      uint8_t *fid, uint64_t *size, int *is_dir)
{
   uint8_t *b = rsmb_begin(c, SMB2_CREATE);
   uint8_t *r;
   size_t   rlen, nlen;

   while (*path == '/' || *path == '\\')
      path++;
   memset(b, 0, 56);
   put16(b, 57);
   b[3] = 0;                                 /* oplock: none */
   put32(b + 4, 2);                          /* impersonation: impersonation */
   put32(b + 24, access);
   put32(b + 28, 0x80);                      /* FILE_ATTRIBUTE_NORMAL */
   put32(b + 32, SMB2_FILE_SHARE_ALL);
   put32(b + 36, disposition);
   put32(b + 40, options);
   put16(b + 44, SMB2_HDR_SIZE + 56);
   nlen = rsmb_utf16(path, b + 56, 2048, 1);
   put16(b + 46, (unsigned)nlen);
   if (!nlen)
      b[56] = 0;                             /* empty name: the share root */
   if (rsmb_call(c, 56 + (nlen ? nlen : 1), &r, &rlen, 0) != 0)
      return -1;
   if (c->status != STATUS_SUCCESS || rlen < 88)
   {
      rsmb_err(c, c->status == STATUS_OBJECT_NAME_NOT_FOUND ? "not found" : "open refused");
      return -1;
   }
   memcpy(fid, r + 64, 16);
   if (size)
      *size = get64(r + 48);
   if (is_dir)
      *is_dir = (get32(r + 56) & SMB2_FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
   return 0;
}

static int rsmb_close_fid(struct rsmb_ctx *c, const uint8_t *fid)
{
   uint8_t *b = rsmb_begin(c, SMB2_CLOSE);
   uint8_t *r;
   size_t   rlen;
   memset(b, 0, 24);
   put16(b, 24);
   memcpy(b + 8, fid, 16);
   if (rsmb_call(c, 24, &r, &rlen, 0) != 0)
      return -1;
   return c->status == STATUS_SUCCESS ? 0 : -1;
}

struct rsmb_file *rsmb_open(struct rsmb_ctx *c, const char *path, int flags)
{
   struct rsmb_file *f;
   uint32_t access = SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE;
   uint32_t disp   = SMB2_FILE_OPEN;
   uint64_t size   = 0;

   if ((flags & 3) == RSMB_O_RDONLY || (flags & 3) == RSMB_O_RDWR)
      access |= SMB2_FILE_READ_DATA | SMB2_FILE_READ_EA;
   if ((flags & 3) == RSMB_O_WRONLY || (flags & 3) == RSMB_O_RDWR)
      access |= SMB2_FILE_WRITE_DATA | SMB2_FILE_APPEND_DATA | SMB2_FILE_WRITE_ATTRIBUTES;
   if (flags & RSMB_O_CREAT)
      disp = (flags & RSMB_O_TRUNC) ? SMB2_FILE_OVERWRITE_IF : SMB2_FILE_OPEN_IF;
   else if (flags & RSMB_O_TRUNC)
      disp = SMB2_FILE_OVERWRITE_IF;

   if (!(f = (struct rsmb_file*)calloc(1, sizeof(*f))))
      return NULL;
   if (rsmb_create(c, path, access, disp, SMB2_FILE_NON_DIRECTORY_FILE, f->fid, &size, NULL) != 0)
   {
      free(f);
      return NULL;
   }
   f->size = size;
   return f;
}

int64_t rsmb_read(struct rsmb_ctx *c, struct rsmb_file *f, void *buf, size_t len)
{
   uint8_t *out  = (uint8_t*)buf;
   size_t   done = 0;

   while (done < len)
   {
      uint8_t *b;
      uint8_t *r;
      size_t   rlen, n = len - done;
      uint32_t got, doff;
      if (n > c->max_read)
         n = c->max_read;
      /* one credit per 64 KiB; never more than the server has granted */
      c->charge = (unsigned)((n + SMB2_MAX_IO - 1) / SMB2_MAX_IO);
      if (c->charge > 1 && c->credits < (int)c->charge)
      {
         c->charge = c->credits > 0 ? (unsigned)c->credits : 1;
         n = (size_t)c->charge * SMB2_MAX_IO;
      }
      b = rsmb_begin(c, SMB2_READ);
      memset(b, 0, 49);
      put16(b, 49);
      b[2] = 0x50;                          /* Padding */
      put32(b + 4, (uint32_t)n);
      put64(b + 8, f->offset);
      memcpy(b + 16, f->fid, 16);
      if (rsmb_call(c, 49, &r, &rlen, 0) != 0)
         return -1;
      if (c->status == STATUS_END_OF_FILE)
         break;
      if (c->status != STATUS_SUCCESS || rlen < 16)
      {
         rsmb_err(c, "read failed");
         return -1;
      }
      doff = r[2];
      got  = get32(r + 4);
      if (doff < SMB2_HDR_SIZE || doff + got > rlen + SMB2_HDR_SIZE)
      {
         rsmb_err(c, "bad read reply");
         return -1;
      }
      memcpy(out + done, c->rx + doff, got);
      done      += got;
      f->offset += got;
      if (got < n)
         break;
   }
   return (int64_t)done;
}

int64_t rsmb_write(struct rsmb_ctx *c, struct rsmb_file *f, const void *buf, size_t len)
{
   const uint8_t *in = (const uint8_t*)buf;
   size_t done = 0;

   while (done < len)
   {
      uint8_t *b;
      uint8_t *r;
      size_t   rlen, n = len - done;
      uint32_t count;
      if (n > c->max_write)
         n = c->max_write;
      c->charge = (unsigned)((n + SMB2_MAX_IO - 1) / SMB2_MAX_IO);
      if (c->charge > 1 && c->credits < (int)c->charge)
      {
         c->charge = c->credits > 0 ? (unsigned)c->credits : 1;
         n = (size_t)c->charge * SMB2_MAX_IO;
      }
      b = rsmb_begin(c, SMB2_WRITE);
      memset(b, 0, 48);
      put16(b, 49);
      put16(b + 2, SMB2_HDR_SIZE + 48);
      put32(b + 4, (uint32_t)n);
      put64(b + 8, f->offset);
      memcpy(b + 16, f->fid, 16);
      memcpy(b + 48, in + done, n);
      if (rsmb_call(c, 48 + n, &r, &rlen, 0) != 0)
         return -1;
      if (c->status != STATUS_SUCCESS || rlen < 16)
      {
         rsmb_err(c, "write failed");
         return -1;
      }
      count      = get32(r + 4);
      done      += count;
      f->offset += count;
      if (f->offset > f->size)
         f->size = f->offset;
      if (count < n)
         break;
   }
   return (int64_t)done;
}

int64_t rsmb_seek(struct rsmb_ctx *c, struct rsmb_file *f, int64_t off, int whence)
{
   int64_t base = whence == 0 ? 0 : whence == 1 ? (int64_t)f->offset : (int64_t)f->size;
   (void)c;
   if (whence < 0 || whence > 2 || base + off < 0)
      return -1;
   f->offset = (uint64_t)(base + off);
   return (int64_t)f->offset;
}

int64_t rsmb_tell(const struct rsmb_file *f)
{
   return (int64_t)f->offset;
}

int rsmb_close(struct rsmb_ctx *c, struct rsmb_file *f)
{
   int r;
   if (!f)
      return -1;
   r = rsmb_close_fid(c, f->fid);
   free(f);
   return r;
}

int rsmb_ftruncate(struct rsmb_ctx *c, struct rsmb_file *f, uint64_t size)
{
   /* SET_INFO FileEndOfFileInformation (class 20) */
   uint8_t *b = rsmb_begin(c, SMB2_SET_INFO);
   uint8_t *r;
   size_t   rlen;
   memset(b, 0, 40);
   put16(b, 33);
   b[2] = 1;
   b[3] = 20;
   put32(b + 4, 8);
   put16(b + 8, SMB2_HDR_SIZE + 32);
   memcpy(b + 16, f->fid, 16);
   put64(b + 32, size);
   if (rsmb_call(c, 40, &r, &rlen, 0) != 0 || c->status != STATUS_SUCCESS)
   {
      rsmb_err(c, "truncate failed");
      return -1;
   }
   f->size = size;
   if (f->offset > size)
      f->offset = size;
   return 0;
}

uint32_t rsmb_max_read(const struct rsmb_ctx *c)  { return c->max_read; }
uint32_t rsmb_max_write(const struct rsmb_ctx *c) { return c->max_write; }

int rsmb_stat(struct rsmb_ctx *c, const char *path, struct rsmb_stat *st)
{
   uint8_t  fid[16];
   uint64_t size = 0;
   int      is_dir = 0;
   if (rsmb_create(c, path, SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE, SMB2_FILE_OPEN, 0,
            fid, &size, &is_dir) != 0)
      return -1;
   /* mtime from the CREATE reply: LastWriteTime at offset 32 */
   st->size   = size;
   st->is_dir = is_dir;
   st->mtime  = rsmb_filetime(get64(c->rx + SMB2_HDR_SIZE + 32));
   rsmb_close_fid(c, fid);
   return 0;
}

int rsmb_unlink(struct rsmb_ctx *c, const char *path)
{
   uint8_t fid[16];
   if (rsmb_create(c, path, SMB2_DELETE | SMB2_SYNCHRONIZE, SMB2_FILE_OPEN,
            SMB2_FILE_DELETE_ON_CLOSE, fid, NULL, NULL) != 0)
      return -1;
   return rsmb_close_fid(c, fid);
}

int rsmb_mkdir(struct rsmb_ctx *c, const char *path)
{
   uint8_t fid[16];
   if (rsmb_create(c, path, SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE,
            SMB2_FILE_CREATE, SMB2_FILE_DIRECTORY_FILE, fid, NULL, NULL) != 0)
      return -1;
   return rsmb_close_fid(c, fid);
}

int rsmb_rename(struct rsmb_ctx *c, const char *from, const char *to)
{
   /* SET_INFO FileRenameInformation on a handle opened with DELETE */
   uint8_t  fid[16];
   uint8_t *b, *r;
   size_t   rlen, nlen;
   int      ret = -1;

   if (rsmb_create(c, from, SMB2_DELETE | SMB2_SYNCHRONIZE, SMB2_FILE_OPEN, 0,
            fid, NULL, NULL) != 0)
      return -1;
   while (*to == '/' || *to == '\\')
      to++;
   b = rsmb_begin(c, SMB2_SET_INFO);
   memset(b, 0, 32);
   put16(b, 33);
   b[2] = 1;                                  /* InfoType: FILE */
   b[3] = 10;                                 /* FileRenameInformation */
   put16(b + 8, SMB2_HDR_SIZE + 32);          /* BufferOffset */
   memcpy(b + 16, fid, 16);
   memset(b + 32, 0, 20);
   b[32] = 1;                                 /* ReplaceIfExists */
   nlen = rsmb_utf16(to, b + 52, 2048, 1);
   put32(b + 48, (uint32_t)nlen);
   put32(b + 4, (uint32_t)(20 + nlen));       /* BufferLength */
   if (rsmb_call(c, 52 + nlen, &r, &rlen, 0) == 0 && c->status == STATUS_SUCCESS)
      ret = 0;
   else
      rsmb_err(c, "rename failed");
   rsmb_close_fid(c, fid);
   return ret;
}

/* ---- directories -------------------------------------------------- */

struct rsmb_dir *rsmb_opendir(struct rsmb_ctx *c, const char *path)
{
   struct rsmb_dir *d = (struct rsmb_dir*)calloc(1, sizeof(*d));
   if (!d)
      return NULL;
   if (rsmb_create(c, path, SMB2_FILE_LIST_DIRECTORY | SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE,
            SMB2_FILE_OPEN, SMB2_FILE_DIRECTORY_FILE, d->fid, NULL, NULL) != 0)
   {
      free(d);
      return NULL;
   }
   d->buf = (uint8_t*)malloc(SMB2_MAX_IO);
   if (!d->buf)
   {
      rsmb_close_fid(c, d->fid);
      free(d);
      return NULL;
   }
   return d;
}

/* One QUERY_DIRECTORY round: FileDirectoryInformation entries. */
static int rsmb_query_dir(struct rsmb_ctx *c, struct rsmb_dir *d)
{
   uint8_t *b = rsmb_begin(c, SMB2_QUERY_DIRECTORY);
   uint8_t *r;
   size_t   rlen;
   uint32_t ooff, olen;

   memset(b, 0, 32);
   put16(b, 33);
   b[2] = 0x01;                              /* FileDirectoryInformation */
   b[3] = 0;                                 /* Flags */
   memcpy(b + 8, d->fid, 16);
   put16(b + 24, SMB2_HDR_SIZE + 32);
   put16(b + 26, 2);
   put32(b + 28, SMB2_MAX_IO);
   put16(b + 32, '*');
   if (rsmb_call(c, 34, &r, &rlen, 0) != 0)
      return -1;
   if (c->status == STATUS_NO_MORE_FILES)
   {
      d->done = 1;
      return 0;
   }
   if (c->status != STATUS_SUCCESS || rlen < 8)
   {
      rsmb_err(c, "query directory failed");
      return -1;
   }
   ooff = get16(r + 2);
   olen = get32(r + 4);
   if (ooff < SMB2_HDR_SIZE || ooff + olen > rlen + SMB2_HDR_SIZE || olen > SMB2_MAX_IO)
   {
      rsmb_err(c, "bad directory reply");
      return -1;
   }
   memcpy(d->buf, c->rx + ooff, olen);
   d->buf_len = olen;
   d->buf_off = 0;
   return 0;
}

const struct rsmb_dirent *rsmb_readdir(struct rsmb_ctx *c, struct rsmb_dir *d)
{
   for (;;)
   {
      const uint8_t *e;
      uint32_t next, name_len;

      if (d->buf_off >= d->buf_len)
      {
         if (d->done || rsmb_query_dir(c, d) != 0 || d->done)
            return NULL;
         if (d->buf_len == 0)
            return NULL;
      }
      e = d->buf + d->buf_off;
      if (d->buf_len - d->buf_off < 64)
         return NULL;
      next     = get32(e);
      name_len = get32(e + 60);
      if (64 + name_len > d->buf_len - d->buf_off)
         return NULL;
      d->ent.st.size   = get64(e + 40);
      d->ent.st.mtime  = rsmb_filetime(get64(e + 24));
      d->ent.st.is_dir = (get32(e + 56) & SMB2_FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
      rsmb_utf8(e + 64, name_len, d->ent.name, sizeof(d->ent.name));
      d->buf_off = next ? d->buf_off + next : d->buf_len;
      if (strcmp(d->ent.name, ".") == 0 || strcmp(d->ent.name, "..") == 0)
         continue;
      return &d->ent;
   }
}

void rsmb_closedir(struct rsmb_ctx *c, struct rsmb_dir *d)
{
   if (!d)
      return;
   rsmb_close_fid(c, d->fid);
   free(d->buf);
   free(d);
}

int rsmb_echo(struct rsmb_ctx *c)
{
   uint8_t *b = rsmb_begin(c, SMB2_ECHO);
   uint8_t *r;
   size_t   rlen;
   if (c->fd < 0)
      return -1;
   put16(b, 4); put16(b + 2, 0);
   if (rsmb_call(c, 4, &r, &rlen, 0) != 0)
      return -1;
   return c->status == STATUS_SUCCESS ? 0 : -1;
}

/* ---- SRVSVC NetrShareEnum over the srvsvc pipe --------------------- */

/* DCERPC on the named pipe: a bind, then one request; both PDUs and
 * the NDR stub are built and picked apart by hand, this being the one
 * RPC the frontend makes. */
static int rsmb_pipe_transceive(struct rsmb_ctx *c, struct rsmb_file *pipe,
      const uint8_t *req, size_t req_len, uint8_t *resp, size_t resp_cap,
      size_t *resp_len)
{
   int64_t n;
   size_t  frag;
   pipe->offset = 0;
   if (rsmb_write(c, pipe, req, req_len) != (int64_t)req_len)
      return -1;
   pipe->offset = 0;
   n = rsmb_read(c, pipe, resp, resp_cap);
   if (n < 16)
      return -1;
   frag = get16(resp + 8);
   while ((size_t)n < frag && (size_t)n < resp_cap)
   {
      int64_t m;
      pipe->offset = 0;
      m = rsmb_read(c, pipe, resp + n, resp_cap - (size_t)n);
      if (m <= 0)
         break;
      n += m;
   }
   *resp_len = (size_t)n;
   return 0;
}

int rsmb_enum_shares(struct rsmb_ctx *c, struct rsmb_share *out, unsigned max)
{
   static const uint8_t srvsvc_uuid[16] = {
      0xc8,0x4f,0x32,0x4b,0x70,0x16,0xd3,0x01,0x12,0x78,0x5a,0x47,0xbf,0x6e,0xe1,0x88 };
   static const uint8_t ndr_uuid[16] = {
      0x04,0x5d,0x88,0x8a,0xeb,0x1c,0xc9,0x11,0x9f,0xe8,0x08,0x00,0x2b,0x10,0x48,0x60 };
   struct rsmb_file *pipe;
   uint8_t *buf;
   uint8_t  req[128];
   size_t   rlen, off;
   uint32_t count, i;
   int      ret = -1;

   if (!(pipe = rsmb_open(c, "srvsvc", RSMB_O_RDWR)))
      return -1;
   if (!(buf = (uint8_t*)malloc(SMB2_MAX_IO)))
   {
      rsmb_close(c, pipe);
      return -1;
   }

   /* bind */
   memset(req, 0, sizeof(req));
   req[0] = 5; req[1] = 0; req[2] = 11; req[3] = 3;
   put32(req + 4, 0x10);                      /* data rep: LE, ASCII, IEEE */
   put16(req + 8, 72);                        /* frag length */
   put32(req + 12, 1);                        /* call id */
   put16(req + 16, 4280); put16(req + 18, 4280);
   put32(req + 20, 0);                        /* assoc group */
   req[24] = 1;                               /* one context */
   put16(req + 28, 0); req[30] = 1;           /* ctx 0, one transfer syntax */
   memcpy(req + 32, srvsvc_uuid, 16); put16(req + 48, 3); put16(req + 50, 0);
   memcpy(req + 52, ndr_uuid, 16);    put32(req + 68, 2);
   if (rsmb_pipe_transceive(c, pipe, req, 72, buf, SMB2_MAX_IO, &rlen) != 0
         || rlen < 16 || buf[2] != 12)
   {
      rsmb_err(c, "srvsvc bind failed");
      goto done;
   }

   /* NetrShareEnum(NULL server, level 1, empty container, max length, resume 0) */
   memset(req, 0, sizeof(req));
   req[0] = 5; req[2] = 0; req[3] = 3;
   put32(req + 4, 0x10);
   put16(req + 8, 24 + 36);
   put32(req + 12, 2);
   put32(req + 16, 36);                       /* alloc hint */
   put16(req + 20, 0);                        /* context id */
   put16(req + 22, 15);                       /* opnum NetrShareEnum */
   off = 24;
   put32(req + off, 0);              off += 4; /* ServerName: NULL */
   put32(req + off, 1);              off += 4; /* Level */
   put32(req + off, 1);              off += 4; /* union switch */
   put32(req + off, 0x00020000);     off += 4; /* container pointer */
   put32(req + off, 0);              off += 4; /*   EntriesRead */
   put32(req + off, 0);              off += 4; /*   Buffer: NULL */
   put32(req + off, 0xffffffffu);    off += 4; /* PreferedMaximumLength */
   put32(req + off, 0x00020004);     off += 4; /* ResumeHandle pointer */
   put32(req + off, 0);              off += 4; /*   value */
   if (rsmb_pipe_transceive(c, pipe, req, off, buf, SMB2_MAX_IO, &rlen) != 0
         || rlen < 24 + 24 || buf[2] != 2)
   {
      rsmb_err(c, "srvsvc request failed");
      goto done;
   }

   /* stub: Level, switch, container ptr, EntriesRead, Buffer ptr, MaxCount */
   off = 24;
   if (get32(buf + off + 8) == 0 || get32(buf + off + 16) == 0)
   {
      ret = 0;                                /* no container / buffer */
      goto done;
   }
   count = get32(buf + off + 12);
   off  += 24;
   if (count > 4096 || off + count * 12 > rlen)
      goto done;
   for (i = 0; i < count; i++)
      if (i < max)
      {
         out[i].type    = get32(buf + off + i * 12 + 4);
         out[i].name[0] = '\0';
      }
   off += count * 12;
   /* then the deferred strings: netname and remark per entry */
   for (i = 0; i < count; i++)
   {
      unsigned s2;
      for (s2 = 0; s2 < 2; s2++)
      {
         uint32_t actual;
         if (off + 12 > rlen)
            goto done;
         actual = get32(buf + off + 8);
         off   += 12;
         if (actual > 512 || off + actual * 2 > rlen)
            goto done;
         if (s2 == 0 && i < max)
            rsmb_utf8(buf + off, actual * 2, out[i].name, sizeof(out[i].name));
         off += actual * 2;
         if (off & 3)
            off += 4 - (off & 3);
      }
   }
   ret = (int)count;
done:
   free(buf);
   rsmb_close(c, pipe);
   return ret;
}
