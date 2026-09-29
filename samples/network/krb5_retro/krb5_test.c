/* The built-in Kerberos client against a KDC: AS exchange (with
 * PA-ENC-TIMESTAMP when required), TGS exchange for a service, the
 * GSS initial token, and - given the service key in hex - proof that
 * the ticket inside it decrypts with that key and names the client.
 *
 *   krb5_test realm kdc port user password service [service-key-hex]
 *
 * Exit 0 on success; a wrong password or unknown service exits 1 with
 * the KDC's error on stderr. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_krb5.h>

static int unhex(const char *h, uint8_t *o, size_t cap)
{
   size_t n = 0;
   while (h[0] && h[1] && n < cap)
   {
      unsigned v;
      if (sscanf(h, "%2x", &v) != 1)
         return -1;
      o[n++] = (uint8_t)v;
      h += 2;
   }
   return (int)n;
}

/* Find the ticket's enc-part inside the token: the largest OCTET
 * STRING that decrypts under the service key with usage 2. */
static int check_ticket(const uint8_t *tok, size_t tl, const struct krb5_key *k, const char *user)
{
   uint8_t *pt = (uint8_t*)malloc(tl);
   size_t i, pl;
   int found = 0;
   if (!pt)
      return -1;
   for (i = 0; i + 4 < tl && !found; i++)
   {
      size_t h, n;
      if (tok[i] != 0x04)
         continue;
      if (tok[i + 1] == 0x82)
      {
         h = 4; n = ((size_t)tok[i + 2] << 8) | tok[i + 3];
      }
      else if (tok[i + 1] == 0x81)
      {
         h = 3; n = tok[i + 2];
      }
      else
         continue;
      if (i + h + n > tl || n < 28)
         continue;
      if (krb5_decrypt(k, 2, tok + i + h, n, pt, &pl) == 0)
      {
         size_t j, ul = strlen(user);
         for (j = 0; j + ul <= pl; j++)
            if (memcmp(pt + j, user, ul) == 0)
               found = 1;
      }
   }
   free(pt);
   return found ? 0 : -1;
}

int main(int argc, char **argv)
{
   struct krb5_ctx *c;
   uint8_t tok[4096];
   size_t  tl;
   int     rc = 1;

   if (argc < 7)
   {
      fprintf(stderr, "usage: krb5_test realm kdc port user password service [service-key-hex]\n");
      return 2;
   }
   network_init();
   c = krb5_new();
   krb5_set_kdc(c, argv[1], argv[2], (uint16_t)atoi(argv[3]));
   krb5_set_timeout(c, 5);
   if (krb5_get_tgt(c, argv[4], argv[5]) != 0)
   {
      fprintf(stderr, "AS: %s\n", krb5_get_error(c));
      goto out;
   }
   if (krb5_get_service_ticket(c, argv[6]) != 0)
   {
      fprintf(stderr, "TGS: %s\n", krb5_get_error(c));
      goto out;
   }
   if (krb5_gss_init_token(c, tok, sizeof(tok), &tl) != 0)
   {
      fprintf(stderr, "GSS: %s\n", krb5_get_error(c));
      goto out;
   }
   if (argc > 7)
   {
      struct krb5_key k;
      int n = unhex(argv[7], k.k, sizeof(k.k));
      if (n != 16 && n != 32)
      {
         fprintf(stderr, "bad key hex\n");
         goto out;
      }
      k.len     = (size_t)n;
      k.enctype = n == 32 ? KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96 : KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96;
      if (check_ticket(tok, tl, &k, argv[4]) != 0)
      {
         fprintf(stderr, "ticket does not decrypt with the service key\n");
         goto out;
      }
   }
   printf("ok: %s@%s -> %s (token %u octets)\n", argv[4], krb5_realm(c), argv[6], (unsigned)tl);
   rc = 0;
out:
   krb5_free(c);
   return rc;
}
