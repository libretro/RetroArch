/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lib/aes128ccm.h"

void test_1(void)
{
  unsigned char key[16]  = {0x40, 0x41, 0x42, 0x43,  0x44, 0x45, 0x46, 0x47,
			    0x48, 0x49, 0x4a, 0x4b,  0x4c, 0x4d, 0x4e, 0x4f};
  unsigned char nonce[7] = {0x10, 0x11, 0x12, 0x13,  0x14, 0x15, 0x16 };
  unsigned char aad[8]   = {0x00, 0x01, 0x02, 0x03,  0x04, 0x05, 0x06, 0x07};
  unsigned char p[4]     = {0x20, 0x21, 0x22, 0x23};
  int mlen = 4;
  unsigned char exp[8]   = {0x71, 0x62, 0x01, 0x5b, 0x4d, 0xac, 0x25, 0x5d};

  unsigned char buf[1024];
  int i, rc;

  memcpy(buf, p, sizeof(p));

  aes128ccm_encrypt(key,
                    nonce, sizeof(nonce),
                    aad, sizeof(aad),
                    buf, sizeof(p),
                    &buf[sizeof(p)], mlen);
  printf("Expected:\n");
  for (i = 0; i < sizeof(p) + mlen; i++) {
          printf("%02x ", exp[i]);
  }
  printf("\n");
  printf("Got:\n");
  for (i = 0; i < sizeof(p) + mlen; i++) {
          printf("%02x ", buf[i]);
  }
  printf("\n");

  rc = aes128ccm_decrypt(key,
                         nonce, sizeof(nonce),
                         aad, sizeof(aad),
                         buf, sizeof(p),
                         &buf[sizeof(p)], mlen);
  
  printf("it ran rc : %d\n", rc);
  if (rc) exit(10);
  printf("Got:\n");
  for (i = 0; i < sizeof(p); i++) {
          printf("%02x ", buf[i]);
  }
  printf("\n");

  printf("Decrypted correct: %d\n", memcmp(p, &buf[0], sizeof(p)));
  if (memcmp(p, &buf[0], sizeof(p))) exit(10);
}

void test_2(void)
{
  unsigned char key[16]  = {0x40, 0x41, 0x42, 0x43,  0x44, 0x45, 0x46, 0x47,
			    0x48, 0x49, 0x4a, 0x4b,  0x4c, 0x4d, 0x4e, 0x4f};
  unsigned char nonce[] = {0x10, 0x11, 0x12, 0x13,  0x14, 0x15, 0x16, 0x17,
                            0x18, 0x19, 0x1a, 0x1b};
  unsigned char aad[]   = {0x00, 0x01, 0x02, 0x03,  0x04, 0x05, 0x06, 0x07,
                            0x08, 0x09, 0x0a, 0x0b,  0x0c, 0x0d, 0x0e, 0x0f,
                            0x10, 0x11, 0x12, 0x13};
  unsigned char p[]     = {0x20, 0x21, 0x22, 0x23,  0x24, 0x25, 0x26, 0x27,
                            0x28, 0x29, 0x2a, 0x2b,  0x2c, 0x2d, 0x2e, 0x2f,
                            0x30, 0x31, 0x32, 0x33,  0x34, 0x35, 0x36, 0x37};
  int mlen = 8;
  unsigned char exp[]   = {0xe3, 0xb2, 0x01, 0xa9,  0xf5, 0xb7, 0x1a, 0x7a,
                           0x9b, 0x1c, 0xea, 0xec,  0xcd, 0x97, 0xe7, 0x0b,
                           0x61, 0x76, 0xaa, 0xd9,  0xa4, 0x42, 0x8a, 0xa5,
                           0x48, 0x43, 0x92, 0xfb,  0xc1, 0xb0, 0x99, 0x51};
  unsigned char buf[1024];
  int i, rc;

  memcpy(buf, p, sizeof(p));

  aes128ccm_encrypt(key,
                    nonce, sizeof(nonce),
                    aad, sizeof(aad),
                    buf, sizeof(p),
                    &buf[sizeof(p)], mlen);
  printf("Expected:\n");
  for (i = 0; i < sizeof(p) + mlen; i++) {
          printf("%02x ", exp[i]);
  }
  printf("\n");
  printf("Got:\n");
  for (i = 0; i < sizeof(p) + mlen; i++) {
          printf("%02x ", buf[i]);
  }
  printf("\n");
  rc = aes128ccm_decrypt(key,
                         nonce, sizeof(nonce),
                         aad, sizeof(aad),
                         buf, sizeof(p),
                         &buf[sizeof(p)], mlen);
  
  printf("it ran rc : %d\n", rc);
  if (rc) exit(10);
  printf("Got:\n");
  for (i = 0; i < sizeof(p); i++) {
          printf("%02x ", buf[i]);
  }
  printf("\n");

  printf("Decrypted correct: %d\n", memcmp(p, &buf[0], sizeof(p)));
  if (memcmp(p, &buf[0], sizeof(p))) exit(10);
}

int main(int argc, char *argv[])
{
        test_1();
        test_2();
        return 0;
}
