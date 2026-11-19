/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
/* net_socket_ssl_bear.c's bundle reader: certificates decoded where
 * they lie whatever their line breaks, one that is not base64 skipped,
 * and a bundle with more roots than the anchor table holds filling it
 * and stopping there. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../libretro-common/net/net_socket_ssl_bear.c"

#define ROOTS 600

static const char root_pem[] =
   "-----BEGIN CERTIFICATE-----\n"
   "MIIBmTCCAT+gAwIBAgIUaUEFr0CYgzYmKnKFApbM0Erp/NQwCgYIKoZIzj0EAwIw\n"
   "ITEfMB0GA1UEAwwWdGxzX3RydXN0X3ByZWxvYWQgcm9vdDAgFw0yNjExMTkxNjM3\n"
   "MjhaGA8yMTI2MTAyNjE2MzcyOFowITEfMB0GA1UEAwwWdGxzX3RydXN0X3ByZWxv\n"
   "YWQgcm9vdDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABN9Dq/xYWEtilEzLhafE\n"
   "97kaOhRmrXrdtvZ2Je2roeUWkQC+vKjGyDeFvsaWjkKc3dOmsnHmIH8FKoWH9KmX\n"
   "I3KjUzBRMB0GA1UdDgQWBBQez7eHsHGNfRYN8Refnjjfg/LSmTAfBgNVHSMEGDAW\n"
   "gBQez7eHsHGNfRYN8Refnjjfg/LSmTAPBgNVHRMBAf8EBTADAQH/MAoGCCqGSM49\n"
   "BAMCA0gAMEUCIQDScnZToUBdDcX1++wWECrwEt+YynfLCyC8HYt0h8Yr5gIgTlXC\n"
   "1emq4+ear9hnJDZhMScFJ284MfTFaOJ/HawhOBo=\n"
   "-----END CERTIFICATE-----\n";
static const char broken_pem[] =
   "-----BEGIN CERTIFICATE-----\n"
   "MIIB*not base64*\n"
   "-----END CERTIFICATE-----\n";

int main(void)
{
   size_t one = sizeof(root_pem) - 1, crlf_len = 0, i, o;
   char  *crlf = (char*)malloc(one * 2);
   char  *all;
   assert(crlf);
   for (i = 0; i < one; i++)
   {
      if (root_pem[i] == '\n')
         crlf[crlf_len++] = '\r';
      crlf[crlf_len++] = root_pem[i];
   }
   all = (char*)malloc(sizeof(broken_pem) + (ROOTS / 2) * (one + crlf_len) + 1);
   assert(all);
   memcpy(all, broken_pem, sizeof(broken_pem) - 1);
   o = sizeof(broken_pem) - 1;
   for (i = 0; i < ROOTS / 2; i++)
   {
      memcpy(all + o, root_pem, one);
      o += one;
      memcpy(all + o, crlf, crlf_len);
      o += crlf_len;
   }
   all[o] = '\0';

   append_certs_pem_x509(all);
   assert(TAs_NUM == sizeof(TAs) / sizeof(TAs[0]));
   assert(TAs[0].pkey.key_type == BR_KEYTYPE_EC);
   assert(TAs[0].pkey.key.ec.qlen == 65);
   /* the LF and the CRLF copies give the same anchor */
   assert(TAs[1].dn.len == TAs[0].dn.len
         && !memcmp(TAs[1].dn.data, TAs[0].dn.data, TAs[0].dn.len));
   assert(!memcmp(TAs[1].pkey.key.ec.q, TAs[0].pkey.key.ec.q, 65));

   for (i = 0; i < TAs_NUM; i++)
   {
      free(TAs[i].dn.data);
      free(TAs[i].pkey.key.ec.q);
   }
   free(all);
   free(crlf);
   printf("tls_bear_anchors: %u anchors, ok\n", (unsigned)TAs_NUM);
   return 0;
}
