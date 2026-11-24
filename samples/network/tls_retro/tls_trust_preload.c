/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
/* A system CA bundle preloaded at startup: bundles without usable roots
 * leave the built-in one to load at the first verify and let the next
 * bundle in, the first usable one stays for the process, a test PEM
 * cannot replace it, and the store in use is logged once. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crypto/x509.h>

static unsigned der_loads;
static int test_load_der_parts(const unsigned char *const *parts,
      const size_t *lens, unsigned count);
#define x509_trust_load_der_parts(p, l, c) test_load_der_parts(p, l, c)

static unsigned    logs;
static const char *logged_source;
static int         logged_anchors;
static void test_log_trust_store(const char *source, int anchors)
{
   logs++;
   logged_source  = source;
   logged_anchors = anchors;
}
#define TLS_LOG_TRUST_STORE test_log_trust_store

#include "../../../libretro-common/net/net_socket_ssl_retro.c"

static int test_load_der_parts(const unsigned char *const *parts,
      const size_t *lens, unsigned count)
{
   der_loads++;
   return (x509_trust_load_der_parts)(parts, lens, count);
}

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
static const char other_pem[] =
   "-----BEGIN CERTIFICATE-----\n"
   "MIIBnDCCAUGgAwIBAgIUPnId2nuovLToRaZxiih/i0T2G9MwCgYIKoZIzj0EAwIw\n"
   "IjEgMB4GA1UEAwwXdGxzX3RydXN0X3ByZWxvYWQgb3RoZXIwIBcNMjYxMTE5MTYz\n"
   "NzI4WhgPMjEyNjEwMjYxNjM3MjhaMCIxIDAeBgNVBAMMF3Rsc190cnVzdF9wcmVs\n"
   "b2FkIG90aGVyMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE6nxmc5S4w/7Sp5eF\n"
   "SEuSEXNBbYXFRscVyy4bCOx/AjyIFOPHiUgc5WSjb7zUOt3nd4B5+2d0z9LbcOAl\n"
   "gRFiXaNTMFEwHQYDVR0OBBYEFImoqZRINzHFMjuiH/RI0ACOFoRKMB8GA1UdIwQY\n"
   "MBaAFImoqZRINzHFMjuiH/RI0ACOFoRKMA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZI\n"
   "zj0EAwIDSQAwRgIhAPLKsf3b1jqAM0LgOTF9vjnJevyD0FeJ0i/7YkZDYU+dAiEA\n"
   "5M4rcrWdB7lQ/E4tuP8v12eDJYOYktBHHKXTURRUwWw=\n"
   "-----END CERTIFICATE-----\n";
static const char empty_pem[]    = "no certificates here\n";
static const char unusable_pem[] =
   "-----BEGIN CERTIFICATE-----\n"
   "AAAAAAAA\n"
   "-----END CERTIFICATE-----\n";

/* The frontend hands over the buffer it read the bundle into. */
static int adopt(const char *pem, size_t len, const char *name)
{
   char *copy = (char*)malloc(len + 1);
   assert(copy);
   memcpy(copy, pem, len + 1);
   return ssl_socket_retro_adopt_trust_pem(copy, len, name);
}

/* One verify as a connection without its own anchors runs it. */
static void verify_once(void)
{
   struct ssl_state *s = (struct ssl_state*)ssl_socket_init(-1, "localhost");
   assert(s);
   assert(tls_verify_peer(s, NULL, NULL, 0) == -1);
   ssl_socket_free(s);
}

int main(void)
{
   ssl_socket_set_verify_mode(0);

   /* a test's PEM keeps the system bundle out */
   ssl_socket_retro_set_trust_pem(root_pem, sizeof(root_pem) - 1);
   assert(adopt(root_pem,
            sizeof(root_pem) - 1, "refused") == 0);
   ssl_socket_retro_set_trust_pem(NULL, 0);

   /* no usable roots: the built-in bundle loads at the first verify */
   assert(adopt(empty_pem,
            sizeof(empty_pem) - 1, "empty") == 0);
   verify_once();
   verify_once();
   assert(der_loads == 2);
   assert(logs == 1 && !logged_source && logged_anchors == 0);

   /* a fresh process for the system bundle */
   x509_trust_free();
   retro_atomic_store_release_int(&tls_trust_logged, 0);
   der_loads = logs = 0;

   /* a bundle whose certificates are all unusable lets the next in */
   assert(adopt(unusable_pem,
            sizeof(unusable_pem) - 1, "unusable") == 0);
   assert(adopt(root_pem,
            sizeof(root_pem) - 1, "first") == 1);
   assert(!strcmp(tls_system_source, "first"));

   /* the first usable bundle stays */
   ssl_socket_retro_set_trust_pem(other_pem, sizeof(other_pem) - 1);
   assert(!tls_test_pem);
   assert(adopt(other_pem,
            sizeof(other_pem) - 1, "second") == 1);
   assert(!strcmp(tls_system_source, "first"));

   /* verifies use it, never the built-in bundle, and log it once */
   verify_once();
   verify_once();
   assert(der_loads == 0);
   assert(logs == 1 && logged_source
         && !strcmp(logged_source, "first") && logged_anchors == 1);

   x509_trust_free();
   puts("tls_trust_preload: ok");
   return 0;
}
