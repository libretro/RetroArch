/*
 * Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bearssl.h"
#include "inner.h"

/*
 * Decode an hexadecimal string. Returned value is the number of decoded
 * bytes.
 */
static size_t
hextobin(unsigned char *dst, const char *src)
{
	size_t num;
	unsigned acc;
	int z;

	num = 0;
	z = 0;
	acc = 0;
	while (*src != 0) {
		int c = *src ++;
		if (c >= '0' && c <= '9') {
			c -= '0';
		} else if (c >= 'A' && c <= 'F') {
			c -= ('A' - 10);
		} else if (c >= 'a' && c <= 'f') {
			c -= ('a' - 10);
		} else {
			continue;
		}
		if (z) {
			*dst ++ = (acc << 4) + c;
			num ++;
		} else {
			acc = c;
		}
		z = !z;
	}
	return num;
}

static void
check_equals(const char *banner, const void *v1, const void *v2, size_t len)
{
	size_t u;
	const unsigned char *b;

	if (memcmp(v1, v2, len) == 0) {
		return;
	}
	fprintf(stderr, "\n%s failed\n", banner);
	fprintf(stderr, "v1: ");
	for (u = 0, b = v1; u < len; u ++) {
		fprintf(stderr, "%02X", b[u]);
	}
	fprintf(stderr, "\nv2: ");
	for (u = 0, b = v2; u < len; u ++) {
		fprintf(stderr, "%02X", b[u]);
	}
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

#define HASH_SIZE(cname)   br_ ## cname ## _SIZE

#define TEST_HASH(Name, cname) \
static void \
test_ ## cname ## _internal(char *data, char *refres) \
{ \
	br_ ## cname ## _context mc; \
	unsigned char res[HASH_SIZE(cname)], ref[HASH_SIZE(cname)]; \
	size_t u, n; \
 \
	hextobin(ref, refres); \
	n = strlen(data); \
	br_ ## cname ## _init(&mc); \
	br_ ## cname ## _update(&mc, data, n); \
	br_ ## cname ## _out(&mc, res); \
	check_equals("KAT " #Name " 1", res, ref, HASH_SIZE(cname)); \
	br_ ## cname ## _init(&mc); \
	for (u = 0; u < n; u ++) { \
		br_ ## cname ## _update(&mc, data + u, 1); \
	} \
	br_ ## cname ## _out(&mc, res); \
	check_equals("KAT " #Name " 2", res, ref, HASH_SIZE(cname)); \
	for (u = 0; u < n; u ++) { \
		br_ ## cname ## _context mc2; \
		br_ ## cname ## _init(&mc); \
		br_ ## cname ## _update(&mc, data, u); \
		mc2 = mc; \
		br_ ## cname ## _update(&mc, data + u, n - u); \
		br_ ## cname ## _out(&mc, res); \
		check_equals("KAT " #Name " 3", res, ref, HASH_SIZE(cname)); \
		br_ ## cname ## _update(&mc2, data + u, n - u); \
		br_ ## cname ## _out(&mc2, res); \
		check_equals("KAT " #Name " 4", res, ref, HASH_SIZE(cname)); \
	} \
	memset(&mc, 0, sizeof mc); \
	memset(res, 0, sizeof res); \
	br_ ## cname ## _vtable.init(&mc.vtable); \
	mc.vtable->update(&mc.vtable, data, n); \
	mc.vtable->out(&mc.vtable, res); \
	check_equals("KAT " #Name " 5", res, ref, HASH_SIZE(cname)); \
	memset(res, 0, sizeof res); \
	mc.vtable->init(&mc.vtable); \
	mc.vtable->update(&mc.vtable, data, n); \
	mc.vtable->out(&mc.vtable, res); \
	check_equals("KAT " #Name " 6", res, ref, HASH_SIZE(cname)); \
}

#define KAT_MILLION_A(Name, cname, refres)   do { \
		br_ ## cname ## _context mc; \
		unsigned char buf[1000]; \
		unsigned char res[HASH_SIZE(cname)], ref[HASH_SIZE(cname)]; \
		int i; \
 \
		hextobin(ref, refres); \
		memset(buf, 'a', sizeof buf); \
		br_ ## cname ## _init(&mc); \
		for (i = 0; i < 1000; i ++) { \
			br_ ## cname ## _update(&mc, buf, sizeof buf); \
		} \
		br_ ## cname ## _out(&mc, res); \
		check_equals("KAT " #Name " 5", res, ref, HASH_SIZE(cname)); \
	} while (0)

TEST_HASH(MD5, md5)
TEST_HASH(SHA-1, sha1)
TEST_HASH(SHA-224, sha224)
TEST_HASH(SHA-256, sha256)
TEST_HASH(SHA-384, sha384)
TEST_HASH(SHA-512, sha512)

static void
test_MD5(void)
{
	printf("Test MD5: ");
	fflush(stdout);
	test_md5_internal("", "d41d8cd98f00b204e9800998ecf8427e");
	test_md5_internal("a", "0cc175b9c0f1b6a831c399e269772661");
	test_md5_internal("abc", "900150983cd24fb0d6963f7d28e17f72");
	test_md5_internal("message digest", "f96b697d7cb7938d525a2f31aaf161d0");
	test_md5_internal("abcdefghijklmnopqrstuvwxyz",
		"c3fcd3d76192e4007dfb496cca67e13b");
	test_md5_internal("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
		"vwxyz0123456789", "d174ab98d277d9f5a5611c2c9f419d9f");
	test_md5_internal("1234567890123456789012345678901234567890123456789"
		"0123456789012345678901234567890",
		"57edf4a22be3c955ac49da2e2107b67a");
	KAT_MILLION_A(MD5, md5,
		"7707d6ae4e027c70eea2a935c2296f21");
	printf("done.\n");
	fflush(stdout);
}

static void
test_SHA1(void)
{
	printf("Test SHA-1: ");
	fflush(stdout);
	test_sha1_internal("abc", "a9993e364706816aba3e25717850c26c9cd0d89d");
	test_sha1_internal("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlm"
		"nomnopnopq", "84983e441c3bd26ebaae4aa1f95129e5e54670f1");

	KAT_MILLION_A(SHA-1, sha1,
		"34aa973cd4c4daa4f61eeb2bdbad27316534016f");
	printf("done.\n");
	fflush(stdout);
}

static void
test_SHA224(void)
{
	printf("Test SHA-224: ");
	fflush(stdout);
	test_sha224_internal("abc",
   "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7");
	test_sha224_internal("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlm"
		"nomnopnopq",
   "75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525");

	KAT_MILLION_A(SHA-224, sha224,
		"20794655980c91d8bbb4c1ea97618a4bf03f42581948b2ee4ee7ad67");
	printf("done.\n");
	fflush(stdout);
}

static void
test_SHA256(void)
{
	printf("Test SHA-256: ");
	fflush(stdout);
	test_sha256_internal("abc",
   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	test_sha256_internal("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlm"
		"nomnopnopq",
   "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

	KAT_MILLION_A(SHA-256, sha256,
   "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
	printf("done.\n");
	fflush(stdout);
}

static void
test_SHA384(void)
{
	printf("Test SHA-384: ");
	fflush(stdout);
	test_sha384_internal("abc",
		"cb00753f45a35e8bb5a03d699ac65007272c32ab0eded163"
		"1a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7");
	test_sha384_internal(
		"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
		"hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
		"09330c33f71147e83d192fc782cd1b4753111b173b3b05d2"
		"2fa08086e3b0f712fcc7c71a557e2db966c3e9fa91746039");

	KAT_MILLION_A(SHA-384, sha384,
		"9d0e1809716474cb086e834e310a4a1ced149e9c00f24852"
		"7972cec5704c2a5b07b8b3dc38ecc4ebae97ddd87f3d8985");
	printf("done.\n");
	fflush(stdout);
}

static void
test_SHA512(void)
{
	printf("Test SHA-512: ");
	fflush(stdout);
	test_sha512_internal("abc",
   "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
   "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
	test_sha512_internal(
		"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
		"hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
   "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
   "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");

	KAT_MILLION_A(SHA-512, sha512,
   "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
   "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b");
	printf("done.\n");
	fflush(stdout);
}

static void
test_MD5_SHA1(void)
{
	unsigned char buf[500], out[36], outM[16], outS[20];
	unsigned char seed[1];
	br_hmac_drbg_context rc;
	br_md5_context mc;
	br_sha1_context sc;
	br_md5sha1_context cc;
	size_t u;

	printf("Test MD5+SHA-1: ");
	fflush(stdout);

	seed[0] = 0;
	br_hmac_drbg_init(&rc, &br_sha256_vtable, seed, sizeof seed);
	for (u = 0; u < sizeof buf; u ++) {
		size_t v;

		br_hmac_drbg_generate(&rc, buf, u);
		br_md5_init(&mc);
		br_md5_update(&mc, buf, u);
		br_md5_out(&mc, outM);
		br_sha1_init(&sc);
		br_sha1_update(&sc, buf, u);
		br_sha1_out(&sc, outS);
		br_md5sha1_init(&cc);
		br_md5sha1_update(&cc, buf, u);
		br_md5sha1_out(&cc, out);
		check_equals("MD5+SHA-1 [1]", out, outM, 16);
		check_equals("MD5+SHA-1 [2]", out + 16, outS, 20);
		br_md5sha1_init(&cc);
		for (v = 0; v < u; v ++) {
			br_md5sha1_update(&cc, buf + v, 1);
		}
		br_md5sha1_out(&cc, out);
		check_equals("MD5+SHA-1 [3]", out, outM, 16);
		check_equals("MD5+SHA-1 [4]", out + 16, outS, 20);
	}

	printf("done.\n");
	fflush(stdout);
}

/*
 * Compute a hash function, on some data, by ID. Returned value is
 * hash output length.
 */
static size_t
do_hash(int id, const void *data, size_t len, void *out)
{
	br_md5_context cmd5;
	br_sha1_context csha1;
	br_sha224_context csha224;
	br_sha256_context csha256;
	br_sha384_context csha384;
	br_sha512_context csha512;

	switch (id) {
	case br_md5_ID:
		br_md5_init(&cmd5);
		br_md5_update(&cmd5, data, len);
		br_md5_out(&cmd5, out);
		return 16;
	case br_sha1_ID:
		br_sha1_init(&csha1);
		br_sha1_update(&csha1, data, len);
		br_sha1_out(&csha1, out);
		return 20;
	case br_sha224_ID:
		br_sha224_init(&csha224);
		br_sha224_update(&csha224, data, len);
		br_sha224_out(&csha224, out);
		return 28;
	case br_sha256_ID:
		br_sha256_init(&csha256);
		br_sha256_update(&csha256, data, len);
		br_sha256_out(&csha256, out);
		return 32;
	case br_sha384_ID:
		br_sha384_init(&csha384);
		br_sha384_update(&csha384, data, len);
		br_sha384_out(&csha384, out);
		return 48;
	case br_sha512_ID:
		br_sha512_init(&csha512);
		br_sha512_update(&csha512, data, len);
		br_sha512_out(&csha512, out);
		return 64;
	default:
		fprintf(stderr, "Uknown hash function: %d\n", id);
		exit(EXIT_FAILURE);
		return 0;
	}
}

/*
 * Tests for a multihash. Returned value should be 258 multiplied by the
 * number of hash functions implemented by the context.
 */
static int
test_multihash_inner(br_multihash_context *mc)
{
	/*
	 * Try hashing messages for all lengths from 0 to 257 bytes
	 * (inclusive). Each attempt is done twice, with data input
	 * either in one go, or byte by byte. In the byte by byte
	 * test, intermediate result are obtained and checked.
	 */
	size_t len;
	unsigned char buf[258];
	int i;
	int tcount;

	tcount = 0;
	for (len = 0; len < sizeof buf; len ++) {
		br_sha1_context sc;
		unsigned char tmp[20];

		br_sha1_init(&sc);
		br_sha1_update(&sc, buf, len);
		br_sha1_out(&sc, tmp);
		buf[len] = tmp[0];
	}
	for (len = 0; len <= 257; len ++) {
		size_t u;

		br_multihash_init(mc);
		br_multihash_update(mc, buf, len);
		for (i = 1; i <= 6; i ++) {
			unsigned char tmp[64], tmp2[64];
			size_t olen, olen2;

			olen = br_multihash_out(mc, i, tmp);
			if (olen == 0) {
				continue;
			}
			olen2 = do_hash(i, buf, len, tmp2);
			if (olen != olen2) {
				fprintf(stderr,
					"Bad hash output length: %u / %u\n",
					(unsigned)olen, (unsigned)olen2);
				exit(EXIT_FAILURE);
			}
			check_equals("Hash output", tmp, tmp2, olen);
			tcount ++;
		}

		br_multihash_init(mc);
		for (u = 0; u < len; u ++) {
			br_multihash_update(mc, buf + u, 1);
			for (i = 1; i <= 6; i ++) {
				unsigned char tmp[64], tmp2[64];
				size_t olen, olen2;

				olen = br_multihash_out(mc, i, tmp);
				if (olen == 0) {
					continue;
				}
				olen2 = do_hash(i, buf, u + 1, tmp2);
				if (olen != olen2) {
					fprintf(stderr, "Bad hash output"
						" length: %u / %u\n",
						(unsigned)olen,
						(unsigned)olen2);
					exit(EXIT_FAILURE);
				}
				check_equals("Hash output", tmp, tmp2, olen);
			}
		}
	}
	return tcount;
}

static void
test_multihash(void)
{
	br_multihash_context mc;

	printf("Test MultiHash: ");
	fflush(stdout);

	br_multihash_zero(&mc);
	br_multihash_setimpl(&mc, br_md5_ID, &br_md5_vtable);
	if (test_multihash_inner(&mc) != 258) {
		fprintf(stderr, "Failed test count\n");
	}
	printf(".");
	fflush(stdout);

	br_multihash_zero(&mc);
	br_multihash_setimpl(&mc, br_sha1_ID, &br_sha1_vtable);
	if (test_multihash_inner(&mc) != 258) {
		fprintf(stderr, "Failed test count\n");
	}
	printf(".");
	fflush(stdout);

	br_multihash_zero(&mc);
	br_multihash_setimpl(&mc, br_sha224_ID, &br_sha224_vtable);
	if (test_multihash_inner(&mc) != 258) {
		fprintf(stderr, "Failed test count\n");
	}
	printf(".");
	fflush(stdout);

	br_multihash_zero(&mc);
	br_multihash_setimpl(&mc, br_sha256_ID, &br_sha256_vtable);
	if (test_multihash_inner(&mc) != 258) {
		fprintf(stderr, "Failed test count\n");
	}
	printf(".");
	fflush(stdout);

	br_multihash_zero(&mc);
	br_multihash_setimpl(&mc, br_sha384_ID, &br_sha384_vtable);
	if (test_multihash_inner(&mc) != 258) {
		fprintf(stderr, "Failed test count\n");
	}
	printf(".");
	fflush(stdout);

	br_multihash_zero(&mc);
	br_multihash_setimpl(&mc, br_sha512_ID, &br_sha512_vtable);
	if (test_multihash_inner(&mc) != 258) {
		fprintf(stderr, "Failed test count\n");
	}
	printf(".");
	fflush(stdout);

	br_multihash_zero(&mc);
	br_multihash_setimpl(&mc, br_md5_ID, &br_md5_vtable);
	br_multihash_setimpl(&mc, br_sha1_ID, &br_sha1_vtable);
	br_multihash_setimpl(&mc, br_sha224_ID, &br_sha224_vtable);
	br_multihash_setimpl(&mc, br_sha256_ID, &br_sha256_vtable);
	br_multihash_setimpl(&mc, br_sha384_ID, &br_sha384_vtable);
	br_multihash_setimpl(&mc, br_sha512_ID, &br_sha512_vtable);
	if (test_multihash_inner(&mc) != 258 * 6) {
		fprintf(stderr, "Failed test count\n");
	}
	printf(".");
	fflush(stdout);

	printf("done.\n");
	fflush(stdout);
}

static void
do_KAT_HMAC_bin_bin(const br_hash_class *digest_class,
	const void *key, size_t key_len,
	const void *data, size_t data_len, const char *href)
{
	br_hmac_key_context kc;
	br_hmac_context ctx;
	unsigned char tmp[64], ref[64];
	size_t u, len;

	len = hextobin(ref, href);
	br_hmac_key_init(&kc, digest_class, key, key_len);
	br_hmac_init(&ctx, &kc, 0);
	br_hmac_update(&ctx, data, data_len);
	br_hmac_out(&ctx, tmp);
	check_equals("KAT HMAC 1", tmp, ref, len);

	br_hmac_init(&ctx, &kc, 0);
	for (u = 0; u < data_len; u ++) {
		br_hmac_update(&ctx, (const unsigned char *)data + u, 1);
	}
	br_hmac_out(&ctx, tmp);
	check_equals("KAT HMAC 2", tmp, ref, len);

	for (u = 0; u < data_len; u ++) {
		br_hmac_init(&ctx, &kc, 0);
		br_hmac_update(&ctx, data, u);
		br_hmac_out(&ctx, tmp);
		br_hmac_update(&ctx,
			(const unsigned char *)data + u, data_len - u);
		br_hmac_out(&ctx, tmp);
		check_equals("KAT HMAC 3", tmp, ref, len);
	}
}

static void
do_KAT_HMAC_str_str(const br_hash_class *digest_class, const char *key,
	const char *data, const char *href)
{
	do_KAT_HMAC_bin_bin(digest_class, key, strlen(key),
		data, strlen(data), href);
}

static void
do_KAT_HMAC_hex_hex(const br_hash_class *digest_class, const char *skey,
	const char *sdata, const char *href)
{
	unsigned char key[1024];
	unsigned char data[1024];

	do_KAT_HMAC_bin_bin(digest_class, key, hextobin(key, skey),
		data, hextobin(data, sdata), href);
}

static void
do_KAT_HMAC_hex_str(const br_hash_class *digest_class,
	const char *skey, const char *data, const char *href)
{
	unsigned char key[1024];

	do_KAT_HMAC_bin_bin(digest_class, key, hextobin(key, skey),
		data, strlen(data), href);
}

static void
test_HMAC_CT(const br_hash_class *digest_class,
	const void *key, size_t key_len, const void *data)
{
	br_hmac_key_context kc;
	br_hmac_context hc1, hc2;
	unsigned char buf1[64], buf2[64];
	size_t u, v;

	br_hmac_key_init(&kc, digest_class, key, key_len);

	for (u = 0; u < 2; u ++) {
		for (v = 0; v < 130; v ++) {
			size_t min_len, max_len;
			size_t w;

			min_len = v;
			max_len = v + 256;
			for (w = min_len; w <= max_len; w ++) {
				char tmp[30];
				size_t hlen1, hlen2;

				br_hmac_init(&hc1, &kc, 0);
				br_hmac_update(&hc1, data, u + w);
				hlen1 = br_hmac_out(&hc1, buf1);
				br_hmac_init(&hc2, &kc, 0);
				br_hmac_update(&hc2, data, u);
				hlen2 = br_hmac_outCT(&hc2,
					(const unsigned char *)data + u, w,
					min_len, max_len, buf2);
				if (hlen1 != hlen2) {
					fprintf(stderr, "HMAC length mismatch:"
						" %u / %u\n", (unsigned)hlen1,
						(unsigned)hlen2);
					exit(EXIT_FAILURE);
				}
				sprintf(tmp, "HMAC CT %u,%u,%u",
					(unsigned)u, (unsigned)v, (unsigned)w);
				check_equals(tmp, buf1, buf2, hlen1);
			}
		}
		printf(".");
		fflush(stdout);
	}
	printf(" ");
	fflush(stdout);
}

static void
test_HMAC(void)
{
	unsigned char data[1000];
	unsigned x;
	size_t u;
	const char key[] = "test HMAC key";

	printf("Test HMAC: ");
	fflush(stdout);
	do_KAT_HMAC_hex_str(&br_md5_vtable,
		"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
		"Hi There",
		"9294727a3638bb1c13f48ef8158bfc9d");
	do_KAT_HMAC_str_str(&br_md5_vtable,
		"Jefe",
		"what do ya want for nothing?",
		"750c783e6ab0b503eaa86e310a5db738");
	do_KAT_HMAC_hex_hex(&br_md5_vtable,
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
		"DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
		"56be34521d144c88dbb8c733f0e8b3f6");
	do_KAT_HMAC_hex_hex(&br_md5_vtable,
		"0102030405060708090a0b0c0d0e0f10111213141516171819",
		"CDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCD",
		"697eaf0aca3a3aea3a75164746ffaa79");
	do_KAT_HMAC_hex_str(&br_md5_vtable,
		"0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c",
		"Test With Truncation",
		"56461ef2342edc00f9bab995690efd4c");
	do_KAT_HMAC_hex_str(&br_md5_vtable,
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
		"Test Using Larger Than Block-Size Key - Hash Key First",
		"6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd");
	do_KAT_HMAC_hex_str(&br_md5_vtable,
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
		"Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data",
		"6f630fad67cda0ee1fb1f562db3aa53e");

	do_KAT_HMAC_hex_str(&br_sha1_vtable,
		"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
		"Hi There",
		"b617318655057264e28bc0b6fb378c8ef146be00");
	do_KAT_HMAC_str_str(&br_sha1_vtable,
		"Jefe",
		"what do ya want for nothing?",
		"effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
	do_KAT_HMAC_hex_hex(&br_sha1_vtable,
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
		"DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
		"125d7342b9ac11cd91a39af48aa17b4f63f175d3");
	do_KAT_HMAC_hex_hex(&br_sha1_vtable,
		"0102030405060708090a0b0c0d0e0f10111213141516171819",
		"CDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCDCD",
		"4c9007f4026250c6bc8414f9bf50c86c2d7235da");
	do_KAT_HMAC_hex_str(&br_sha1_vtable,
		"0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c",
		"Test With Truncation",
		"4c1a03424b55e07fe7f27be1d58bb9324a9a5a04");
	do_KAT_HMAC_hex_str(&br_sha1_vtable,
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
		"Test Using Larger Than Block-Size Key - Hash Key First",
		"aa4ae5e15272d00e95705637ce8a3b55ed402112");
	do_KAT_HMAC_hex_str(&br_sha1_vtable,
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
		"Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data",
		"e8e99d0f45237d786d6bbaa7965c7808bbff1a91");

	/* From RFC 4231 */

	do_KAT_HMAC_hex_hex(&br_sha224_vtable,
		"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
		"4869205468657265",
		"896fb1128abbdf196832107cd49df33f"
		"47b4b1169912ba4f53684b22");

	do_KAT_HMAC_hex_hex(&br_sha256_vtable,
		"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
		"4869205468657265",
		"b0344c61d8db38535ca8afceaf0bf12b"
		"881dc200c9833da726e9376c2e32cff7");

	do_KAT_HMAC_hex_hex(&br_sha384_vtable,
		"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
		"4869205468657265",
		"afd03944d84895626b0825f4ab46907f"
		"15f9dadbe4101ec682aa034c7cebc59c"
		"faea9ea9076ede7f4af152e8b2fa9cb6");

	do_KAT_HMAC_hex_hex(&br_sha512_vtable,
		"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
		"4869205468657265",
		"87aa7cdea5ef619d4ff0b4241a1d6cb0"
		"2379f4e2ce4ec2787ad0b30545e17cde"
		"daa833b7d6b8a702038b274eaea3f4e4"
		"be9d914eeb61f1702e696c203a126854");

	do_KAT_HMAC_hex_hex(&br_sha224_vtable,
		"4a656665",
		"7768617420646f2079612077616e7420"
		"666f72206e6f7468696e673f",
		"a30e01098bc6dbbf45690f3a7e9e6d0f"
		"8bbea2a39e6148008fd05e44");

	do_KAT_HMAC_hex_hex(&br_sha256_vtable,
		"4a656665",
		"7768617420646f2079612077616e7420"
		"666f72206e6f7468696e673f",
		"5bdcc146bf60754e6a042426089575c7"
		"5a003f089d2739839dec58b964ec3843");

	do_KAT_HMAC_hex_hex(&br_sha384_vtable,
		"4a656665",
		"7768617420646f2079612077616e7420"
		"666f72206e6f7468696e673f",
		"af45d2e376484031617f78d2b58a6b1b"
		"9c7ef464f5a01b47e42ec3736322445e"
		"8e2240ca5e69e2c78b3239ecfab21649");

	do_KAT_HMAC_hex_hex(&br_sha512_vtable,
		"4a656665",
		"7768617420646f2079612077616e7420"
		"666f72206e6f7468696e673f",
		"164b7a7bfcf819e2e395fbe73b56e0a3"
		"87bd64222e831fd610270cd7ea250554"
		"9758bf75c05a994a6d034f65f8f0e6fd"
		"caeab1a34d4a6b4b636e070a38bce737");

	do_KAT_HMAC_hex_hex(&br_sha224_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaa",
		"dddddddddddddddddddddddddddddddd"
		"dddddddddddddddddddddddddddddddd"
		"dddddddddddddddddddddddddddddddd"
		"dddd",
		"7fb3cb3588c6c1f6ffa9694d7d6ad264"
		"9365b0c1f65d69d1ec8333ea");

	do_KAT_HMAC_hex_hex(&br_sha256_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaa",
		"dddddddddddddddddddddddddddddddd"
		"dddddddddddddddddddddddddddddddd"
		"dddddddddddddddddddddddddddddddd"
		"dddd",
		"773ea91e36800e46854db8ebd09181a7"
		"2959098b3ef8c122d9635514ced565fe");

	do_KAT_HMAC_hex_hex(&br_sha384_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaa",
		"dddddddddddddddddddddddddddddddd"
		"dddddddddddddddddddddddddddddddd"
		"dddddddddddddddddddddddddddddddd"
		"dddd",
		"88062608d3e6ad8a0aa2ace014c8a86f"
		"0aa635d947ac9febe83ef4e55966144b"
		"2a5ab39dc13814b94e3ab6e101a34f27");

	do_KAT_HMAC_hex_hex(&br_sha512_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaa",
		"dddddddddddddddddddddddddddddddd"
		"dddddddddddddddddddddddddddddddd"
		"dddddddddddddddddddddddddddddddd"
		"dddd",
		"fa73b0089d56a284efb0f0756c890be9"
		"b1b5dbdd8ee81a3655f83e33b2279d39"
		"bf3e848279a722c806b485a47e67c807"
		"b946a337bee8942674278859e13292fb");

	do_KAT_HMAC_hex_hex(&br_sha224_vtable,
		"0102030405060708090a0b0c0d0e0f10"
		"111213141516171819",
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcd",
		"6c11506874013cac6a2abc1bb382627c"
		"ec6a90d86efc012de7afec5a");

	do_KAT_HMAC_hex_hex(&br_sha256_vtable,
		"0102030405060708090a0b0c0d0e0f10"
		"111213141516171819",
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcd",
		"82558a389a443c0ea4cc819899f2083a"
		"85f0faa3e578f8077a2e3ff46729665b");

	do_KAT_HMAC_hex_hex(&br_sha384_vtable,
		"0102030405060708090a0b0c0d0e0f10"
		"111213141516171819",
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcd",
		"3e8a69b7783c25851933ab6290af6ca7"
		"7a9981480850009cc5577c6e1f573b4e"
		"6801dd23c4a7d679ccf8a386c674cffb");

	do_KAT_HMAC_hex_hex(&br_sha512_vtable,
		"0102030405060708090a0b0c0d0e0f10"
		"111213141516171819",
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
		"cdcd",
		"b0ba465637458c6990e5a8c5f61d4af7"
		"e576d97ff94b872de76f8050361ee3db"
		"a91ca5c11aa25eb4d679275cc5788063"
		"a5f19741120c4f2de2adebeb10a298dd");

	do_KAT_HMAC_hex_hex(&br_sha224_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaa",
		"54657374205573696e67204c61726765"
		"72205468616e20426c6f636b2d53697a"
		"65204b6579202d2048617368204b6579"
		"204669727374",
		"95e9a0db962095adaebe9b2d6f0dbce2"
		"d499f112f2d2b7273fa6870e");

	do_KAT_HMAC_hex_hex(&br_sha256_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaa",
		"54657374205573696e67204c61726765"
		"72205468616e20426c6f636b2d53697a"
		"65204b6579202d2048617368204b6579"
		"204669727374",
		"60e431591ee0b67f0d8a26aacbf5b77f"
		"8e0bc6213728c5140546040f0ee37f54");

	do_KAT_HMAC_hex_hex(&br_sha384_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaa",
		"54657374205573696e67204c61726765"
		"72205468616e20426c6f636b2d53697a"
		"65204b6579202d2048617368204b6579"
		"204669727374",
		"4ece084485813e9088d2c63a041bc5b4"
		"4f9ef1012a2b588f3cd11f05033ac4c6"
		"0c2ef6ab4030fe8296248df163f44952");

	do_KAT_HMAC_hex_hex(&br_sha512_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaa",
		"54657374205573696e67204c61726765"
		"72205468616e20426c6f636b2d53697a"
		"65204b6579202d2048617368204b6579"
		"204669727374",
		"80b24263c7c1a3ebb71493c1dd7be8b4"
		"9b46d1f41b4aeec1121b013783f8f352"
		"6b56d037e05f2598bd0fd2215d6a1e52"
		"95e64f73f63f0aec8b915a985d786598");

	do_KAT_HMAC_hex_hex(&br_sha224_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaa",
		"54686973206973206120746573742075"
		"73696e672061206c6172676572207468"
		"616e20626c6f636b2d73697a65206b65"
		"7920616e642061206c61726765722074"
		"68616e20626c6f636b2d73697a652064"
		"6174612e20546865206b6579206e6565"
		"647320746f2062652068617368656420"
		"6265666f7265206265696e6720757365"
		"642062792074686520484d414320616c"
		"676f726974686d2e",
		"3a854166ac5d9f023f54d517d0b39dbd"
		"946770db9c2b95c9f6f565d1");

	do_KAT_HMAC_hex_hex(&br_sha256_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaa",
		"54686973206973206120746573742075"
		"73696e672061206c6172676572207468"
		"616e20626c6f636b2d73697a65206b65"
		"7920616e642061206c61726765722074"
		"68616e20626c6f636b2d73697a652064"
		"6174612e20546865206b6579206e6565"
		"647320746f2062652068617368656420"
		"6265666f7265206265696e6720757365"
		"642062792074686520484d414320616c"
		"676f726974686d2e",
		"9b09ffa71b942fcb27635fbcd5b0e944"
		"bfdc63644f0713938a7f51535c3a35e2");

	do_KAT_HMAC_hex_hex(&br_sha384_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaa",
		"54686973206973206120746573742075"
		"73696e672061206c6172676572207468"
		"616e20626c6f636b2d73697a65206b65"
		"7920616e642061206c61726765722074"
		"68616e20626c6f636b2d73697a652064"
		"6174612e20546865206b6579206e6565"
		"647320746f2062652068617368656420"
		"6265666f7265206265696e6720757365"
		"642062792074686520484d414320616c"
		"676f726974686d2e",
		"6617178e941f020d351e2f254e8fd32c"
		"602420feb0b8fb9adccebb82461e99c5"
		"a678cc31e799176d3860e6110c46523e");

	do_KAT_HMAC_hex_hex(&br_sha512_vtable,
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaa",
		"54686973206973206120746573742075"
		"73696e672061206c6172676572207468"
		"616e20626c6f636b2d73697a65206b65"
		"7920616e642061206c61726765722074"
		"68616e20626c6f636b2d73697a652064"
		"6174612e20546865206b6579206e6565"
		"647320746f2062652068617368656420"
		"6265666f7265206265696e6720757365"
		"642062792074686520484d414320616c"
		"676f726974686d2e",
		"e37b6a775dc87dbaa4dfa9f96e5e3ffd"
		"debd71f8867289865df5a32d20cdc944"
		"b6022cac3c4982b10d5eeb55c3e4de15"
		"134676fb6de0446065c97440fa8c6a58");

	for (x = 1, u = 0; u < sizeof data; u ++) {
		data[u] = x;
		x = (x * 45) % 257;
	}
	printf("(MD5) ");
	test_HMAC_CT(&br_md5_vtable, key, sizeof key, data);
	printf("(SHA-1) ");
	test_HMAC_CT(&br_sha1_vtable, key, sizeof key, data);
	printf("(SHA-224) ");
	test_HMAC_CT(&br_sha224_vtable, key, sizeof key, data);
	printf("(SHA-256) ");
	test_HMAC_CT(&br_sha256_vtable, key, sizeof key, data);
	printf("(SHA-384) ");
	test_HMAC_CT(&br_sha384_vtable, key, sizeof key, data);
	printf("(SHA-512) ");
	test_HMAC_CT(&br_sha512_vtable, key, sizeof key, data);

	printf("done.\n");
	fflush(stdout);
}

static void
test_HKDF_inner(const br_hash_class *dig, const char *ikmhex,
	const char *salthex, const char *infohex, const char *okmhex)
{
	unsigned char ikm[100], saltbuf[100], info[100], okm[100], tmp[107];
	const unsigned char *salt;
	size_t ikm_len, salt_len, info_len, okm_len;
	br_hkdf_context hc;
	size_t u;

	ikm_len = hextobin(ikm, ikmhex);
	if (salthex == NULL) {
		salt = BR_HKDF_NO_SALT;
		salt_len = 0;
	} else {
		salt = saltbuf;
		salt_len = hextobin(saltbuf, salthex);
	}
	info_len = hextobin(info, infohex);
	okm_len = hextobin(okm, okmhex);

	br_hkdf_init(&hc, dig, salt, salt_len);
	br_hkdf_inject(&hc, ikm, ikm_len);
	br_hkdf_flip(&hc);
	br_hkdf_produce(&hc, info, info_len, tmp, okm_len);
	check_equals("KAT HKDF 1", tmp, okm, okm_len);

	br_hkdf_init(&hc, dig, salt, salt_len);
	for (u = 0; u < ikm_len; u ++) {
		br_hkdf_inject(&hc, &ikm[u], 1);
	}
	br_hkdf_flip(&hc);
	for (u = 0; u < okm_len; u ++) {
		br_hkdf_produce(&hc, info, info_len, &tmp[u], 1);
	}
	check_equals("KAT HKDF 2", tmp, okm, okm_len);

	br_hkdf_init(&hc, dig, salt, salt_len);
	br_hkdf_inject(&hc, ikm, ikm_len);
	br_hkdf_flip(&hc);
	for (u = 0; u < okm_len; u += 7) {
		br_hkdf_produce(&hc, info, info_len, &tmp[u], 7);
	}
	check_equals("KAT HKDF 3", tmp, okm, okm_len);

	printf(".");
	fflush(stdout);
}

static void
test_HKDF(void)
{
	printf("Test HKDF: ");
	fflush(stdout);

	test_HKDF_inner(&br_sha256_vtable,
		"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
		"000102030405060708090a0b0c",
		"f0f1f2f3f4f5f6f7f8f9",
		"3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865");

	test_HKDF_inner(&br_sha256_vtable,
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f",
		"606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeaf",
		"b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
		"b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71cc30c58179ec3e87c14c01d5c1f3434f1d87");

	test_HKDF_inner(&br_sha256_vtable,
		"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
		"",
		"",
		"8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8");

	test_HKDF_inner(&br_sha1_vtable,
		"0b0b0b0b0b0b0b0b0b0b0b",
		"000102030405060708090a0b0c",
		"f0f1f2f3f4f5f6f7f8f9",
		"085a01ea1b10f36933068b56efa5ad81a4f14b822f5b091568a9cdd4f155fda2c22e422478d305f3f896");

	test_HKDF_inner(&br_sha1_vtable,
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f",
		"606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeaf",
		"b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
		"0bd770a74d1160f7c9f12cd5912a06ebff6adcae899d92191fe4305673ba2ffe8fa3f1a4e5ad79f3f334b3b202b2173c486ea37ce3d397ed034c7f9dfeb15c5e927336d0441f4c4300e2cff0d0900b52d3b4");

	test_HKDF_inner(&br_sha1_vtable,
		"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
		"",
		"",
		"0ac1af7002b3d761d1e55298da9d0506b9ae52057220a306e07b6b87e8df21d0ea00033de03984d34918");

	test_HKDF_inner(&br_sha1_vtable,
		"0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c",
		NULL,
		"",
		"2c91117204d745f3500d636a62f64f0ab3bae548aa53d423b0d1f27ebba6f5e5673a081d70cce7acfc48");

	printf(" done.\n");
	fflush(stdout);
}

static void
test_HMAC_DRBG(void)
{
	br_hmac_drbg_context ctx;
	unsigned char seed[42], tmp[30];
	unsigned char ref1[30], ref2[30], ref3[30];
	size_t seed_len;

	printf("Test HMAC_DRBG: ");
	fflush(stdout);

	seed_len = hextobin(seed,
		"009A4D6792295A7F730FC3F2B49CBC0F62E862272F"
		"01795EDF0D54DB760F156D0DAC04C0322B3A204224");
	hextobin(ref1,
		"9305A46DE7FF8EB107194DEBD3FD48AA"
		"20D5E7656CBE0EA69D2A8D4E7C67");
	hextobin(ref2,
		"C70C78608A3B5BE9289BE90EF6E81A9E"
		"2C1516D5751D2F75F50033E45F73");
	hextobin(ref3,
		"475E80E992140567FCC3A50DAB90FE84"
		"BCD7BB03638E9C4656A06F37F650");
	br_hmac_drbg_init(&ctx, &br_sha256_vtable, seed, seed_len);
	br_hmac_drbg_generate(&ctx, tmp, sizeof tmp);
	check_equals("KAT HMAC_DRBG 1", tmp, ref1, sizeof tmp);
	br_hmac_drbg_generate(&ctx, tmp, sizeof tmp);
	check_equals("KAT HMAC_DRBG 2", tmp, ref2, sizeof tmp);
	br_hmac_drbg_generate(&ctx, tmp, sizeof tmp);
	check_equals("KAT HMAC_DRBG 3", tmp, ref3, sizeof tmp);

	memset(&ctx, 0, sizeof ctx);
	br_hmac_drbg_vtable.init(&ctx.vtable,
		&br_sha256_vtable, seed, seed_len);
	ctx.vtable->generate(&ctx.vtable, tmp, sizeof tmp);
	check_equals("KAT HMAC_DRBG 4", tmp, ref1, sizeof tmp);
	ctx.vtable->generate(&ctx.vtable, tmp, sizeof tmp);
	check_equals("KAT HMAC_DRBG 5", tmp, ref2, sizeof tmp);
	ctx.vtable->generate(&ctx.vtable, tmp, sizeof tmp);
	check_equals("KAT HMAC_DRBG 6", tmp, ref3, sizeof tmp);

	printf("done.\n");
	fflush(stdout);
}

static void
test_AESCTR_DRBG(void)
{
	br_aesctr_drbg_context ctx;
	const br_block_ctr_class *ictr;
	unsigned char tmp1[64], tmp2[64];

	printf("Test AESCTR_DRBG: ");
	fflush(stdout);

	ictr = br_aes_x86ni_ctr_get_vtable();
	if (ictr == NULL) {
		ictr = br_aes_pwr8_ctr_get_vtable();
		if (ictr == NULL) {
#if BR_64
			ictr = &br_aes_ct64_ctr_vtable;
#else
			ictr = &br_aes_ct_ctr_vtable;
#endif
		}
	}
	br_aesctr_drbg_init(&ctx, ictr, NULL, 0);
	ctx.vtable->generate(&ctx.vtable, tmp1, sizeof tmp1);
	ctx.vtable->update(&ctx.vtable, "new seed", 8);
	ctx.vtable->generate(&ctx.vtable, tmp2, sizeof tmp2);

	if (memcmp(tmp1, tmp2, sizeof tmp1) == 0) {
		fprintf(stderr, "AESCTR_DRBG failure\n");
		exit(EXIT_FAILURE);
	}

	printf("done.\n");
	fflush(stdout);
}

static void
do_KAT_PRF(br_tls_prf_impl prf,
	const char *ssecret, const char *label, const char *sseed,
	const char *sref)
{
	unsigned char secret[100], seed[100], ref[500], out[500];
	size_t secret_len, seed_len, ref_len;
	br_tls_prf_seed_chunk chunks[2];

	secret_len = hextobin(secret, ssecret);
	seed_len = hextobin(seed, sseed);
	ref_len = hextobin(ref, sref);

	chunks[0].data = seed;
	chunks[0].len = seed_len;
	prf(out, ref_len, secret, secret_len, label, 1, chunks);
	check_equals("TLS PRF KAT 1", out, ref, ref_len);

	chunks[0].data = seed;
	chunks[0].len = seed_len;
	chunks[1].data = NULL;
	chunks[1].len = 0;
	prf(out, ref_len, secret, secret_len, label, 2, chunks);
	check_equals("TLS PRF KAT 2", out, ref, ref_len);

	chunks[0].data = NULL;
	chunks[0].len = 0;
	chunks[1].data = seed;
	chunks[1].len = seed_len;
	prf(out, ref_len, secret, secret_len, label, 2, chunks);
	check_equals("TLS PRF KAT 3", out, ref, ref_len);

	chunks[0].data = seed;
	chunks[0].len = seed_len >> 1;
	chunks[1].data = seed + chunks[0].len;
	chunks[1].len = seed_len - chunks[0].len;
	prf(out, ref_len, secret, secret_len, label, 2, chunks);
	check_equals("TLS PRF KAT 4", out, ref, ref_len);
}

static void
test_PRF(void)
{
	printf("Test TLS PRF: ");
	fflush(stdout);

	/*
	 * Test vector taken from an email that was on:
	 * http://www.imc.org/ietf-tls/mail-archive/msg01589.html
	 * but no longer exists there; a version archived in 2008
	 * can be found on http://www.archive.org/
	 */
	do_KAT_PRF(&br_tls10_prf,
		"abababababababababababababababababababababababababababababababababababababababababababababababab",
		"PRF Testvector",
		"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd",
		"d3d4d1e349b5d515044666d51de32bab258cb521b6b053463e354832fd976754443bcf9a296519bc289abcbc1187e4ebd31e602353776c408aafb74cbc85eff69255f9788faa184cbb957a9819d84a5d7eb006eb459d3ae8de9810454b8b2d8f1afbc655a8c9a013");

	/*
	 * Test vectors are taken from:
	 * https://www.ietf.org/mail-archive/web/tls/current/msg03416.html
	 */
	do_KAT_PRF(&br_tls12_sha256_prf,
		"9bbe436ba940f017b17652849a71db35",
		"test label",
		"a0ba9f936cda311827a6f796ffd5198c",
		"e3f229ba727be17b8d122620557cd453c2aab21d07c3d495329b52d4e61edb5a6b301791e90d35c9c9a46b4e14baf9af0fa022f7077def17abfd3797c0564bab4fbc91666e9def9b97fce34f796789baa48082d122ee42c5a72e5a5110fff70187347b66");
	do_KAT_PRF(&br_tls12_sha384_prf,
		"b80b733d6ceefcdc71566ea48e5567df",
		"test label",
		"cd665cf6a8447dd6ff8b27555edb7465",
		"7b0c18e9ced410ed1804f2cfa34a336a1c14dffb4900bb5fd7942107e81c83cde9ca0faa60be9fe34f82b1233c9146a0e534cb400fed2700884f9dc236f80edd8bfa961144c9e8d792eca722a7b32fc3d416d473ebc2c5fd4abfdad05d9184259b5bf8cd4d90fa0d31e2dec479e4f1a26066f2eea9a69236a3e52655c9e9aee691c8f3a26854308d5eaa3be85e0990703d73e56f");

	printf("done.\n");
	fflush(stdout);
}

/*
 * AES known-answer tests. Order: key, plaintext, ciphertext.
 */
static const char *const KAT_AES[] = {
	/*
	 * From FIPS-197.
	 */
	"000102030405060708090a0b0c0d0e0f",
	"00112233445566778899aabbccddeeff",
	"69c4e0d86a7b0430d8cdb78070b4c55a",

	"000102030405060708090a0b0c0d0e0f1011121314151617",
	"00112233445566778899aabbccddeeff",
	"dda97ca4864cdfe06eaf70a0ec0d7191",

	"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
	"00112233445566778899aabbccddeeff",
	"8ea2b7ca516745bfeafc49904b496089",

	/*
	 * From NIST validation suite (ECBVarTxt128.rsp).
	 */
	"00000000000000000000000000000000",
	"80000000000000000000000000000000",
	"3ad78e726c1ec02b7ebfe92b23d9ec34",

	"00000000000000000000000000000000",
	"c0000000000000000000000000000000",
	"aae5939c8efdf2f04e60b9fe7117b2c2",

	"00000000000000000000000000000000",
	"e0000000000000000000000000000000",
	"f031d4d74f5dcbf39daaf8ca3af6e527",

	"00000000000000000000000000000000",
	"f0000000000000000000000000000000",
	"96d9fd5cc4f07441727df0f33e401a36",

	"00000000000000000000000000000000",
	"f8000000000000000000000000000000",
	"30ccdb044646d7e1f3ccea3dca08b8c0",

	"00000000000000000000000000000000",
	"fc000000000000000000000000000000",
	"16ae4ce5042a67ee8e177b7c587ecc82",

	"00000000000000000000000000000000",
	"fe000000000000000000000000000000",
	"b6da0bb11a23855d9c5cb1b4c6412e0a",

	"00000000000000000000000000000000",
	"ff000000000000000000000000000000",
	"db4f1aa530967d6732ce4715eb0ee24b",

	"00000000000000000000000000000000",
	"ff800000000000000000000000000000",
	"a81738252621dd180a34f3455b4baa2f",

	"00000000000000000000000000000000",
	"ffc00000000000000000000000000000",
	"77e2b508db7fd89234caf7939ee5621a",

	"00000000000000000000000000000000",
	"ffe00000000000000000000000000000",
	"b8499c251f8442ee13f0933b688fcd19",

	"00000000000000000000000000000000",
	"fff00000000000000000000000000000",
	"965135f8a81f25c9d630b17502f68e53",

	"00000000000000000000000000000000",
	"fff80000000000000000000000000000",
	"8b87145a01ad1c6cede995ea3670454f",

	"00000000000000000000000000000000",
	"fffc0000000000000000000000000000",
	"8eae3b10a0c8ca6d1d3b0fa61e56b0b2",

	"00000000000000000000000000000000",
	"fffe0000000000000000000000000000",
	"64b4d629810fda6bafdf08f3b0d8d2c5",

	"00000000000000000000000000000000",
	"ffff0000000000000000000000000000",
	"d7e5dbd3324595f8fdc7d7c571da6c2a",

	"00000000000000000000000000000000",
	"ffff8000000000000000000000000000",
	"f3f72375264e167fca9de2c1527d9606",

	"00000000000000000000000000000000",
	"ffffc000000000000000000000000000",
	"8ee79dd4f401ff9b7ea945d86666c13b",

	"00000000000000000000000000000000",
	"ffffe000000000000000000000000000",
	"dd35cea2799940b40db3f819cb94c08b",

	"00000000000000000000000000000000",
	"fffff000000000000000000000000000",
	"6941cb6b3e08c2b7afa581ebdd607b87",

	"00000000000000000000000000000000",
	"fffff800000000000000000000000000",
	"2c20f439f6bb097b29b8bd6d99aad799",

	"00000000000000000000000000000000",
	"fffffc00000000000000000000000000",
	"625d01f058e565f77ae86378bd2c49b3",

	"00000000000000000000000000000000",
	"fffffe00000000000000000000000000",
	"c0b5fd98190ef45fbb4301438d095950",

	"00000000000000000000000000000000",
	"ffffff00000000000000000000000000",
	"13001ff5d99806efd25da34f56be854b",

	"00000000000000000000000000000000",
	"ffffff80000000000000000000000000",
	"3b594c60f5c8277a5113677f94208d82",

	"00000000000000000000000000000000",
	"ffffffc0000000000000000000000000",
	"e9c0fc1818e4aa46bd2e39d638f89e05",

	"00000000000000000000000000000000",
	"ffffffe0000000000000000000000000",
	"f8023ee9c3fdc45a019b4e985c7e1a54",

	"00000000000000000000000000000000",
	"fffffff0000000000000000000000000",
	"35f40182ab4662f3023baec1ee796b57",

	"00000000000000000000000000000000",
	"fffffff8000000000000000000000000",
	"3aebbad7303649b4194a6945c6cc3694",

	"00000000000000000000000000000000",
	"fffffffc000000000000000000000000",
	"a2124bea53ec2834279bed7f7eb0f938",

	"00000000000000000000000000000000",
	"fffffffe000000000000000000000000",
	"b9fb4399fa4facc7309e14ec98360b0a",

	"00000000000000000000000000000000",
	"ffffffff000000000000000000000000",
	"c26277437420c5d634f715aea81a9132",

	"00000000000000000000000000000000",
	"ffffffff800000000000000000000000",
	"171a0e1b2dd424f0e089af2c4c10f32f",

	"00000000000000000000000000000000",
	"ffffffffc00000000000000000000000",
	"7cadbe402d1b208fe735edce00aee7ce",

	"00000000000000000000000000000000",
	"ffffffffe00000000000000000000000",
	"43b02ff929a1485af6f5c6d6558baa0f",

	"00000000000000000000000000000000",
	"fffffffff00000000000000000000000",
	"092faacc9bf43508bf8fa8613ca75dea",

	"00000000000000000000000000000000",
	"fffffffff80000000000000000000000",
	"cb2bf8280f3f9742c7ed513fe802629c",

	"00000000000000000000000000000000",
	"fffffffffc0000000000000000000000",
	"215a41ee442fa992a6e323986ded3f68",

	"00000000000000000000000000000000",
	"fffffffffe0000000000000000000000",
	"f21e99cf4f0f77cea836e11a2fe75fb1",

	"00000000000000000000000000000000",
	"ffffffffff0000000000000000000000",
	"95e3a0ca9079e646331df8b4e70d2cd6",

	"00000000000000000000000000000000",
	"ffffffffff8000000000000000000000",
	"4afe7f120ce7613f74fc12a01a828073",

	"00000000000000000000000000000000",
	"ffffffffffc000000000000000000000",
	"827f000e75e2c8b9d479beed913fe678",

	"00000000000000000000000000000000",
	"ffffffffffe000000000000000000000",
	"35830c8e7aaefe2d30310ef381cbf691",

	"00000000000000000000000000000000",
	"fffffffffff000000000000000000000",
	"191aa0f2c8570144f38657ea4085ebe5",

	"00000000000000000000000000000000",
	"fffffffffff800000000000000000000",
	"85062c2c909f15d9269b6c18ce99c4f0",

	"00000000000000000000000000000000",
	"fffffffffffc00000000000000000000",
	"678034dc9e41b5a560ed239eeab1bc78",

	"00000000000000000000000000000000",
	"fffffffffffe00000000000000000000",
	"c2f93a4ce5ab6d5d56f1b93cf19911c1",

	"00000000000000000000000000000000",
	"ffffffffffff00000000000000000000",
	"1c3112bcb0c1dcc749d799743691bf82",

	"00000000000000000000000000000000",
	"ffffffffffff80000000000000000000",
	"00c55bd75c7f9c881989d3ec1911c0d4",

	"00000000000000000000000000000000",
	"ffffffffffffc0000000000000000000",
	"ea2e6b5ef182b7dff3629abd6a12045f",

	"00000000000000000000000000000000",
	"ffffffffffffe0000000000000000000",
	"22322327e01780b17397f24087f8cc6f",

	"00000000000000000000000000000000",
	"fffffffffffff0000000000000000000",
	"c9cacb5cd11692c373b2411768149ee7",

	"00000000000000000000000000000000",
	"fffffffffffff8000000000000000000",
	"a18e3dbbca577860dab6b80da3139256",

	"00000000000000000000000000000000",
	"fffffffffffffc000000000000000000",
	"79b61c37bf328ecca8d743265a3d425c",

	"00000000000000000000000000000000",
	"fffffffffffffe000000000000000000",
	"d2d99c6bcc1f06fda8e27e8ae3f1ccc7",

	"00000000000000000000000000000000",
	"ffffffffffffff000000000000000000",
	"1bfd4b91c701fd6b61b7f997829d663b",

	"00000000000000000000000000000000",
	"ffffffffffffff800000000000000000",
	"11005d52f25f16bdc9545a876a63490a",

	"00000000000000000000000000000000",
	"ffffffffffffffc00000000000000000",
	"3a4d354f02bb5a5e47d39666867f246a",

	"00000000000000000000000000000000",
	"ffffffffffffffe00000000000000000",
	"d451b8d6e1e1a0ebb155fbbf6e7b7dc3",

	"00000000000000000000000000000000",
	"fffffffffffffff00000000000000000",
	"6898d4f42fa7ba6a10ac05e87b9f2080",

	"00000000000000000000000000000000",
	"fffffffffffffff80000000000000000",
	"b611295e739ca7d9b50f8e4c0e754a3f",

	"00000000000000000000000000000000",
	"fffffffffffffffc0000000000000000",
	"7d33fc7d8abe3ca1936759f8f5deaf20",

	"00000000000000000000000000000000",
	"fffffffffffffffe0000000000000000",
	"3b5e0f566dc96c298f0c12637539b25c",

	"00000000000000000000000000000000",
	"ffffffffffffffff0000000000000000",
	"f807c3e7985fe0f5a50e2cdb25c5109e",

	"00000000000000000000000000000000",
	"ffffffffffffffff8000000000000000",
	"41f992a856fb278b389a62f5d274d7e9",

	"00000000000000000000000000000000",
	"ffffffffffffffffc000000000000000",
	"10d3ed7a6fe15ab4d91acbc7d0767ab1",

	"00000000000000000000000000000000",
	"ffffffffffffffffe000000000000000",
	"21feecd45b2e675973ac33bf0c5424fc",

	"00000000000000000000000000000000",
	"fffffffffffffffff000000000000000",
	"1480cb3955ba62d09eea668f7c708817",

	"00000000000000000000000000000000",
	"fffffffffffffffff800000000000000",
	"66404033d6b72b609354d5496e7eb511",

	"00000000000000000000000000000000",
	"fffffffffffffffffc00000000000000",
	"1c317a220a7d700da2b1e075b00266e1",

	"00000000000000000000000000000000",
	"fffffffffffffffffe00000000000000",
	"ab3b89542233f1271bf8fd0c0f403545",

	"00000000000000000000000000000000",
	"ffffffffffffffffff00000000000000",
	"d93eae966fac46dca927d6b114fa3f9e",

	"00000000000000000000000000000000",
	"ffffffffffffffffff80000000000000",
	"1bdec521316503d9d5ee65df3ea94ddf",

	"00000000000000000000000000000000",
	"ffffffffffffffffffc0000000000000",
	"eef456431dea8b4acf83bdae3717f75f",

	"00000000000000000000000000000000",
	"ffffffffffffffffffe0000000000000",
	"06f2519a2fafaa596bfef5cfa15c21b9",

	"00000000000000000000000000000000",
	"fffffffffffffffffff0000000000000",
	"251a7eac7e2fe809e4aa8d0d7012531a",

	"00000000000000000000000000000000",
	"fffffffffffffffffff8000000000000",
	"3bffc16e4c49b268a20f8d96a60b4058",

	"00000000000000000000000000000000",
	"fffffffffffffffffffc000000000000",
	"e886f9281999c5bb3b3e8862e2f7c988",

	"00000000000000000000000000000000",
	"fffffffffffffffffffe000000000000",
	"563bf90d61beef39f48dd625fcef1361",

	"00000000000000000000000000000000",
	"ffffffffffffffffffff000000000000",
	"4d37c850644563c69fd0acd9a049325b",

	"00000000000000000000000000000000",
	"ffffffffffffffffffff800000000000",
	"b87c921b91829ef3b13ca541ee1130a6",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffc00000000000",
	"2e65eb6b6ea383e109accce8326b0393",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffe00000000000",
	"9ca547f7439edc3e255c0f4d49aa8990",

	"00000000000000000000000000000000",
	"fffffffffffffffffffff00000000000",
	"a5e652614c9300f37816b1f9fd0c87f9",

	"00000000000000000000000000000000",
	"fffffffffffffffffffff80000000000",
	"14954f0b4697776f44494fe458d814ed",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffc0000000000",
	"7c8d9ab6c2761723fe42f8bb506cbcf7",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffe0000000000",
	"db7e1932679fdd99742aab04aa0d5a80",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffff0000000000",
	"4c6a1c83e568cd10f27c2d73ded19c28",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffff8000000000",
	"90ecbe6177e674c98de412413f7ac915",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffc000000000",
	"90684a2ac55fe1ec2b8ebd5622520b73",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffe000000000",
	"7472f9a7988607ca79707795991035e6",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffff000000000",
	"56aff089878bf3352f8df172a3ae47d8",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffff800000000",
	"65c0526cbe40161b8019a2a3171abd23",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffc00000000",
	"377be0be33b4e3e310b4aabda173f84f",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffe00000000",
	"9402e9aa6f69de6504da8d20c4fcaa2f",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffff00000000",
	"123c1f4af313ad8c2ce648b2e71fb6e1",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffff80000000",
	"1ffc626d30203dcdb0019fb80f726cf4",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffc0000000",
	"76da1fbe3a50728c50fd2e621b5ad885",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffe0000000",
	"082eb8be35f442fb52668e16a591d1d6",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffff0000000",
	"e656f9ecf5fe27ec3e4a73d00c282fb3",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffff8000000",
	"2ca8209d63274cd9a29bb74bcd77683a",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffc000000",
	"79bf5dce14bb7dd73a8e3611de7ce026",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffe000000",
	"3c849939a5d29399f344c4a0eca8a576",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffff000000",
	"ed3c0a94d59bece98835da7aa4f07ca2",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffff800000",
	"63919ed4ce10196438b6ad09d99cd795",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffffc00000",
	"7678f3a833f19fea95f3c6029e2bc610",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffffe00000",
	"3aa426831067d36b92be7c5f81c13c56",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffff00000",
	"9272e2d2cdd11050998c845077a30ea0",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffff80000",
	"088c4b53f5ec0ff814c19adae7f6246c",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffffc0000",
	"4010a5e401fdf0a0354ddbcc0d012b17",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffffe0000",
	"a87a385736c0a6189bd6589bd8445a93",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffffff0000",
	"545f2b83d9616dccf60fa9830e9cd287",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffffff8000",
	"4b706f7f92406352394037a6d4f4688d",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffffffc000",
	"b7972b3941c44b90afa7b264bfba7387",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffffffe000",
	"6f45732cf10881546f0fd23896d2bb60",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffffff000",
	"2e3579ca15af27f64b3c955a5bfc30ba",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffffff800",
	"34a2c5a91ae2aec99b7d1b5fa6780447",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffffffc00",
	"a4d6616bd04f87335b0e53351227a9ee",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffffffe00",
	"7f692b03945867d16179a8cefc83ea3f",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffffffff00",
	"3bd141ee84a0e6414a26e7a4f281f8a2",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffffffff80",
	"d1788f572d98b2b16ec5d5f3922b99bc",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffffffffc0",
	"0833ff6f61d98a57b288e8c3586b85a6",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffffffffe0",
	"8568261797de176bf0b43becc6285afb",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffffffff0",
	"f9b0fda0c4a898f5b9e6f661c4ce4d07",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffffffff8",
	"8ade895913685c67c5269f8aae42983e",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffffffffc",
	"39bde67d5c8ed8a8b1c37eb8fa9f5ac0",

	"00000000000000000000000000000000",
	"fffffffffffffffffffffffffffffffe",
	"5c005e72c1418c44f569f2ea33ba54f3",

	"00000000000000000000000000000000",
	"ffffffffffffffffffffffffffffffff",
	"3f5b8cc9ea855a0afa7347d23e8d664e",

	/*
	 * From NIST validation suite (ECBVarTxt192.rsp).
	 */
	"000000000000000000000000000000000000000000000000",
	"80000000000000000000000000000000",
	"6cd02513e8d4dc986b4afe087a60bd0c",

	"000000000000000000000000000000000000000000000000",
	"c0000000000000000000000000000000",
	"2ce1f8b7e30627c1c4519eada44bc436",

	"000000000000000000000000000000000000000000000000",
	"e0000000000000000000000000000000",
	"9946b5f87af446f5796c1fee63a2da24",

	"000000000000000000000000000000000000000000000000",
	"f0000000000000000000000000000000",
	"2a560364ce529efc21788779568d5555",

	"000000000000000000000000000000000000000000000000",
	"f8000000000000000000000000000000",
	"35c1471837af446153bce55d5ba72a0a",

	"000000000000000000000000000000000000000000000000",
	"fc000000000000000000000000000000",
	"ce60bc52386234f158f84341e534cd9e",

	"000000000000000000000000000000000000000000000000",
	"fe000000000000000000000000000000",
	"8c7c27ff32bcf8dc2dc57c90c2903961",

	"000000000000000000000000000000000000000000000000",
	"ff000000000000000000000000000000",
	"32bb6a7ec84499e166f936003d55a5bb",

	"000000000000000000000000000000000000000000000000",
	"ff800000000000000000000000000000",
	"a5c772e5c62631ef660ee1d5877f6d1b",

	"000000000000000000000000000000000000000000000000",
	"ffc00000000000000000000000000000",
	"030d7e5b64f380a7e4ea5387b5cd7f49",

	"000000000000000000000000000000000000000000000000",
	"ffe00000000000000000000000000000",
	"0dc9a2610037009b698f11bb7e86c83e",

	"000000000000000000000000000000000000000000000000",
	"fff00000000000000000000000000000",
	"0046612c766d1840c226364f1fa7ed72",

	"000000000000000000000000000000000000000000000000",
	"fff80000000000000000000000000000",
	"4880c7e08f27befe78590743c05e698b",

	"000000000000000000000000000000000000000000000000",
	"fffc0000000000000000000000000000",
	"2520ce829a26577f0f4822c4ecc87401",

	"000000000000000000000000000000000000000000000000",
	"fffe0000000000000000000000000000",
	"8765e8acc169758319cb46dc7bcf3dca",

	"000000000000000000000000000000000000000000000000",
	"ffff0000000000000000000000000000",
	"e98f4ba4f073df4baa116d011dc24a28",

	"000000000000000000000000000000000000000000000000",
	"ffff8000000000000000000000000000",
	"f378f68c5dbf59e211b3a659a7317d94",

	"000000000000000000000000000000000000000000000000",
	"ffffc000000000000000000000000000",
	"283d3b069d8eb9fb432d74b96ca762b4",

	"000000000000000000000000000000000000000000000000",
	"ffffe000000000000000000000000000",
	"a7e1842e8a87861c221a500883245c51",

	"000000000000000000000000000000000000000000000000",
	"fffff000000000000000000000000000",
	"77aa270471881be070fb52c7067ce732",

	"000000000000000000000000000000000000000000000000",
	"fffff800000000000000000000000000",
	"01b0f476d484f43f1aeb6efa9361a8ac",

	"000000000000000000000000000000000000000000000000",
	"fffffc00000000000000000000000000",
	"1c3a94f1c052c55c2d8359aff2163b4f",

	"000000000000000000000000000000000000000000000000",
	"fffffe00000000000000000000000000",
	"e8a067b604d5373d8b0f2e05a03b341b",

	"000000000000000000000000000000000000000000000000",
	"ffffff00000000000000000000000000",
	"a7876ec87f5a09bfea42c77da30fd50e",

	"000000000000000000000000000000000000000000000000",
	"ffffff80000000000000000000000000",
	"0cf3e9d3a42be5b854ca65b13f35f48d",

	"000000000000000000000000000000000000000000000000",
	"ffffffc0000000000000000000000000",
	"6c62f6bbcab7c3e821c9290f08892dda",

	"000000000000000000000000000000000000000000000000",
	"ffffffe0000000000000000000000000",
	"7f5e05bd2068738196fee79ace7e3aec",

	"000000000000000000000000000000000000000000000000",
	"fffffff0000000000000000000000000",
	"440e0d733255cda92fb46e842fe58054",

	"000000000000000000000000000000000000000000000000",
	"fffffff8000000000000000000000000",
	"aa5d5b1c4ea1b7a22e5583ac2e9ed8a7",

	"000000000000000000000000000000000000000000000000",
	"fffffffc000000000000000000000000",
	"77e537e89e8491e8662aae3bc809421d",

	"000000000000000000000000000000000000000000000000",
	"fffffffe000000000000000000000000",
	"997dd3e9f1598bfa73f75973f7e93b76",

	"000000000000000000000000000000000000000000000000",
	"ffffffff000000000000000000000000",
	"1b38d4f7452afefcb7fc721244e4b72e",

	"000000000000000000000000000000000000000000000000",
	"ffffffff800000000000000000000000",
	"0be2b18252e774dda30cdda02c6906e3",

	"000000000000000000000000000000000000000000000000",
	"ffffffffc00000000000000000000000",
	"d2695e59c20361d82652d7d58b6f11b2",

	"000000000000000000000000000000000000000000000000",
	"ffffffffe00000000000000000000000",
	"902d88d13eae52089abd6143cfe394e9",

	"000000000000000000000000000000000000000000000000",
	"fffffffff00000000000000000000000",
	"d49bceb3b823fedd602c305345734bd2",

	"000000000000000000000000000000000000000000000000",
	"fffffffff80000000000000000000000",
	"707b1dbb0ffa40ef7d95def421233fae",

	"000000000000000000000000000000000000000000000000",
	"fffffffffc0000000000000000000000",
	"7ca0c1d93356d9eb8aa952084d75f913",

	"000000000000000000000000000000000000000000000000",
	"fffffffffe0000000000000000000000",
	"f2cbf9cb186e270dd7bdb0c28febc57d",

	"000000000000000000000000000000000000000000000000",
	"ffffffffff0000000000000000000000",
	"c94337c37c4e790ab45780bd9c3674a0",

	"000000000000000000000000000000000000000000000000",
	"ffffffffff8000000000000000000000",
	"8e3558c135252fb9c9f367ed609467a1",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffc000000000000000000000",
	"1b72eeaee4899b443914e5b3a57fba92",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffe000000000000000000000",
	"011865f91bc56868d051e52c9efd59b7",

	"000000000000000000000000000000000000000000000000",
	"fffffffffff000000000000000000000",
	"e4771318ad7a63dd680f6e583b7747ea",

	"000000000000000000000000000000000000000000000000",
	"fffffffffff800000000000000000000",
	"61e3d194088dc8d97e9e6db37457eac5",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffc00000000000000000000",
	"36ff1ec9ccfbc349e5d356d063693ad6",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffe00000000000000000000",
	"3cc9e9a9be8cc3f6fb2ea24088e9bb19",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffff00000000000000000000",
	"1ee5ab003dc8722e74905d9a8fe3d350",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffff80000000000000000000",
	"245339319584b0a412412869d6c2eada",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffc0000000000000000000",
	"7bd496918115d14ed5380852716c8814",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffe0000000000000000000",
	"273ab2f2b4a366a57d582a339313c8b1",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffff0000000000000000000",
	"113365a9ffbe3b0ca61e98507554168b",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffff8000000000000000000",
	"afa99c997ac478a0dea4119c9e45f8b1",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffc000000000000000000",
	"9216309a7842430b83ffb98638011512",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffe000000000000000000",
	"62abc792288258492a7cb45145f4b759",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffff000000000000000000",
	"534923c169d504d7519c15d30e756c50",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffff800000000000000000",
	"fa75e05bcdc7e00c273fa33f6ee441d2",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffc00000000000000000",
	"7d350fa6057080f1086a56b17ec240db",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffe00000000000000000",
	"f34e4a6324ea4a5c39a661c8fe5ada8f",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffff00000000000000000",
	"0882a16f44088d42447a29ac090ec17e",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffff80000000000000000",
	"3a3c15bfc11a9537c130687004e136ee",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffc0000000000000000",
	"22c0a7678dc6d8cf5c8a6d5a9960767c",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffe0000000000000000",
	"b46b09809d68b9a456432a79bdc2e38c",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffff0000000000000000",
	"93baaffb35fbe739c17c6ac22eecf18f",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffff8000000000000000",
	"c8aa80a7850675bc007c46df06b49868",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffc000000000000000",
	"12c6f3877af421a918a84b775858021d",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffe000000000000000",
	"33f123282c5d633924f7d5ba3f3cab11",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffff000000000000000",
	"a8f161002733e93ca4527d22c1a0c5bb",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffff800000000000000",
	"b72f70ebf3e3fda23f508eec76b42c02",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffc00000000000000",
	"6a9d965e6274143f25afdcfc88ffd77c",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffe00000000000000",
	"a0c74fd0b9361764ce91c5200b095357",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffff00000000000000",
	"091d1fdc2bd2c346cd5046a8c6209146",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffff80000000000000",
	"e2a37580116cfb71856254496ab0aca8",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffc0000000000000",
	"e0b3a00785917c7efc9adba322813571",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffe0000000000000",
	"733d41f4727b5ef0df4af4cf3cffa0cb",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffff0000000000000",
	"a99ebb030260826f981ad3e64490aa4f",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffff8000000000000",
	"73f34c7d3eae5e80082c1647524308ee",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffc000000000000",
	"40ebd5ad082345b7a2097ccd3464da02",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffe000000000000",
	"7cc4ae9a424b2cec90c97153c2457ec5",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffff000000000000",
	"54d632d03aba0bd0f91877ebdd4d09cb",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffff800000000000",
	"d3427be7e4d27cd54f5fe37b03cf0897",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffc00000000000",
	"b2099795e88cc158fd75ea133d7e7fbe",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffe00000000000",
	"a6cae46fb6fadfe7a2c302a34242817b",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffff00000000000",
	"026a7024d6a902e0b3ffccbaa910cc3f",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffff80000000000",
	"156f07767a85a4312321f63968338a01",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffc0000000000",
	"15eec9ebf42b9ca76897d2cd6c5a12e2",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffe0000000000",
	"db0d3a6fdcc13f915e2b302ceeb70fd8",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffff0000000000",
	"71dbf37e87a2e34d15b20e8f10e48924",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffff8000000000",
	"c745c451e96ff3c045e4367c833e3b54",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffc000000000",
	"340da09c2dd11c3b679d08ccd27dd595",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffe000000000",
	"8279f7c0c2a03ee660c6d392db025d18",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffff000000000",
	"a4b2c7d8eba531ff47c5041a55fbd1ec",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffff800000000",
	"74569a2ca5a7bd5131ce8dc7cbfbf72f",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffc00000000",
	"3713da0c0219b63454035613b5a403dd",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffe00000000",
	"8827551ddcc9df23fa72a3de4e9f0b07",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffff00000000",
	"2e3febfd625bfcd0a2c06eb460da1732",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffff80000000",
	"ee82e6ba488156f76496311da6941deb",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffc0000000",
	"4770446f01d1f391256e85a1b30d89d3",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffe0000000",
	"af04b68f104f21ef2afb4767cf74143c",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffff0000000",
	"cf3579a9ba38c8e43653173e14f3a4c6",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffff8000000",
	"b3bba904f4953e09b54800af2f62e7d4",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffc000000",
	"fc4249656e14b29eb9c44829b4c59a46",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffe000000",
	"9b31568febe81cfc2e65af1c86d1a308",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffff000000",
	"9ca09c25f273a766db98a480ce8dfedc",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffff800000",
	"b909925786f34c3c92d971883c9fbedf",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffc00000",
	"82647f1332fe570a9d4d92b2ee771d3b",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffe00000",
	"3604a7e80832b3a99954bca6f5b9f501",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffff00000",
	"884607b128c5de3ab39a529a1ef51bef",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffff80000",
	"670cfa093d1dbdb2317041404102435e",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffc0000",
	"7a867195f3ce8769cbd336502fbb5130",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffe0000",
	"52efcf64c72b2f7ca5b3c836b1078c15",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffff0000",
	"4019250f6eefb2ac5ccbcae044e75c7e",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffff8000",
	"022c4f6f5a017d292785627667ddef24",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffc000",
	"e9c21078a2eb7e03250f71000fa9e3ed",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffe000",
	"a13eaeeb9cd391da4e2b09490b3e7fad",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffff000",
	"c958a171dca1d4ed53e1af1d380803a9",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffff800",
	"21442e07a110667f2583eaeeee44dc8c",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffc00",
	"59bbb353cf1dd867a6e33737af655e99",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffe00",
	"43cd3b25375d0ce41087ff9fe2829639",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffff00",
	"6b98b17e80d1118e3516bd768b285a84",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffff80",
	"ae47ed3676ca0c08deea02d95b81db58",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffffc0",
	"34ec40dc20413795ed53628ea748720b",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffffe0",
	"4dc68163f8e9835473253542c8a65d46",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffff0",
	"2aabb999f43693175af65c6c612c46fb",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffff8",
	"e01f94499dac3547515c5b1d756f0f58",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffffc",
	"9d12435a46480ce00ea349f71799df9a",

	"000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffffe",
	"cef41d16d266bdfe46938ad7884cc0cf",

	"000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffffff",
	"b13db4da1f718bc6904797c82bcf2d32",

	/*
	 * From NIST validation suite (ECBVarTxt256.rsp).
	 */
	"0000000000000000000000000000000000000000000000000000000000000000",
	"80000000000000000000000000000000",
	"ddc6bf790c15760d8d9aeb6f9a75fd4e",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"c0000000000000000000000000000000",
	"0a6bdc6d4c1e6280301fd8e97ddbe601",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"e0000000000000000000000000000000",
	"9b80eefb7ebe2d2b16247aa0efc72f5d",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"f0000000000000000000000000000000",
	"7f2c5ece07a98d8bee13c51177395ff7",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"f8000000000000000000000000000000",
	"7818d800dcf6f4be1e0e94f403d1e4c2",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fc000000000000000000000000000000",
	"e74cd1c92f0919c35a0324123d6177d3",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fe000000000000000000000000000000",
	"8092a4dcf2da7e77e93bdd371dfed82e",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ff000000000000000000000000000000",
	"49af6b372135acef10132e548f217b17",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ff800000000000000000000000000000",
	"8bcd40f94ebb63b9f7909676e667f1e7",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffc00000000000000000000000000000",
	"fe1cffb83f45dcfb38b29be438dbd3ab",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffe00000000000000000000000000000",
	"0dc58a8d886623705aec15cb1e70dc0e",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fff00000000000000000000000000000",
	"c218faa16056bd0774c3e8d79c35a5e4",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fff80000000000000000000000000000",
	"047bba83f7aa841731504e012208fc9e",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffc0000000000000000000000000000",
	"dc8f0e4915fd81ba70a331310882f6da",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffe0000000000000000000000000000",
	"1569859ea6b7206c30bf4fd0cbfac33c",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffff0000000000000000000000000000",
	"300ade92f88f48fa2df730ec16ef44cd",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffff8000000000000000000000000000",
	"1fe6cc3c05965dc08eb0590c95ac71d0",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffc000000000000000000000000000",
	"59e858eaaa97fec38111275b6cf5abc0",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffe000000000000000000000000000",
	"2239455e7afe3b0616100288cc5a723b",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffff000000000000000000000000000",
	"3ee500c5c8d63479717163e55c5c4522",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffff800000000000000000000000000",
	"d5e38bf15f16d90e3e214041d774daa8",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffc00000000000000000000000000",
	"b1f4066e6f4f187dfe5f2ad1b17819d0",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffe00000000000000000000000000",
	"6ef4cc4de49b11065d7af2909854794a",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffff00000000000000000000000000",
	"ac86bc606b6640c309e782f232bf367f",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffff80000000000000000000000000",
	"36aff0ef7bf3280772cf4cac80a0d2b2",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffc0000000000000000000000000",
	"1f8eedea0f62a1406d58cfc3ecea72cf",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffe0000000000000000000000000",
	"abf4154a3375a1d3e6b1d454438f95a6",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffff0000000000000000000000000",
	"96f96e9d607f6615fc192061ee648b07",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffff8000000000000000000000000",
	"cf37cdaaa0d2d536c71857634c792064",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffc000000000000000000000000",
	"fbd6640c80245c2b805373f130703127",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffe000000000000000000000000",
	"8d6a8afe55a6e481badae0d146f436db",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffff000000000000000000000000",
	"6a4981f2915e3e68af6c22385dd06756",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffff800000000000000000000000",
	"42a1136e5f8d8d21d3101998642d573b",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffc00000000000000000000000",
	"9b471596dc69ae1586cee6158b0b0181",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffe00000000000000000000000",
	"753665c4af1eff33aa8b628bf8741cfd",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffff00000000000000000000000",
	"9a682acf40be01f5b2a4193c9a82404d",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffff80000000000000000000000",
	"54fafe26e4287f17d1935f87eb9ade01",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffc0000000000000000000000",
	"49d541b2e74cfe73e6a8e8225f7bd449",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffe0000000000000000000000",
	"11a45530f624ff6f76a1b3826626ff7b",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffff0000000000000000000000",
	"f96b0c4a8bc6c86130289f60b43b8fba",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffff8000000000000000000000",
	"48c7d0e80834ebdc35b6735f76b46c8b",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffc000000000000000000000",
	"2463531ab54d66955e73edc4cb8eaa45",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffe000000000000000000000",
	"ac9bd8e2530469134b9d5b065d4f565b",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffff000000000000000000000",
	"3f5f9106d0e52f973d4890e6f37e8a00",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffff800000000000000000000",
	"20ebc86f1304d272e2e207e59db639f0",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffc00000000000000000000",
	"e67ae6426bf9526c972cff072b52252c",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffe00000000000000000000",
	"1a518dddaf9efa0d002cc58d107edfc8",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffff00000000000000000000",
	"ead731af4d3a2fe3b34bed047942a49f",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffff80000000000000000000",
	"b1d4efe40242f83e93b6c8d7efb5eae9",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffc0000000000000000000",
	"cd2b1fec11fd906c5c7630099443610a",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffe0000000000000000000",
	"a1853fe47fe29289d153161d06387d21",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffff0000000000000000000",
	"4632154179a555c17ea604d0889fab14",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffff8000000000000000000",
	"dd27cac6401a022e8f38f9f93e774417",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffc000000000000000000",
	"c090313eb98674f35f3123385fb95d4d",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffe000000000000000000",
	"cc3526262b92f02edce548f716b9f45c",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffff000000000000000000",
	"c0838d1a2b16a7c7f0dfcc433c399c33",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffff800000000000000000",
	"0d9ac756eb297695eed4d382eb126d26",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffc00000000000000000",
	"56ede9dda3f6f141bff1757fa689c3e1",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffe00000000000000000",
	"768f520efe0f23e61d3ec8ad9ce91774",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffff00000000000000000",
	"b1144ddfa75755213390e7c596660490",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffff80000000000000000",
	"1d7c0c4040b355b9d107a99325e3b050",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffc0000000000000000",
	"d8e2bb1ae8ee3dcf5bf7d6c38da82a1a",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffe0000000000000000",
	"faf82d178af25a9886a47e7f789b98d7",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffff0000000000000000",
	"9b58dbfd77fe5aca9cfc190cd1b82d19",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffff8000000000000000",
	"77f392089042e478ac16c0c86a0b5db5",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffc000000000000000",
	"19f08e3420ee69b477ca1420281c4782",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffe000000000000000",
	"a1b19beee4e117139f74b3c53fdcb875",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffff000000000000000",
	"a37a5869b218a9f3a0868d19aea0ad6a",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffff800000000000000",
	"bc3594e865bcd0261b13202731f33580",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffc00000000000000",
	"811441ce1d309eee7185e8c752c07557",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffe00000000000000",
	"959971ce4134190563518e700b9874d1",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffff00000000000000",
	"76b5614a042707c98e2132e2e805fe63",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffff80000000000000",
	"7d9fa6a57530d0f036fec31c230b0cc6",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffc0000000000000",
	"964153a83bf6989a4ba80daa91c3e081",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffe0000000000000",
	"a013014d4ce8054cf2591d06f6f2f176",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffff0000000000000",
	"d1c5f6399bf382502e385eee1474a869",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffff8000000000000",
	"0007e20b8298ec354f0f5fe7470f36bd",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffc000000000000",
	"b95ba05b332da61ef63a2b31fcad9879",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffe000000000000",
	"4620a49bd967491561669ab25dce45f4",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffff000000000000",
	"12e71214ae8e04f0bb63d7425c6f14d5",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffff800000000000",
	"4cc42fc1407b008fe350907c092e80ac",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffc00000000000",
	"08b244ce7cbc8ee97fbba808cb146fda",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffe00000000000",
	"39b333e8694f21546ad1edd9d87ed95b",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffff00000000000",
	"3b271f8ab2e6e4a20ba8090f43ba78f3",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffff80000000000",
	"9ad983f3bf651cd0393f0a73cccdea50",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffc0000000000",
	"8f476cbff75c1f725ce18e4bbcd19b32",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffe0000000000",
	"905b6267f1d6ab5320835a133f096f2a",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffff0000000000",
	"145b60d6d0193c23f4221848a892d61a",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffff8000000000",
	"55cfb3fb6d75cad0445bbc8dafa25b0f",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffc000000000",
	"7b8e7098e357ef71237d46d8b075b0f5",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffe000000000",
	"2bf27229901eb40f2df9d8398d1505ae",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffff000000000",
	"83a63402a77f9ad5c1e931a931ecd706",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffff800000000",
	"6f8ba6521152d31f2bada1843e26b973",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffc00000000",
	"e5c3b8e30fd2d8e6239b17b44bd23bbd",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffe00000000",
	"1ac1f7102c59933e8b2ddc3f14e94baa",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffff00000000",
	"21d9ba49f276b45f11af8fc71a088e3d",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffff80000000",
	"649f1cddc3792b4638635a392bc9bade",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffc0000000",
	"e2775e4b59c1bc2e31a2078c11b5a08c",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffe0000000",
	"2be1fae5048a25582a679ca10905eb80",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffff0000000",
	"da86f292c6f41ea34fb2068df75ecc29",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffff8000000",
	"220df19f85d69b1b562fa69a3c5beca5",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffc000000",
	"1f11d5d0355e0b556ccdb6c7f5083b4d",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffe000000",
	"62526b78be79cb384633c91f83b4151b",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffff000000",
	"90ddbcb950843592dd47bbef00fdc876",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffff800000",
	"2fd0e41c5b8402277354a7391d2618e2",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffc00000",
	"3cdf13e72dee4c581bafec70b85f9660",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffe00000",
	"afa2ffc137577092e2b654fa199d2c43",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffff00000",
	"8d683ee63e60d208e343ce48dbc44cac",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffff80000",
	"705a4ef8ba2133729c20185c3d3a4763",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffc0000",
	"0861a861c3db4e94194211b77ed761b9",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffe0000",
	"4b00c27e8b26da7eab9d3a88dec8b031",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffff0000",
	"5f397bf03084820cc8810d52e5b666e9",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffff8000",
	"63fafabb72c07bfbd3ddc9b1203104b8",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffc000",
	"683e2140585b18452dd4ffbb93c95df9",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffe000",
	"286894e48e537f8763b56707d7d155c8",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffff000",
	"a423deabc173dcf7e2c4c53e77d37cd1",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffff800",
	"eb8168313e1cfdfdb5e986d5429cf172",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffc00",
	"27127daafc9accd2fb334ec3eba52323",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffe00",
	"ee0715b96f72e3f7a22a5064fc592f4c",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffff00",
	"29ee526770f2a11dcfa989d1ce88830f",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffff80",
	"0493370e054b09871130fe49af730a5a",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffffc0",
	"9b7b940f6c509f9e44a4ee140448ee46",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffffe0",
	"2915be4a1ecfdcbe3e023811a12bb6c7",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffff0",
	"7240e524bc51d8c4d440b1be55d1062c",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffff8",
	"da63039d38cb4612b2dc36ba26684b93",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffffc",
	"0f59cb5a4b522e2ac56c1a64f558ad9a",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"fffffffffffffffffffffffffffffffe",
	"7bfe9d876c6d63c1d035da8fe21c409d",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"ffffffffffffffffffffffffffffffff",
	"acdace8078a32b1a182bfa4987ca1347",

	/*
	 * Table end marker.
	 */
	NULL
};

/*
 * AES known-answer tests for CBC. Order: key, IV, plaintext, ciphertext.
 */
static const char *const KAT_AES_CBC[] = {
	/*
	 * From NIST validation suite "Multiblock Message Test"
	 * (cbcmmt128.rsp).
	 */
	"1f8e4973953f3fb0bd6b16662e9a3c17",
	"2fe2b333ceda8f98f4a99b40d2cd34a8",
	"45cf12964fc824ab76616ae2f4bf0822",
	"0f61c4d44c5147c03c195ad7e2cc12b2",

	"0700d603a1c514e46b6191ba430a3a0c",
	"aad1583cd91365e3bb2f0c3430d065bb",
	"068b25c7bfb1f8bdd4cfc908f69dffc5ddc726a197f0e5f720f730393279be91",
	"c4dc61d9725967a3020104a9738f23868527ce839aab1752fd8bdb95a82c4d00",

	"3348aa51e9a45c2dbe33ccc47f96e8de",
	"19153c673160df2b1d38c28060e59b96",
	"9b7cee827a26575afdbb7c7a329f887238052e3601a7917456ba61251c214763d5e1847a6ad5d54127a399ab07ee3599",
	"d5aed6c9622ec451a15db12819952b6752501cf05cdbf8cda34a457726ded97818e1f127a28d72db5652749f0c6afee5",

	"b7f3c9576e12dd0db63e8f8fac2b9a39",
	"c80f095d8bb1a060699f7c19974a1aa0",
	"9ac19954ce1319b354d3220460f71c1e373f1cd336240881160cfde46ebfed2e791e8d5a1a136ebd1dc469dec00c4187722b841cdabcb22c1be8a14657da200e",
	"19b9609772c63f338608bf6eb52ca10be65097f89c1e0905c42401fd47791ae2c5440b2d473116ca78bd9ff2fb6015cfd316524eae7dcb95ae738ebeae84a467",

	"b6f9afbfe5a1562bba1368fc72ac9d9c",
	"3f9d5ebe250ee7ce384b0d00ee849322",
	"db397ec22718dbffb9c9d13de0efcd4611bf792be4fce0dc5f25d4f577ed8cdbd4eb9208d593dda3d4653954ab64f05676caa3ce9bfa795b08b67ceebc923fdc89a8c431188e9e482d8553982cf304d1",
	"10ea27b19e16b93af169c4a88e06e35c99d8b420980b058e34b4b8f132b13766f72728202b089f428fecdb41c79f8aa0d0ef68f5786481cca29e2126f69bc14160f1ae2187878ba5c49cf3961e1b7ee9",

	"bbe7b7ba07124ff1ae7c3416fe8b465e",
	"7f65b5ee3630bed6b84202d97fb97a1e",
	"2aad0c2c4306568bad7447460fd3dac054346d26feddbc9abd9110914011b4794be2a9a00a519a51a5b5124014f4ed2735480db21b434e99a911bb0b60fe0253763725b628d5739a5117b7ee3aefafc5b4c1bf446467e7bf5f78f31ff7caf187",
	"3b8611bfc4973c5cd8e982b073b33184cd26110159172e44988eb5ff5661a1e16fad67258fcbfee55469267a12dc374893b4e3533d36f5634c3095583596f135aa8cd1138dc898bc5651ee35a92ebf89ab6aeb5366653bc60a70e0074fc11efe",

	"89a553730433f7e6d67d16d373bd5360",
	"f724558db3433a523f4e51a5bea70497",
	"807bc4ea684eedcfdcca30180680b0f1ae2814f35f36d053c5aea6595a386c1442770f4d7297d8b91825ee7237241da8925dd594ccf676aecd46ca2068e8d37a3a0ec8a7d5185a201e663b5ff36ae197110188a23503763b8218826d23ced74b31e9f6e2d7fbfa6cb43420c7807a8625",
	"406af1429a478c3d07e555c5287a60500d37fc39b68e5bbb9bafd6ddb223828561d6171a308d5b1a4551e8a5e7d572918d25c968d3871848d2f16635caa9847f38590b1df58ab5efb985f2c66cfaf86f61b3f9c0afad6c963c49cee9b8bc81a2ddb06c967f325515a4849eec37ce721a",

	"c491ca31f91708458e29a925ec558d78",
	"9ef934946e5cd0ae97bd58532cb49381",
	"cb6a787e0dec56f9a165957f81af336ca6b40785d9e94093c6190e5152649f882e874d79ac5e167bd2a74ce5ae088d2ee854f6539e0a94796b1e1bd4c9fcdbc79acbef4d01eeb89776d18af71ae2a4fc47dd66df6c4dbe1d1850e466549a47b636bcc7c2b3a62495b56bb67b6d455f1eebd9bfefecbca6c7f335cfce9b45cb9d",
	"7b2931f5855f717145e00f152a9f4794359b1ffcb3e55f594e33098b51c23a6c74a06c1d94fded7fd2ae42c7db7acaef5844cb33aeddc6852585ed0020a6699d2cb53809cefd169148ce42292afab063443978306c582c18b9ce0da3d084ce4d3c482cfd8fcf1a85084e89fb88b40a084d5e972466d07666126fb761f84078f2",

	"f6e87d71b0104d6eb06a68dc6a71f498",
	"1c245f26195b76ebebc2edcac412a2f8",
	"f82bef3c73a6f7f80db285726d691db6bf55eec25a859d3ba0e0445f26b9bb3b16a3161ed1866e4dd8f2e5f8ecb4e46d74a7a78c20cdfc7bcc9e479ba7a0caba9438238ad0c01651d5d98de37f03ddce6e6b4bd4ab03cf9e8ed818aedfa1cf963b932067b97d776dce1087196e7e913f7448e38244509f0caf36bd8217e15336d35c149fd4e41707893fdb84014f8729",
	"b09512f3eff9ed0d85890983a73dadbb7c3678d52581be64a8a8fc586f490f2521297a478a0598040ebd0f5509fafb0969f9d9e600eaef33b1b93eed99687b167f89a5065aac439ce46f3b8d22d30865e64e45ef8cd30b6984353a844a11c8cd60dba0e8866b3ee30d24b3fa8a643b328353e06010fa8273c8fd54ef0a2b6930e5520aae5cd5902f9b86a33592ca4365",

	"2c14413751c31e2730570ba3361c786b",
	"1dbbeb2f19abb448af849796244a19d7",
	"40d930f9a05334d9816fe204999c3f82a03f6a0457a8c475c94553d1d116693adc618049f0a769a2eed6a6cb14c0143ec5cccdbc8dec4ce560cfd206225709326d4de7948e54d603d01b12d7fed752fb23f1aa4494fbb00130e9ded4e77e37c079042d828040c325b1a5efd15fc842e44014ca4374bf38f3c3fc3ee327733b0c8aee1abcd055772f18dc04603f7b2c1ea69ff662361f2be0a171bbdcea1e5d3f",
	"6be8a12800455a320538853e0cba31bd2d80ea0c85164a4c5c261ae485417d93effe2ebc0d0a0b51d6ea18633d210cf63c0c4ddbc27607f2e81ed9113191ef86d56f3b99be6c415a4150299fb846ce7160b40b63baf1179d19275a2e83698376d28b92548c68e06e6d994e2c1501ed297014e702cdefee2f656447706009614d801de1caaf73f8b7fa56cf1ba94b631933bbe577624380850f117435a0355b2b",

	/*
	 * From NIST validation suite "Multiblock Message Test"
	 * (cbcmmt192.rsp).
	 */
	"ba75f4d1d9d7cf7f551445d56cc1a8ab2a078e15e049dc2c",
	"531ce78176401666aa30db94ec4a30eb",
	"c51fc276774dad94bcdc1d2891ec8668",
	"70dd95a14ee975e239df36ff4aee1d5d",

	"eab3b19c581aa873e1981c83ab8d83bbf8025111fb2e6b21",
	"f3d6667e8d4d791e60f7505ba383eb05",
	"9d4e4cccd1682321856df069e3f1c6fa391a083a9fb02d59db74c14081b3acc4",
	"51d44779f90d40a80048276c035cb49ca2a47bcb9b9cf7270b9144793787d53f",

	"16c93bb398f1fc0cf6d68fc7a5673cdf431fa147852b4a2d",
	"eaaeca2e07ddedf562f94df63f0a650f",
	"c5ce958613bf741718c17444484ebaf1050ddcacb59b9590178cbe69d7ad7919608cb03af13bbe04f3506b718a301ea0",
	"ed6a50e0c6921d52d6647f75d67b4fd56ace1fedb8b5a6a997b4d131640547d22c5d884a75e6752b5846b5b33a5181f4",

	"067bb17b4df785697eaccf961f98e212cb75e6797ce935cb",
	"8b59c9209c529ca8391c9fc0ce033c38",
	"db3785a889b4bd387754da222f0e4c2d2bfe0d79e05bc910fba941beea30f1239eacf0068f4619ec01c368e986fca6b7c58e490579d29611bd10087986eff54f",
	"d5f5589760bf9c762228fde236de1fa2dd2dad448db3fa9be0c4196efd46a35c84dd1ac77d9db58c95918cb317a6430a08d2fb6a8e8b0f1c9b72c7a344dc349f",

	"0fd39de83e0be77a79c8a4a612e3dd9c8aae2ce35e7a2bf8",
	"7e1d629b84f93b079be51f9a5f5cb23c",
	"38fbda37e28fa86d9d83a4345e419dea95d28c7818ff25925db6ac3aedaf0a86154e20a4dfcc5b1b4192895393e5eb5846c88bdbd41ecf7af3104f410eaee470f5d9017ed460475f626953035a13db1f",
	"edadae2f9a45ff3473e02d904c94d94a30a4d92da4deb6bcb4b0774472694571842039f21c496ef93fd658842c735f8a81fcd0aa578442ab893b18f606aed1bab11f81452dd45e9b56adf2eccf4ea095",

	"e3fecc75f0075a09b383dfd389a3d33cc9b854b3b254c0f4",
	"36eab883afef936cc38f63284619cd19",
	"931b2f5f3a5820d53a6beaaa6431083a3488f4eb03b0f5b57ef838e1579623103bd6e6800377538b2e51ef708f3c4956432e8a8ee6a34e190642b26ad8bdae6c2af9a6c7996f3b6004d2671e41f1c9f40ee03d1c4a52b0a0654a331f15f34dce",
	"75395974bd32b3665654a6c8e396b88ae34b123575872a7ab687d8e76b46df911a8a590cd01d2f5c330be3a6626e9dd3aa5e10ed14e8ff829811b6fed50f3f533ca4385a1cbca78f5c4744e50f2f8359165c2485d1324e76c3eae76a0ccac629",

	"f9c27565eb07947c8cb51b79248430f7b1066c3d2fdc3d13",
	"2bd67cc89ab7948d644a49672843cbd9",
	"6abcc270173cf114d44847e911a050db57ba7a2e2c161c6f37ccb6aaa4677bddcaf50cad0b5f8758fcf7c0ebc650ceb5cd52cafb8f8dd3edcece55d9f1f08b9fa8f54365cf56e28b9596a7e1dd1d3418e4444a7724add4cf79d527b183ec88de4be4eeff29c80a97e54f85351cb189ee",
	"ca282924a61187feb40520979106e5cc861957f23828dcb7285e0eaac8a0ca2a6b60503d63d6039f4693dba32fa1f73ae2e709ca94911f28a5edd1f30eaddd54680c43acc9c74cd90d8bb648b4e544275f47e514daa20697f66c738eb30337f017fca1a26da4d1a0cc0a0e98e2463070",

	"fb09cf9e00dbf883689d079c920077c0073c31890b55bab5",
	"e3c89bd097c3abddf64f4881db6dbfe2",
	"c1a37683fb289467dd1b2c89efba16bbd2ee24cf18d19d44596ded2682c79a2f711c7a32bf6a24badd32a4ee637c73b7a41da6258635650f91fb9ffa45bdfc3cb122136241b3deced8996aa51ea8d3e81c9d70e006a44bc0571ed48623a0d622a93fa9da290baaedf5d9e876c94620945ff8ecc83f27379ed55cf490c5790f27",
	"8158e21420f25b59d6ae943fa1cbf21f02e979f419dab0126a721b7eef55bee9ad97f5ccff7d239057bbc19a8c378142f7672f1d5e7e17d7bebcb0070e8355cace6660171a53b61816ae824a6ef69ce470b6ffd3b5bb4b438874d91d27854d3b6f25860d3868958de3307d62b1339bdddb8a318c0ce0f33c17caf0e9f6040820",

	"bca6fa3c67fd294e958f66fe8bd64f45f428f5bc8e9733a7",
	"92a47f2833f1450d1da41717bdc6e83c",
	"5becbc31d8bead6d36ae014a5863d14a431e6b55d29ea6baaa417271716db3a33b2e506b452086dfe690834ac2de30bc41254ec5401ec47d064237c7792fdcd7914d8af20eb114756642d519021a8c75a92f6bc53d326ae9a5b7e1b10a9756574692934d9939fc399e0c203f7edf8e7e6482eadd31a0400770e897b48c6bca2b404593045080e93377358c42a0f4dede",
	"926db248cc1ba20f0c57631a7c8aef094f791937b905949e3460240e8bfa6fa483115a1b310b6e4369caebc5262888377b1ddaa5800ea496a2bdff0f9a1031e7129c9a20e35621e7f0b8baca0d87030f2ae7ca8593c8599677a06fd4b26009ead08fecac24caa9cf2cad3b470c8227415a7b1e0f2eab3fad96d70a209c8bb26c627677e2531b9435ca6e3c444d195b5f",

	"162ad50ee64a0702aa551f571dedc16b2c1b6a1e4d4b5eee",
	"24408038161a2ccae07b029bb66355c1",
	"be8abf00901363987a82cc77d0ec91697ba3857f9e4f84bd79406c138d02698f003276d0449120bef4578d78fecabe8e070e11710b3f0a2744bd52434ec70015884c181ebdfd51c604a71c52e4c0e110bc408cd462b248a80b8a8ac06bb952ac1d7faed144807f1a731b7febcaf7835762defe92eccfc7a9944e1c702cffe6bc86733ed321423121085ac02df8962bcbc1937092eebf0e90a8b20e3dd8c244ae",
	"c82cf2c476dea8cb6a6e607a40d2f0391be82ea9ec84a537a6820f9afb997b76397d005424faa6a74dc4e8c7aa4a8900690f894b6d1dca80675393d2243adac762f159301e357e98b724762310cd5a7bafe1c2a030dba46fd93a9fdb89cc132ca9c17dc72031ec6822ee5a9d99dbca66c784c01b0885cbb62e29d97801927ec415a5d215158d325f9ee689437ad1b7684ad33c0d92739451ac87f39ff8c31b84",

	/*
	 * From NIST validation suite "Multiblock Message Test"
	 * (cbcmmt256.rsp).
	 */
	"6ed76d2d97c69fd1339589523931f2a6cff554b15f738f21ec72dd97a7330907",
	"851e8764776e6796aab722dbb644ace8",
	"6282b8c05c5c1530b97d4816ca434762",
	"6acc04142e100a65f51b97adf5172c41",

	"dce26c6b4cfb286510da4eecd2cffe6cdf430f33db9b5f77b460679bd49d13ae",
	"fdeaa134c8d7379d457175fd1a57d3fc",
	"50e9eee1ac528009e8cbcd356975881f957254b13f91d7c6662d10312052eb00",
	"2fa0df722a9fd3b64cb18fb2b3db55ff2267422757289413f8f657507412a64c",

	"fe8901fecd3ccd2ec5fdc7c7a0b50519c245b42d611a5ef9e90268d59f3edf33",
	"bd416cb3b9892228d8f1df575692e4d0",
	"8d3aa196ec3d7c9b5bb122e7fe77fb1295a6da75abe5d3a510194d3a8a4157d5c89d40619716619859da3ec9b247ced9",
	"608e82c7ab04007adb22e389a44797fed7de090c8c03ca8a2c5acd9e84df37fbc58ce8edb293e98f02b640d6d1d72464",

	"0493ff637108af6a5b8e90ac1fdf035a3d4bafd1afb573be7ade9e8682e663e5",
	"c0cd2bebccbb6c49920bd5482ac756e8",
	"8b37f9148df4bb25956be6310c73c8dc58ea9714ff49b643107b34c9bff096a94fedd6823526abc27a8e0b16616eee254ab4567dd68e8ccd4c38ac563b13639c",
	"05d5c77729421b08b737e41119fa4438d1f570cc772a4d6c3df7ffeda0384ef84288ce37fc4c4c7d1125a499b051364c389fd639bdda647daa3bdadab2eb5594",

	"9adc8fbd506e032af7fa20cf5343719de6d1288c158c63d6878aaf64ce26ca85",
	"11958dc6ab81e1c7f01631e9944e620f",
	"c7917f84f747cd8c4b4fedc2219bdbc5f4d07588389d8248854cf2c2f89667a2d7bcf53e73d32684535f42318e24cd45793950b3825e5d5c5c8fcd3e5dda4ce9246d18337ef3052d8b21c5561c8b660e",
	"9c99e68236bb2e929db1089c7750f1b356d39ab9d0c40c3e2f05108ae9d0c30b04832ccdbdc08ebfa426b7f5efde986ed05784ce368193bb3699bc691065ac62e258b9aa4cc557e2b45b49ce05511e65",

	"73b8faf00b3302ac99855cf6f9e9e48518690a5906a4869d4dcf48d282faae2a",
	"b3cb97a80a539912b8c21f450d3b9395",
	"3adea6e06e42c4f041021491f2775ef6378cb08824165edc4f6448e232175b60d0345b9f9c78df6596ec9d22b7b9e76e8f3c76b32d5d67273f1d83fe7a6fc3dd3c49139170fa5701b3beac61b490f0a9e13f844640c4500f9ad3087adfb0ae10",
	"ac3d6dbafe2e0f740632fd9e820bf6044cd5b1551cbb9cc03c0b25c39ccb7f33b83aacfca40a3265f2bbff879153448acacb88fcfb3bb7b10fe463a68c0109f028382e3e557b1adf02ed648ab6bb895df0205d26ebbfa9a5fd8cebd8e4bee3dc",

	"9ddf3745896504ff360a51a3eb49c01b79fccebc71c3abcb94a949408b05b2c9",
	"e79026639d4aa230b5ccffb0b29d79bc",
	"cf52e5c3954c51b94c9e38acb8c9a7c76aebdaa9943eae0a1ce155a2efdb4d46985d935511471452d9ee64d2461cb2991d59fc0060697f9a671672163230f367fed1422316e52d29eceacb8768f56d9b80f6d278093c9a8acd3cfd7edd8ebd5c293859f64d2f8486ae1bd593c65bc014",
	"34df561bd2cfebbcb7af3b4b8d21ca5258312e7e2e4e538e35ad2490b6112f0d7f148f6aa8d522a7f3c61d785bd667db0e1dc4606c318ea4f26af4fe7d11d4dcff0456511b4aed1a0d91ba4a1fd6cd9029187bc5881a5a07fe02049d39368e83139b12825bae2c7be81e6f12c61bb5c5",

	"458b67bf212d20f3a57fce392065582dcefbf381aa22949f8338ab9052260e1d",
	"4c12effc5963d40459602675153e9649",
	"256fd73ce35ae3ea9c25dd2a9454493e96d8633fe633b56176dce8785ce5dbbb84dbf2c8a2eeb1e96b51899605e4f13bbc11b93bf6f39b3469be14858b5b720d4a522d36feed7a329c9b1e852c9280c47db8039c17c4921571a07d1864128330e09c308ddea1694e95c84500f1a61e614197e86a30ecc28df64ccb3ccf5437aa",
	"90b7b9630a2378f53f501ab7beff039155008071bc8438e789932cfd3eb1299195465e6633849463fdb44375278e2fdb1310821e6492cf80ff15cb772509fb426f3aeee27bd4938882fd2ae6b5bd9d91fa4a43b17bb439ebbe59c042310163a82a5fe5388796eee35a181a1271f00be29b852d8fa759bad01ff4678f010594cd",

	"d2412db0845d84e5732b8bbd642957473b81fb99ca8bff70e7920d16c1dbec89",
	"51c619fcf0b23f0c7925f400a6cacb6d",
	"026006c4a71a180c9929824d9d095b8faaa86fc4fa25ecac61d85ff6de92dfa8702688c02a282c1b8af4449707f22d75e91991015db22374c95f8f195d5bb0afeb03040ff8965e0e1339dba5653e174f8aa5a1b39fe3ac839ce307a4e44b4f8f1b0063f738ec18acdbff2ebfe07383e734558723e741f0a1836dafdf9de82210a9248bc113b3c1bc8b4e252ca01bd803",
	"0254b23463bcabec5a395eb74c8fb0eb137a07bc6f5e9f61ec0b057de305714f8fa294221c91a159c315939b81e300ee902192ec5f15254428d8772f79324ec43298ca21c00b370273ee5e5ed90e43efa1e05a5d171209fe34f9f29237dba2a6726650fd3b1321747d1208863c6c3c6b3e2d879ab5f25782f08ba8f2abbe63e0bedb4a227e81afb36bb6645508356d34",

	"48be597e632c16772324c8d3fa1d9c5a9ecd010f14ec5d110d3bfec376c5532b",
	"d6d581b8cf04ebd3b6eaa1b53f047ee1",
	"0c63d413d3864570e70bb6618bf8a4b9585586688c32bba0a5ecc1362fada74ada32c52acfd1aa7444ba567b4e7daaecf7cc1cb29182af164ae5232b002868695635599807a9a7f07a1f137e97b1e1c9dabc89b6a5e4afa9db5855edaa575056a8f4f8242216242bb0c256310d9d329826ac353d715fa39f80cec144d6424558f9f70b98c920096e0f2c855d594885a00625880e9dfb734163cecef72cf030b8",
	"fc5873e50de8faf4c6b84ba707b0854e9db9ab2e9f7d707fbba338c6843a18fc6facebaf663d26296fb329b4d26f18494c79e09e779647f9bafa87489630d79f4301610c2300c19dbf3148b7cac8c4f4944102754f332e92b6f7c5e75bc6179eb877a078d4719009021744c14f13fd2a55a2b9c44d18000685a845a4f632c7c56a77306efa66a24d05d088dcd7c13fe24fc447275965db9e4d37fbc9304448cd",

	/*
	 * End-of-table marker.
	 */
	NULL
};

/*
 * AES known-answer tests for CTR. Order: key, IV, plaintext, ciphertext.
 */
static const char *const KAT_AES_CTR[] = {
	/*
	 * From RFC 3686.
	 */
	"ae6852f8121067cc4bf7a5765577f39e",
	"000000300000000000000000",
	"53696e676c6520626c6f636b206d7367",
	"e4095d4fb7a7b3792d6175a3261311b8",

	"7e24067817fae0d743d6ce1f32539163",
	"006cb6dbc0543b59da48d90b",
	"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
	"5104a106168a72d9790d41ee8edad388eb2e1efc46da57c8fce630df9141be28",

	"7691be035e5020a8ac6e618529f9a0dc",
	"00e0017b27777f3f4a1786f0",
	"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20212223",
	"c1cf48a89f2ffdd9cf4652e9efdb72d74540a42bde6d7836d59a5ceaaef3105325b2072f",

	"16af5b145fc9f579c175f93e3bfb0eed863d06ccfdb78515",
	"0000004836733c147d6d93cb",
	"53696e676c6520626c6f636b206d7367",
	"4b55384fe259c9c84e7935a003cbe928",

	"7c5cb2401b3dc33c19e7340819e0f69c678c3db8e6f6a91a",
	"0096b03b020c6eadc2cb500d",
	"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
	"453243fc609b23327edfaafa7131cd9f8490701c5ad4a79cfc1fe0ff42f4fb00",

	"02bf391ee8ecb159b959617b0965279bf59b60a786d3e0fe",
	"0007bdfd5cbd60278dcc0912",
	"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20212223",
	"96893fc55e5c722f540b7dd1ddf7e758d288bc95c69165884536c811662f2188abee0935",

	"776beff2851db06f4c8a0542c8696f6c6a81af1eec96b4d37fc1d689e6c1c104",
	"00000060db5672c97aa8f0b2",
	"53696e676c6520626c6f636b206d7367",
	"145ad01dbf824ec7560863dc71e3e0c0",

	"f6d66d6bd52d59bb0796365879eff886c66dd51a5b6a99744b50590c87a23884",
	"00faac24c1585ef15a43d875",
	"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
	"f05e231b3894612c49ee000b804eb2a9b8306b508f839d6a5530831d9344af1c",

	"ff7a617ce69148e4f1726e2f43581de2aa62d9f805532edff1eed687fb54153d",
	"001cc5b751a51d70a1c11148",
	"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20212223",
	"eb6c52821d0bbbf7ce7594462aca4faab407df866569fd07f48cc0b583d6071f1ec0e6b8",

	/*
	 * End-of-table marker.
	 */
	NULL
};

static void
monte_carlo_AES_encrypt(const br_block_cbcenc_class *ve,
	char *skey, char *splain, char *scipher)
{
	unsigned char key[32];
	unsigned char buf[16];
	unsigned char pbuf[16];
	unsigned char cipher[16];
	size_t key_len;
	int i, j, k;
	br_aes_gen_cbcenc_keys v_ec;
	const br_block_cbcenc_class **ec;

	ec = &v_ec.vtable;
	key_len = hextobin(key, skey);
	hextobin(buf, splain);
	hextobin(cipher, scipher);
	for (i = 0; i < 100; i ++) {
		ve->init(ec, key, key_len);
		for (j = 0; j < 1000; j ++) {
			unsigned char iv[16];

			memcpy(pbuf, buf, sizeof buf);
			memset(iv, 0, sizeof iv);
			ve->run(ec, iv, buf, sizeof buf);
		}
		switch (key_len) {
		case 16:
			for (k = 0; k < 16; k ++) {
				key[k] ^= buf[k];
			}
			break;
		case 24:
			for (k = 0; k < 8; k ++) {
				key[k] ^= pbuf[8 + k];
			}
			for (k = 0; k < 16; k ++) {
				key[8 + k] ^= buf[k];
			}
			break;
		default:
			for (k = 0; k < 16; k ++) {
				key[k] ^= pbuf[k];
				key[16 + k] ^= buf[k];
			}
			break;
		}
		printf(".");
		fflush(stdout);
	}
	printf(" ");
	fflush(stdout);
	check_equals("MC AES encrypt", buf, cipher, sizeof buf);
}

static void
monte_carlo_AES_decrypt(const br_block_cbcdec_class *vd,
	char *skey, char *scipher, char *splain)
{
	unsigned char key[32];
	unsigned char buf[16];
	unsigned char pbuf[16];
	unsigned char plain[16];
	size_t key_len;
	int i, j, k;
	br_aes_gen_cbcdec_keys v_dc;
	const br_block_cbcdec_class **dc;

	dc = &v_dc.vtable;
	key_len = hextobin(key, skey);
	hextobin(buf, scipher);
	hextobin(plain, splain);
	for (i = 0; i < 100; i ++) {
		vd->init(dc, key, key_len);
		for (j = 0; j < 1000; j ++) {
			unsigned char iv[16];

			memcpy(pbuf, buf, sizeof buf);
			memset(iv, 0, sizeof iv);
			vd->run(dc, iv, buf, sizeof buf);
		}
		switch (key_len) {
		case 16:
			for (k = 0; k < 16; k ++) {
				key[k] ^= buf[k];
			}
			break;
		case 24:
			for (k = 0; k < 8; k ++) {
				key[k] ^= pbuf[8 + k];
			}
			for (k = 0; k < 16; k ++) {
				key[8 + k] ^= buf[k];
			}
			break;
		default:
			for (k = 0; k < 16; k ++) {
				key[k] ^= pbuf[k];
				key[16 + k] ^= buf[k];
			}
			break;
		}
		printf(".");
		fflush(stdout);
	}
	printf(" ");
	fflush(stdout);
	check_equals("MC AES decrypt", buf, plain, sizeof buf);
}

static void
test_AES_generic(char *name,
	const br_block_cbcenc_class *ve,
	const br_block_cbcdec_class *vd,
	const br_block_ctr_class *vc,
	int with_MC, int with_CBC)
{
	size_t u;

	printf("Test %s: ", name);
	fflush(stdout);

	if (ve->block_size != 16 || vd->block_size != 16
		|| ve->log_block_size != 4 || vd->log_block_size != 4)
	{
		fprintf(stderr, "%s failed: wrong block size\n", name);
		exit(EXIT_FAILURE);
	}

	for (u = 0; KAT_AES[u]; u += 3) {
		unsigned char key[32];
		unsigned char plain[16];
		unsigned char cipher[16];
		unsigned char buf[16];
		unsigned char iv[16];
		size_t key_len;
		br_aes_gen_cbcenc_keys v_ec;
		br_aes_gen_cbcdec_keys v_dc;
		const br_block_cbcenc_class **ec;
		const br_block_cbcdec_class **dc;

		ec = &v_ec.vtable;
		dc = &v_dc.vtable;
		key_len = hextobin(key, KAT_AES[u]);
		hextobin(plain, KAT_AES[u + 1]);
		hextobin(cipher, KAT_AES[u + 2]);
		ve->init(ec, key, key_len);
		memcpy(buf, plain, sizeof plain);
		memset(iv, 0, sizeof iv);
		ve->run(ec, iv, buf, sizeof buf);
		check_equals("KAT AES encrypt", buf, cipher, sizeof cipher);
		vd->init(dc, key, key_len);
		memset(iv, 0, sizeof iv);
		vd->run(dc, iv, buf, sizeof buf);
		check_equals("KAT AES decrypt", buf, plain, sizeof plain);
	}

	if (with_CBC) {
		for (u = 0; KAT_AES_CBC[u]; u += 4) {
			unsigned char key[32];
			unsigned char ivref[16];
			unsigned char plain[200];
			unsigned char cipher[200];
			unsigned char buf[200];
			unsigned char iv[16];
			size_t key_len, data_len, v;
			br_aes_gen_cbcenc_keys v_ec;
			br_aes_gen_cbcdec_keys v_dc;
			const br_block_cbcenc_class **ec;
			const br_block_cbcdec_class **dc;

			ec = &v_ec.vtable;
			dc = &v_dc.vtable;
			key_len = hextobin(key, KAT_AES_CBC[u]);
			hextobin(ivref, KAT_AES_CBC[u + 1]);
			data_len = hextobin(plain, KAT_AES_CBC[u + 2]);
			hextobin(cipher, KAT_AES_CBC[u + 3]);
			ve->init(ec, key, key_len);

			memcpy(buf, plain, data_len);
			memcpy(iv, ivref, 16);
			ve->run(ec, iv, buf, data_len);
			check_equals("KAT CBC AES encrypt",
				buf, cipher, data_len);
			vd->init(dc, key, key_len);
			memcpy(iv, ivref, 16);
			vd->run(dc, iv, buf, data_len);
			check_equals("KAT CBC AES decrypt",
				buf, plain, data_len);

			memcpy(buf, plain, data_len);
			memcpy(iv, ivref, 16);
			for (v = 0; v < data_len; v += 16) {
				ve->run(ec, iv, buf + v, 16);
			}
			check_equals("KAT CBC AES encrypt (2)",
				buf, cipher, data_len);
			memcpy(iv, ivref, 16);
			for (v = 0; v < data_len; v += 16) {
				vd->run(dc, iv, buf + v, 16);
			}
			check_equals("KAT CBC AES decrypt (2)",
				buf, plain, data_len);
		}

		/*
		 * We want to check proper IV management for CBC:
		 * encryption and decryption must properly copy the _last_
		 * encrypted block as new IV, for all sizes.
		 */
		for (u = 1; u <= 35; u ++) {
			br_hmac_drbg_context rng;
			unsigned char x;
			size_t key_len, data_len;
			size_t v;

			br_hmac_drbg_init(&rng, &br_sha256_vtable,
				"seed for AES/CBC", 16);
			x = u;
			br_hmac_drbg_update(&rng, &x, 1);
			data_len = u << 4;
			for (key_len = 16; key_len <= 32; key_len += 16) {
				unsigned char key[32];
				unsigned char iv[16], iv1[16], iv2[16];
				unsigned char plain[35 * 16];
				unsigned char tmp1[sizeof plain];
				unsigned char tmp2[sizeof plain];
				br_aes_gen_cbcenc_keys v_ec;
				br_aes_gen_cbcdec_keys v_dc;
				const br_block_cbcenc_class **ec;
				const br_block_cbcdec_class **dc;

				br_hmac_drbg_generate(&rng, key, key_len);
				br_hmac_drbg_generate(&rng, iv, sizeof iv);
				br_hmac_drbg_generate(&rng, plain, data_len);

				ec = &v_ec.vtable;
				ve->init(ec, key, key_len);
				memcpy(iv1, iv, sizeof iv);
				memcpy(tmp1, plain, data_len);
				ve->run(ec, iv1, tmp1, data_len);
				check_equals("IV CBC AES (1)",
					tmp1 + data_len - 16, iv1, 16);
				memcpy(iv2, iv, sizeof iv);
				memcpy(tmp2, plain, data_len);
				for (v = 0; v < data_len; v += 16) {
					ve->run(ec, iv2, tmp2 + v, 16);
				}
				check_equals("IV CBC AES (2)",
					tmp2 + data_len - 16, iv2, 16);
				check_equals("IV CBC AES (3)",
					tmp1, tmp2, data_len);

				dc = &v_dc.vtable;
				vd->init(dc, key, key_len);
				memcpy(iv1, iv, sizeof iv);
				vd->run(dc, iv1, tmp1, data_len);
				check_equals("IV CBC AES (4)", iv1, iv2, 16);
				check_equals("IV CBC AES (5)",
					tmp1, plain, data_len);
				memcpy(iv2, iv, sizeof iv);
				for (v = 0; v < data_len; v += 16) {
					vd->run(dc, iv2, tmp2 + v, 16);
				}
				check_equals("IV CBC AES (6)", iv1, iv2, 16);
				check_equals("IV CBC AES (7)",
					tmp2, plain, data_len);
			}
		}
	}

	if (vc != NULL) {
		if (vc->block_size != 16 || vc->log_block_size != 4) {
			fprintf(stderr, "%s failed: wrong block size\n", name);
			exit(EXIT_FAILURE);
		}
		for (u = 0; KAT_AES_CTR[u]; u += 4) {
			unsigned char key[32];
			unsigned char iv[12];
			unsigned char plain[200];
			unsigned char cipher[200];
			unsigned char buf[200];
			size_t key_len, data_len, v;
			uint32_t c;
			br_aes_gen_ctr_keys v_xc;
			const br_block_ctr_class **xc;

			xc = &v_xc.vtable;
			key_len = hextobin(key, KAT_AES_CTR[u]);
			hextobin(iv, KAT_AES_CTR[u + 1]);
			data_len = hextobin(plain, KAT_AES_CTR[u + 2]);
			hextobin(cipher, KAT_AES_CTR[u + 3]);
			vc->init(xc, key, key_len);
			memcpy(buf, plain, data_len);
			vc->run(xc, iv, 1, buf, data_len);
			check_equals("KAT CTR AES (1)", buf, cipher, data_len);
			vc->run(xc, iv, 1, buf, data_len);
			check_equals("KAT CTR AES (2)", buf, plain, data_len);

			memcpy(buf, plain, data_len);
			c = 1;
			for (v = 0; v < data_len; v += 32) {
				size_t clen;

				clen = data_len - v;
				if (clen > 32) {
					clen = 32;
				}
				c = vc->run(xc, iv, c, buf + v, clen);
			}
			check_equals("KAT CTR AES (3)", buf, cipher, data_len);

			memcpy(buf, plain, data_len);
			c = 1;
			for (v = 0; v < data_len; v += 16) {
				size_t clen;

				clen = data_len - v;
				if (clen > 16) {
					clen = 16;
				}
				c = vc->run(xc, iv, c, buf + v, clen);
			}
			check_equals("KAT CTR AES (4)", buf, cipher, data_len);
		}
	}

	if (with_MC) {
		monte_carlo_AES_encrypt(
			ve,
			"139a35422f1d61de3c91787fe0507afd",
			"b9145a768b7dc489a096b546f43b231f",
			"fb2649694783b551eacd9d5db6126d47");
		monte_carlo_AES_decrypt(
			vd,
			"0c60e7bf20ada9baa9e1ddf0d1540726",
			"b08a29b11a500ea3aca42c36675b9785",
			"d1d2bfdc58ffcad2341b095bce55221e");

		monte_carlo_AES_encrypt(
			ve,
			"b9a63e09e1dfc42e93a90d9bad739e5967aef672eedd5da9",
			"85a1f7a58167b389cddc8a9ff175ee26",
			"5d1196da8f184975e240949a25104554");
		monte_carlo_AES_decrypt(
			vd,
			"4b97585701c03fbebdfa8555024f589f1482c58a00fdd9fd",
			"d0bd0e02ded155e4516be83f42d347a4",
			"b63ef1b79507a62eba3dafcec54a6328");

		monte_carlo_AES_encrypt(
			ve,
			"f9e8389f5b80712e3886cc1fa2d28a3b8c9cd88a2d4a54c6aa86ce0fef944be0",
			"b379777f9050e2a818f2940cbbd9aba4",
			"c5d2cb3d5b7ff0e23e308967ee074825");
		monte_carlo_AES_decrypt(
			vd,
			"2b09ba39b834062b9e93f48373b8dd018dedf1e5ba1b8af831ebbacbc92a2643",
			"89649bd0115f30bd878567610223a59d",
			"e3d3868f578caf34e36445bf14cefc68");
	}

	printf("done.\n");
	fflush(stdout);
}

static void
test_AES_big(void)
{
	test_AES_generic("AES_big",
		&br_aes_big_cbcenc_vtable,
		&br_aes_big_cbcdec_vtable,
		&br_aes_big_ctr_vtable,
		1, 1);
}

static void
test_AES_small(void)
{
	test_AES_generic("AES_small",
		&br_aes_small_cbcenc_vtable,
		&br_aes_small_cbcdec_vtable,
		&br_aes_small_ctr_vtable,
		1, 1);
}

static void
test_AES_ct(void)
{
	test_AES_generic("AES_ct",
		&br_aes_ct_cbcenc_vtable,
		&br_aes_ct_cbcdec_vtable,
		&br_aes_ct_ctr_vtable,
		1, 1);
}

static void
test_AES_ct64(void)
{
	test_AES_generic("AES_ct64",
		&br_aes_ct64_cbcenc_vtable,
		&br_aes_ct64_cbcdec_vtable,
		&br_aes_ct64_ctr_vtable,
		1, 1);
}

static void
test_AES_x86ni(void)
{
	const br_block_cbcenc_class *x_cbcenc;
	const br_block_cbcdec_class *x_cbcdec;
	const br_block_ctr_class *x_ctr;
	int hcbcenc, hcbcdec, hctr;

	x_cbcenc = br_aes_x86ni_cbcenc_get_vtable();
	x_cbcdec = br_aes_x86ni_cbcdec_get_vtable();
	x_ctr = br_aes_x86ni_ctr_get_vtable();
	hcbcenc = (x_cbcenc != NULL);
	hcbcdec = (x_cbcdec != NULL);
	hctr = (x_ctr != NULL);
	if (hcbcenc != hctr || hcbcdec != hctr) {
		fprintf(stderr, "AES_x86ni availability mismatch (%d/%d/%d)\n",
			hcbcenc, hcbcdec, hctr);
		exit(EXIT_FAILURE);
	}
	if (hctr) {
		test_AES_generic("AES_x86ni",
			x_cbcenc, x_cbcdec, x_ctr, 1, 1);
	} else {
		printf("Test AES_x86ni: UNAVAILABLE\n");
	}
}

static void
test_AES_pwr8(void)
{
	const br_block_cbcenc_class *x_cbcenc;
	const br_block_cbcdec_class *x_cbcdec;
	const br_block_ctr_class *x_ctr;
	int hcbcenc, hcbcdec, hctr;

	x_cbcenc = br_aes_pwr8_cbcenc_get_vtable();
	x_cbcdec = br_aes_pwr8_cbcdec_get_vtable();
	x_ctr = br_aes_pwr8_ctr_get_vtable();
	hcbcenc = (x_cbcenc != NULL);
	hcbcdec = (x_cbcdec != NULL);
	hctr = (x_ctr != NULL);
	if (hcbcenc != hctr || hcbcdec != hctr) {
		fprintf(stderr, "AES_pwr8 availability mismatch (%d/%d/%d)\n",
			hcbcenc, hcbcdec, hctr);
		exit(EXIT_FAILURE);
	}
	if (hctr) {
		test_AES_generic("AES_pwr8",
			x_cbcenc, x_cbcdec, x_ctr, 1, 1);
	} else {
		printf("Test AES_pwr8: UNAVAILABLE\n");
	}
}

/*
 * Custom CTR + CBC-MAC AES implementation. Can also do CTR-only, and
 * CBC-MAC-only. The 'aes_big' implementation (CTR) is used. This is
 * meant for comparisons.
 *
 * If 'ctr' is NULL then no encryption/decryption is done; otherwise,
 * CTR encryption/decryption is performed (full-block counter) and the
 * 'ctr' array is updated with the new counter value.
 *
 * If 'cbcmac' is NULL then no CBC-MAC is done; otherwise, CBC-MAC is
 * applied on the encrypted data, with 'cbcmac' as IV and destination
 * buffer for the output. If 'ctr' is not NULL and 'encrypt' is non-zero,
 * then CBC-MAC is computed over the result of CTR processing; otherwise,
 * CBC-MAC is computed over the input data itself.
 */
static void
do_aes_ctrcbc(const void *key, size_t key_len, int encrypt,
	void *ctr, void *cbcmac, unsigned char *data, size_t len)
{
	br_aes_big_ctr_keys bc;
	int i;

	br_aes_big_ctr_init(&bc, key, key_len);
	for (i = 0; i < 2; i ++) {
		/*
		 * CBC-MAC is computed on the encrypted data, so in
		 * first pass if decrypting, second pass if encrypting.
		 */
		if (cbcmac != NULL
			&& ((encrypt && i == 1) || (!encrypt && i == 0)))
		{
			unsigned char zz[16];
			size_t u;

			memcpy(zz, cbcmac, sizeof zz);
			for (u = 0; u < len; u += 16) {
				unsigned char tmp[16];
				size_t v;

				for (v = 0; v < 16; v ++) {
					tmp[v] = zz[v] ^ data[u + v];
				}
				memset(zz, 0, sizeof zz);
				br_aes_big_ctr_run(&bc,
					tmp, br_dec32be(tmp + 12), zz, 16);
			}
			memcpy(cbcmac, zz, sizeof zz);
		}

		/*
		 * CTR encryption/decryption is done only in the first pass.
		 * We process data block per block, because the CTR-only
		 * class uses a 32-bit counter, while the CTR+CBC-MAC
		 * class uses a 128-bit counter.
		 */
		if (ctr != NULL && i == 0) {
			unsigned char zz[16];
			size_t u;

			memcpy(zz, ctr, sizeof zz);
			for (u = 0; u < len; u += 16) {
				int i;

				br_aes_big_ctr_run(&bc,
					zz, br_dec32be(zz + 12), data + u, 16);
				for (i = 15; i >= 0; i --) {
					zz[i] = (zz[i] + 1) & 0xFF;
					if (zz[i] != 0) {
						break;
					}
				}
			}
			memcpy(ctr, zz, sizeof zz);
		}
	}
}

static void
test_AES_CTRCBC_inner(const char *name, const br_block_ctrcbc_class *vt)
{
	br_hmac_drbg_context rng;
	size_t key_len;

	printf("Test AES CTR/CBC-MAC %s: ", name);
	fflush(stdout);

	br_hmac_drbg_init(&rng, &br_sha256_vtable, name, strlen(name));
	for (key_len = 16; key_len <= 32; key_len += 8) {
		br_aes_gen_ctrcbc_keys bc;
		unsigned char key[32];
		size_t data_len;

		br_hmac_drbg_generate(&rng, key, key_len);
		vt->init(&bc.vtable, key, key_len);
		for (data_len = 0; data_len <= 512; data_len += 16) {
			unsigned char plain[512];
			unsigned char data1[sizeof plain];
			unsigned char data2[sizeof plain];
			unsigned char ctr[16], cbcmac[16];
			unsigned char ctr1[16], cbcmac1[16];
			unsigned char ctr2[16], cbcmac2[16];
			int i;

			br_hmac_drbg_generate(&rng, plain, data_len);

			for (i = 0; i <= 16; i ++) {
				if (i == 0) {
					br_hmac_drbg_generate(&rng, ctr, 16);
				} else {
					memset(ctr, 0, i - 1);
					memset(ctr + i - 1, 0xFF, 17 - i);
				}
				br_hmac_drbg_generate(&rng, cbcmac, 16);

				memcpy(data1, plain, data_len);
				memcpy(ctr1, ctr, 16);
				vt->ctr(&bc.vtable, ctr1, data1, data_len);
				memcpy(data2, plain, data_len);
				memcpy(ctr2, ctr, 16);
				do_aes_ctrcbc(key, key_len, 1,
					ctr2, NULL, data2, data_len);
				check_equals("CTR-only data",
					data1, data2, data_len);
				check_equals("CTR-only counter",
					ctr1, ctr2, 16);

				memcpy(data1, plain, data_len);
				memcpy(cbcmac1, cbcmac, 16);
				vt->mac(&bc.vtable, cbcmac1, data1, data_len);
				memcpy(data2, plain, data_len);
				memcpy(cbcmac2, cbcmac, 16);
				do_aes_ctrcbc(key, key_len, 1,
					NULL, cbcmac2, data2, data_len);
				check_equals("CBC-MAC-only",
					cbcmac1, cbcmac2, 16);

				memcpy(data1, plain, data_len);
				memcpy(ctr1, ctr, 16);
				memcpy(cbcmac1, cbcmac, 16);
				vt->encrypt(&bc.vtable,
					ctr1, cbcmac1, data1, data_len);
				memcpy(data2, plain, data_len);
				memcpy(ctr2, ctr, 16);
				memcpy(cbcmac2, cbcmac, 16);
				do_aes_ctrcbc(key, key_len, 1,
					ctr2, cbcmac2, data2, data_len);
				check_equals("encrypt: combined data",
					data1, data2, data_len);
				check_equals("encrypt: combined counter",
					ctr1, ctr2, 16);
				check_equals("encrypt: combined CBC-MAC",
					cbcmac1, cbcmac2, 16);

				memcpy(ctr1, ctr, 16);
				memcpy(cbcmac1, cbcmac, 16);
				vt->decrypt(&bc.vtable,
					ctr1, cbcmac1, data1, data_len);
				memcpy(ctr2, ctr, 16);
				memcpy(cbcmac2, cbcmac, 16);
				do_aes_ctrcbc(key, key_len, 0,
					ctr2, cbcmac2, data2, data_len);
				check_equals("decrypt: combined data",
					data1, data2, data_len);
				check_equals("decrypt: combined counter",
					ctr1, ctr2, 16);
				check_equals("decrypt: combined CBC-MAC",
					cbcmac1, cbcmac2, 16);
			}

			printf(".");
			fflush(stdout);
		}

		printf(" ");
		fflush(stdout);
	}

	printf("done.\n");
	fflush(stdout);
}

static void
test_AES_CTRCBC_big(void)
{
	test_AES_CTRCBC_inner("big", &br_aes_big_ctrcbc_vtable);
}

static void
test_AES_CTRCBC_small(void)
{
	test_AES_CTRCBC_inner("small", &br_aes_small_ctrcbc_vtable);
}

static void
test_AES_CTRCBC_ct(void)
{
	test_AES_CTRCBC_inner("ct", &br_aes_ct_ctrcbc_vtable);
}

static void
test_AES_CTRCBC_ct64(void)
{
	test_AES_CTRCBC_inner("ct64", &br_aes_ct64_ctrcbc_vtable);
}

static void
test_AES_CTRCBC_x86ni(void)
{
	const br_block_ctrcbc_class *vt;

	vt = br_aes_x86ni_ctrcbc_get_vtable();
	if (vt != NULL) {
		test_AES_CTRCBC_inner("x86ni", vt);
	} else {
		printf("Test AES CTR/CBC-MAC x86ni: UNAVAILABLE\n");
	}
}

static void
test_AES_CTRCBC_pwr8(void)
{
	const br_block_ctrcbc_class *vt;

	vt = br_aes_pwr8_ctrcbc_get_vtable();
	if (vt != NULL) {
		test_AES_CTRCBC_inner("pwr8", vt);
	} else {
		printf("Test AES CTR/CBC-MAC pwr8: UNAVAILABLE\n");
	}
}

/*
 * DES known-answer tests. Order: plaintext, key, ciphertext.
 * (mostly from NIST SP 800-20).
 */
static const char *const KAT_DES[] = {
	"10316E028C8F3B4A", "0000000000000000", "82DCBAFBDEAB6602",
	"8000000000000000", "0000000000000000", "95A8D72813DAA94D",
	"4000000000000000", "0000000000000000", "0EEC1487DD8C26D5",
	"2000000000000000", "0000000000000000", "7AD16FFB79C45926",
	"1000000000000000", "0000000000000000", "D3746294CA6A6CF3",
	"0800000000000000", "0000000000000000", "809F5F873C1FD761",
	"0400000000000000", "0000000000000000", "C02FAFFEC989D1FC",
	"0200000000000000", "0000000000000000", "4615AA1D33E72F10",
	"0100000000000000", "0000000000000000", "8CA64DE9C1B123A7",
	"0080000000000000", "0000000000000000", "2055123350C00858",
	"0040000000000000", "0000000000000000", "DF3B99D6577397C8",
	"0020000000000000", "0000000000000000", "31FE17369B5288C9",
	"0010000000000000", "0000000000000000", "DFDD3CC64DAE1642",
	"0008000000000000", "0000000000000000", "178C83CE2B399D94",
	"0004000000000000", "0000000000000000", "50F636324A9B7F80",
	"0002000000000000", "0000000000000000", "A8468EE3BC18F06D",
	"0001000000000000", "0000000000000000", "8CA64DE9C1B123A7",
	"0000800000000000", "0000000000000000", "A2DC9E92FD3CDE92",
	"0000400000000000", "0000000000000000", "CAC09F797D031287",
	"0000200000000000", "0000000000000000", "90BA680B22AEB525",
	"0000100000000000", "0000000000000000", "CE7A24F350E280B6",
	"0000080000000000", "0000000000000000", "882BFF0AA01A0B87",
	"0000040000000000", "0000000000000000", "25610288924511C2",
	"0000020000000000", "0000000000000000", "C71516C29C75D170",
	"0000010000000000", "0000000000000000", "8CA64DE9C1B123A7",
	"0000008000000000", "0000000000000000", "5199C29A52C9F059",
	"0000004000000000", "0000000000000000", "C22F0A294A71F29F",
	"0000002000000000", "0000000000000000", "EE371483714C02EA",
	"0000001000000000", "0000000000000000", "A81FBD448F9E522F",
	"0000000800000000", "0000000000000000", "4F644C92E192DFED",
	"0000000400000000", "0000000000000000", "1AFA9A66A6DF92AE",
	"0000000200000000", "0000000000000000", "B3C1CC715CB879D8",
	"0000000100000000", "0000000000000000", "8CA64DE9C1B123A7",
	"0000000080000000", "0000000000000000", "19D032E64AB0BD8B",
	"0000000040000000", "0000000000000000", "3CFAA7A7DC8720DC",
	"0000000020000000", "0000000000000000", "B7265F7F447AC6F3",
	"0000000010000000", "0000000000000000", "9DB73B3C0D163F54",
	"0000000008000000", "0000000000000000", "8181B65BABF4A975",
	"0000000004000000", "0000000000000000", "93C9B64042EAA240",
	"0000000002000000", "0000000000000000", "5570530829705592",
	"0000000001000000", "0000000000000000", "8CA64DE9C1B123A7",
	"0000000000800000", "0000000000000000", "8638809E878787A0",
	"0000000000400000", "0000000000000000", "41B9A79AF79AC208",
	"0000000000200000", "0000000000000000", "7A9BE42F2009A892",
	"0000000000100000", "0000000000000000", "29038D56BA6D2745",
	"0000000000080000", "0000000000000000", "5495C6ABF1E5DF51",
	"0000000000040000", "0000000000000000", "AE13DBD561488933",
	"0000000000020000", "0000000000000000", "024D1FFA8904E389",
	"0000000000010000", "0000000000000000", "8CA64DE9C1B123A7",
	"0000000000008000", "0000000000000000", "D1399712F99BF02E",
	"0000000000004000", "0000000000000000", "14C1D7C1CFFEC79E",
	"0000000000002000", "0000000000000000", "1DE5279DAE3BED6F",
	"0000000000001000", "0000000000000000", "E941A33F85501303",
	"0000000000000800", "0000000000000000", "DA99DBBC9A03F379",
	"0000000000000400", "0000000000000000", "B7FC92F91D8E92E9",
	"0000000000000200", "0000000000000000", "AE8E5CAA3CA04E85",
	"0000000000000100", "0000000000000000", "8CA64DE9C1B123A7",
	"0000000000000080", "0000000000000000", "9CC62DF43B6EED74",
	"0000000000000040", "0000000000000000", "D863DBB5C59A91A0",
	"0000000000000020", "0000000000000000", "A1AB2190545B91D7",
	"0000000000000010", "0000000000000000", "0875041E64C570F7",
	"0000000000000008", "0000000000000000", "5A594528BEBEF1CC",
	"0000000000000004", "0000000000000000", "FCDB3291DE21F0C0",
	"0000000000000002", "0000000000000000", "869EFD7F9F265A09",
	"0000000000000001", "0000000000000000", "8CA64DE9C1B123A7",
	"0000000000000000", "8000000000000000", "95F8A5E5DD31D900",
	"0000000000000000", "4000000000000000", "DD7F121CA5015619",
	"0000000000000000", "2000000000000000", "2E8653104F3834EA",
	"0000000000000000", "1000000000000000", "4BD388FF6CD81D4F",
	"0000000000000000", "0800000000000000", "20B9E767B2FB1456",
	"0000000000000000", "0400000000000000", "55579380D77138EF",
	"0000000000000000", "0200000000000000", "6CC5DEFAAF04512F",
	"0000000000000000", "0100000000000000", "0D9F279BA5D87260",
	"0000000000000000", "0080000000000000", "D9031B0271BD5A0A",
	"0000000000000000", "0040000000000000", "424250B37C3DD951",
	"0000000000000000", "0020000000000000", "B8061B7ECD9A21E5",
	"0000000000000000", "0010000000000000", "F15D0F286B65BD28",
	"0000000000000000", "0008000000000000", "ADD0CC8D6E5DEBA1",
	"0000000000000000", "0004000000000000", "E6D5F82752AD63D1",
	"0000000000000000", "0002000000000000", "ECBFE3BD3F591A5E",
	"0000000000000000", "0001000000000000", "F356834379D165CD",
	"0000000000000000", "0000800000000000", "2B9F982F20037FA9",
	"0000000000000000", "0000400000000000", "889DE068A16F0BE6",
	"0000000000000000", "0000200000000000", "E19E275D846A1298",
	"0000000000000000", "0000100000000000", "329A8ED523D71AEC",
	"0000000000000000", "0000080000000000", "E7FCE22557D23C97",
	"0000000000000000", "0000040000000000", "12A9F5817FF2D65D",
	"0000000000000000", "0000020000000000", "A484C3AD38DC9C19",
	"0000000000000000", "0000010000000000", "FBE00A8A1EF8AD72",
	"0000000000000000", "0000008000000000", "750D079407521363",
	"0000000000000000", "0000004000000000", "64FEED9C724C2FAF",
	"0000000000000000", "0000002000000000", "F02B263B328E2B60",
	"0000000000000000", "0000001000000000", "9D64555A9A10B852",
	"0000000000000000", "0000000800000000", "D106FF0BED5255D7",
	"0000000000000000", "0000000400000000", "E1652C6B138C64A5",
	"0000000000000000", "0000000200000000", "E428581186EC8F46",
	"0000000000000000", "0000000100000000", "AEB5F5EDE22D1A36",
	"0000000000000000", "0000000080000000", "E943D7568AEC0C5C",
	"0000000000000000", "0000000040000000", "DF98C8276F54B04B",
	"0000000000000000", "0000000020000000", "B160E4680F6C696F",
	"0000000000000000", "0000000010000000", "FA0752B07D9C4AB8",
	"0000000000000000", "0000000008000000", "CA3A2B036DBC8502",
	"0000000000000000", "0000000004000000", "5E0905517BB59BCF",
	"0000000000000000", "0000000002000000", "814EEB3B91D90726",
	"0000000000000000", "0000000001000000", "4D49DB1532919C9F",
	"0000000000000000", "0000000000800000", "25EB5FC3F8CF0621",
	"0000000000000000", "0000000000400000", "AB6A20C0620D1C6F",
	"0000000000000000", "0000000000200000", "79E90DBC98F92CCA",
	"0000000000000000", "0000000000100000", "866ECEDD8072BB0E",
	"0000000000000000", "0000000000080000", "8B54536F2F3E64A8",
	"0000000000000000", "0000000000040000", "EA51D3975595B86B",
	"0000000000000000", "0000000000020000", "CAFFC6AC4542DE31",
	"0000000000000000", "0000000000010000", "8DD45A2DDF90796C",
	"0000000000000000", "0000000000008000", "1029D55E880EC2D0",
	"0000000000000000", "0000000000004000", "5D86CB23639DBEA9",
	"0000000000000000", "0000000000002000", "1D1CA853AE7C0C5F",
	"0000000000000000", "0000000000001000", "CE332329248F3228",
	"0000000000000000", "0000000000000800", "8405D1ABE24FB942",
	"0000000000000000", "0000000000000400", "E643D78090CA4207",
	"0000000000000000", "0000000000000200", "48221B9937748A23",
	"0000000000000000", "0000000000000100", "DD7C0BBD61FAFD54",
	"0000000000000000", "0000000000000080", "2FBC291A570DB5C4",
	"0000000000000000", "0000000000000040", "E07C30D7E4E26E12",
	"0000000000000000", "0000000000000020", "0953E2258E8E90A1",
	"0000000000000000", "0000000000000010", "5B711BC4CEEBF2EE",
	"0000000000000000", "0000000000000008", "CC083F1E6D9E85F6",
	"0000000000000000", "0000000000000004", "D2FD8867D50D2DFE",
	"0000000000000000", "0000000000000002", "06E7EA22CE92708F",
	"0000000000000000", "0000000000000001", "166B40B44ABA4BD6",
	"0000000000000000", "0000000000000000", "8CA64DE9C1B123A7",
	"0101010101010101", "0101010101010101", "994D4DC157B96C52",
	"0202020202020202", "0202020202020202", "E127C2B61D98E6E2",
	"0303030303030303", "0303030303030303", "984C91D78A269CE3",
	"0404040404040404", "0404040404040404", "1F4570BB77550683",
	"0505050505050505", "0505050505050505", "3990ABF98D672B16",
	"0606060606060606", "0606060606060606", "3F5150BBA081D585",
	"0707070707070707", "0707070707070707", "C65242248C9CF6F2",
	"0808080808080808", "0808080808080808", "10772D40FAD24257",
	"0909090909090909", "0909090909090909", "F0139440647A6E7B",
	"0A0A0A0A0A0A0A0A", "0A0A0A0A0A0A0A0A", "0A288603044D740C",
	"0B0B0B0B0B0B0B0B", "0B0B0B0B0B0B0B0B", "6359916942F7438F",
	"0C0C0C0C0C0C0C0C", "0C0C0C0C0C0C0C0C", "934316AE443CF08B",
	"0D0D0D0D0D0D0D0D", "0D0D0D0D0D0D0D0D", "E3F56D7F1130A2B7",
	"0E0E0E0E0E0E0E0E", "0E0E0E0E0E0E0E0E", "A2E4705087C6B6B4",
	"0F0F0F0F0F0F0F0F", "0F0F0F0F0F0F0F0F", "D5D76E09A447E8C3",
	"1010101010101010", "1010101010101010", "DD7515F2BFC17F85",
	"1111111111111111", "1111111111111111", "F40379AB9E0EC533",
	"1212121212121212", "1212121212121212", "96CD27784D1563E5",
	"1313131313131313", "1313131313131313", "2911CF5E94D33FE1",
	"1414141414141414", "1414141414141414", "377B7F7CA3E5BBB3",
	"1515151515151515", "1515151515151515", "701AA63832905A92",
	"1616161616161616", "1616161616161616", "2006E716C4252D6D",
	"1717171717171717", "1717171717171717", "452C1197422469F8",
	"1818181818181818", "1818181818181818", "C33FD1EB49CB64DA",
	"1919191919191919", "1919191919191919", "7572278F364EB50D",
	"1A1A1A1A1A1A1A1A", "1A1A1A1A1A1A1A1A", "69E51488403EF4C3",
	"1B1B1B1B1B1B1B1B", "1B1B1B1B1B1B1B1B", "FF847E0ADF192825",
	"1C1C1C1C1C1C1C1C", "1C1C1C1C1C1C1C1C", "521B7FB3B41BB791",
	"1D1D1D1D1D1D1D1D", "1D1D1D1D1D1D1D1D", "26059A6A0F3F6B35",
	"1E1E1E1E1E1E1E1E", "1E1E1E1E1E1E1E1E", "F24A8D2231C77538",
	"1F1F1F1F1F1F1F1F", "1F1F1F1F1F1F1F1F", "4FD96EC0D3304EF6",
	"2020202020202020", "2020202020202020", "18A9D580A900B699",
	"2121212121212121", "2121212121212121", "88586E1D755B9B5A",
	"2222222222222222", "2222222222222222", "0F8ADFFB11DC2784",
	"2323232323232323", "2323232323232323", "2F30446C8312404A",
	"2424242424242424", "2424242424242424", "0BA03D9E6C196511",
	"2525252525252525", "2525252525252525", "3E55E997611E4B7D",
	"2626262626262626", "2626262626262626", "B2522FB5F158F0DF",
	"2727272727272727", "2727272727272727", "2109425935406AB8",
	"2828282828282828", "2828282828282828", "11A16028F310FF16",
	"2929292929292929", "2929292929292929", "73F0C45F379FE67F",
	"2A2A2A2A2A2A2A2A", "2A2A2A2A2A2A2A2A", "DCAD4338F7523816",
	"2B2B2B2B2B2B2B2B", "2B2B2B2B2B2B2B2B", "B81634C1CEAB298C",
	"2C2C2C2C2C2C2C2C", "2C2C2C2C2C2C2C2C", "DD2CCB29B6C4C349",
	"2D2D2D2D2D2D2D2D", "2D2D2D2D2D2D2D2D", "7D07A77A2ABD50A7",
	"2E2E2E2E2E2E2E2E", "2E2E2E2E2E2E2E2E", "30C1B0C1FD91D371",
	"2F2F2F2F2F2F2F2F", "2F2F2F2F2F2F2F2F", "C4427B31AC61973B",
	"3030303030303030", "3030303030303030", "F47BB46273B15EB5",
	"3131313131313131", "3131313131313131", "655EA628CF62585F",
	"3232323232323232", "3232323232323232", "AC978C247863388F",
	"3333333333333333", "3333333333333333", "0432ED386F2DE328",
	"3434343434343434", "3434343434343434", "D254014CB986B3C2",
	"3535353535353535", "3535353535353535", "B256E34BEDB49801",
	"3636363636363636", "3636363636363636", "37F8759EB77E7BFC",
	"3737373737373737", "3737373737373737", "5013CA4F62C9CEA0",
	"3838383838383838", "3838383838383838", "8940F7B3EACA5939",
	"3939393939393939", "3939393939393939", "E22B19A55086774B",
	"3A3A3A3A3A3A3A3A", "3A3A3A3A3A3A3A3A", "B04A2AAC925ABB0B",
	"3B3B3B3B3B3B3B3B", "3B3B3B3B3B3B3B3B", "8D250D58361597FC",
	"3C3C3C3C3C3C3C3C", "3C3C3C3C3C3C3C3C", "51F0114FB6A6CD37",
	"3D3D3D3D3D3D3D3D", "3D3D3D3D3D3D3D3D", "9D0BB4DB830ECB73",
	"3E3E3E3E3E3E3E3E", "3E3E3E3E3E3E3E3E", "E96089D6368F3E1A",
	"3F3F3F3F3F3F3F3F", "3F3F3F3F3F3F3F3F", "5C4CA877A4E1E92D",
	"4040404040404040", "4040404040404040", "6D55DDBC8DEA95FF",
	"4141414141414141", "4141414141414141", "19DF84AC95551003",
	"4242424242424242", "4242424242424242", "724E7332696D08A7",
	"4343434343434343", "4343434343434343", "B91810B8CDC58FE2",
	"4444444444444444", "4444444444444444", "06E23526EDCCD0C4",
	"4545454545454545", "4545454545454545", "EF52491D5468D441",
	"4646464646464646", "4646464646464646", "48019C59E39B90C5",
	"4747474747474747", "4747474747474747", "0544083FB902D8C0",
	"4848484848484848", "4848484848484848", "63B15CADA668CE12",
	"4949494949494949", "4949494949494949", "EACC0C1264171071",
	"4A4A4A4A4A4A4A4A", "4A4A4A4A4A4A4A4A", "9D2B8C0AC605F274",
	"4B4B4B4B4B4B4B4B", "4B4B4B4B4B4B4B4B", "C90F2F4C98A8FB2A",
	"4C4C4C4C4C4C4C4C", "4C4C4C4C4C4C4C4C", "03481B4828FD1D04",
	"4D4D4D4D4D4D4D4D", "4D4D4D4D4D4D4D4D", "C78FC45A1DCEA2E2",
	"4E4E4E4E4E4E4E4E", "4E4E4E4E4E4E4E4E", "DB96D88C3460D801",
	"4F4F4F4F4F4F4F4F", "4F4F4F4F4F4F4F4F", "6C69E720F5105518",
	"5050505050505050", "5050505050505050", "0D262E418BC893F3",
	"5151515151515151", "5151515151515151", "6AD84FD7848A0A5C",
	"5252525252525252", "5252525252525252", "C365CB35B34B6114",
	"5353535353535353", "5353535353535353", "1155392E877F42A9",
	"5454545454545454", "5454545454545454", "531BE5F9405DA715",
	"5555555555555555", "5555555555555555", "3BCDD41E6165A5E8",
	"5656565656565656", "5656565656565656", "2B1FF5610A19270C",
	"5757575757575757", "5757575757575757", "D90772CF3F047CFD",
	"5858585858585858", "5858585858585858", "1BEA27FFB72457B7",
	"5959595959595959", "5959595959595959", "85C3E0C429F34C27",
	"5A5A5A5A5A5A5A5A", "5A5A5A5A5A5A5A5A", "F9038021E37C7618",
	"5B5B5B5B5B5B5B5B", "5B5B5B5B5B5B5B5B", "35BC6FF838DBA32F",
	"5C5C5C5C5C5C5C5C", "5C5C5C5C5C5C5C5C", "4927ACC8CE45ECE7",
	"5D5D5D5D5D5D5D5D", "5D5D5D5D5D5D5D5D", "E812EE6E3572985C",
	"5E5E5E5E5E5E5E5E", "5E5E5E5E5E5E5E5E", "9BB93A89627BF65F",
	"5F5F5F5F5F5F5F5F", "5F5F5F5F5F5F5F5F", "EF12476884CB74CA",
	"6060606060606060", "6060606060606060", "1BF17E00C09E7CBF",
	"6161616161616161", "6161616161616161", "29932350C098DB5D",
	"6262626262626262", "6262626262626262", "B476E6499842AC54",
	"6363636363636363", "6363636363636363", "5C662C29C1E96056",
	"6464646464646464", "6464646464646464", "3AF1703D76442789",
	"6565656565656565", "6565656565656565", "86405D9B425A8C8C",
	"6666666666666666", "6666666666666666", "EBBF4810619C2C55",
	"6767676767676767", "6767676767676767", "F8D1CD7367B21B5D",
	"6868686868686868", "6868686868686868", "9EE703142BF8D7E2",
	"6969696969696969", "6969696969696969", "5FDFFFC3AAAB0CB3",
	"6A6A6A6A6A6A6A6A", "6A6A6A6A6A6A6A6A", "26C940AB13574231",
	"6B6B6B6B6B6B6B6B", "6B6B6B6B6B6B6B6B", "1E2DC77E36A84693",
	"6C6C6C6C6C6C6C6C", "6C6C6C6C6C6C6C6C", "0F4FF4D9BC7E2244",
	"6D6D6D6D6D6D6D6D", "6D6D6D6D6D6D6D6D", "A4C9A0D04D3280CD",
	"6E6E6E6E6E6E6E6E", "6E6E6E6E6E6E6E6E", "9FAF2C96FE84919D",
	"6F6F6F6F6F6F6F6F", "6F6F6F6F6F6F6F6F", "115DBC965E6096C8",
	"7070707070707070", "7070707070707070", "AF531E9520994017",
	"7171717171717171", "7171717171717171", "B971ADE70E5C89EE",
	"7272727272727272", "7272727272727272", "415D81C86AF9C376",
	"7373737373737373", "7373737373737373", "8DFB864FDB3C6811",
	"7474747474747474", "7474747474747474", "10B1C170E3398F91",
	"7575757575757575", "7575757575757575", "CFEF7A1C0218DB1E",
	"7676767676767676", "7676767676767676", "DBAC30A2A40B1B9C",
	"7777777777777777", "7777777777777777", "89D3BF37052162E9",
	"7878787878787878", "7878787878787878", "80D9230BDAEB67DC",
	"7979797979797979", "7979797979797979", "3440911019AD68D7",
	"7A7A7A7A7A7A7A7A", "7A7A7A7A7A7A7A7A", "9626FE57596E199E",
	"7B7B7B7B7B7B7B7B", "7B7B7B7B7B7B7B7B", "DEA0B796624BB5BA",
	"7C7C7C7C7C7C7C7C", "7C7C7C7C7C7C7C7C", "E9E40542BDDB3E9D",
	"7D7D7D7D7D7D7D7D", "7D7D7D7D7D7D7D7D", "8AD99914B354B911",
	"7E7E7E7E7E7E7E7E", "7E7E7E7E7E7E7E7E", "6F85B98DD12CB13B",
	"7F7F7F7F7F7F7F7F", "7F7F7F7F7F7F7F7F", "10130DA3C3A23924",
	"8080808080808080", "8080808080808080", "EFECF25C3C5DC6DB",
	"8181818181818181", "8181818181818181", "907A46722ED34EC4",
	"8282828282828282", "8282828282828282", "752666EB4CAB46EE",
	"8383838383838383", "8383838383838383", "161BFABD4224C162",
	"8484848484848484", "8484848484848484", "215F48699DB44A45",
	"8585858585858585", "8585858585858585", "69D901A8A691E661",
	"8686868686868686", "8686868686868686", "CBBF6EEFE6529728",
	"8787878787878787", "8787878787878787", "7F26DCF425149823",
	"8888888888888888", "8888888888888888", "762C40C8FADE9D16",
	"8989898989898989", "8989898989898989", "2453CF5D5BF4E463",
	"8A8A8A8A8A8A8A8A", "8A8A8A8A8A8A8A8A", "301085E3FDE724E1",
	"8B8B8B8B8B8B8B8B", "8B8B8B8B8B8B8B8B", "EF4E3E8F1CC6706E",
	"8C8C8C8C8C8C8C8C", "8C8C8C8C8C8C8C8C", "720479B024C397EE",
	"8D8D8D8D8D8D8D8D", "8D8D8D8D8D8D8D8D", "BEA27E3795063C89",
	"8E8E8E8E8E8E8E8E", "8E8E8E8E8E8E8E8E", "468E5218F1A37611",
	"8F8F8F8F8F8F8F8F", "8F8F8F8F8F8F8F8F", "50ACE16ADF66BFE8",
	"9090909090909090", "9090909090909090", "EEA24369A19F6937",
	"9191919191919191", "9191919191919191", "6050D369017B6E62",
	"9292929292929292", "9292929292929292", "5B365F2FB2CD7F32",
	"9393939393939393", "9393939393939393", "F0B00B264381DDBB",
	"9494949494949494", "9494949494949494", "E1D23881C957B96C",
	"9595959595959595", "9595959595959595", "D936BF54ECA8BDCE",
	"9696969696969696", "9696969696969696", "A020003C5554F34C",
	"9797979797979797", "9797979797979797", "6118FCEBD407281D",
	"9898989898989898", "9898989898989898", "072E328C984DE4A2",
	"9999999999999999", "9999999999999999", "1440B7EF9E63D3AA",
	"9A9A9A9A9A9A9A9A", "9A9A9A9A9A9A9A9A", "79BFA264BDA57373",
	"9B9B9B9B9B9B9B9B", "9B9B9B9B9B9B9B9B", "C50E8FC289BBD876",
	"9C9C9C9C9C9C9C9C", "9C9C9C9C9C9C9C9C", "A399D3D63E169FA9",
	"9D9D9D9D9D9D9D9D", "9D9D9D9D9D9D9D9D", "4B8919B667BD53AB",
	"9E9E9E9E9E9E9E9E", "9E9E9E9E9E9E9E9E", "D66CDCAF3F6724A2",
	"9F9F9F9F9F9F9F9F", "9F9F9F9F9F9F9F9F", "E40E81FF3F618340",
	"A0A0A0A0A0A0A0A0", "A0A0A0A0A0A0A0A0", "10EDB8977B348B35",
	"A1A1A1A1A1A1A1A1", "A1A1A1A1A1A1A1A1", "6446C5769D8409A0",
	"A2A2A2A2A2A2A2A2", "A2A2A2A2A2A2A2A2", "17ED1191CA8D67A3",
	"A3A3A3A3A3A3A3A3", "A3A3A3A3A3A3A3A3", "B6D8533731BA1318",
	"A4A4A4A4A4A4A4A4", "A4A4A4A4A4A4A4A4", "CA439007C7245CD0",
	"A5A5A5A5A5A5A5A5", "A5A5A5A5A5A5A5A5", "06FC7FDE1C8389E7",
	"A6A6A6A6A6A6A6A6", "A6A6A6A6A6A6A6A6", "7A3C1F3BD60CB3D8",
	"A7A7A7A7A7A7A7A7", "A7A7A7A7A7A7A7A7", "E415D80048DBA848",
	"A8A8A8A8A8A8A8A8", "A8A8A8A8A8A8A8A8", "26F88D30C0FB8302",
	"A9A9A9A9A9A9A9A9", "A9A9A9A9A9A9A9A9", "D4E00A9EF5E6D8F3",
	"AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAA", "C4322BE19E9A5A17",
	"ABABABABABABABAB", "ABABABABABABABAB", "ACE41A06BFA258EA",
	"ACACACACACACACAC", "ACACACACACACACAC", "EEAAC6D17880BD56",
	"ADADADADADADADAD", "ADADADADADADADAD", "3C9A34CA4CB49EEB",
	"AEAEAEAEAEAEAEAE", "AEAEAEAEAEAEAEAE", "9527B0287B75F5A3",
	"AFAFAFAFAFAFAFAF", "AFAFAFAFAFAFAFAF", "F2D9D1BE74376C0C",
	"B0B0B0B0B0B0B0B0", "B0B0B0B0B0B0B0B0", "939618DF0AEFAAE7",
	"B1B1B1B1B1B1B1B1", "B1B1B1B1B1B1B1B1", "24692773CB9F27FE",
	"B2B2B2B2B2B2B2B2", "B2B2B2B2B2B2B2B2", "38703BA5E2315D1D",
	"B3B3B3B3B3B3B3B3", "B3B3B3B3B3B3B3B3", "FCB7E4B7D702E2FB",
	"B4B4B4B4B4B4B4B4", "B4B4B4B4B4B4B4B4", "36F0D0B3675704D5",
	"B5B5B5B5B5B5B5B5", "B5B5B5B5B5B5B5B5", "62D473F539FA0D8B",
	"B6B6B6B6B6B6B6B6", "B6B6B6B6B6B6B6B6", "1533F3ED9BE8EF8E",
	"B7B7B7B7B7B7B7B7", "B7B7B7B7B7B7B7B7", "9C4EA352599731ED",
	"B8B8B8B8B8B8B8B8", "B8B8B8B8B8B8B8B8", "FABBF7C046FD273F",
	"B9B9B9B9B9B9B9B9", "B9B9B9B9B9B9B9B9", "B7FE63A61C646F3A",
	"BABABABABABABABA", "BABABABABABABABA", "10ADB6E2AB972BBE",
	"BBBBBBBBBBBBBBBB", "BBBBBBBBBBBBBBBB", "F91DCAD912332F3B",
	"BCBCBCBCBCBCBCBC", "BCBCBCBCBCBCBCBC", "46E7EF47323A701D",
	"BDBDBDBDBDBDBDBD", "BDBDBDBDBDBDBDBD", "8DB18CCD9692F758",
	"BEBEBEBEBEBEBEBE", "BEBEBEBEBEBEBEBE", "E6207B536AAAEFFC",
	"BFBFBFBFBFBFBFBF", "BFBFBFBFBFBFBFBF", "92AA224372156A00",
	"C0C0C0C0C0C0C0C0", "C0C0C0C0C0C0C0C0", "A3B357885B1E16D2",
	"C1C1C1C1C1C1C1C1", "C1C1C1C1C1C1C1C1", "169F7629C970C1E5",
	"C2C2C2C2C2C2C2C2", "C2C2C2C2C2C2C2C2", "62F44B247CF1348C",
	"C3C3C3C3C3C3C3C3", "C3C3C3C3C3C3C3C3", "AE0FEEB0495932C8",
	"C4C4C4C4C4C4C4C4", "C4C4C4C4C4C4C4C4", "72DAF2A7C9EA6803",
	"C5C5C5C5C5C5C5C5", "C5C5C5C5C5C5C5C5", "4FB5D5536DA544F4",
	"C6C6C6C6C6C6C6C6", "C6C6C6C6C6C6C6C6", "1DD4E65AAF7988B4",
	"C7C7C7C7C7C7C7C7", "C7C7C7C7C7C7C7C7", "76BF084C1535A6C6",
	"C8C8C8C8C8C8C8C8", "C8C8C8C8C8C8C8C8", "AFEC35B09D36315F",
	"C9C9C9C9C9C9C9C9", "C9C9C9C9C9C9C9C9", "C8078A6148818403",
	"CACACACACACACACA", "CACACACACACACACA", "4DA91CB4124B67FE",
	"CBCBCBCBCBCBCBCB", "CBCBCBCBCBCBCBCB", "2DABFEB346794C3D",
	"CCCCCCCCCCCCCCCC", "CCCCCCCCCCCCCCCC", "FBCD12C790D21CD7",
	"CDCDCDCDCDCDCDCD", "CDCDCDCDCDCDCDCD", "536873DB879CC770",
	"CECECECECECECECE", "CECECECECECECECE", "9AA159D7309DA7A0",
	"CFCFCFCFCFCFCFCF", "CFCFCFCFCFCFCFCF", "0B844B9D8C4EA14A",
	"D0D0D0D0D0D0D0D0", "D0D0D0D0D0D0D0D0", "3BBD84CE539E68C4",
	"D1D1D1D1D1D1D1D1", "D1D1D1D1D1D1D1D1", "CF3E4F3E026E2C8E",
	"D2D2D2D2D2D2D2D2", "D2D2D2D2D2D2D2D2", "82F85885D542AF58",
	"D3D3D3D3D3D3D3D3", "D3D3D3D3D3D3D3D3", "22D334D6493B3CB6",
	"D4D4D4D4D4D4D4D4", "D4D4D4D4D4D4D4D4", "47E9CB3E3154D673",
	"D5D5D5D5D5D5D5D5", "D5D5D5D5D5D5D5D5", "2352BCC708ADC7E9",
	"D6D6D6D6D6D6D6D6", "D6D6D6D6D6D6D6D6", "8C0F3BA0C8601980",
	"D7D7D7D7D7D7D7D7", "D7D7D7D7D7D7D7D7", "EE5E9FD70CEF00E9",
	"D8D8D8D8D8D8D8D8", "D8D8D8D8D8D8D8D8", "DEF6BDA6CABF9547",
	"D9D9D9D9D9D9D9D9", "D9D9D9D9D9D9D9D9", "4DADD04A0EA70F20",
	"DADADADADADADADA", "DADADADADADADADA", "C1AA16689EE1B482",
	"DBDBDBDBDBDBDBDB", "DBDBDBDBDBDBDBDB", "F45FC26193E69AEE",
	"DCDCDCDCDCDCDCDC", "DCDCDCDCDCDCDCDC", "D0CFBB937CEDBFB5",
	"DDDDDDDDDDDDDDDD", "DDDDDDDDDDDDDDDD", "F0752004EE23D87B",
	"DEDEDEDEDEDEDEDE", "DEDEDEDEDEDEDEDE", "77A791E28AA464A5",
	"DFDFDFDFDFDFDFDF", "DFDFDFDFDFDFDFDF", "E7562A7F56FF4966",
	"E0E0E0E0E0E0E0E0", "E0E0E0E0E0E0E0E0", "B026913F2CCFB109",
	"E1E1E1E1E1E1E1E1", "E1E1E1E1E1E1E1E1", "0DB572DDCE388AC7",
	"E2E2E2E2E2E2E2E2", "E2E2E2E2E2E2E2E2", "D9FA6595F0C094CA",
	"E3E3E3E3E3E3E3E3", "E3E3E3E3E3E3E3E3", "ADE4804C4BE4486E",
	"E4E4E4E4E4E4E4E4", "E4E4E4E4E4E4E4E4", "007B81F520E6D7DA",
	"E5E5E5E5E5E5E5E5", "E5E5E5E5E5E5E5E5", "961AEB77BFC10B3C",
	"E6E6E6E6E6E6E6E6", "E6E6E6E6E6E6E6E6", "8A8DD870C9B14AF2",
	"E7E7E7E7E7E7E7E7", "E7E7E7E7E7E7E7E7", "3CC02E14B6349B25",
	"E8E8E8E8E8E8E8E8", "E8E8E8E8E8E8E8E8", "BAD3EE68BDDB9607",
	"E9E9E9E9E9E9E9E9", "E9E9E9E9E9E9E9E9", "DFF918E93BDAD292",
	"EAEAEAEAEAEAEAEA", "EAEAEAEAEAEAEAEA", "8FE559C7CD6FA56D",
	"EBEBEBEBEBEBEBEB", "EBEBEBEBEBEBEBEB", "C88480835C1A444C",
	"ECECECECECECECEC", "ECECECECECECECEC", "D6EE30A16B2CC01E",
	"EDEDEDEDEDEDEDED", "EDEDEDEDEDEDEDED", "6932D887B2EA9C1A",
	"EEEEEEEEEEEEEEEE", "EEEEEEEEEEEEEEEE", "0BFC865461F13ACC",
	"EFEFEFEFEFEFEFEF", "EFEFEFEFEFEFEFEF", "228AEA0D403E807A",
	"F0F0F0F0F0F0F0F0", "F0F0F0F0F0F0F0F0", "2A2891F65BB8173C",
	"F1F1F1F1F1F1F1F1", "F1F1F1F1F1F1F1F1", "5D1B8FAF7839494B",
	"F2F2F2F2F2F2F2F2", "F2F2F2F2F2F2F2F2", "1C0A9280EECF5D48",
	"F3F3F3F3F3F3F3F3", "F3F3F3F3F3F3F3F3", "6CBCE951BBC30F74",
	"F4F4F4F4F4F4F4F4", "F4F4F4F4F4F4F4F4", "9CA66E96BD08BC70",
	"F5F5F5F5F5F5F5F5", "F5F5F5F5F5F5F5F5", "F5D779FCFBB28BF3",
	"F6F6F6F6F6F6F6F6", "F6F6F6F6F6F6F6F6", "0FEC6BBF9B859184",
	"F7F7F7F7F7F7F7F7", "F7F7F7F7F7F7F7F7", "EF88D2BF052DBDA8",
	"F8F8F8F8F8F8F8F8", "F8F8F8F8F8F8F8F8", "39ADBDDB7363090D",
	"F9F9F9F9F9F9F9F9", "F9F9F9F9F9F9F9F9", "C0AEAF445F7E2A7A",
	"FAFAFAFAFAFAFAFA", "FAFAFAFAFAFAFAFA", "C66F54067298D4E9",
	"FBFBFBFBFBFBFBFB", "FBFBFBFBFBFBFBFB", "E0BA8F4488AAF97C",
	"FCFCFCFCFCFCFCFC", "FCFCFCFCFCFCFCFC", "67B36E2875D9631C",
	"FDFDFDFDFDFDFDFD", "FDFDFDFDFDFDFDFD", "1ED83D49E267191D",
	"FEFEFEFEFEFEFEFE", "FEFEFEFEFEFEFEFE", "66B2B23EA84693AD",
	"FFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFF", "7359B2163E4EDC58",
	"0001020304050607", "0011223344556677", "3EF0A891CF8ED990",
	"2BD6459F82C5B300", "EA024714AD5C4D84", "126EFE8ED312190A",

	NULL
};

/*
 * Known-answer tests for DES/3DES in CBC mode. Order: key, IV,
 * plaintext, ciphertext.
 */
static const char *const KAT_DES_CBC[] = {
	/*
	 * From NIST validation suite (tdesmmt.zip).
	 */
	"34a41a8c293176c1b30732ecfe38ae8a34a41a8c293176c1",
	"f55b4855228bd0b4",
	"7dd880d2a9ab411c",
	"c91892948b6cadb4",

	"70a88fa1dfb9942fa77f40157ffef2ad70a88fa1dfb9942f",
	"ece08ce2fdc6ce80",
	"bc225304d5a3a5c9918fc5006cbc40cc",
	"27f67dc87af7ddb4b68f63fa7c2d454a",

	"e091790be55be0bc0780153861a84adce091790be55be0bc",
	"fd7d430f86fbbffe",
	"03c7fffd7f36499c703dedc9df4de4a92dd4382e576d6ae9",
	"053aeba85dd3a23bfbe8440a432f9578f312be60fb9f0035",

	"857feacd16157c58e5347a70e56e578a857feacd16157c58",
	"002dcb6d46ef0969",
	"1f13701c7f0d7385307507a18e89843ebd295bd5e239ef109347a6898c6d3fd5",
	"a0e4edde34f05bd8397ce279e49853e9387ba04be562f5fa19c3289c3f5a3391",

	"a173545b265875ba852331fbb95b49a8a173545b265875ba",
	"ab385756391d364c",
	"d08894c565608d9ae51dda63b85b3b33b1703bb5e4f1abcbb8794e743da5d6f3bf630f2e9b6d5b54",
	"370b47acf89ac6bdbb13c9a7336787dc41e1ad8beead32281d0609fb54968404bdf2894892590658",

	"26376bcb2f23df1083cd684fe00ed3c726376bcb2f23df10",
	"33acfb0f3d240ea6",
	"903a1911da1e6877f23c1985a9b61786ef438e0ce1240885035ad60fc916b18e5d71a1fb9c5d1eff61db75c0076f6efb",
	"7a4f7510f6ec0b93e2495d21a8355684d303a770ebda2e0e51ff33d72b20cb73e58e2e3de2ef6b2e12c504c0f181ba63",

	"3e1f98135d027cec752f67765408a7913e1f98135d027cec",
	"11f5f2304b28f68b",
	"7c022f5af24f7925d323d4d0e20a2ce49272c5e764b22c806f4b6ddc406d864fe5bd1c3f45556d3eb30c8676c2f8b54a5a32423a0bd95a07",
	"2bb4b131fa4ae0b4f0378a2cdb68556af6eee837613016d7ea936f3931f25f8b3ae351d5e9d00be665676e2400408b5db9892d95421e7f1a",

	"13b9d549cd136ec7bf9e9810ef2cdcbf13b9d549cd136ec7",
	"a82c1b1057badcc8",
	"1fff1563bc1645b55cb23ea34a0049dfc06607150614b621dedcb07f20433402a2d869c95ac4a070c7a3da838c928a385f899c5d21ecb58f4e5cbdad98d39b8c",
	"75f804d4a2c542a31703e23df26cc38861a0729090e6eae5672c1db8c0b09fba9b125bbca7d6c7d330b3859e6725c6d26de21c4e3af7f5ea94df3cde2349ce37",

	"20320dfdad579bb57c6e4acd769dbadf20320dfdad579bb5",
	"879201b5857ccdea",
	"0431283cc8bb4dc7750a9d5c68578486932091632a12d0a79f2c54e3d122130881fff727050f317a40fcd1a8d13793458b99fc98254ba6a233e3d95b55cf5a3faff78809999ea4bf",
	"85d17840eb2af5fc727027336bfd71a2b31bd14a1d9eb64f8a08bfc4f56eaa9ca7654a5ae698287869cc27324813730de4f1384e0b8cfbc472ff5470e3c5e4bd8ceb23dc2d91988c",

	"23abb073a2df34cb3d1fdce6b092582c23abb073a2df34cb",
	"7d7fbf19e8562d32",
	"31e718fd95e6d7ca4f94763191add2674ab07c909d88c486916c16d60a048a0cf8cdb631cebec791362cd0c202eb61e166b65c1f65d0047c8aec57d3d84b9e17032442dce148e1191b06a12c284cc41e",
	"c9a3f75ab6a7cd08a7fd53ca540aafe731d257ee1c379fadcc4cc1a06e7c12bddbeb7562c436d1da849ed072629e82a97b56d9becc25ff4f16f21c5f2a01911604f0b5c49df96cb641faee662ca8aa68",

	"b5cb1504802326c73df186e3e352a20de643b0d63ee30e37",
	"43f791134c5647ba",
	"dcc153cef81d6f24",
	"92538bd8af18d3ba",

	"a49d7564199e97cb529d2c9d97bf2f98d35edf57ba1f7358",
	"c2e999cb6249023c",
	"c689aee38a301bb316da75db36f110b5",
	"e9afaba5ec75ea1bbe65506655bb4ecb",

	"1a5d4c0825072a15a8ad9dfdaeda8c048adffb85bc4fced0",
	"7fcfa736f7548b6f",
	"983c3edacd939406010e1bc6ff9e12320ac5008117fa8f84",
	"d84fa24f38cf451ca2c9adc960120bd8ff9871584fe31cee",

	"d98aadc76d4a3716158c32866efbb9ce834af2297379a49d",
	"3c5220327c502b44",
	"6174079dda53ca723ebf00a66837f8d5ce648c08acaa5ee45ffe62210ef79d3e",
	"f5bd4d600bed77bec78409e3530ebda1d815506ed53103015b87e371ae000958",

	"ef6d3e54266d978ffb0b8ce6689d803e2cd34cc802fd0252",
	"38bae5bce06d0ad9",
	"c4f228b537223cd01c0debb5d9d4e12ba71656618d119b2f8f0af29d23efa3a9e43c4c458a1b79a0",
	"9e3289fb18379f55aa4e45a7e0e6df160b33b75f8627ad0954f8fdcb78cee55a4664caeda1000fe5",

	"625bc19b19df83abfb2f5bec9d4f2062017525a75bc26e70",
	"bd0cff364ff69a91",
	"8152d2ab876c3c8201403a5a406d3feaf27319dbea6ad01e24f4d18203704b86de70da6bbb6d638e5aba3ff576b79b28",
	"706fe7a973fac40e25b2b4499ce527078944c70e976d017b6af86a3a7a6b52943a72ba18a58000d2b61fdc3bfef2bc4a",

	"b6383176046e6880a1023bf45768b5bf5119022fe054bfe5",
	"ec13ca541c43401e",
	"cd5a886e9af011346c4dba36a424f96a78a1ddf28aaa4188bf65451f4efaffc7179a6dd237c0ae35d9b672314e5cb032612597f7e462c6f3",
	"b030f976f46277ee211c4a324d5c87555d1084513a1223d3b84416b52bbc28f4b77f3a9d8d0d91dc37d3dbe8af8be98f74674b02f9a38527",

	"3d8cf273d343b9aedccddacb91ad86206737adc86b4a49a7",
	"bb3a9a0c71c62ef0",
	"1fde3991c32ce220b5b6666a9234f2fd7bd24b921829fd9cdc6eb4218be9eac9faa9c2351777349128086b6d58776bc86ff2f76ee1b3b2850a318462b8983fa1",
	"422ce705a46bb52ad928dab6c863166d617c6fc24003633120d91918314bbf464cea7345c3c35f2042f2d6929735d74d7728f22fea618a0b9cf5b1281acb13fb",

	"fbceb5cb646b925be0b92f7f6b493d5e5b16e9159732732a",
	"2e17b3c7025ae86b",
	"4c309bc8e1e464fdd2a2b8978645d668d455f7526bd8d7b6716a722f6a900b815c4a73cc30e788065c1dfca7bf5958a6cc5440a5ebe7f8691c20278cde95db764ff8ce8994ece89c",
	"c02129bdf4bbbd75e71605a00b12c80db6b4e05308e916615011f09147ed915dd1bc67f27f9e027e4e13df36b55464a31c11b4d1fe3d855d89df492e1a7201b995c1ba16a8dbabee",

	"9b162a0df8ad9b61c88676e3d586434570b902f12a2046e0",
	"ebd6fefe029ad54b",
	"f4c1c918e77355c8156f0fd778da52bff121ae5f2f44eaf4d2754946d0e10d1f18ce3a0176e69c18b7d20b6e0d0bee5eb5edfe4bd60e4d92adcd86bce72e76f94ee5cbcaa8b01cfddcea2ade575e66ac",
	"1ff3c8709f403a8eff291aedf50c010df5c5ff64a8b205f1fce68564798897a390db16ee0d053856b75898009731da290fcc119dad987277aacef694872e880c4bb41471063fae05c89f25e4bd0cad6a",

	NULL
};

static void
xor_buf(unsigned char *dst, const unsigned char *src, size_t len)
{
	while (len -- > 0) {
		*dst ++ ^= *src ++;
	}
}

static void
monte_carlo_DES_encrypt(const br_block_cbcenc_class *ve)
{
	unsigned char k1[8], k2[8], k3[8];
	unsigned char buf[8];
	unsigned char cipher[8];
	int i, j;
	br_des_gen_cbcenc_keys v_ec;
	void *ec;

	ec = &v_ec;
	hextobin(k1, "9ec2372c86379df4");
	hextobin(k2, "ad7ac4464f73805d");
	hextobin(k3, "20c4f87564527c91");
	hextobin(buf, "b624d6bd41783ab1");
	hextobin(cipher, "eafd97b190b167fe");
	for (i = 0; i < 400; i ++) {
		unsigned char key[24];

		memcpy(key, k1, 8);
		memcpy(key + 8, k2, 8);
		memcpy(key + 16, k3, 8);
		ve->init(ec, key, sizeof key);
		for (j = 0; j < 10000; j ++) {
			unsigned char iv[8];

			memset(iv, 0, sizeof iv);
			ve->run(ec, iv, buf, sizeof buf);
			switch (j) {
			case 9997: xor_buf(k3, buf, 8); break;
			case 9998: xor_buf(k2, buf, 8); break;
			case 9999: xor_buf(k1, buf, 8); break;
			}
		}
		printf(".");
		fflush(stdout);
	}
	printf(" ");
	fflush(stdout);
	check_equals("MC DES encrypt", buf, cipher, sizeof buf);
}

static void
monte_carlo_DES_decrypt(const br_block_cbcdec_class *vd)
{
	unsigned char k1[8], k2[8], k3[8];
	unsigned char buf[8];
	unsigned char plain[8];
	int i, j;
	br_des_gen_cbcdec_keys v_dc;
	void *dc;

	dc = &v_dc;
	hextobin(k1, "79b63486e0ce37e0");
	hextobin(k2, "08e65231abae3710");
	hextobin(k3, "1f5eb69e925ef185");
	hextobin(buf, "2783aa729432fe96");
	hextobin(plain, "44937ca532cdbf98");
	for (i = 0; i < 400; i ++) {
		unsigned char key[24];

		memcpy(key, k1, 8);
		memcpy(key + 8, k2, 8);
		memcpy(key + 16, k3, 8);
		vd->init(dc, key, sizeof key);
		for (j = 0; j < 10000; j ++) {
			unsigned char iv[8];

			memset(iv, 0, sizeof iv);
			vd->run(dc, iv, buf, sizeof buf);
			switch (j) {
			case 9997: xor_buf(k3, buf, 8); break;
			case 9998: xor_buf(k2, buf, 8); break;
			case 9999: xor_buf(k1, buf, 8); break;
			}
		}
		printf(".");
		fflush(stdout);
	}
	printf(" ");
	fflush(stdout);
	check_equals("MC DES decrypt", buf, plain, sizeof buf);
}

static void
test_DES_generic(char *name,
	const br_block_cbcenc_class *ve,
	const br_block_cbcdec_class *vd,
	int with_MC, int with_CBC)
{
	size_t u;

	printf("Test %s: ", name);
	fflush(stdout);

	if (ve->block_size != 8 || vd->block_size != 8) {
		fprintf(stderr, "%s failed: wrong block size\n", name);
		exit(EXIT_FAILURE);
	}

	for (u = 0; KAT_DES[u]; u += 3) {
		unsigned char key[24];
		unsigned char plain[8];
		unsigned char cipher[8];
		unsigned char buf[8];
		unsigned char iv[8];
		size_t key_len;
		br_des_gen_cbcenc_keys v_ec;
		br_des_gen_cbcdec_keys v_dc;
		const br_block_cbcenc_class **ec;
		const br_block_cbcdec_class **dc;

		ec = &v_ec.vtable;
		dc = &v_dc.vtable;
		key_len = hextobin(key, KAT_DES[u]);
		hextobin(plain, KAT_DES[u + 1]);
		hextobin(cipher, KAT_DES[u + 2]);
		ve->init(ec, key, key_len);
		memcpy(buf, plain, sizeof plain);
		memset(iv, 0, sizeof iv);
		ve->run(ec, iv, buf, sizeof buf);
		check_equals("KAT DES encrypt", buf, cipher, sizeof cipher);
		vd->init(dc, key, key_len);
		memset(iv, 0, sizeof iv);
		vd->run(dc, iv, buf, sizeof buf);
		check_equals("KAT DES decrypt", buf, plain, sizeof plain);

		if (key_len == 8) {
			memcpy(key + 8, key, 8);
			memcpy(key + 16, key, 8);
			ve->init(ec, key, 24);
			memcpy(buf, plain, sizeof plain);
			memset(iv, 0, sizeof iv);
			ve->run(ec, iv, buf, sizeof buf);
			check_equals("KAT DES->3 encrypt",
				buf, cipher, sizeof cipher);
			vd->init(dc, key, 24);
			memset(iv, 0, sizeof iv);
			vd->run(dc, iv, buf, sizeof buf);
			check_equals("KAT DES->3 decrypt",
				buf, plain, sizeof plain);
		}
	}

	if (with_CBC) {
		for (u = 0; KAT_DES_CBC[u]; u += 4) {
			unsigned char key[24];
			unsigned char ivref[8];
			unsigned char plain[200];
			unsigned char cipher[200];
			unsigned char buf[200];
			unsigned char iv[8];
			size_t key_len, data_len, v;
			br_des_gen_cbcenc_keys v_ec;
			br_des_gen_cbcdec_keys v_dc;
			const br_block_cbcenc_class **ec;
			const br_block_cbcdec_class **dc;

			ec = &v_ec.vtable;
			dc = &v_dc.vtable;
			key_len = hextobin(key, KAT_DES_CBC[u]);
			hextobin(ivref, KAT_DES_CBC[u + 1]);
			data_len = hextobin(plain, KAT_DES_CBC[u + 2]);
			hextobin(cipher, KAT_DES_CBC[u + 3]);
			ve->init(ec, key, key_len);

			memcpy(buf, plain, data_len);
			memcpy(iv, ivref, 8);
			ve->run(ec, iv, buf, data_len);
			check_equals("KAT CBC DES encrypt",
				buf, cipher, data_len);
			vd->init(dc, key, key_len);
			memcpy(iv, ivref, 8);
			vd->run(dc, iv, buf, data_len);
			check_equals("KAT CBC DES decrypt",
				buf, plain, data_len);

			memcpy(buf, plain, data_len);
			memcpy(iv, ivref, 8);
			for (v = 0; v < data_len; v += 8) {
				ve->run(ec, iv, buf + v, 8);
			}
			check_equals("KAT CBC DES encrypt (2)",
				buf, cipher, data_len);
			memcpy(iv, ivref, 8);
			for (v = 0; v < data_len; v += 8) {
				vd->run(dc, iv, buf + v, 8);
			}
			check_equals("KAT CBC DES decrypt (2)",
				buf, plain, data_len);
		}
	}

	if (with_MC) {
		monte_carlo_DES_encrypt(ve);
		monte_carlo_DES_decrypt(vd);
	}

	printf("done.\n");
	fflush(stdout);
}

static void
test_DES_tab(void)
{
	test_DES_generic("DES_tab",
		&br_des_tab_cbcenc_vtable,
		&br_des_tab_cbcdec_vtable,
		1, 1);
}

static void
test_DES_ct(void)
{
	test_DES_generic("DES_ct",
		&br_des_ct_cbcenc_vtable,
		&br_des_ct_cbcdec_vtable,
		1, 1);
}

static const struct {
	const char *skey;
	const char *snonce;
	uint32_t counter;
	const char *splain;
	const char *scipher;
} KAT_CHACHA20[] = {
	{
		"0000000000000000000000000000000000000000000000000000000000000000",
		"000000000000000000000000",
		0,
		"00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
		"76b8e0ada0f13d90405d6ae55386bd28bdd219b8a08ded1aa836efcc8b770dc7da41597c5157488d7724e03fb8d84a376a43b8f41518a11cc387b669b2ee6586"
	},
	{
		"0000000000000000000000000000000000000000000000000000000000000001",
		"000000000000000000000002",
		1,
		"416e79207375626d697373696f6e20746f20746865204945544620696e74656e6465642062792074686520436f6e7472696275746f7220666f72207075626c69636174696f6e20617320616c6c206f722070617274206f6620616e204945544620496e7465726e65742d4472616674206f722052464320616e6420616e792073746174656d656e74206d6164652077697468696e2074686520636f6e74657874206f6620616e204945544620616374697669747920697320636f6e7369646572656420616e20224945544620436f6e747269627574696f6e222e20537563682073746174656d656e747320696e636c756465206f72616c2073746174656d656e747320696e20494554462073657373696f6e732c2061732077656c6c206173207772697474656e20616e6420656c656374726f6e696320636f6d6d756e69636174696f6e73206d61646520617420616e792074696d65206f7220706c6163652c207768696368206172652061646472657373656420746f",
		"a3fbf07df3fa2fde4f376ca23e82737041605d9f4f4f57bd8cff2c1d4b7955ec2a97948bd3722915c8f3d337f7d370050e9e96d647b7c39f56e031ca5eb6250d4042e02785ececfa4b4bb5e8ead0440e20b6e8db09d881a7c6132f420e52795042bdfa7773d8a9051447b3291ce1411c680465552aa6c405b7764d5e87bea85ad00f8449ed8f72d0d662ab052691ca66424bc86d2df80ea41f43abf937d3259dc4b2d0dfb48a6c9139ddd7f76966e928e635553ba76c5c879d7b35d49eb2e62b0871cdac638939e25e8a1e0ef9d5280fa8ca328b351c3c765989cbcf3daa8b6ccc3aaf9f3979c92b3720fc88dc95ed84a1be059c6499b9fda236e7e818b04b0bc39c1e876b193bfe5569753f88128cc08aaa9b63d1a16f80ef2554d7189c411f5869ca52c5b83fa36ff216b9c1d30062bebcfd2dc5bce0911934fda79a86f6e698ced759c3ff9b6477338f3da4f9cd8514ea9982ccafb341b2384dd902f3d1ab7ac61dd29c6f21ba5b862f3730e37cfdc4fd806c22f221"
	},
	{
		"1c9240a5eb55d38af333888604f6b5f0473917c1402b80099dca5cbc207075c0",
		"000000000000000000000002",
		42,
		"2754776173206272696c6c69672c20616e642074686520736c6974687920746f7665730a446964206779726520616e642067696d626c6520696e2074686520776162653a0a416c6c206d696d737920776572652074686520626f726f676f7665732c0a416e6420746865206d6f6d65207261746873206f757467726162652e",
		"62e6347f95ed87a45ffae7426f27a1df5fb69110044c0d73118effa95b01e5cf166d3df2d721caf9b21e5fb14c616871fd84c54f9d65b283196c7fe4f60553ebf39c6402c42234e32a356b3e764312a61a5532055716ead6962568f87d3f3f7704c6a8d1bcd1bf4d50d6154b6da731b187b58dfd728afa36757a797ac188d1"
	},
	{ 0, 0, 0, 0, 0 }
};

static void
test_ChaCha20_generic(const char *name, br_chacha20_run cr)
{
	size_t u;

	printf("Test %s: ", name);
	fflush(stdout);
	if (cr == 0) {
		printf("UNAVAILABLE\n");
		return;
	}

	for (u = 0; KAT_CHACHA20[u].skey; u ++) {
		unsigned char key[32], nonce[12], plain[400], cipher[400];
		uint32_t cc;
		size_t v, len;

		hextobin(key, KAT_CHACHA20[u].skey);
		hextobin(nonce, KAT_CHACHA20[u].snonce);
		cc = KAT_CHACHA20[u].counter;
		len = hextobin(plain, KAT_CHACHA20[u].splain);
		hextobin(cipher, KAT_CHACHA20[u].scipher);

		for (v = 0; v < len; v ++) {
			unsigned char tmp[400];
			size_t w;
			uint32_t cc2;

			memset(tmp, 0, sizeof tmp);
			memcpy(tmp, plain, v);
			if (cr(key, nonce, cc, tmp, v)
				!= cc + (uint32_t)((v + 63) >> 6))
			{
				fprintf(stderr, "ChaCha20: wrong counter\n");
				exit(EXIT_FAILURE);
			}
			if (memcmp(tmp, cipher, v) != 0) {
				fprintf(stderr, "ChaCha20 KAT fail (1)\n");
				exit(EXIT_FAILURE);
			}
			for (w = v; w < sizeof tmp; w ++) {
				if (tmp[w] != 0) {
					fprintf(stderr, "ChaCha20: overrun\n");
					exit(EXIT_FAILURE);
				}
			}
			for (w = 0, cc2 = cc; w < v; w += 64, cc2 ++) {
				size_t x;

				x = v - w;
				if (x > 64) {
					x = 64;
				}
				if (cr(key, nonce, cc2, tmp + w, x)
					!= (cc2 + 1))
				{
					fprintf(stderr, "ChaCha20:"
						" wrong counter (2)\n");
					exit(EXIT_FAILURE);
				}
			}
			if (memcmp(tmp, plain, v) != 0) {
				fprintf(stderr, "ChaCha20 KAT fail (2)\n");
				exit(EXIT_FAILURE);
			}
		}

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
	fflush(stdout);
}

static void
test_ChaCha20_ct(void)
{
	test_ChaCha20_generic("ChaCha20_ct", &br_chacha20_ct_run);
}

static void
test_ChaCha20_sse2(void)
{
	test_ChaCha20_generic("ChaCha20_sse2", br_chacha20_sse2_get());
}

static const struct {
	const char *splain;
	const char *saad;
	const char *skey;
	const char *snonce;
	const char *scipher;
	const char *stag;
} KAT_POLY1305[] = {
	{
		"4c616469657320616e642047656e746c656d656e206f662074686520636c617373206f66202739393a204966204920636f756c64206f6666657220796f75206f6e6c79206f6e652074697020666f7220746865206675747572652c2073756e73637265656e20776f756c642062652069742e",
		"50515253c0c1c2c3c4c5c6c7",
		"808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f",
		"070000004041424344454647",
		"d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116",
		"1ae10b594f09e26a7e902ecbd0600691"
	},
	{ 0, 0, 0, 0, 0, 0 }
};

static void
test_Poly1305_inner(const char *name, br_poly1305_run ipoly,
	br_poly1305_run iref)
{
	size_t u;
	br_hmac_drbg_context rng;

	printf("Test %s: ", name);
	fflush(stdout);

	for (u = 0; KAT_POLY1305[u].skey; u ++) {
		unsigned char key[32], nonce[12], plain[400], cipher[400];
		unsigned char aad[400], tag[16], data[400], tmp[16];
		size_t len, aad_len;

		len = hextobin(plain, KAT_POLY1305[u].splain);
		aad_len = hextobin(aad, KAT_POLY1305[u].saad);
		hextobin(key, KAT_POLY1305[u].skey);
		hextobin(nonce, KAT_POLY1305[u].snonce);
		hextobin(cipher, KAT_POLY1305[u].scipher);
		hextobin(tag, KAT_POLY1305[u].stag);

		memcpy(data, plain, len);
		ipoly(key, nonce, data, len,
			aad, aad_len, tmp, br_chacha20_ct_run, 1);
		check_equals("ChaCha20+Poly1305 KAT (1)", data, cipher, len);
		check_equals("ChaCha20+Poly1305 KAT (2)", tmp, tag, 16);
		ipoly(key, nonce, data, len,
			aad, aad_len, tmp, br_chacha20_ct_run, 0);
		check_equals("ChaCha20+Poly1305 KAT (3)", data, plain, len);
		check_equals("ChaCha20+Poly1305 KAT (4)", tmp, tag, 16);

		printf(".");
		fflush(stdout);
	}

	printf(" ");
	fflush(stdout);

	/*
	 * We compare the "ipoly" and "iref" implementations together on
	 * a bunch of pseudo-random messages.
	 */
	br_hmac_drbg_init(&rng, &br_sha256_vtable, "seed for Poly1305", 17);
	for (u = 0; u < 100; u ++) {
		unsigned char plain[100], aad[100], tmp[100];
		unsigned char key[32], iv[12], tag1[16], tag2[16];

		br_hmac_drbg_generate(&rng, key, sizeof key);
		br_hmac_drbg_generate(&rng, iv, sizeof iv);
		br_hmac_drbg_generate(&rng, plain, u);
		br_hmac_drbg_generate(&rng, aad, u);
		memcpy(tmp, plain, u);
		memset(tmp + u, 0xFF, (sizeof tmp) - u);
		ipoly(key, iv, tmp, u, aad, u, tag1,
			&br_chacha20_ct_run, 1);
		memset(tmp + u, 0x00, (sizeof tmp) - u);
		iref(key, iv, tmp, u, aad, u, tag2,
			&br_chacha20_ct_run, 0);
		if (memcmp(tmp, plain, u) != 0) {
			fprintf(stderr, "cross enc/dec failed\n");
			exit(EXIT_FAILURE);
		}
		if (memcmp(tag1, tag2, sizeof tag1) != 0) {
			fprintf(stderr, "cross MAC failed\n");
			exit(EXIT_FAILURE);
		}
		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
	fflush(stdout);
}

static void
test_Poly1305_ctmul(void)
{
	test_Poly1305_inner("Poly1305_ctmul", &br_poly1305_ctmul_run,
		&br_poly1305_i15_run);
}

static void
test_Poly1305_ctmul32(void)
{
	test_Poly1305_inner("Poly1305_ctmul32", &br_poly1305_ctmul32_run,
		&br_poly1305_i15_run);
}

static void
test_Poly1305_i15(void)
{
	test_Poly1305_inner("Poly1305_i15", &br_poly1305_i15_run,
		&br_poly1305_ctmul_run);
}

static void
test_Poly1305_ctmulq(void)
{
	br_poly1305_run bp;

	bp = br_poly1305_ctmulq_get();
	if (bp == 0) {
		printf("Test Poly1305_ctmulq: UNAVAILABLE\n");
	} else {
		test_Poly1305_inner("Poly1305_ctmulq", bp,
			&br_poly1305_ctmul_run);
	}
}

/*
 * A 1024-bit RSA key, generated with OpenSSL.
 */
static const unsigned char RSA_N[] = {
	0xBF, 0xB4, 0xA6, 0x2E, 0x87, 0x3F, 0x9C, 0x8D,
	0xA0, 0xC4, 0x2E, 0x7B, 0x59, 0x36, 0x0F, 0xB0,
	0xFF, 0xE1, 0x25, 0x49, 0xE5, 0xE6, 0x36, 0xB0,
	0x48, 0xC2, 0x08, 0x6B, 0x77, 0xA7, 0xC0, 0x51,
	0x66, 0x35, 0x06, 0xA9, 0x59, 0xDF, 0x17, 0x7F,
	0x15, 0xF6, 0xB4, 0xE5, 0x44, 0xEE, 0x72, 0x3C,
	0x53, 0x11, 0x52, 0xC9, 0xC9, 0x61, 0x4F, 0x92,
	0x33, 0x64, 0x70, 0x43, 0x07, 0xF1, 0x3F, 0x7F,
	0x15, 0xAC, 0xF0, 0xC1, 0x54, 0x7D, 0x55, 0xC0,
	0x29, 0xDC, 0x9E, 0xCC, 0xE4, 0x1D, 0x11, 0x72,
	0x45, 0xF4, 0xD2, 0x70, 0xFC, 0x34, 0xB2, 0x1F,
	0xF3, 0xAD, 0x6A, 0xF0, 0xE5, 0x56, 0x11, 0xF8,
	0x0C, 0x3A, 0x8B, 0x04, 0x46, 0x7C, 0x77, 0xD9,
	0x41, 0x1F, 0x40, 0xBE, 0x93, 0x80, 0x9D, 0x23,
	0x75, 0x80, 0x12, 0x26, 0x5A, 0x72, 0x1C, 0xDD,
	0x47, 0xB3, 0x2A, 0x33, 0xD8, 0x19, 0x61, 0xE3
};
static const unsigned char RSA_E[] = {
	0x01, 0x00, 0x01
};
/* unused
static const unsigned char RSA_D[] = {
	0xAE, 0x56, 0x0B, 0x56, 0x7E, 0xDA, 0x83, 0x75,
	0x6C, 0xC1, 0x5C, 0x00, 0x02, 0x96, 0x1E, 0x58,
	0xF9, 0xA9, 0xF7, 0x2E, 0x27, 0xEB, 0x5E, 0xCA,
	0x9B, 0xB0, 0x10, 0xD6, 0x22, 0x7F, 0xA4, 0x6E,
	0xA2, 0x03, 0x10, 0xE6, 0xCB, 0x7B, 0x0D, 0x34,
	0x1E, 0x76, 0x37, 0xF5, 0xD3, 0xE5, 0x00, 0x70,
	0x09, 0x9E, 0xD4, 0x69, 0xFB, 0x40, 0x0A, 0x8B,
	0xCB, 0x3E, 0xC8, 0xB4, 0xBC, 0xB1, 0x50, 0xEA,
	0x9D, 0xD9, 0x89, 0x8A, 0x98, 0x40, 0x79, 0xD1,
	0x07, 0x66, 0xA7, 0x90, 0x63, 0x82, 0xB1, 0xE0,
	0x24, 0xD0, 0x89, 0x6A, 0xEC, 0xC5, 0xF3, 0x21,
	0x7D, 0xB8, 0xA5, 0x45, 0x3A, 0x3B, 0x34, 0x42,
	0xC2, 0x82, 0x3C, 0x8D, 0xFA, 0x5D, 0xA0, 0xA8,
	0x24, 0xC8, 0x40, 0x22, 0x19, 0xCB, 0xB5, 0x85,
	0x67, 0x69, 0x60, 0xE4, 0xD0, 0x7E, 0xA3, 0x3B,
	0xF7, 0x70, 0x50, 0xC9, 0x5C, 0x97, 0x29, 0x49
};
*/
static const unsigned char RSA_P[] = {
	0xF2, 0xE7, 0x6F, 0x66, 0x2E, 0xC4, 0x03, 0xD4,
	0x89, 0x24, 0xCC, 0xE1, 0xCD, 0x3F, 0x01, 0x82,
	0xC1, 0xFB, 0xAF, 0x44, 0xFA, 0xCC, 0x0E, 0xAA,
	0x9D, 0x74, 0xA9, 0x65, 0xEF, 0xED, 0x4C, 0x87,
	0xF0, 0xB3, 0xC6, 0xEA, 0x61, 0x85, 0xDE, 0x4E,
	0x66, 0xB2, 0x5A, 0x9F, 0x7A, 0x41, 0xC5, 0x66,
	0x57, 0xDF, 0x88, 0xF0, 0xB5, 0xF2, 0xC7, 0x7E,
	0xE6, 0x55, 0x21, 0x96, 0x83, 0xD8, 0xAB, 0x57
};
static const unsigned char RSA_Q[] = {
	0xCA, 0x0A, 0x92, 0xBF, 0x58, 0xB0, 0x2E, 0xF6,
	0x66, 0x50, 0xB1, 0x48, 0x29, 0x42, 0x86, 0x6C,
	0x98, 0x06, 0x7E, 0xB8, 0xB5, 0x4F, 0xFB, 0xC4,
	0xF3, 0xC3, 0x36, 0x91, 0x07, 0xB6, 0xDB, 0xE9,
	0x56, 0x3C, 0x51, 0x7D, 0xB5, 0xEC, 0x0A, 0xA9,
	0x7C, 0x66, 0xF9, 0xD8, 0x25, 0xDE, 0xD2, 0x94,
	0x5A, 0x58, 0xF1, 0x93, 0xE4, 0xF0, 0x5F, 0x27,
	0xBD, 0x83, 0xC7, 0xCA, 0x48, 0x6A, 0xB2, 0x55
};
static const unsigned char RSA_DP[] = {
	0xAF, 0x97, 0xBE, 0x60, 0x0F, 0xCE, 0x83, 0x36,
	0x51, 0x2D, 0xD9, 0x2E, 0x22, 0x41, 0x39, 0xC6,
	0x5C, 0x94, 0xA4, 0xCF, 0x28, 0xBD, 0xFA, 0x9C,
	0x3B, 0xD6, 0xE9, 0xDE, 0x56, 0xE3, 0x24, 0x3F,
	0xE1, 0x31, 0x14, 0xCA, 0xBA, 0x55, 0x1B, 0xAF,
	0x71, 0x6D, 0xDD, 0x35, 0x0C, 0x1C, 0x1F, 0xA7,
	0x2C, 0x3E, 0xDB, 0xAF, 0xA6, 0xD8, 0x2A, 0x7F,
	0x01, 0xE2, 0xE8, 0xB4, 0xF5, 0xFA, 0xDB, 0x61
};
static const unsigned char RSA_DQ[] = {
	0x29, 0xC0, 0x4B, 0x98, 0xFD, 0x13, 0xD3, 0x70,
	0x99, 0xAE, 0x1D, 0x24, 0x83, 0x5A, 0x3A, 0xFB,
	0x1F, 0xE3, 0x5F, 0xB6, 0x7D, 0xC9, 0x5C, 0x86,
	0xD3, 0xB4, 0xC8, 0x86, 0xE9, 0xE8, 0x30, 0xC3,
	0xA4, 0x4D, 0x6C, 0xAD, 0xA4, 0xB5, 0x75, 0x72,
	0x96, 0xC1, 0x94, 0xE9, 0xC4, 0xD1, 0xAA, 0x04,
	0x7C, 0x33, 0x1B, 0x20, 0xEB, 0xD3, 0x7C, 0x66,
	0x72, 0xF4, 0x53, 0x8A, 0x0A, 0xB2, 0xF9, 0xCD
};
static const unsigned char RSA_IQ[] = {
	0xE8, 0xEB, 0x04, 0x79, 0xA5, 0xC1, 0x79, 0xDE,
	0xD5, 0x49, 0xA1, 0x0B, 0x48, 0xB9, 0x0E, 0x55,
	0x74, 0x2C, 0x54, 0xEE, 0xA8, 0xB0, 0x01, 0xC2,
	0xD2, 0x3C, 0x3E, 0x47, 0x3A, 0x7C, 0xC8, 0x3D,
	0x2E, 0x33, 0x54, 0x4D, 0x40, 0x29, 0x41, 0x74,
	0xBA, 0xE1, 0x93, 0x09, 0xEC, 0xE0, 0x1B, 0x4D,
	0x1F, 0x2A, 0xCA, 0x4A, 0x0B, 0x5F, 0xE6, 0xBE,
	0x59, 0x0A, 0xC4, 0xC9, 0xD9, 0x82, 0xAC, 0xE1
};

static const br_rsa_public_key RSA_PK = {
	(void *)RSA_N, sizeof RSA_N,
	(void *)RSA_E, sizeof RSA_E
};

static const br_rsa_private_key RSA_SK = {
	1024,
	(void *)RSA_P, sizeof RSA_P,
	(void *)RSA_Q, sizeof RSA_Q,
	(void *)RSA_DP, sizeof RSA_DP,
	(void *)RSA_DQ, sizeof RSA_DQ,
	(void *)RSA_IQ, sizeof RSA_IQ
};

/*
 * A 2048-bit RSA key, generated with OpenSSL.
 */
static const unsigned char RSA2048_N[] = {
	0xEA, 0xB1, 0xB0, 0x87, 0x60, 0xE2, 0x69, 0xF5,
	0xC9, 0x3F, 0xCB, 0x4F, 0x9E, 0x7D, 0xD0, 0x56,
	0x54, 0x8F, 0xF5, 0x59, 0x97, 0x04, 0x3F, 0x30,
	0xE1, 0xFB, 0x7B, 0xF5, 0xA0, 0xEB, 0xA7, 0x7B,
	0x29, 0x96, 0x7B, 0x32, 0x48, 0x48, 0xA4, 0x99,
	0x90, 0x92, 0x48, 0xFB, 0xDC, 0xEC, 0x8A, 0x3B,
	0xE0, 0x57, 0x6E, 0xED, 0x1C, 0x5B, 0x78, 0xCF,
	0x07, 0x41, 0x96, 0x4C, 0x2F, 0xA2, 0xD1, 0xC8,
	0xA0, 0x5F, 0xFC, 0x2A, 0x5B, 0x3F, 0xBC, 0xD7,
	0xE6, 0x91, 0xF1, 0x44, 0xD6, 0xD8, 0x41, 0x66,
	0x3E, 0x80, 0xEE, 0x98, 0x73, 0xD5, 0x32, 0x60,
	0x7F, 0xDF, 0xBF, 0xB2, 0x0B, 0xA5, 0xCA, 0x11,
	0x88, 0x1A, 0x0E, 0xA1, 0x61, 0x4C, 0x5A, 0x70,
	0xCE, 0x12, 0xC0, 0x61, 0xF5, 0x50, 0x0E, 0xF6,
	0xC1, 0xC2, 0x88, 0x8B, 0xE5, 0xCE, 0xAE, 0x90,
	0x65, 0x23, 0xA7, 0xAD, 0xCB, 0x04, 0x17, 0x00,
	0xA2, 0xDB, 0xB0, 0x21, 0x49, 0xDD, 0x3C, 0x2E,
	0x8C, 0x47, 0x27, 0xF2, 0x84, 0x51, 0x63, 0xEB,
	0xF8, 0xAF, 0x63, 0xA7, 0x89, 0xE1, 0xF0, 0x2F,
	0xF9, 0x9C, 0x0A, 0x8A, 0xBC, 0x57, 0x05, 0xB0,
	0xEF, 0xA0, 0xDA, 0x67, 0x70, 0xAF, 0x3F, 0xA4,
	0x92, 0xFC, 0x4A, 0xAC, 0xEF, 0x89, 0x41, 0x58,
	0x57, 0x63, 0x0F, 0x6A, 0x89, 0x68, 0x45, 0x4C,
	0x20, 0xF9, 0x7F, 0x50, 0x9D, 0x8C, 0x52, 0xC4,
	0xC1, 0x33, 0xCD, 0x42, 0x35, 0x12, 0xEC, 0x82,
	0xF9, 0xC1, 0xB7, 0x60, 0x7B, 0x52, 0x61, 0xD0,
	0xAE, 0xFD, 0x4B, 0x68, 0xB1, 0x55, 0x0E, 0xAB,
	0x99, 0x24, 0x52, 0x60, 0x8E, 0xDB, 0x90, 0x34,
	0x61, 0xE3, 0x95, 0x7C, 0x34, 0x64, 0x06, 0xCB,
	0x44, 0x17, 0x70, 0x78, 0xC1, 0x1B, 0x87, 0x8F,
	0xCF, 0xB0, 0x7D, 0x93, 0x59, 0x84, 0x49, 0xF5,
	0x55, 0xBB, 0x48, 0xCA, 0xD3, 0x76, 0x1E, 0x7F
};
static const unsigned char RSA2048_E[] = {
	0x01, 0x00, 0x01
};
static const unsigned char RSA2048_P[] = {
	0xF9, 0xA7, 0xB5, 0xC4, 0xE8, 0x52, 0xEC, 0xB1,
	0x33, 0x6A, 0x68, 0x32, 0x63, 0x2D, 0xBA, 0xE5,
	0x61, 0x14, 0x69, 0x82, 0xC8, 0x31, 0x14, 0xD5,
	0xC2, 0x6C, 0x1A, 0xBE, 0xA0, 0x68, 0xA6, 0xC5,
	0xEA, 0x40, 0x59, 0xFB, 0x0A, 0x30, 0x3D, 0xD5,
	0xDD, 0x94, 0xAE, 0x0C, 0x9F, 0xEE, 0x19, 0x0C,
	0xA8, 0xF2, 0x85, 0x27, 0x60, 0xAA, 0xD5, 0x7C,
	0x59, 0x91, 0x1F, 0xAF, 0x5E, 0x00, 0xC8, 0x2D,
	0xCA, 0xB4, 0x70, 0xA1, 0xF8, 0x8C, 0x0A, 0xB3,
	0x08, 0x95, 0x03, 0x9E, 0xA4, 0x6B, 0x9D, 0x55,
	0x47, 0xE0, 0xEC, 0xB3, 0x21, 0x7C, 0xE4, 0x16,
	0x91, 0xE3, 0xD7, 0x1B, 0x3D, 0x81, 0xF1, 0xED,
	0x16, 0xF9, 0x05, 0x0E, 0xA6, 0x9F, 0x37, 0x73,
	0x18, 0x1B, 0x9C, 0x9D, 0x33, 0xAD, 0x25, 0xEF,
	0x3A, 0xC0, 0x4B, 0x34, 0x24, 0xF5, 0xFD, 0x59,
	0xF5, 0x65, 0xE6, 0x92, 0x2A, 0x04, 0x06, 0x3D
};
static const unsigned char RSA2048_Q[] = {
	0xF0, 0xA8, 0xA4, 0x20, 0xDD, 0xF3, 0x99, 0xE6,
	0x1C, 0xB1, 0x21, 0xE8, 0x66, 0x68, 0x48, 0x00,
	0x04, 0xE3, 0x21, 0xA3, 0xE8, 0xC5, 0xFD, 0x85,
	0x6D, 0x2C, 0x98, 0xE3, 0x36, 0x39, 0x3E, 0x80,
	0xB7, 0x36, 0xA5, 0xA9, 0xBB, 0xEB, 0x1E, 0xB8,
	0xEB, 0x44, 0x65, 0xE8, 0x81, 0x7D, 0xE0, 0x87,
	0xC1, 0x08, 0x94, 0xDD, 0x92, 0x40, 0xF4, 0x8B,
	0x3C, 0xB5, 0xC1, 0xAD, 0x9D, 0x4C, 0x14, 0xCD,
	0xD9, 0x2D, 0xB6, 0xE4, 0x99, 0xB3, 0x71, 0x63,
	0x64, 0xE1, 0x31, 0x7E, 0x34, 0x95, 0x96, 0x52,
	0x85, 0x27, 0xBE, 0x40, 0x10, 0x0A, 0x9E, 0x01,
	0x1C, 0xBB, 0xB2, 0x5B, 0x40, 0x85, 0x65, 0x6E,
	0xA0, 0x88, 0x73, 0xF6, 0x22, 0xCC, 0x23, 0x26,
	0x62, 0xAD, 0x92, 0x57, 0x57, 0xF4, 0xD4, 0xDF,
	0xD9, 0x7C, 0xDE, 0xAD, 0xD2, 0x1F, 0x32, 0x29,
	0xBA, 0xE7, 0xE2, 0x32, 0xA1, 0xA0, 0xBF, 0x6B
};
static const unsigned char RSA2048_DP[] = {
	0xB2, 0xF9, 0xD7, 0x66, 0xC5, 0x83, 0x05, 0x6A,
	0x77, 0xC8, 0xB5, 0xD0, 0x41, 0xA7, 0xBC, 0x0F,
	0xCB, 0x4B, 0xFD, 0xE4, 0x23, 0x2E, 0x84, 0x98,
	0x46, 0x1C, 0x88, 0x03, 0xD7, 0x2D, 0x8F, 0x39,
	0xDD, 0x98, 0xAA, 0xA9, 0x3D, 0x01, 0x9E, 0xA2,
	0xDE, 0x8A, 0x43, 0x48, 0x8B, 0xB2, 0xFE, 0xC4,
	0x43, 0xAE, 0x31, 0x65, 0x2C, 0x78, 0xEC, 0x39,
	0x8C, 0x60, 0x6C, 0xCD, 0xA4, 0xDF, 0x7C, 0xA2,
	0xCF, 0x6A, 0x12, 0x41, 0x1B, 0xD5, 0x11, 0xAA,
	0x8D, 0xE1, 0x7E, 0x49, 0xD1, 0xE7, 0xD0, 0x50,
	0x1E, 0x0A, 0x92, 0xC6, 0x4C, 0xA0, 0xA3, 0x47,
	0xC6, 0xE9, 0x07, 0x01, 0xE1, 0x53, 0x72, 0x23,
	0x9D, 0x4F, 0x82, 0x9F, 0xA1, 0x36, 0x0D, 0x63,
	0x76, 0x89, 0xFC, 0xF9, 0xF9, 0xDD, 0x0C, 0x8F,
	0xF7, 0x97, 0x79, 0x92, 0x75, 0x58, 0xE0, 0x7B,
	0x08, 0x61, 0x38, 0x2D, 0xDA, 0xEF, 0x2D, 0xA5
};
static const unsigned char RSA2048_DQ[] = {
	0x8B, 0x69, 0x56, 0x33, 0x08, 0x00, 0x8F, 0x3D,
	0xC3, 0x8F, 0x45, 0x52, 0x48, 0xC8, 0xCE, 0x34,
	0xDC, 0x9F, 0xEB, 0x23, 0xF5, 0xBB, 0x84, 0x62,
	0xDF, 0xDC, 0xBE, 0xF0, 0x98, 0xBF, 0xCE, 0x9A,
	0x68, 0x08, 0x4B, 0x2D, 0xA9, 0x83, 0xC9, 0xF7,
	0x5B, 0xAA, 0xF2, 0xD2, 0x1E, 0xF9, 0x99, 0xB1,
	0x6A, 0xBC, 0x9A, 0xE8, 0x44, 0x4A, 0x46, 0x9F,
	0xC6, 0x5A, 0x90, 0x49, 0x0F, 0xDF, 0x3C, 0x0A,
	0x07, 0x6E, 0xB9, 0x0D, 0x72, 0x90, 0x85, 0xF6,
	0x0B, 0x41, 0x7D, 0x17, 0x5C, 0x44, 0xEF, 0xA0,
	0xFC, 0x2C, 0x0A, 0xC5, 0x37, 0xC5, 0xBE, 0xC4,
	0x6C, 0x2D, 0xBB, 0x63, 0xAB, 0x5B, 0xDB, 0x67,
	0x9B, 0xAD, 0x90, 0x67, 0x9C, 0xBE, 0xDE, 0xF9,
	0xE4, 0x9E, 0x22, 0x31, 0x60, 0xED, 0x9E, 0xC7,
	0xD2, 0x48, 0xC9, 0x02, 0xAE, 0xBF, 0x8D, 0xA2,
	0xA8, 0xF8, 0x9D, 0x8B, 0xB1, 0x1F, 0xDA, 0xE3
};
static const unsigned char RSA2048_IQ[] = {
	0xB5, 0x48, 0xD4, 0x48, 0x5A, 0x33, 0xCD, 0x13,
	0xFE, 0xC6, 0xF7, 0x01, 0x0A, 0x3E, 0x40, 0xA3,
	0x45, 0x94, 0x6F, 0x85, 0xE4, 0x68, 0x66, 0xEC,
	0x69, 0x6A, 0x3E, 0xE0, 0x62, 0x3F, 0x0C, 0xEF,
	0x21, 0xCC, 0xDA, 0xAD, 0x75, 0x98, 0x12, 0xCA,
	0x9E, 0x31, 0xDD, 0x95, 0x0D, 0xBD, 0x55, 0xEB,
	0x92, 0xF7, 0x9E, 0xBD, 0xFC, 0x28, 0x35, 0x96,
	0x31, 0xDC, 0x53, 0x80, 0xA3, 0x57, 0x89, 0x3C,
	0x4A, 0xEC, 0x40, 0x75, 0x13, 0xAC, 0x4F, 0x36,
	0x3A, 0x86, 0x9A, 0xA6, 0x58, 0xC9, 0xED, 0xCB,
	0xD6, 0xBB, 0xB2, 0xD9, 0xAA, 0x04, 0xC4, 0xE8,
	0x47, 0x3E, 0xBD, 0x14, 0x9B, 0x8F, 0x61, 0x70,
	0x69, 0x66, 0x23, 0x62, 0x18, 0xE3, 0x52, 0x98,
	0xE3, 0x22, 0xE9, 0x6F, 0xDA, 0x28, 0x68, 0x08,
	0xB8, 0xB9, 0x8B, 0x97, 0x8B, 0x77, 0x3F, 0xCA,
	0x9D, 0x9D, 0xBE, 0xD5, 0x2D, 0x3E, 0xC2, 0x11
};

static const br_rsa_public_key RSA2048_PK = {
	(void *)RSA2048_N, sizeof RSA2048_N,
	(void *)RSA2048_E, sizeof RSA2048_E
};

static const br_rsa_private_key RSA2048_SK = {
	2048,
	(void *)RSA2048_P, sizeof RSA2048_P,
	(void *)RSA2048_Q, sizeof RSA2048_Q,
	(void *)RSA2048_DP, sizeof RSA2048_DP,
	(void *)RSA2048_DQ, sizeof RSA2048_DQ,
	(void *)RSA2048_IQ, sizeof RSA2048_IQ
};

/*
 * A 4096-bit RSA key, generated with OpenSSL.
 */
static const unsigned char RSA4096_N[] = {
	0xAA, 0x17, 0x71, 0xBC, 0x92, 0x3E, 0xB5, 0xBD,
	0x3E, 0x64, 0xCF, 0x03, 0x9B, 0x24, 0x65, 0x33,
	0x5F, 0xB4, 0x47, 0x89, 0xE5, 0x63, 0xE4, 0xA0,
	0x5A, 0x51, 0x95, 0x07, 0x73, 0xEE, 0x00, 0xF6,
	0x3E, 0x31, 0x0E, 0xDA, 0x15, 0xC3, 0xAA, 0x21,
	0x6A, 0xCD, 0xFF, 0x46, 0x6B, 0xDF, 0x0A, 0x7F,
	0x8A, 0xC2, 0x25, 0x19, 0x47, 0x44, 0xD8, 0x52,
	0xC1, 0x56, 0x25, 0x6A, 0xE0, 0xD2, 0x61, 0x11,
	0x2C, 0xF7, 0x73, 0x9F, 0x5F, 0x74, 0xAA, 0xDD,
	0xDE, 0xAF, 0x81, 0xF6, 0x0C, 0x1A, 0x3A, 0xF9,
	0xC5, 0x47, 0x82, 0x75, 0x1D, 0x41, 0xF0, 0xB2,
	0xFD, 0xBA, 0xE2, 0xA4, 0xA1, 0xB8, 0x32, 0x48,
	0x06, 0x0D, 0x29, 0x2F, 0x44, 0x14, 0xF5, 0xAC,
	0x54, 0x83, 0xC4, 0xB6, 0x85, 0x85, 0x9B, 0x1C,
	0x05, 0x61, 0x28, 0x62, 0x24, 0xA8, 0xF0, 0xE6,
	0x80, 0xA7, 0x91, 0xE8, 0xC7, 0x8E, 0x52, 0x17,
	0xBE, 0xAF, 0xC6, 0x0A, 0xA3, 0xFB, 0xD1, 0x04,
	0x15, 0x3B, 0x14, 0x35, 0xA5, 0x41, 0xF5, 0x30,
	0xFE, 0xEF, 0x53, 0xA7, 0x89, 0x91, 0x78, 0x30,
	0xBE, 0x3A, 0xB1, 0x4B, 0x2E, 0x4A, 0x0E, 0x25,
	0x1D, 0xCF, 0x51, 0x54, 0x52, 0xF1, 0x88, 0x85,
	0x36, 0x23, 0xDE, 0xBA, 0x66, 0x25, 0x60, 0x8D,
	0x45, 0xD7, 0xD8, 0x10, 0x41, 0x64, 0xC7, 0x4B,
	0xCE, 0x72, 0x13, 0xD7, 0x20, 0xF8, 0x2A, 0x74,
	0xA5, 0x05, 0xF4, 0x5A, 0x90, 0xF4, 0x9C, 0xE7,
	0xC9, 0xCF, 0x1E, 0xD5, 0x9C, 0xAC, 0xE5, 0x00,
	0x83, 0x73, 0x9F, 0xE7, 0xC6, 0x93, 0xC0, 0x06,
	0xA7, 0xB8, 0xF8, 0x46, 0x90, 0xC8, 0x78, 0x27,
	0x2E, 0xCC, 0xC0, 0x2A, 0x20, 0xC5, 0xFC, 0x63,
	0x22, 0xA1, 0xD6, 0x16, 0xAD, 0x9C, 0xD6, 0xFC,
	0x7A, 0x6E, 0x9C, 0x98, 0x51, 0xEE, 0x6B, 0x6D,
	0x8F, 0xEF, 0xCE, 0x7C, 0x5D, 0x16, 0xB0, 0xCE,
	0x9C, 0xEE, 0x92, 0xCF, 0xB7, 0xEB, 0x41, 0x36,
	0x3A, 0x6C, 0xF2, 0x0D, 0x26, 0x11, 0x2F, 0x6C,
	0x27, 0x62, 0xA2, 0xCC, 0x63, 0x53, 0xBD, 0xFC,
	0x9F, 0xBE, 0x9B, 0xBD, 0xE5, 0xA7, 0xDA, 0xD4,
	0xF8, 0xED, 0x5E, 0x59, 0x2D, 0xAC, 0xCD, 0x13,
	0xEB, 0xE5, 0x9E, 0x39, 0x82, 0x8B, 0xFD, 0xA8,
	0xFB, 0xCB, 0x86, 0x27, 0xC7, 0x4B, 0x4C, 0xD0,
	0xBA, 0x12, 0xD0, 0x76, 0x1A, 0xDB, 0x30, 0xC5,
	0xB3, 0x2C, 0x4C, 0xC5, 0x32, 0x03, 0x05, 0x67,
	0x8D, 0xD0, 0x14, 0x37, 0x59, 0x2B, 0xE3, 0x1C,
	0x25, 0x3E, 0xA5, 0xE4, 0xF1, 0x0D, 0x34, 0xBB,
	0xD5, 0xF6, 0x76, 0x45, 0x5B, 0x0F, 0x1E, 0x07,
	0x0A, 0xBA, 0x9D, 0x71, 0x87, 0xDE, 0x45, 0x50,
	0xE5, 0x0F, 0x32, 0xBB, 0x5C, 0x32, 0x2D, 0x40,
	0xCD, 0x19, 0x95, 0x4E, 0xC5, 0x54, 0x3A, 0x9A,
	0x46, 0x9B, 0x85, 0xFE, 0x53, 0xB7, 0xD8, 0x65,
	0x6D, 0x68, 0x0C, 0xBB, 0xE3, 0x3D, 0x8E, 0x64,
	0xBE, 0x27, 0x15, 0xAB, 0x12, 0x20, 0xD9, 0x84,
	0xF5, 0x02, 0xE4, 0xBB, 0xDD, 0xAB, 0x59, 0x51,
	0xF4, 0xE1, 0x79, 0xBE, 0xB8, 0xA3, 0x8E, 0xD1,
	0x1C, 0xB0, 0xFA, 0x48, 0x76, 0xC2, 0x9D, 0x7A,
	0x01, 0xA5, 0xAF, 0x8C, 0xBA, 0xAA, 0x4C, 0x06,
	0x2B, 0x0A, 0x62, 0xF0, 0x79, 0x5B, 0x42, 0xFC,
	0xF8, 0xBF, 0xD4, 0xDD, 0x62, 0x32, 0xE3, 0xCE,
	0xF1, 0x2C, 0xE6, 0xED, 0xA8, 0x8A, 0x41, 0xA3,
	0xC1, 0x1E, 0x07, 0xB6, 0x43, 0x10, 0x80, 0xB7,
	0xF3, 0xD0, 0x53, 0x2A, 0x9A, 0x98, 0xA7, 0x4F,
	0x9E, 0xA3, 0x3E, 0x1B, 0xDA, 0x93, 0x15, 0xF2,
	0xF4, 0x20, 0xA5, 0xA8, 0x4F, 0x8A, 0xBA, 0xED,
	0xB1, 0x17, 0x6C, 0x0F, 0xD9, 0x8F, 0x38, 0x11,
	0xF3, 0xD9, 0x5E, 0x88, 0xA1, 0xA1, 0x82, 0x8B,
	0x30, 0xD7, 0xC6, 0xCE, 0x4E, 0x30, 0x55, 0x57
};
static const unsigned char RSA4096_E[] = {
	0x01, 0x00, 0x01
};
static const unsigned char RSA4096_P[] = {
	0xD3, 0x7A, 0x22, 0xD8, 0x9B, 0xBF, 0x42, 0xB4,
	0x53, 0x04, 0x10, 0x6A, 0x84, 0xFD, 0x7C, 0x1D,
	0xF6, 0xF4, 0x10, 0x65, 0xAA, 0xE5, 0xE1, 0x4E,
	0xB4, 0x37, 0xF7, 0xAC, 0xF7, 0xD3, 0xB2, 0x3B,
	0xFE, 0xE7, 0x63, 0x42, 0xE9, 0xF0, 0x3C, 0xE0,
	0x42, 0xB4, 0xBB, 0x09, 0xD0, 0xB2, 0x7C, 0x70,
	0xA4, 0x11, 0x97, 0x90, 0x01, 0xD0, 0x0E, 0x7B,
	0xAF, 0x7D, 0x30, 0x4E, 0x6B, 0x3A, 0xCC, 0x50,
	0x4E, 0xAF, 0x2F, 0xC3, 0xC2, 0x4F, 0x7E, 0xC5,
	0xB3, 0x76, 0x33, 0xFB, 0xA7, 0xB1, 0x96, 0xA5,
	0x46, 0x41, 0xC6, 0xDA, 0x5A, 0xFD, 0x17, 0x0A,
	0x6A, 0x86, 0x54, 0x83, 0xE1, 0x57, 0xE7, 0xAF,
	0x8C, 0x42, 0xE5, 0x39, 0xF2, 0xC7, 0xFC, 0x4A,
	0x3D, 0x3C, 0x94, 0x89, 0xC2, 0xC6, 0x2D, 0x0A,
	0x5F, 0xD0, 0x21, 0x23, 0x5C, 0xC9, 0xC8, 0x44,
	0x8A, 0x96, 0x72, 0x4D, 0x96, 0xC6, 0x17, 0x0C,
	0x36, 0x43, 0x7F, 0xD8, 0xA0, 0x7A, 0x31, 0x7E,
	0xCE, 0x13, 0xE3, 0x13, 0x2E, 0xE0, 0x91, 0xC2,
	0x61, 0x13, 0x16, 0x8D, 0x99, 0xCB, 0xA9, 0x2C,
	0x4D, 0x9D, 0xDD, 0x1D, 0x03, 0xE7, 0xA7, 0x50,
	0xF4, 0x16, 0x43, 0xB1, 0x7F, 0x99, 0x61, 0x3F,
	0xA5, 0x59, 0x91, 0x16, 0xC3, 0x06, 0x63, 0x59,
	0xE9, 0xDA, 0xB5, 0x06, 0x2E, 0x0C, 0xD9, 0xAB,
	0x93, 0x89, 0x12, 0x82, 0xFB, 0x90, 0xD9, 0x30,
	0x60, 0xF7, 0x35, 0x2D, 0x18, 0x78, 0xEB, 0x2B,
	0xA1, 0x06, 0x67, 0x37, 0xDE, 0x72, 0x20, 0xD2,
	0x80, 0xE5, 0x2C, 0xD7, 0x5E, 0xC7, 0x67, 0x2D,
	0x40, 0xE7, 0x7A, 0xCF, 0x4A, 0x69, 0x9D, 0xA7,
	0x90, 0x9F, 0x3B, 0xDF, 0x07, 0x97, 0x64, 0x69,
	0x06, 0x4F, 0xBA, 0xF4, 0xE5, 0xBD, 0x71, 0x60,
	0x36, 0xB7, 0xA3, 0xDE, 0x76, 0xC5, 0x38, 0xD7,
	0x1D, 0x9A, 0xFC, 0x36, 0x3D, 0x3B, 0xDC, 0xCF
};
static const unsigned char RSA4096_Q[] = {
	0xCD, 0xE6, 0xC6, 0xA6, 0x42, 0x4C, 0x45, 0x65,
	0x8B, 0x85, 0x76, 0xFC, 0x21, 0xB6, 0x57, 0x79,
	0x3C, 0xE4, 0xE3, 0x85, 0x55, 0x2F, 0x59, 0xD3,
	0x3F, 0x74, 0xAF, 0x9F, 0x11, 0x04, 0x10, 0x8B,
	0xF9, 0x5F, 0x4D, 0x25, 0xEE, 0x20, 0xF9, 0x69,
	0x3B, 0x02, 0xB6, 0x43, 0x0D, 0x0C, 0xED, 0x30,
	0x31, 0x57, 0xE7, 0x9A, 0x57, 0x24, 0x6B, 0x4A,
	0x5E, 0xA2, 0xBF, 0xD4, 0x47, 0x7D, 0xFA, 0x78,
	0x51, 0x86, 0x80, 0x68, 0x85, 0x7C, 0x7B, 0x08,
	0x4A, 0x35, 0x24, 0x4F, 0x8B, 0x24, 0x49, 0xF8,
	0x16, 0x06, 0x9C, 0x57, 0x4E, 0x94, 0x4C, 0xBD,
	0x6E, 0x53, 0x52, 0xC9, 0xC1, 0x64, 0x43, 0x22,
	0x1E, 0xDD, 0xEB, 0xAC, 0x90, 0x58, 0xCA, 0xBA,
	0x9C, 0xAC, 0xCF, 0xDD, 0x08, 0x6D, 0xB7, 0x31,
	0xDB, 0x0D, 0x83, 0xE6, 0x50, 0xA6, 0x69, 0xB1,
	0x1C, 0x68, 0x92, 0xB4, 0xB5, 0x76, 0xDE, 0xBD,
	0x4F, 0xA5, 0x30, 0xED, 0x23, 0xFF, 0xE5, 0x80,
	0x21, 0xAB, 0xED, 0xE6, 0xDC, 0x32, 0x3D, 0xF7,
	0x45, 0xB8, 0x19, 0x3D, 0x8E, 0x15, 0x7C, 0xE5,
	0x0D, 0xC8, 0x9B, 0x7D, 0x1F, 0x7C, 0x14, 0x14,
	0x41, 0x09, 0xA7, 0xEB, 0xFB, 0xD9, 0x5F, 0x9A,
	0x94, 0xB6, 0xD5, 0xA0, 0x2C, 0xAF, 0xB5, 0xEF,
	0x5C, 0x5A, 0x8E, 0x34, 0xA1, 0x8F, 0xEB, 0x38,
	0x0F, 0x31, 0x6E, 0x45, 0x21, 0x7A, 0xAA, 0xAF,
	0x6C, 0xB1, 0x8E, 0xB2, 0xB9, 0xD4, 0x1E, 0xEF,
	0x66, 0xD8, 0x4E, 0x3D, 0xF2, 0x0C, 0xF1, 0xBA,
	0xFB, 0xA9, 0x27, 0xD2, 0x45, 0x54, 0x83, 0x4B,
	0x10, 0xC4, 0x9A, 0x32, 0x9C, 0xC7, 0x9A, 0xCF,
	0x4E, 0xBF, 0x07, 0xFC, 0x27, 0xB7, 0x96, 0x1D,
	0xDE, 0x9D, 0xE4, 0x84, 0x68, 0x00, 0x9A, 0x9F,
	0x3D, 0xE6, 0xC7, 0x26, 0x11, 0x48, 0x79, 0xFA,
	0x09, 0x76, 0xC8, 0x25, 0x3A, 0xE4, 0x70, 0xF9
};
static const unsigned char RSA4096_DP[] = {
	0x5C, 0xE3, 0x3E, 0xBF, 0x09, 0xD9, 0xFE, 0x80,
	0x9A, 0x1E, 0x24, 0xDF, 0xC4, 0xBE, 0x5A, 0x70,
	0x06, 0xF2, 0xB8, 0xE9, 0x0F, 0x21, 0x9D, 0xCF,
	0x26, 0x15, 0x97, 0x32, 0x60, 0x40, 0x99, 0xFF,
	0x04, 0x3D, 0xBA, 0x39, 0xBF, 0xEB, 0x87, 0xB1,
	0xB1, 0x5B, 0x14, 0xF4, 0x80, 0xB8, 0x85, 0x34,
	0x2C, 0xBC, 0x95, 0x67, 0xE9, 0x83, 0xEB, 0x78,
	0xA4, 0x62, 0x46, 0x7F, 0x8B, 0x55, 0xEE, 0x3C,
	0x2F, 0xF3, 0x7E, 0xF5, 0x6B, 0x39, 0xE3, 0xA3,
	0x0E, 0xEA, 0x92, 0x76, 0xAC, 0xF7, 0xB2, 0x05,
	0xB2, 0x50, 0x5D, 0xF9, 0xB7, 0x11, 0x87, 0xB7,
	0x49, 0x86, 0xEB, 0x44, 0x6A, 0x0C, 0x64, 0x75,
	0x95, 0x14, 0x24, 0xFF, 0x49, 0x06, 0x52, 0x68,
	0x81, 0x71, 0x44, 0x85, 0x26, 0x0A, 0x49, 0xEA,
	0x4E, 0x9F, 0x6A, 0x8E, 0xCF, 0xC8, 0xC9, 0xB0,
	0x61, 0x77, 0x27, 0x89, 0xB0, 0xFA, 0x1D, 0x51,
	0x7D, 0xDC, 0x34, 0x21, 0x80, 0x8B, 0x6B, 0x86,
	0x19, 0x1A, 0x5F, 0x19, 0x23, 0xF3, 0xFB, 0xD1,
	0xF7, 0x35, 0x9D, 0x28, 0x61, 0x2F, 0x35, 0x85,
	0x82, 0x2A, 0x1E, 0xDF, 0x09, 0xC2, 0x0C, 0x99,
	0xE0, 0x3C, 0x8F, 0x4B, 0x3D, 0x92, 0xAF, 0x46,
	0x77, 0x68, 0x59, 0xF4, 0x37, 0x81, 0x6C, 0xCE,
	0x27, 0x8B, 0xAB, 0x0B, 0xA5, 0xDA, 0x7B, 0x19,
	0x83, 0xDA, 0x27, 0x49, 0x65, 0x1A, 0x00, 0x6B,
	0xE1, 0x8B, 0x73, 0xCD, 0xF4, 0xFB, 0xD7, 0xBF,
	0xF8, 0x20, 0x89, 0xE1, 0xDE, 0x51, 0x1E, 0xDD,
	0x97, 0x44, 0x12, 0x68, 0x1E, 0xF7, 0x52, 0xF8,
	0x6B, 0x93, 0xC1, 0x3B, 0x9F, 0xA1, 0xB8, 0x5F,
	0xCB, 0x84, 0x45, 0x95, 0xF7, 0x0D, 0xA6, 0x4B,
	0x03, 0x3C, 0xAE, 0x0F, 0xB7, 0x81, 0x78, 0x75,
	0x1C, 0x53, 0x99, 0x24, 0xB3, 0xE2, 0x78, 0xCE,
	0xF3, 0xF0, 0x09, 0x6C, 0x01, 0x85, 0x73, 0xBD
};
static const unsigned char RSA4096_DQ[] = {
	0xCD, 0x88, 0xAC, 0x8B, 0x92, 0x6A, 0xA8, 0x6B,
	0x71, 0x16, 0xCD, 0x6B, 0x6A, 0x0B, 0xA6, 0xCD,
	0xF3, 0x27, 0x58, 0xA6, 0xE4, 0x1D, 0xDC, 0x40,
	0xAF, 0x7B, 0x3F, 0x44, 0x3D, 0xAC, 0x1D, 0x08,
	0x5C, 0xE9, 0xF1, 0x0D, 0x07, 0xE4, 0x0A, 0x94,
	0x2C, 0xBF, 0xCC, 0x48, 0xAA, 0x62, 0x58, 0xF2,
	0x5E, 0x8F, 0x2D, 0x36, 0x37, 0xFE, 0xB6, 0xCB,
	0x0A, 0x24, 0xD3, 0xF0, 0x87, 0x5D, 0x0E, 0x05,
	0xC4, 0xFB, 0xCA, 0x7A, 0x8B, 0xA5, 0x72, 0xFB,
	0x17, 0x78, 0x6C, 0xC2, 0xAA, 0x56, 0x93, 0x2F,
	0xFE, 0x6C, 0xA2, 0xEB, 0xD4, 0x18, 0xDD, 0x71,
	0xCB, 0x0B, 0x89, 0xFC, 0xB3, 0xFB, 0xED, 0xB7,
	0xC5, 0xB0, 0x29, 0x6D, 0x9C, 0xB9, 0xC5, 0xC4,
	0xFA, 0x58, 0xD7, 0x36, 0x01, 0x0F, 0xE4, 0x6A,
	0xF4, 0x0B, 0x4D, 0xBB, 0x3E, 0x8E, 0x9F, 0xBA,
	0x98, 0x6D, 0x1A, 0xE5, 0x20, 0xAF, 0x84, 0x30,
	0xDD, 0xAC, 0x3C, 0x66, 0xBC, 0x24, 0xD9, 0x67,
	0x4A, 0x35, 0x61, 0xC9, 0xAD, 0xCC, 0xC9, 0x66,
	0x68, 0x46, 0x19, 0x8C, 0x04, 0xA5, 0x16, 0x83,
	0x5F, 0x7A, 0xFD, 0x1B, 0xAD, 0xAE, 0x22, 0x2D,
	0x05, 0xAF, 0x29, 0xDC, 0xBB, 0x0E, 0x86, 0x0C,
	0xBC, 0x9E, 0xB6, 0x28, 0xA9, 0xF2, 0xCC, 0x5E,
	0x1F, 0x86, 0x95, 0xA5, 0x9C, 0x11, 0x19, 0xF0,
	0x5F, 0xDA, 0x2C, 0x04, 0xFE, 0x22, 0x80, 0xF7,
	0x94, 0x3C, 0xBA, 0x01, 0x56, 0xD6, 0x93, 0xFA,
	0xCE, 0x62, 0xE5, 0xD7, 0x98, 0x23, 0xAB, 0xB9,
	0xC7, 0x35, 0x57, 0xF6, 0xE2, 0x16, 0x36, 0xE9,
	0x5B, 0xD7, 0xA5, 0x45, 0x18, 0x93, 0x77, 0xC9,
	0xB1, 0x05, 0xA8, 0x66, 0xE1, 0x0E, 0xB5, 0xDF,
	0x23, 0x35, 0xE1, 0xC2, 0xFA, 0x3E, 0x80, 0x1A,
	0xAD, 0xA4, 0x0C, 0xEF, 0xC7, 0x18, 0xDE, 0x09,
	0xE6, 0x20, 0x98, 0x31, 0xF1, 0xD3, 0xCF, 0xA1
};
static const unsigned char RSA4096_IQ[] = {
	0x76, 0xD7, 0x75, 0xDF, 0xA3, 0x0C, 0x9D, 0x64,
	0x6E, 0x00, 0x82, 0x2E, 0x5C, 0x5E, 0x43, 0xC4,
	0xD2, 0x28, 0xB0, 0xB1, 0xA8, 0xD8, 0x26, 0x91,
	0xA0, 0xF5, 0xC8, 0x69, 0xFF, 0x24, 0x33, 0xAB,
	0x67, 0xC7, 0xA3, 0xAE, 0xBB, 0x17, 0x27, 0x5B,
	0x5A, 0xCD, 0x67, 0xA3, 0x70, 0x91, 0x9E, 0xD5,
	0xF1, 0x97, 0x00, 0x0A, 0x30, 0x64, 0x3D, 0x9B,
	0xBF, 0xB5, 0x8C, 0xAC, 0xC7, 0x20, 0x0A, 0xD2,
	0x76, 0x36, 0x36, 0x5D, 0xE4, 0xAC, 0x5D, 0xBC,
	0x44, 0x32, 0xB0, 0x76, 0x33, 0x40, 0xDD, 0x29,
	0x22, 0xE0, 0xFF, 0x55, 0x4C, 0xCE, 0x3F, 0x43,
	0x34, 0x95, 0x94, 0x7C, 0x22, 0x0D, 0xAB, 0x20,
	0x38, 0x70, 0xC3, 0x4A, 0x19, 0xCF, 0x81, 0xCE,
	0x79, 0x28, 0x6C, 0xC2, 0xA3, 0xB3, 0x48, 0x20,
	0x2D, 0x3E, 0x74, 0x45, 0x2C, 0xAA, 0x9F, 0xA5,
	0xC2, 0xE3, 0x2D, 0x41, 0x95, 0xBD, 0x78, 0xAB,
	0x6A, 0xA8, 0x7A, 0x45, 0x52, 0xE2, 0x66, 0xE7,
	0x6C, 0x38, 0x03, 0xA5, 0xDA, 0xAD, 0x94, 0x3C,
	0x6A, 0xA1, 0xA2, 0xD5, 0xCD, 0xDE, 0x05, 0xCC,
	0x6E, 0x3D, 0x8A, 0xF6, 0x9A, 0xA5, 0x0F, 0xA9,
	0x18, 0xC4, 0xF9, 0x9C, 0x2F, 0xB3, 0xF1, 0x30,
	0x38, 0x60, 0x69, 0x09, 0x67, 0x2C, 0xE9, 0x42,
	0x68, 0x3C, 0x70, 0x32, 0x1A, 0x44, 0x32, 0x02,
	0x82, 0x9F, 0x60, 0xE8, 0xA4, 0x42, 0x74, 0xA2,
	0xA2, 0x5A, 0x99, 0xDC, 0xC8, 0xCA, 0x15, 0x4D,
	0xFF, 0xF1, 0x8A, 0x23, 0xD8, 0xD3, 0xB1, 0x9A,
	0xB4, 0x0B, 0xBB, 0xE8, 0x38, 0x74, 0x0C, 0x52,
	0xC7, 0x8B, 0x63, 0x4C, 0xEA, 0x7D, 0x5F, 0x58,
	0x34, 0x53, 0x3E, 0x23, 0x10, 0xBB, 0x60, 0x6B,
	0x52, 0x9D, 0x89, 0x9F, 0xF0, 0x5F, 0xCE, 0xB3,
	0x9C, 0x0E, 0x75, 0x0F, 0x87, 0xF6, 0x66, 0xA5,
	0x4C, 0x94, 0x84, 0xFE, 0x94, 0xB9, 0x04, 0xB7
};

static const br_rsa_public_key RSA4096_PK = {
	(void *)RSA4096_N, sizeof RSA4096_N,
	(void *)RSA4096_E, sizeof RSA4096_E
};

static const br_rsa_private_key RSA4096_SK = {
	4096,
	(void *)RSA4096_P, sizeof RSA4096_P,
	(void *)RSA4096_Q, sizeof RSA4096_Q,
	(void *)RSA4096_DP, sizeof RSA4096_DP,
	(void *)RSA4096_DQ, sizeof RSA4096_DQ,
	(void *)RSA4096_IQ, sizeof RSA4096_IQ
};

static void
test_RSA_core(const char *name, br_rsa_public fpub, br_rsa_private fpriv)
{
	unsigned char t1[512], t2[512], t3[512];
	size_t len;

	printf("Test %s: ", name);
	fflush(stdout);

	/*
	 * A KAT test (computed with OpenSSL).
	 */
	len = hextobin(t1, "45A3DC6A106BCD3BD0E48FB579643AA3FF801E5903E80AA9B43A695A8E7F454E93FA208B69995FF7A6D5617C2FEB8E546375A664977A48931842AAE796B5A0D64393DCA35F3490FC157F5BD83B9D58C2F7926E6AE648A2BD96CAB8FCCD3D35BB11424AD47D973FF6D69CA774841AEC45DFAE99CCF79893E7047FDE6CB00AA76D");
	hextobin(t2, "0001FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF003021300906052B0E03021A05000414A94A8FE5CCB19BA61C4C0873D391E987982FBBD3");
	memcpy(t3, t1, len);
	if (!fpub(t3, len, &RSA_PK)) {
		fprintf(stderr, "RSA public operation failed (1)\n");
		exit(EXIT_FAILURE);
	}
	check_equals("KAT RSA pub", t2, t3, len);
	if (!fpriv(t3, &RSA_SK)) {
		fprintf(stderr, "RSA private operation failed (1)\n");
		exit(EXIT_FAILURE);
	}
	check_equals("KAT RSA priv (1)", t1, t3, len);

	/*
	 * Another KAT test, with a (fake) hash value slightly different
	 * (last byte is 0xD9 instead of 0xD3).
	 */
	len = hextobin(t1, "32C2DB8B2C73BBCA9960CB3F11FEDEE7B699359EF2EEC3A632E56B7FF3DE2F371E5179BAB03F17E0BB20D2891ACAB679F95DA9B43A01DAAD192FADD25D8ACCF1498EC80F5BBCAC88EA59D60E3BC9D3CE27743981DE42385FFFFF04DD2D716E1A46C04A28ECAF6CD200DAB81083A830D61538D69BB39A183107BD50302AA6BC28");
	hextobin(t2, "0001FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF003021300906052B0E03021A05000414A94A8FE5CCB19BA61C4C0873D391E987982FBBD9");
	memcpy(t3, t1, len);
	if (!fpub(t3, len, &RSA_PK)) {
		fprintf(stderr, "RSA public operation failed (2)\n");
		exit(EXIT_FAILURE);
	}
	check_equals("KAT RSA pub", t2, t3, len);
	if (!fpriv(t3, &RSA_SK)) {
		fprintf(stderr, "RSA private operation failed (2)\n");
		exit(EXIT_FAILURE);
	}
	check_equals("KAT RSA priv (2)", t1, t3, len);

	/*
	 * Third KAT vector is invalid, because the encrypted value is
	 * out of range: instead of x, value is x+n (where n is the
	 * modulus). Mathematically, this still works, but implementations
	 * are supposed to reject such cases.
	 */
	len = hextobin(t1, "F27781B9B3B358583A24F9BA6B34EE98B67A5AE8D8D4FA567BA773EB6B85EF88848680640A1E2F5FD117876E5FB928B64C6EFC7E03632A3F4C941E15657C0C705F3BB8D0B03A0249143674DB1FE6E5406D690BF2DA76EA7FF3AC6FCE12C7801252FAD52D332BE4AB41F9F8CF1728CDF98AB8E8C20E0C350E4F707A6402C01E0B");
	hextobin(t2, "BFB6A62E873F9C8DA0C42E7B59360FB0FFE12549E5E636B048C2086B77A7C051663506A959DF177F15F6B4E544EE723C531152C9C9614F923364704307F13F7F15ACF0C1547D55C029DC9ECCE41D117245F4D270FC34B21FF3AD6AEFE58633281540902F547F79F3461F44D33CCB2D094231ADCC76BE25511B4513BB70491DBC");
	memcpy(t3, t1, len);
	if (fpub(t3, len, &RSA_PK)) {
		size_t u;
		fprintf(stderr, "RSA public operation should have failed"
			" (value out of range)\n");
		fprintf(stderr, "x = ");
		for (u = 0; u < len; u ++) {
			fprintf(stderr, "%02X", t3[u]);
		}
		fprintf(stderr, "\n");
		exit(EXIT_FAILURE);
	}
	memcpy(t3, t2, len);
	if (fpriv(t3, &RSA_SK)) {
		size_t u;
		fprintf(stderr, "RSA private operation should have failed"
			" (value out of range)\n");
		fprintf(stderr, "x = ");
		for (u = 0; u < len; u ++) {
			fprintf(stderr, "%02X", t3[u]);
		}
		fprintf(stderr, "\n");
		exit(EXIT_FAILURE);
	}

	/*
	 * RSA-2048 test vector.
	 */
	len = hextobin(t1, "B188ED4EF173A30AED3889926E3CF1CE03FE3BAA7AB122B119A8CD529062F235A7B321008FB898894A624B3E6C8C5374950E78FAC86651345FE2ABA0791968284F23B0D794F8DCDDA924518854822CB7FF2AA9F205AACD909BB5EA541534CC00DBC2EF7727B9FE1BAFE6241B931E8BD01E13632E5AF9E94F4A335772B61F24D6F6AA642AEABB173E36F546CB02B19A1E5D4E27E3EB67F2E986E9F084D4BD266543800B1DC96088A05DFA9AFA595398E9A766D41DD8DA4F74F36C9D74867F0BF7BFA8622EE43C79DA0CEAC14B5D39DE074BDB89D84145BC19D8B2D0EA74DBF2DC29E907BF7C7506A2603CD8BC25EFE955D0125EDB2685EF158B020C9FC539242A");
	hextobin(t2, "0001FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF003031300D060960864801650304020105000420A5A0A792A09438811584A68E240C6C89F1FB1C53C0C86E270B942635F4F6B24A");
	memcpy(t3, t1, len);
	if (!fpub(t3, len, &RSA2048_PK)) {
		fprintf(stderr, "RSA public operation failed (2048)\n");
		exit(EXIT_FAILURE);
	}
	check_equals("KAT RSA pub", t2, t3, len);
	if (!fpriv(t3, &RSA2048_SK)) {
		fprintf(stderr, "RSA private operation failed (2048)\n");
		exit(EXIT_FAILURE);
	}
	check_equals("KAT RSA priv (2048)", t1, t3, len);

	/*
	 * RSA-4096 test vector.
	 */
	len = hextobin(t1, "7D35B6B4D85252D08A2658C0B04126CC617B0E56B2A782A5FA2722AD05BD49538111682C12DA2C5FA1B9C30FB1AB8DA2C6A49EB4226A4D32290CF091FBB22EC499C7B18192C230B29F957DAF551F1EAD1917BA9E03D757100BD1F96B829708A6188A3927436113BB21E175D436BBB7A90E20162203FFB8F675313DFB21EFDA3EA0C7CC9B605AE7FB47E2DD2A9C4D5F124D7DE1B690AF9ADFEDC6055E0F9D2C9A891FB2501F3055D6DA7E94D51672BA1E86AEB782E4B020F70E0DF5399262909FC5B4770B987F2826EF2099A15F3CD5A0D6FE82E0C85FBA2C53C77305F534A7B0C7EA0D5244E37F1C1318EEF7079995F0642E4AB80EB0ED60DB4955FB652ED372DAC787581054A827C37A25C7B4DE7AE7EF3D099D47D6682ADF02BCC4DE04DDF2920F7124CF5B4955705E4BDB97A0BF341B584797878B4D3795134A9469FB391E4E4988F0AA451027CBC2ED6121FC23B26BF593E3C51DEDD53B62E23050D5B41CA34204679916A87AF1B17873A0867924D0C303942ADA478B769487FCEF861D4B20DCEE6942CCB84184833CDB258167258631C796BC1977D001354E2EE168ABE3B45FC969EA7F22B8E133C57A10FBB25ED19694E89C399CF7723B3C0DF0CC9F57A8ED0959EFC392FB31B8ADAEA969E2DEE8282CB245E5677368F00CCE4BA52C07C16BE7F9889D57191D5B2FE552D72B3415C64C09EE622457766EC809344A1EFE");
	hextobin(t2, "0001FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF003031300D0609608648016503040201050004205B60DD5AD5B3C62E0DA25FD0D8CB26325E1CE32CC9ED234B288235BCCF6ED2C8");
	memcpy(t3, t1, len);
	if (!fpub(t3, len, &RSA4096_PK)) {
		fprintf(stderr, "RSA public operation failed (4096)\n");
		exit(EXIT_FAILURE);
	}
	check_equals("KAT RSA pub", t2, t3, len);
	if (!fpriv(t3, &RSA4096_SK)) {
		fprintf(stderr, "RSA private operation failed (4096)\n");
		exit(EXIT_FAILURE);
	}
	check_equals("KAT RSA priv (4096)", t1, t3, len);

	printf("done.\n");
	fflush(stdout);
}

static const unsigned char SHA1_OID[] = {
	0x05, 0x2B, 0x0E, 0x03, 0x02, 0x1A
};

static void
test_RSA_sign(const char *name, br_rsa_private fpriv,
	br_rsa_pkcs1_sign fsign, br_rsa_pkcs1_vrfy fvrfy)
{
	unsigned char t1[128], t2[128];
	unsigned char hv[20], tmp[20];
	unsigned char rsa_n[128], rsa_e[3], rsa_p[64], rsa_q[64];
	unsigned char rsa_dp[64], rsa_dq[64], rsa_iq[64];
	br_rsa_public_key rsa_pk;
	br_rsa_private_key rsa_sk;
	unsigned char hv2[64], tmp2[64], sig[128];
	br_sha1_context hc;
	size_t u;

	printf("Test %s: ", name);
	fflush(stdout);

	/*
	 * Verify the KAT test (computed with OpenSSL).
	 */
	hextobin(t1, "45A3DC6A106BCD3BD0E48FB579643AA3FF801E5903E80AA9B43A695A8E7F454E93FA208B69995FF7A6D5617C2FEB8E546375A664977A48931842AAE796B5A0D64393DCA35F3490FC157F5BD83B9D58C2F7926E6AE648A2BD96CAB8FCCD3D35BB11424AD47D973FF6D69CA774841AEC45DFAE99CCF79893E7047FDE6CB00AA76D");
	br_sha1_init(&hc);
	br_sha1_update(&hc, "test", 4);
	br_sha1_out(&hc, hv);
	if (!fvrfy(t1, sizeof t1, SHA1_OID, sizeof tmp, &RSA_PK, tmp)) {
		fprintf(stderr, "Signature verification failed\n");
		exit(EXIT_FAILURE);
	}
	check_equals("Extracted hash value", hv, tmp, sizeof tmp);

	/*
	 * Regenerate the signature. This should yield the same value as
	 * the KAT test, since PKCS#1 v1.5 signatures are deterministic
	 * (except the usual detail about hash function parameter
	 * encoding, but OpenSSL uses the same convention as BearSSL).
	 */
	if (!fsign(SHA1_OID, hv, 20, &RSA_SK, t2)) {
		fprintf(stderr, "Signature generation failed\n");
		exit(EXIT_FAILURE);
	}
	check_equals("Regenerated signature", t1, t2, sizeof t1);

	/*
	 * Use the raw private core to generate fake signatures, where
	 * one byte of the padded hash value is altered. They should all be
	 * rejected.
	 */
	hextobin(t2, "0001FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF003021300906052B0E03021A05000414A94A8FE5CCB19BA61C4C0873D391E987982FBBD3");
	for (u = 0; u < (sizeof t2) - 20; u ++) {
		memcpy(t1, t2, sizeof t2);
		t1[u] ^= 0x01;
		if (!fpriv(t1, &RSA_SK)) {
			fprintf(stderr, "RSA private key operation failed\n");
			exit(EXIT_FAILURE);
		}
		if (fvrfy(t1, sizeof t1, SHA1_OID, sizeof tmp, &RSA_PK, tmp)) {
			fprintf(stderr,
				"Signature verification should have failed\n");
			exit(EXIT_FAILURE);
		}
		printf(".");
		fflush(stdout);
	}

	/*
	 * Another KAT test, which historically showed a bug.
	 */
	rsa_pk.n = rsa_n;
	rsa_pk.nlen = hextobin(rsa_n, "E65DAEF196D22C300B3DAE1CE5157EDF821BB6038E419D8D363A8B2DA84A1321042330E6F87A8BD8FE6BA1D2A17031955ED2315CC5FD2397197E238A5E0D2D0AFD25717E814EC4D2BBA887327A3C5B3A450FD8D547BDFCBB0F73B997CA13DD5E7572C4D5BAA764A349BAB2F868ACF4574AE2C7AEC94B77D2EE00A21B6CB175BB");
	rsa_pk.e = rsa_e;
	rsa_pk.elen = hextobin(rsa_e, "010001");

	rsa_sk.n_bitlen = 1024;
	rsa_sk.p = rsa_p;
	rsa_sk.plen = hextobin(rsa_p, "FF58513DBA4F3F42DFDFD3E6AFB6BD62DE27E06BA3C9D9F9B542CB21228C2AAE67936514161C8FDC1A248A50195CAF22ADC50DA89BFED1B9EEFBB37304241357");
	rsa_sk.q = rsa_q;
	rsa_sk.qlen = hextobin(rsa_q, "E6F4F66818B7442297DDEB45E9B3D438E5B57BB5EF86EFF2462AD6B9C10F383517CDD2E7E36EAD4BEBCC57CFE8AA985F7E7B38B96D30FFBE9ED9FE21B1CFB63D");
	rsa_sk.dp = rsa_dp;
	rsa_sk.dplen = hextobin(rsa_dp, "6F89517B682D83919F9EF2BDBA955526A1A9C382E139A3A84AC01160B8E9871F458901C7035D988D6931FAE4C01F57350BB89E9DBEFE50F829E6F25CD43B39E3");
	rsa_sk.dq = rsa_dq;
	rsa_sk.dqlen = hextobin(rsa_dq, "409E08D2D7176F58BE64B88EB6F4394C31F8B4C412600E821A5FA1F416AFCB6A0F5EE6C33A3E9CFDC0DB4B3640427A9F3D23FC9AE491F0FBC435F98433DB8981");
	rsa_sk.iq = rsa_iq;
	rsa_sk.iqlen = hextobin(rsa_iq, "CF333D6AD66D02B4D11C8C23CA669D14D71803ADC3943BE03B1E48F52F385BCFDDFD0F85AD02A984E504FC6612549D4E7867B7D09DD13196BFC3FAA4B57393A9");
	hextobin(sig, "CFB84D161E6DB130736FC6212EBE575571AF341CEF5757C19952A5364C90E3C47549E520E26253DAE70F645F31FA8B5DA9AE282741D3CA4B1CC365B7BD75D6D61D4CFD9AD9EDD17D23E0BA7D9775138DBABC7FF2A57587FE1EA1B51E8F3C68326E26FF89D8CF92BDD4C787D04857DFC3266E6B33B92AA08809929C72642F35C2");

	hextobin(hv2, "F66C62B38E1CC69C378C0E16574AE5C6443FDFA3E85C6205C00B3231CAA3074EC1481BDC22AB575E6CF3CCD9EDA6B39F83923FC0E6475C799D257545F77233B4");
	if (!fsign(BR_HASH_OID_SHA512, hv2, 64, &rsa_sk, t2)) {
		fprintf(stderr, "Signature generation failed (2)\n");
		exit(EXIT_FAILURE);
	}
	check_equals("Regenerated signature (2)", t2, sig, sizeof t2);
	if (!fvrfy(t2, sizeof t2, BR_HASH_OID_SHA512,
		sizeof tmp2, &rsa_pk, tmp2))
	{
		fprintf(stderr, "Signature verification failed (2)\n");
		exit(EXIT_FAILURE);
	}
	check_equals("Extracted hash value (2)", hv2, tmp2, sizeof tmp2);

	printf(" done.\n");
	fflush(stdout);
}

/*
 * Test vectors from pkcs-1v2-1d2-vec.zip (originally from ftp.rsa.com).
 * There are ten RSA keys, and for each RSA key, there are 6 messages,
 * each with an explicit seed.
 *
 * Field order:
 *    modulus (n)
 *    public exponent (e)
 *    first factor (p)
 *    second factor (q)
 *    first private exponent (dp)
 *    second private exponent (dq)
 *    CRT coefficient (iq)
 *    cleartext 1
 *    seed 1 (20-byte random value)
 *    ciphertext 1
 *    cleartext 2
 *    seed 2 (20-byte random value)
 *    ciphertext 2
 *    ...
 *    cleartext 6
 *    seed 6 (20-byte random value)
 *    ciphertext 6
 *
 * This pattern is repeated for all keys. The array stops on a NULL.
 */
static const char *KAT_RSA_OAEP[] = {
	/* 1024-bit key, from oeap-int.txt */
	"BBF82F090682CE9C2338AC2B9DA871F7368D07EED41043A440D6B6F07454F51FB8DFBAAF035C02AB61EA48CEEB6FCD4876ED520D60E1EC4619719D8A5B8B807FAFB8E0A3DFC737723EE6B4B7D93A2584EE6A649D060953748834B2454598394EE0AAB12D7B61A51F527A9A41F6C1687FE2537298CA2A8F5946F8E5FD091DBDCB",
	"11",
	"EECFAE81B1B9B3C908810B10A1B5600199EB9F44AEF4FDA493B81A9E3D84F632124EF0236E5D1E3B7E28FAE7AA040A2D5B252176459D1F397541BA2A58FB6599",
	"C97FB1F027F453F6341233EAAAD1D9353F6C42D08866B1D05A0F2035028B9D869840B41666B42E92EA0DA3B43204B5CFCE3352524D0416A5A441E700AF461503",
	"54494CA63EBA0337E4E24023FCD69A5AEB07DDDC0183A4D0AC9B54B051F2B13ED9490975EAB77414FF59C1F7692E9A2E202B38FC910A474174ADC93C1F67C981",
	"471E0290FF0AF0750351B7F878864CA961ADBD3A8A7E991C5C0556A94C3146A7F9803F8F6F8AE342E931FD8AE47A220D1B99A495849807FE39F9245A9836DA3D",
	"B06C4FDABB6301198D265BDBAE9423B380F271F73453885093077FCD39E2119FC98632154F5883B167A967BF402B4E9E2E0F9656E698EA3666EDFB25798039F7",

	/* oaep-int.txt contains only one message, so we repeat it six
	   times to respect our array format. */
	"D436E99569FD32A7C8A05BBC90D32C49",
	"AAFD12F659CAE63489B479E5076DDEC2F06CB58F",
	"1253E04DC0A5397BB44A7AB87E9BF2A039A33D1E996FC82A94CCD30074C95DF763722017069E5268DA5D1C0B4F872CF653C11DF82314A67968DFEAE28DEF04BB6D84B1C31D654A1970E5783BD6EB96A024C2CA2F4A90FE9F2EF5C9C140E5BB48DA9536AD8700C84FC9130ADEA74E558D51A74DDF85D8B50DE96838D6063E0955",

	"D436E99569FD32A7C8A05BBC90D32C49",
	"AAFD12F659CAE63489B479E5076DDEC2F06CB58F",
	"1253E04DC0A5397BB44A7AB87E9BF2A039A33D1E996FC82A94CCD30074C95DF763722017069E5268DA5D1C0B4F872CF653C11DF82314A67968DFEAE28DEF04BB6D84B1C31D654A1970E5783BD6EB96A024C2CA2F4A90FE9F2EF5C9C140E5BB48DA9536AD8700C84FC9130ADEA74E558D51A74DDF85D8B50DE96838D6063E0955",

	"D436E99569FD32A7C8A05BBC90D32C49",
	"AAFD12F659CAE63489B479E5076DDEC2F06CB58F",
	"1253E04DC0A5397BB44A7AB87E9BF2A039A33D1E996FC82A94CCD30074C95DF763722017069E5268DA5D1C0B4F872CF653C11DF82314A67968DFEAE28DEF04BB6D84B1C31D654A1970E5783BD6EB96A024C2CA2F4A90FE9F2EF5C9C140E5BB48DA9536AD8700C84FC9130ADEA74E558D51A74DDF85D8B50DE96838D6063E0955",

	"D436E99569FD32A7C8A05BBC90D32C49",
	"AAFD12F659CAE63489B479E5076DDEC2F06CB58F",
	"1253E04DC0A5397BB44A7AB87E9BF2A039A33D1E996FC82A94CCD30074C95DF763722017069E5268DA5D1C0B4F872CF653C11DF82314A67968DFEAE28DEF04BB6D84B1C31D654A1970E5783BD6EB96A024C2CA2F4A90FE9F2EF5C9C140E5BB48DA9536AD8700C84FC9130ADEA74E558D51A74DDF85D8B50DE96838D6063E0955",

	"D436E99569FD32A7C8A05BBC90D32C49",
	"AAFD12F659CAE63489B479E5076DDEC2F06CB58F",
	"1253E04DC0A5397BB44A7AB87E9BF2A039A33D1E996FC82A94CCD30074C95DF763722017069E5268DA5D1C0B4F872CF653C11DF82314A67968DFEAE28DEF04BB6D84B1C31D654A1970E5783BD6EB96A024C2CA2F4A90FE9F2EF5C9C140E5BB48DA9536AD8700C84FC9130ADEA74E558D51A74DDF85D8B50DE96838D6063E0955",

	"D436E99569FD32A7C8A05BBC90D32C49",
	"AAFD12F659CAE63489B479E5076DDEC2F06CB58F",
	"1253E04DC0A5397BB44A7AB87E9BF2A039A33D1E996FC82A94CCD30074C95DF763722017069E5268DA5D1C0B4F872CF653C11DF82314A67968DFEAE28DEF04BB6D84B1C31D654A1970E5783BD6EB96A024C2CA2F4A90FE9F2EF5C9C140E5BB48DA9536AD8700C84FC9130ADEA74E558D51A74DDF85D8B50DE96838D6063E0955",

	/* 1024-bit key */
	"A8B3B284AF8EB50B387034A860F146C4919F318763CD6C5598C8AE4811A1E0ABC4C7E0B082D693A5E7FCED675CF4668512772C0CBC64A742C6C630F533C8CC72F62AE833C40BF25842E984BB78BDBF97C0107D55BDB662F5C4E0FAB9845CB5148EF7392DD3AAFF93AE1E6B667BB3D4247616D4F5BA10D4CFD226DE88D39F16FB",
	"010001",
	"D32737E7267FFE1341B2D5C0D150A81B586FB3132BED2F8D5262864A9CB9F30AF38BE448598D413A172EFB802C21ACF1C11C520C2F26A471DCAD212EAC7CA39D",
	"CC8853D1D54DA630FAC004F471F281C7B8982D8224A490EDBEB33D3E3D5CC93C4765703D1DD791642F1F116A0DD852BE2419B2AF72BFE9A030E860B0288B5D77",
	"0E12BF1718E9CEF5599BA1C3882FE8046A90874EEFCE8F2CCC20E4F2741FB0A33A3848AEC9C9305FBECBD2D76819967D4671ACC6431E4037968DB37878E695C1",
	"95297B0F95A2FA67D00707D609DFD4FC05C89DAFC2EF6D6EA55BEC771EA333734D9251E79082ECDA866EFEF13C459E1A631386B7E354C899F5F112CA85D71583",
	"4F456C502493BDC0ED2AB756A3A6ED4D67352A697D4216E93212B127A63D5411CE6FA98D5DBEFD73263E3728142743818166ED7DD63687DD2A8CA1D2F4FBD8E1",

	"6628194E12073DB03BA94CDA9EF9532397D50DBA79B987004AFEFE34",
	"18B776EA21069D69776A33E96BAD48E1DDA0A5EF",
	"354FE67B4A126D5D35FE36C777791A3F7BA13DEF484E2D3908AFF722FAD468FB21696DE95D0BE911C2D3174F8AFCC201035F7B6D8E69402DE5451618C21A535FA9D7BFC5B8DD9FC243F8CF927DB31322D6E881EAA91A996170E657A05A266426D98C88003F8477C1227094A0D9FA1E8C4024309CE1ECCCB5210035D47AC72E8A",

	"750C4047F547E8E41411856523298AC9BAE245EFAF1397FBE56F9DD5",
	"0CC742CE4A9B7F32F951BCB251EFD925FE4FE35F",
	"640DB1ACC58E0568FE5407E5F9B701DFF8C3C91E716C536FC7FCEC6CB5B71C1165988D4A279E1577D730FC7A29932E3F00C81515236D8D8E31017A7A09DF4352D904CDEB79AA583ADCC31EA698A4C05283DABA9089BE5491F67C1A4EE48DC74BBBE6643AEF846679B4CB395A352D5ED115912DF696FFE0702932946D71492B44",

	"D94AE0832E6445CE42331CB06D531A82B1DB4BAAD30F746DC916DF24D4E3C2451FFF59A6423EB0E1D02D4FE646CF699DFD818C6E97B051",
	"2514DF4695755A67B288EAF4905C36EEC66FD2FD",
	"423736ED035F6026AF276C35C0B3741B365E5F76CA091B4E8C29E2F0BEFEE603595AA8322D602D2E625E95EB81B2F1C9724E822ECA76DB8618CF09C5343503A4360835B5903BC637E3879FB05E0EF32685D5AEC5067CD7CC96FE4B2670B6EAC3066B1FCF5686B68589AAFB7D629B02D8F8625CA3833624D4800FB081B1CF94EB",

	"52E650D98E7F2A048B4F86852153B97E01DD316F346A19F67A85",
	"C4435A3E1A18A68B6820436290A37CEFB85DB3FB",
	"45EAD4CA551E662C9800F1ACA8283B0525E6ABAE30BE4B4ABA762FA40FD3D38E22ABEFC69794F6EBBBC05DDBB11216247D2F412FD0FBA87C6E3ACD888813646FD0E48E785204F9C3F73D6D8239562722DDDD8771FEC48B83A31EE6F592C4CFD4BC88174F3B13A112AAE3B9F7B80E0FC6F7255BA880DC7D8021E22AD6A85F0755",

	"8DA89FD9E5F974A29FEFFB462B49180F6CF9E802",
	"B318C42DF3BE0F83FEA823F5A7B47ED5E425A3B5",
	"36F6E34D94A8D34DAACBA33A2139D00AD85A9345A86051E73071620056B920E219005855A213A0F23897CDCD731B45257C777FE908202BEFDD0B58386B1244EA0CF539A05D5D10329DA44E13030FD760DCD644CFEF2094D1910D3F433E1C7C6DD18BC1F2DF7F643D662FB9DD37EAD9059190F4FA66CA39E869C4EB449CBDC439",

	"26521050844271",
	"E4EC0982C2336F3A677F6A356174EB0CE887ABC2",
	"42CEE2617B1ECEA4DB3F4829386FBD61DAFBF038E180D837C96366DF24C097B4AB0FAC6BDF590D821C9F10642E681AD05B8D78B378C0F46CE2FAD63F74E0AD3DF06B075D7EB5F5636F8D403B9059CA761B5C62BB52AA45002EA70BAACE08DED243B9D8CBD62A68ADE265832B56564E43A6FA42ED199A099769742DF1539E8255",

	/* 1025-bit key */
	"01947C7FCE90425F47279E70851F25D5E62316FE8A1DF19371E3E628E260543E4901EF6081F68C0B8141190D2AE8DABA7D1250EC6DB636E944EC3722877C7C1D0A67F14B1694C5F0379451A43E49A32DDE83670B73DA91A1C99BC23B436A60055C610F0BAF99C1A079565B95A3F1526632D1D4DA60F20EDA25E653C4F002766F45",
	"010001",
	"0159DBDE04A33EF06FB608B80B190F4D3E22BCC13AC8E4A081033ABFA416EDB0B338AA08B57309EA5A5240E7DC6E54378C69414C31D97DDB1F406DB3769CC41A43",
	"012B652F30403B38B40995FD6FF41A1ACC8ADA70373236B7202D39B2EE30CFB46DB09511F6F307CC61CC21606C18A75B8A62F822DF031BA0DF0DAFD5506F568BD7",
	"436EF508DE736519C2DA4C580D98C82CB7452A3FB5EFADC3B9C7789A1BC6584F795ADDBBD32439C74686552ECB6C2C307A4D3AF7F539EEC157248C7B31F1A255",
	"012B15A89F3DFB2B39073E73F02BDD0C1A7B379DD435F05CDDE2EFF9E462948B7CEC62EE9050D5E0816E0785A856B49108DCB75F3683874D1CA6329A19013066FF",
	"0270DB17D5914B018D76118B24389A7350EC836B0063A21721236FD8EDB6D89B51E7EEB87B611B7132CB7EA7356C23151C1E7751507C786D9EE1794170A8C8E8",

	"8FF00CAA605C702830634D9A6C3D42C652B58CF1D92FEC570BEEE7",
	"8C407B5EC2899E5099C53E8CE793BF94E71B1782",
	"0181AF8922B9FCB4D79D92EBE19815992FC0C1439D8BCD491398A0F4AD3A329A5BD9385560DB532683C8B7DA04E4B12AED6AACDF471C34C9CDA891ADDCC2DF3456653AA6382E9AE59B54455257EB099D562BBE10453F2B6D13C59C02E10F1F8ABB5DA0D0570932DACF2D0901DB729D0FEFCC054E70968EA540C81B04BCAEFE720E",

	"2D",
	"B600CF3C2E506D7F16778C910D3A8B003EEE61D5",
	"018759FF1DF63B2792410562314416A8AEAF2AC634B46F940AB82D64DBF165EEE33011DA749D4BAB6E2FCD18129C9E49277D8453112B429A222A8471B070993998E758861C4D3F6D749D91C4290D332C7A4AB3F7EA35FF3A07D497C955FF0FFC95006B62C6D296810D9BFAB024196C7934012C2DF978EF299ABA239940CBA10245",

	"74FC88C51BC90F77AF9D5E9A4A70133D4B4E0B34DA3C37C7EF8E",
	"A73768AEEAA91F9D8C1ED6F9D2B63467F07CCAE3",
	"018802BAB04C60325E81C4962311F2BE7C2ADCE93041A00719C88F957575F2C79F1B7BC8CED115C706B311C08A2D986CA3B6A9336B147C29C6F229409DDEC651BD1FDD5A0B7F610C9937FDB4A3A762364B8B3206B4EA485FD098D08F63D4AA8BB2697D027B750C32D7F74EAF5180D2E9B66B17CB2FA55523BC280DA10D14BE2053",

	"A7EB2A5036931D27D4E891326D99692FFADDA9BF7EFD3E34E622C4ADC085F721DFE885072C78A203B151739BE540FA8C153A10F00A",
	"9A7B3B0E708BD96F8190ECAB4FB9B2B3805A8156",
	"00A4578CBC176318A638FBA7D01DF15746AF44D4F6CD96D7E7C495CBF425B09C649D32BF886DA48FBAF989A2117187CAFB1FB580317690E3CCD446920B7AF82B31DB5804D87D01514ACBFA9156E782F867F6BED9449E0E9A2C09BCECC6AA087636965E34B3EC766F2FE2E43018A2FDDEB140616A0E9D82E5331024EE0652FC7641",

	"2EF2B066F854C33F3BDCBB5994A435E73D6C6C",
	"EB3CEBBC4ADC16BB48E88C8AEC0E34AF7F427FD3",
	"00EBC5F5FDA77CFDAD3C83641A9025E77D72D8A6FB33A810F5950F8D74C73E8D931E8634D86AB1246256AE07B6005B71B7F2FB98351218331CE69B8FFBDC9DA08BBC9C704F876DEB9DF9FC2EC065CAD87F9090B07ACC17AA7F997B27ACA48806E897F771D95141FE4526D8A5301B678627EFAB707FD40FBEBD6E792A25613E7AEC",

	"8A7FB344C8B6CB2CF2EF1F643F9A3218F6E19BBA89C0",
	"4C45CF4D57C98E3D6D2095ADC51C489EB50DFF84",
	"010839EC20C27B9052E55BEFB9B77E6FC26E9075D7A54378C646ABDF51E445BD5715DE81789F56F1803D9170764A9E93CB78798694023EE7393CE04BC5D8F8C5A52C171D43837E3ACA62F609EB0AA5FFB0960EF04198DD754F57F7FBE6ABF765CF118B4CA443B23B5AAB266F952326AC4581100644325F8B721ACD5D04FF14EF3A",

	/* 2048-bit key */
	"AE45ED5601CEC6B8CC05F803935C674DDBE0D75C4C09FD7951FC6B0CAEC313A8DF39970C518BFFBA5ED68F3F0D7F22A4029D413F1AE07E4EBE9E4177CE23E7F5404B569E4EE1BDCF3C1FB03EF113802D4F855EB9B5134B5A7C8085ADCAE6FA2FA1417EC3763BE171B0C62B760EDE23C12AD92B980884C641F5A8FAC26BDAD4A03381A22FE1B754885094C82506D4019A535A286AFEB271BB9BA592DE18DCF600C2AEEAE56E02F7CF79FC14CF3BDC7CD84FEBBBF950CA90304B2219A7AA063AEFA2C3C1980E560CD64AFE779585B6107657B957857EFDE6010988AB7DE417FC88D8F384C4E6E72C3F943E0C31C0C4A5CC36F879D8A3AC9D7D59860EAADA6B83BB",
	"010001",
	"ECF5AECD1E5515FFFACBD75A2816C6EBF49018CDFB4638E185D66A7396B6F8090F8018C7FD95CC34B857DC17F0CC6516BB1346AB4D582CADAD7B4103352387B70338D084047C9D9539B6496204B3DD6EA442499207BEC01F964287FF6336C3984658336846F56E46861881C10233D2176BF15A5E96DDC780BC868AA77D3CE769",
	"BC46C464FC6AC4CA783B0EB08A3C841B772F7E9B2F28BABD588AE885E1A0C61E4858A0FB25AC299990F35BE85164C259BA1175CDD7192707135184992B6C29B746DD0D2CABE142835F7D148CC161524B4A09946D48B828473F1CE76B6CB6886C345C03E05F41D51B5C3A90A3F24073C7D74A4FE25D9CF21C75960F3FC3863183",
	"C73564571D00FB15D08A3DE9957A50915D7126E9442DACF42BC82E862E5673FF6A008ED4D2E374617DF89F17A160B43B7FDA9CB6B6B74218609815F7D45CA263C159AA32D272D127FAF4BC8CA2D77378E8AEB19B0AD7DA3CB3DE0AE7314980F62B6D4B0A875D1DF03C1BAE39CCD833EF6CD7E2D9528BF084D1F969E794E9F6C1",
	"2658B37F6DF9C1030BE1DB68117FA9D87E39EA2B693B7E6D3A2F70947413EEC6142E18FB8DFCB6AC545D7C86A0AD48F8457170F0EFB26BC48126C53EFD1D16920198DC2A1107DC282DB6A80CD3062360BA3FA13F70E4312FF1A6CD6B8FC4CD9C5C3DB17C6D6A57212F73AE29F619327BAD59B153858585BA4E28B60A62A45E49",
	"6F38526B3925085534EF3E415A836EDE8B86158A2C7CBFECCB0BD834304FEC683BA8D4F479C433D43416E63269623CEA100776D85AFF401D3FFF610EE65411CE3B1363D63A9709EEDE42647CEA561493D54570A879C18682CD97710B96205EC31117D73B5F36223FADD6E8BA90DD7C0EE61D44E163251E20C7F66EB305117CB8",

	"8BBA6BF82A6C0F86D5F1756E97956870B08953B06B4EB205BC1694EE",
	"47E1AB7119FEE56C95EE5EAAD86F40D0AA63BD33",
	"53EA5DC08CD260FB3B858567287FA91552C30B2FEBFBA213F0AE87702D068D19BAB07FE574523DFB42139D68C3C5AFEEE0BFE4CB7969CBF382B804D6E61396144E2D0E60741F8993C3014B58B9B1957A8BABCD23AF854F4C356FB1662AA72BFCC7E586559DC4280D160C126785A723EBEEBEFF71F11594440AAEF87D10793A8774A239D4A04C87FE1467B9DAF85208EC6C7255794A96CC29142F9A8BD418E3C1FD67344B0CD0829DF3B2BEC60253196293C6B34D3F75D32F213DD45C6273D505ADF4CCED1057CB758FC26AEEFA441255ED4E64C199EE075E7F16646182FDB464739B68AB5DAFF0E63E9552016824F054BF4D3C8C90A97BB6B6553284EB429FCC",

	"E6AD181F053B58A904F2457510373E57",
	"6D17F5B4C1FFAC351D195BF7B09D09F09A4079CF",
	"A2B1A430A9D657E2FA1C2BB5ED43FFB25C05A308FE9093C01031795F5874400110828AE58FB9B581CE9DDDD3E549AE04A0985459BDE6C626594E7B05DC4278B2A1465C1368408823C85E96DC66C3A30983C639664FC4569A37FE21E5A195B5776EED2DF8D8D361AF686E750229BBD663F161868A50615E0C337BEC0CA35FEC0BB19C36EB2E0BBCC0582FA1D93AACDB061063F59F2CE1EE43605E5D89ECA183D2ACDFE9F81011022AD3B43A3DD417DAC94B4E11EA81B192966E966B182082E71964607B4F8002F36299844A11F2AE0FAEAC2EAE70F8F4F98088ACDCD0AC556E9FCCC511521908FAD26F04C64201450305778758B0538BF8B5BB144A828E629795",

	"510A2CF60E866FA2340553C94EA39FBC256311E83E94454B4124",
	"385387514DECCC7C740DD8CDF9DAEE49A1CBFD54",
	"9886C3E6764A8B9A84E84148EBD8C3B1AA8050381A78F668714C16D9CFD2A6EDC56979C535D9DEE3B44B85C18BE8928992371711472216D95DDA98D2EE8347C9B14DFFDFF84AA48D25AC06F7D7E65398AC967B1CE90925F67DCE049B7F812DB0742997A74D44FE81DBE0E7A3FEAF2E5C40AF888D550DDBBE3BC20657A29543F8FC2913B9BD1A61B2AB2256EC409BBD7DC0D17717EA25C43F42ED27DF8738BF4AFC6766FF7AFF0859555EE283920F4C8A63C4A7340CBAFDDC339ECDB4B0515002F96C932B5B79167AF699C0AD3FCCFDF0F44E85A70262BF2E18FE34B850589975E867FF969D48EABF212271546CDC05A69ECB526E52870C836F307BD798780EDE",

	"BCDD190DA3B7D300DF9A06E22CAAE2A75F10C91FF667B7C16BDE8B53064A2649A94045C9",
	"5CACA6A0F764161A9684F85D92B6E0EF37CA8B65",
	"6318E9FB5C0D05E5307E1683436E903293AC4642358AAA223D7163013ABA87E2DFDA8E60C6860E29A1E92686163EA0B9175F329CA3B131A1EDD3A77759A8B97BAD6A4F8F4396F28CF6F39CA58112E48160D6E203DAA5856F3ACA5FFED577AF499408E3DFD233E3E604DBE34A9C4C9082DE65527CAC6331D29DC80E0508A0FA7122E7F329F6CCA5CFA34D4D1DA417805457E008BEC549E478FF9E12A763C477D15BBB78F5B69BD57830FC2C4ED686D79BC72A95D85F88134C6B0AFE56A8CCFBC855828BB339BD17909CF1D70DE3335AE07039093E606D655365DE6550B872CD6DE1D440EE031B61945F629AD8A353B0D40939E96A3C450D2A8D5EEE9F678093C8",

	"A7DD6C7DC24B46F9DD5F1E91ADA4C3B3DF947E877232A9",
	"95BCA9E3859894B3DD869FA7ECD5BBC6401BF3E4",
	"75290872CCFD4A4505660D651F56DA6DAA09CA1301D890632F6A992F3D565CEE464AFDED40ED3B5BE9356714EA5AA7655F4A1366C2F17C728F6F2C5A5D1F8E28429BC4E6F8F2CFF8DA8DC0E0A9808E45FD09EA2FA40CB2B6CE6FFFF5C0E159D11B68D90A85F7B84E103B09E682666480C657505C0929259468A314786D74EAB131573CF234BF57DB7D9E66CC6748192E002DC0DEEA930585F0831FDCD9BC33D51F79ED2FFC16BCF4D59812FCEBCAA3F9069B0E445686D644C25CCF63B456EE5FA6FFE96F19CDF751FED9EAF35957754DBF4BFEA5216AA1844DC507CB2D080E722EBA150308C2B5FF1193620F1766ECF4481BAFB943BD292877F2136CA494ABA0",

	"EAF1A73A1B0C4609537DE69CD9228BBCFB9A8CA8C6C3EFAF056FE4A7F4634ED00B7C39EC6922D7B8EA2C04EBAC",
	"9F47DDF42E97EEA856A9BDBC714EB3AC22F6EB32",
	"2D207A73432A8FB4C03051B3F73B28A61764098DFA34C47A20995F8115AA6816679B557E82DBEE584908C6E69782D7DEB34DBD65AF063D57FCA76A5FD069492FD6068D9984D209350565A62E5C77F23038C12CB10C6634709B547C46F6B4A709BD85CA122D74465EF97762C29763E06DBC7A9E738C78BFCA0102DC5E79D65B973F28240CAAB2E161A78B57D262457ED8195D53E3C7AE9DA021883C6DB7C24AFDD2322EAC972AD3C354C5FCEF1E146C3A0290FB67ADF007066E00428D2CEC18CE58F9328698DEFEF4B2EB5EC76918FDE1C198CBB38B7AFC67626A9AEFEC4322BFD90D2563481C9A221F78C8272C82D1B62AB914E1C69F6AF6EF30CA5260DB4A46",

	NULL
};

/*
 * Fake RNG that returns exactly the provided bytes.
 */
typedef struct {
	const br_prng_class *vtable;
	unsigned char buf[128];
	size_t ptr, len;
} rng_oaep_ctx;

static void rng_oaep_init(rng_oaep_ctx *cc,
	const void *params, const void *seed, size_t len);
static void rng_oaep_generate(rng_oaep_ctx *cc, void *dst, size_t len);
static void rng_oaep_update(rng_oaep_ctx *cc, const void *src, size_t len);

static const br_prng_class rng_oaep_vtable = {
	sizeof(rng_oaep_ctx),
	(void (*)(const br_prng_class **,
		const void *, const void *, size_t))&rng_oaep_init,
	(void (*)(const br_prng_class **,
		void *, size_t))&rng_oaep_generate,
	(void (*)(const br_prng_class **,
		const void *, size_t))&rng_oaep_update
};

static void
rng_oaep_init(rng_oaep_ctx *cc, const void *params,
	const void *seed, size_t len)
{
	(void)params;
	if (len > sizeof cc->buf) {
		fprintf(stderr, "seed is too large (%lu bytes)\n",
			(unsigned long)len);
		exit(EXIT_FAILURE);
	}
	cc->vtable = &rng_oaep_vtable;
	memcpy(cc->buf, seed, len);
	cc->ptr = 0;
	cc->len = len;
}

static void
rng_oaep_generate(rng_oaep_ctx *cc, void *dst, size_t len)
{
	if (len > (cc->len - cc->ptr)) {
		fprintf(stderr, "asking for more data than expected\n");
		exit(EXIT_FAILURE);
	}
	memcpy(dst, cc->buf + cc->ptr, len);
	cc->ptr += len;
}

static void
rng_oaep_update(rng_oaep_ctx *cc, const void *src, size_t len)
{
	(void)cc;
	(void)src;
	(void)len;
	fprintf(stderr, "unexpected update\n");
	exit(EXIT_FAILURE);
}

static void
test_RSA_OAEP(const char *name,
	br_rsa_oaep_encrypt menc, br_rsa_oaep_decrypt mdec)
{
	size_t u;

	printf("Test %s: ", name);
	fflush(stdout);

	u = 0;
	while (KAT_RSA_OAEP[u] != NULL) {
		unsigned char n[512];
		unsigned char e[8];
		unsigned char p[256];
		unsigned char q[256];
		unsigned char dp[256];
		unsigned char dq[256];
		unsigned char iq[256];
		br_rsa_public_key pk;
		br_rsa_private_key sk;
		size_t v;

		pk.n = n;
		pk.nlen = hextobin(n, KAT_RSA_OAEP[u ++]);
		pk.e = e;
		pk.elen = hextobin(e, KAT_RSA_OAEP[u ++]);

		for (v = 0; n[v] == 0; v ++);
		sk.n_bitlen = BIT_LENGTH(n[v]) + ((pk.nlen - 1 - v) << 3);
		sk.p = p;
		sk.plen = hextobin(p, KAT_RSA_OAEP[u ++]);
		sk.q = q;
		sk.qlen = hextobin(q, KAT_RSA_OAEP[u ++]);
		sk.dp = dp;
		sk.dplen = hextobin(dp, KAT_RSA_OAEP[u ++]);
		sk.dq = dq;
		sk.dqlen = hextobin(dq, KAT_RSA_OAEP[u ++]);
		sk.iq = iq;
		sk.iqlen = hextobin(iq, KAT_RSA_OAEP[u ++]);

		for (v = 0; v < 6; v ++) {
			unsigned char plain[512], seed[128], cipher[512];
			size_t plain_len, seed_len, cipher_len;
			rng_oaep_ctx rng;
			unsigned char tmp[513];
			size_t len;

			plain_len = hextobin(plain, KAT_RSA_OAEP[u ++]);
			seed_len = hextobin(seed, KAT_RSA_OAEP[u ++]);
			cipher_len = hextobin(cipher, KAT_RSA_OAEP[u ++]);
			rng_oaep_init(&rng, NULL, seed, seed_len);

			len = menc(&rng.vtable, &br_sha1_vtable, NULL, 0, &pk,
				tmp, sizeof tmp, plain, plain_len);
			if (len != cipher_len) {
				fprintf(stderr,
					"wrong encrypted length: %lu vs %lu\n",
					(unsigned long)len,
					(unsigned long)cipher_len);
			}
			if (rng.ptr != rng.len) {
				fprintf(stderr, "seed not fully consumed\n");
				exit(EXIT_FAILURE);
			}
			check_equals("KAT RSA/OAEP encrypt", tmp, cipher, len);

			if (mdec(&br_sha1_vtable, NULL, 0,
				&sk, tmp, &len) != 1)
			{
				fprintf(stderr, "decryption failed\n");
				exit(EXIT_FAILURE);
			}
			if (len != plain_len) {
				fprintf(stderr,
					"wrong decrypted length: %lu vs %lu\n",
					(unsigned long)len,
					(unsigned long)plain_len);
			}
			check_equals("KAT RSA/OAEP decrypt", tmp, plain, len);

			/*
			 * Try with a different label; it should fail.
			 */
			memcpy(tmp, cipher, cipher_len);
			len = cipher_len;
			if (mdec(&br_sha1_vtable, "T", 1,
				&sk, tmp, &len) != 0)
			{
				fprintf(stderr, "decryption should have failed"
					" (wrong label)\n");
				exit(EXIT_FAILURE);
			}

			/*
			 * Try with a the wrong length; it should fail.
			 */
			tmp[0] = 0x00;
			memcpy(tmp + 1, cipher, cipher_len);
			len = cipher_len + 1;
			if (mdec(&br_sha1_vtable, "T", 1,
				&sk, tmp, &len) != 0)
			{
				fprintf(stderr, "decryption should have failed"
					" (wrong length)\n");
				exit(EXIT_FAILURE);
			}

			printf(".");
			fflush(stdout);
		}
	}

	printf(" done.\n");
	fflush(stdout);
}

static void
test_RSA_keygen(const char *name, br_rsa_keygen kg, br_rsa_compute_modulus cm,
	br_rsa_compute_pubexp ce, br_rsa_compute_privexp cd,
	br_rsa_public pub, br_rsa_pkcs1_sign sign, br_rsa_pkcs1_vrfy vrfy)
{
	br_hmac_drbg_context rng;
	int i;

	printf("Test %s: ", name);
	fflush(stdout);

	br_hmac_drbg_init(&rng, &br_sha256_vtable, "seed for RSA keygen", 19);

	for (i = 0; i <= 42; i ++) {
		unsigned size;
		uint32_t pubexp, z;
		br_rsa_private_key sk;
		br_rsa_public_key pk, pk2;
		unsigned char kbuf_priv[BR_RSA_KBUF_PRIV_SIZE(2048)];
		unsigned char kbuf_pub[BR_RSA_KBUF_PUB_SIZE(2048)];
		unsigned char n2[256], d[256], msg1[256], msg2[256];
		uint32_t mod[256];
		uint32_t cc;
		size_t u, v;
		unsigned char sig[257], hv[32], hv2[sizeof hv];
		unsigned mask1, mask2;
		int j;

		if (i <= 35) {
			size = 1024 + i;
			pubexp = 17;
		} else if (i <= 40) {
			size = 2048;
			pubexp = (i << 1) - 69;
		} else {
			size = 2048;
			pubexp = 0xFFFFFFFF;
		}

		if (!kg(&rng.vtable,
			&sk, kbuf_priv, &pk, kbuf_pub, size, pubexp))
		{
			fprintf(stderr, "RSA key pair generation failure\n");
			exit(EXIT_FAILURE);
		}

		z = pubexp;
		for (u = pk.elen; u > 0; u --) {
			if (pk.e[u - 1] != (z & 0xFF)) {
				fprintf(stderr, "wrong public exponent\n");
				exit(EXIT_FAILURE);
			}
			z >>= 8;
		}
		if (z != 0) {
			fprintf(stderr, "truncated public exponent\n");
			exit(EXIT_FAILURE);
		}

		memset(mod, 0, sizeof mod);
		for (u = 0; u < sk.plen; u ++) {
			for (v = 0; v < sk.qlen; v ++) {
				mod[u + v] += (uint32_t)sk.p[sk.plen - 1 - u]
					* (uint32_t)sk.q[sk.qlen - 1 - v];
			}
		}
		cc = 0;
		for (u = 0; u < sk.plen + sk.qlen; u ++) {
			mod[u] += cc;
			cc = mod[u] >> 8;
			mod[u] &= 0xFF;
		}
		for (u = 0; u < pk.nlen; u ++) {
			if (mod[pk.nlen - 1 - u] != pk.n[u]) {
				fprintf(stderr, "wrong modulus\n");
				exit(EXIT_FAILURE);
			}
		}
		if (sk.n_bitlen != size) {
			fprintf(stderr, "wrong key size\n");
			exit(EXIT_FAILURE);
		}
		if (pk.nlen != (size + 7) >> 3) {
			fprintf(stderr, "wrong modulus size (bytes)\n");
			exit(EXIT_FAILURE);
		}
		mask1 = 0x01 << ((size + 7) & 7);
		mask2 = 0xFF & -mask1;
		if ((pk.n[0] & mask2) != mask1) {
			fprintf(stderr, "wrong modulus size (bits)\n");
			exit(EXIT_FAILURE);
		}

		if (cm(NULL, &sk) != pk.nlen) {
			fprintf(stderr, "wrong recomputed modulus length\n");
			exit(EXIT_FAILURE);
		}
		if (cm(n2, &sk) != pk.nlen || memcmp(pk.n, n2, pk.nlen) != 0) {
			fprintf(stderr, "wrong recomputed modulus value\n");
			exit(EXIT_FAILURE);
		}

		z = ce(&sk);
		if (z != pubexp) {
			fprintf(stderr,
				"wrong recomputed pubexp: %lu (exp: %lu)\n",
				(unsigned long)z, (unsigned long)pubexp);
			exit(EXIT_FAILURE);
		}

		if (cd(NULL, &sk, pubexp) != pk.nlen) {
			fprintf(stderr,
				"wrong recomputed privexp length (1)\n");
			exit(EXIT_FAILURE);
		}
		if (cd(d, &sk, pubexp) != pk.nlen) {
			fprintf(stderr,
				"wrong recomputed privexp length (2)\n");
			exit(EXIT_FAILURE);
		}
		/*
		 * To check that the private exponent is correct, we make
		 * it into a _public_ key, and use the public-key operation
		 * to perform the modular exponentiation.
		 */
		pk2 = pk;
		pk2.e = d;
		pk2.elen = pk.nlen;
		rng.vtable->generate(&rng.vtable, msg1, pk.nlen);
		msg1[0] = 0x00;
		memcpy(msg2, msg1, pk.nlen);
		if (!pub(msg2, pk.nlen, &pk2) || !pub(msg2, pk.nlen, &pk)) {
			fprintf(stderr, "public-key operation error\n");
			exit(EXIT_FAILURE);
		}
		if (memcmp(msg1, msg2, pk.nlen) != 0) {
			fprintf(stderr, "wrong recomputed privexp\n");
			exit(EXIT_FAILURE);
		}

		/*
		 * We test the RSA operation over a some random messages.
		 */
		for (j = 0; j < 20; j ++) {
			rng.vtable->generate(&rng.vtable, hv, sizeof hv);
			memset(sig, 0, sizeof sig);
			sig[pk.nlen] = 0x00;
			if (!sign(BR_HASH_OID_SHA256,
				hv, sizeof hv, &sk, sig))
			{
				fprintf(stderr,
					"signature error (%d)\n", j);
				exit(EXIT_FAILURE);
			}
			if (sig[pk.nlen] != 0x00) {
				fprintf(stderr,
					"signature length error (%d)\n", j);
				exit(EXIT_FAILURE);
			}
			if (!vrfy(sig, pk.nlen, BR_HASH_OID_SHA256, sizeof hv,
				&pk, hv2))
			{
				fprintf(stderr,
					"signature verif error (%d)\n", j);
				exit(EXIT_FAILURE);
			}
			if (memcmp(hv, hv2, sizeof hv) != 0) {
				fprintf(stderr,
					"signature extract error (%d)\n", j);
				exit(EXIT_FAILURE);
			}
		}

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
	fflush(stdout);
}

static void
test_RSA_i15(void)
{
	test_RSA_core("RSA i15 core", &br_rsa_i15_public, &br_rsa_i15_private);
	test_RSA_sign("RSA i15 sign", &br_rsa_i15_private,
		&br_rsa_i15_pkcs1_sign, &br_rsa_i15_pkcs1_vrfy);
	test_RSA_OAEP("RSA i15 OAEP",
		&br_rsa_i15_oaep_encrypt, &br_rsa_i15_oaep_decrypt);
	test_RSA_keygen("RSA i15 keygen", &br_rsa_i15_keygen,
		&br_rsa_i15_compute_modulus, &br_rsa_i15_compute_pubexp,
		&br_rsa_i15_compute_privexp, &br_rsa_i15_public,
		&br_rsa_i15_pkcs1_sign, &br_rsa_i15_pkcs1_vrfy);
}

static void
test_RSA_i31(void)
{
	test_RSA_core("RSA i31 core", &br_rsa_i31_public, &br_rsa_i31_private);
	test_RSA_sign("RSA i31 sign", &br_rsa_i31_private,
		&br_rsa_i31_pkcs1_sign, &br_rsa_i31_pkcs1_vrfy);
	test_RSA_OAEP("RSA i31 OAEP",
		&br_rsa_i31_oaep_encrypt, &br_rsa_i31_oaep_decrypt);
	test_RSA_keygen("RSA i31 keygen", &br_rsa_i31_keygen,
		&br_rsa_i31_compute_modulus, &br_rsa_i31_compute_pubexp,
		&br_rsa_i31_compute_privexp, &br_rsa_i31_public,
		&br_rsa_i31_pkcs1_sign, &br_rsa_i31_pkcs1_vrfy);
}

static void
test_RSA_i32(void)
{
	test_RSA_core("RSA i32 core", &br_rsa_i32_public, &br_rsa_i32_private);
	test_RSA_sign("RSA i32 sign", &br_rsa_i32_private,
		&br_rsa_i32_pkcs1_sign, &br_rsa_i32_pkcs1_vrfy);
	test_RSA_OAEP("RSA i32 OAEP",
		&br_rsa_i32_oaep_encrypt, &br_rsa_i32_oaep_decrypt);
}

static void
test_RSA_i62(void)
{
	br_rsa_public pub;
	br_rsa_private priv;
	br_rsa_pkcs1_sign sign;
	br_rsa_pkcs1_vrfy vrfy;
	br_rsa_oaep_encrypt menc;
	br_rsa_oaep_decrypt mdec;
	br_rsa_keygen kgen;

	pub = br_rsa_i62_public_get();
	priv = br_rsa_i62_private_get();
	sign = br_rsa_i62_pkcs1_sign_get();
	vrfy = br_rsa_i62_pkcs1_vrfy_get();
	menc = br_rsa_i62_oaep_encrypt_get();
	mdec = br_rsa_i62_oaep_decrypt_get();
	kgen = br_rsa_i62_keygen_get();
	if (pub) {
		if (!priv || !sign || !vrfy || !menc || !mdec || !kgen) {
			fprintf(stderr, "Inconsistent i62 availability\n");
			exit(EXIT_FAILURE);
		}
		test_RSA_core("RSA i62 core", pub, priv);
		test_RSA_sign("RSA i62 sign", priv, sign, vrfy);
		test_RSA_OAEP("RSA i62 OAEP", menc, mdec);
		test_RSA_keygen("RSA i62 keygen", kgen,
			&br_rsa_i31_compute_modulus, &br_rsa_i31_compute_pubexp,
			&br_rsa_i31_compute_privexp, pub,
			sign, vrfy);
	} else {
		if (priv || sign || vrfy || menc || mdec || kgen) {
			fprintf(stderr, "Inconsistent i62 availability\n");
			exit(EXIT_FAILURE);
		}
		printf("Test RSA i62: UNAVAILABLE\n");
	}
}

#if 0
static void
test_RSA_signatures(void)
{
	uint32_t n[40], e[2], p[20], q[20], dp[20], dq[20], iq[20], x[40];
	unsigned char hv[20], sig[128];
	unsigned char ref[128], tmp[128];
	br_sha1_context hc;

	printf("Test RSA signatures: ");
	fflush(stdout);

	/*
	 * Decode RSA key elements.
	 */
	br_int_decode(n, sizeof n / sizeof n[0], RSA_N, sizeof RSA_N);
	br_int_decode(e, sizeof e / sizeof e[0], RSA_E, sizeof RSA_E);
	br_int_decode(p, sizeof p / sizeof p[0], RSA_P, sizeof RSA_P);
	br_int_decode(q, sizeof q / sizeof q[0], RSA_Q, sizeof RSA_Q);
	br_int_decode(dp, sizeof dp / sizeof dp[0], RSA_DP, sizeof RSA_DP);
	br_int_decode(dq, sizeof dq / sizeof dq[0], RSA_DQ, sizeof RSA_DQ);
	br_int_decode(iq, sizeof iq / sizeof iq[0], RSA_IQ, sizeof RSA_IQ);

	/*
	 * Decode reference signature (computed with OpenSSL).
	 */
	hextobin(ref, "45A3DC6A106BCD3BD0E48FB579643AA3FF801E5903E80AA9B43A695A8E7F454E93FA208B69995FF7A6D5617C2FEB8E546375A664977A48931842AAE796B5A0D64393DCA35F3490FC157F5BD83B9D58C2F7926E6AE648A2BD96CAB8FCCD3D35BB11424AD47D973FF6D69CA774841AEC45DFAE99CCF79893E7047FDE6CB00AA76D");

	/*
	 * Recompute signature. Since PKCS#1 v1.5 signatures are
	 * deterministic, we should get the same as the reference signature.
	 */
	br_sha1_init(&hc);
	br_sha1_update(&hc, "test", 4);
	br_sha1_out(&hc, hv);
	if (!br_rsa_sign(sig, sizeof sig, p, q, dp, dq, iq, br_sha1_ID, hv)) {
		fprintf(stderr, "RSA-1024/SHA-1 sig generate failed\n");
		exit(EXIT_FAILURE);
	}
	check_equals("KAT RSA-sign 1", sig, ref, sizeof sig);

	/*
	 * Verify signature.
	 */
	if (!br_rsa_verify(sig, sizeof sig, n, e, br_sha1_ID, hv)) {
		fprintf(stderr, "RSA-1024/SHA-1 sig verify failed\n");
		exit(EXIT_FAILURE);
	}
	hv[5] ^= 0x01;
	if (br_rsa_verify(sig, sizeof sig, n, e, br_sha1_ID, hv)) {
		fprintf(stderr, "RSA-1024/SHA-1 sig verify should have failed\n");
		exit(EXIT_FAILURE);
	}
	hv[5] ^= 0x01;

	/*
	 * Generate a signature with the alternate encoding (no NULL) and
	 * verify it.
	 */
	hextobin(tmp, "0001FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00301F300706052B0E03021A0414A94A8FE5CCB19BA61C4C0873D391E987982FBBD3");
	br_int_decode(x, sizeof x / sizeof x[0], tmp, sizeof tmp);
	x[0] = n[0];
	br_rsa_private_core(x, p, q, dp, dq, iq);
	br_int_encode(sig, sizeof sig, x);
	if (!br_rsa_verify(sig, sizeof sig, n, e, br_sha1_ID, hv)) {
		fprintf(stderr, "RSA-1024/SHA-1 sig verify (alt) failed\n");
		exit(EXIT_FAILURE);
	}
	hv[5] ^= 0x01;
	if (br_rsa_verify(sig, sizeof sig, n, e, br_sha1_ID, hv)) {
		fprintf(stderr, "RSA-1024/SHA-1 sig verify (alt) should have failed\n");
		exit(EXIT_FAILURE);
	}
	hv[5] ^= 0x01;

	printf("done.\n");
	fflush(stdout);
}
#endif

/*
 * From: http://csrc.nist.gov/groups/ST/toolkit/BCM/documents/proposedmodes/gcm/gcm-revised-spec.pdf
 */
static const char *const KAT_GHASH[] = {

	"66e94bd4ef8a2c3b884cfa59ca342b2e",
	"",
	"",
	"00000000000000000000000000000000",

	"66e94bd4ef8a2c3b884cfa59ca342b2e",
	"",
	"0388dace60b6a392f328c2b971b2fe78",
	"f38cbb1ad69223dcc3457ae5b6b0f885",

	"b83b533708bf535d0aa6e52980d53b78",
	"",
	"42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985",
	"7f1b32b81b820d02614f8895ac1d4eac",

	"b83b533708bf535d0aa6e52980d53b78",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091",
	"698e57f70e6ecc7fd9463b7260a9ae5f",

	"b83b533708bf535d0aa6e52980d53b78",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"61353b4c2806934a777ff51fa22a4755699b2a714fcdc6f83766e5f97b6c742373806900e49f24b22b097544d4896b424989b5e1ebac0f07c23f4598",
	"df586bb4c249b92cb6922877e444d37b",

	"b83b533708bf535d0aa6e52980d53b78",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"8ce24998625615b603a033aca13fb894be9112a5c3a211a8ba262a3cca7e2ca701e4a9a4fba43c90ccdcb281d48c7c6fd62875d2aca417034c34aee5",
	"1c5afe9760d3932f3c9a878aac3dc3de",

	"aae06992acbf52a3e8f4a96ec9300bd7",
	"",
	"98e7247c07f0fe411c267e4384b0f600",
	"e2c63f0ac44ad0e02efa05ab6743d4ce",

	"466923ec9ae682214f2c082badb39249",
	"",
	"3980ca0b3c00e841eb06fac4872a2757859e1ceaa6efd984628593b40ca1e19c7d773d00c144c525ac619d18c84a3f4718e2448b2fe324d9ccda2710acade256",
	"51110d40f6c8fff0eb1ae33445a889f0",

	"466923ec9ae682214f2c082badb39249",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"3980ca0b3c00e841eb06fac4872a2757859e1ceaa6efd984628593b40ca1e19c7d773d00c144c525ac619d18c84a3f4718e2448b2fe324d9ccda2710",
	"ed2ce3062e4a8ec06db8b4c490e8a268",

	"466923ec9ae682214f2c082badb39249",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"0f10f599ae14a154ed24b36e25324db8c566632ef2bbb34f8347280fc4507057fddc29df9a471f75c66541d4d4dad1c9e93a19a58e8b473fa0f062f7",
	"1e6a133806607858ee80eaf237064089",

	"466923ec9ae682214f2c082badb39249",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"d27e88681ce3243c4830165a8fdcf9ff1de9a1d8e6b447ef6ef7b79828666e4581e79012af34ddd9e2f037589b292db3e67c036745fa22e7e9b7373b",
	"82567fb0b4cc371801eadec005968e94",

	"dc95c078a2408989ad48a21492842087",
	"",
	"cea7403d4d606b6e074ec5d3baf39d18",
	"83de425c5edc5d498f382c441041ca92",

	"acbef20579b4b8ebce889bac8732dad7",
	"",
	"522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662898015ad",
	"4db870d37cb75fcb46097c36230d1612",

	"acbef20579b4b8ebce889bac8732dad7",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662",
	"8bd0c4d8aacd391e67cca447e8c38f65",

	"acbef20579b4b8ebce889bac8732dad7",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"c3762df1ca787d32ae47c13bf19844cbaf1ae14d0b976afac52ff7d79bba9de0feb582d33934a4f0954cc2363bc73f7862ac430e64abe499f47c9b1f",
	"75a34288b8c68f811c52b2e9a2f97f63",

	"acbef20579b4b8ebce889bac8732dad7",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"5a8def2f0c9e53f1f75d7853659e2a20eeb2b22aafde6419a058ab4f6f746bf40fc0c3b780f244452da3ebf1c5d82cdea2418997200ef82e44ae7e3f",
	"d5ffcf6fc5ac4d69722187421a7f170b",

	NULL,
};

static void
test_GHASH(const char *name, br_ghash gh)
{
	size_t u;

	printf("Test %s: ", name);
	fflush(stdout);

	for (u = 0; KAT_GHASH[u]; u += 4) {
		unsigned char h[16];
		unsigned char a[100];
		size_t a_len;
		unsigned char c[100];
		size_t c_len;
		unsigned char p[16];
		unsigned char y[16];
		unsigned char ref[16];

		hextobin(h, KAT_GHASH[u]);
		a_len = hextobin(a, KAT_GHASH[u + 1]);
		c_len = hextobin(c, KAT_GHASH[u + 2]);
		hextobin(ref, KAT_GHASH[u + 3]);
		memset(y, 0, sizeof y);
		gh(y, h, a, a_len);
		gh(y, h, c, c_len);
		memset(p, 0, sizeof p);
		br_enc32be(p + 4, (uint32_t)a_len << 3);
		br_enc32be(p + 12, (uint32_t)c_len << 3);
		gh(y, h, p, sizeof p);
		check_equals("KAT GHASH", y, ref, sizeof ref);
	}

	for (u = 0; u <= 1024; u ++) {
		unsigned char key[32], iv[12];
		unsigned char buf[1024 + 32];
		unsigned char y0[16], y1[16];
		char tmp[100];

		memset(key, 0, sizeof key);
		memset(iv, 0, sizeof iv);
		br_enc32be(key, u);
		memset(buf, 0, sizeof buf);
		br_chacha20_ct_run(key, iv, 1, buf, sizeof buf);

		memcpy(y0, buf, 16);
		br_ghash_ctmul32(y0, buf + 16, buf + 32, u);
		memcpy(y1, buf, 16);
		gh(y1, buf + 16, buf + 32, u);
		sprintf(tmp, "XREF %s (len = %u)", name, (unsigned)u);
		check_equals(tmp, y0, y1, 16);

		if ((u & 31) == 0) {
			printf(".");
			fflush(stdout);
		}
	}

	printf("done.\n");
	fflush(stdout);
}

static void
test_GHASH_ctmul(void)
{
	test_GHASH("GHASH_ctmul", br_ghash_ctmul);
}

static void
test_GHASH_ctmul32(void)
{
	test_GHASH("GHASH_ctmul32", br_ghash_ctmul32);
}

static void
test_GHASH_ctmul64(void)
{
	test_GHASH("GHASH_ctmul64", br_ghash_ctmul64);
}

static void
test_GHASH_pclmul(void)
{
	br_ghash gh;

	gh = br_ghash_pclmul_get();
	if (gh == 0) {
		printf("Test GHASH_pclmul: UNAVAILABLE\n");
	} else {
		test_GHASH("GHASH_pclmul", gh);
	}
}

static void
test_GHASH_pwr8(void)
{
	br_ghash gh;

	gh = br_ghash_pwr8_get();
	if (gh == 0) {
		printf("Test GHASH_pwr8: UNAVAILABLE\n");
	} else {
		test_GHASH("GHASH_pwr8", gh);
	}
}

/*
 * From: http://csrc.nist.gov/groups/ST/toolkit/BCM/documents/proposedmodes/gcm/gcm-revised-spec.pdf
 *
 * Order: key, plaintext, AAD, IV, ciphertext, tag
 */
static const char *const KAT_GCM[] = {
	"00000000000000000000000000000000",
	"",
	"",
	"000000000000000000000000",
	"",
	"58e2fccefa7e3061367f1d57a4e7455a",

	"00000000000000000000000000000000",
	"00000000000000000000000000000000",
	"",
	"000000000000000000000000",
	"0388dace60b6a392f328c2b971b2fe78",
	"ab6e47d42cec13bdf53a67b21257bddf",

	"feffe9928665731c6d6a8f9467308308",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255",
	"",
	"cafebabefacedbaddecaf888",
	"42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985",
	"4d5c2af327cd64a62cf35abd2ba6fab4",

	"feffe9928665731c6d6a8f9467308308",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"cafebabefacedbaddecaf888",
	"42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091",
	"5bc94fbc3221a5db94fae95ae7121a47",

	"feffe9928665731c6d6a8f9467308308",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"cafebabefacedbad",
	"61353b4c2806934a777ff51fa22a4755699b2a714fcdc6f83766e5f97b6c742373806900e49f24b22b097544d4896b424989b5e1ebac0f07c23f4598",
	"3612d2e79e3b0785561be14aaca2fccb",

	"feffe9928665731c6d6a8f9467308308",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"9313225df88406e555909c5aff5269aa6a7a9538534f7da1e4c303d2a318a728c3c0c95156809539fcf0e2429a6b525416aedbf5a0de6a57a637b39b",
	"8ce24998625615b603a033aca13fb894be9112a5c3a211a8ba262a3cca7e2ca701e4a9a4fba43c90ccdcb281d48c7c6fd62875d2aca417034c34aee5",
	"619cc5aefffe0bfa462af43c1699d050",

	"000000000000000000000000000000000000000000000000",
	"",
	"",
	"000000000000000000000000",
	"",
	"cd33b28ac773f74ba00ed1f312572435",

	"000000000000000000000000000000000000000000000000",
	"00000000000000000000000000000000",
	"",
	"000000000000000000000000",
	"98e7247c07f0fe411c267e4384b0f600",
	"2ff58d80033927ab8ef4d4587514f0fb",

	"feffe9928665731c6d6a8f9467308308feffe9928665731c",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255",
	"",
	"cafebabefacedbaddecaf888",
	"3980ca0b3c00e841eb06fac4872a2757859e1ceaa6efd984628593b40ca1e19c7d773d00c144c525ac619d18c84a3f4718e2448b2fe324d9ccda2710acade256",
	"9924a7c8587336bfb118024db8674a14",

	"feffe9928665731c6d6a8f9467308308feffe9928665731c",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"cafebabefacedbaddecaf888",
	"3980ca0b3c00e841eb06fac4872a2757859e1ceaa6efd984628593b40ca1e19c7d773d00c144c525ac619d18c84a3f4718e2448b2fe324d9ccda2710",
	"2519498e80f1478f37ba55bd6d27618c",

	"feffe9928665731c6d6a8f9467308308feffe9928665731c",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"cafebabefacedbad",
	"0f10f599ae14a154ed24b36e25324db8c566632ef2bbb34f8347280fc4507057fddc29df9a471f75c66541d4d4dad1c9e93a19a58e8b473fa0f062f7",
	"65dcc57fcf623a24094fcca40d3533f8",

	"feffe9928665731c6d6a8f9467308308feffe9928665731c",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"9313225df88406e555909c5aff5269aa6a7a9538534f7da1e4c303d2a318a728c3c0c95156809539fcf0e2429a6b525416aedbf5a0de6a57a637b39b",
	"d27e88681ce3243c4830165a8fdcf9ff1de9a1d8e6b447ef6ef7b79828666e4581e79012af34ddd9e2f037589b292db3e67c036745fa22e7e9b7373b",
	"dcf566ff291c25bbb8568fc3d376a6d9",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"",
	"",
	"000000000000000000000000",
	"",
	"530f8afbc74536b9a963b4f1c4cb738b",

	"0000000000000000000000000000000000000000000000000000000000000000",
	"00000000000000000000000000000000",
	"",
	"000000000000000000000000",
	"cea7403d4d606b6e074ec5d3baf39d18",
	"d0d1c8a799996bf0265b98b5d48ab919",

	"feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255",
	"",
	"cafebabefacedbaddecaf888",
	"522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662898015ad",
	"b094dac5d93471bdec1a502270e3cc6c",

	"feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"cafebabefacedbaddecaf888",
	"522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662",
	"76fc6ece0f4e1768cddf8853bb2d551b",

	"feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"cafebabefacedbad",
	"c3762df1ca787d32ae47c13bf19844cbaf1ae14d0b976afac52ff7d79bba9de0feb582d33934a4f0954cc2363bc73f7862ac430e64abe499f47c9b1f",
	"3a337dbf46a792c45e454913fe2ea8f2",

	"feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308",
	"d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39",
	"feedfacedeadbeeffeedfacedeadbeefabaddad2",
	"9313225df88406e555909c5aff5269aa6a7a9538534f7da1e4c303d2a318a728c3c0c95156809539fcf0e2429a6b525416aedbf5a0de6a57a637b39b",
	"5a8def2f0c9e53f1f75d7853659e2a20eeb2b22aafde6419a058ab4f6f746bf40fc0c3b780f244452da3ebf1c5d82cdea2418997200ef82e44ae7e3f",
	"a44a8266ee1c8eb0c8b5d4cf5ae9f19a",

	NULL
};

static void
test_GCM(void)
{
	size_t u;

	printf("Test GCM: ");
	fflush(stdout);

	for (u = 0; KAT_GCM[u]; u += 6) {
		unsigned char key[32];
		unsigned char plain[100];
		unsigned char aad[100];
		unsigned char iv[100];
		unsigned char cipher[100];
		unsigned char tag[100];
		size_t key_len, plain_len, aad_len, iv_len;
		br_aes_ct_ctr_keys bc;
		br_gcm_context gc;
		unsigned char tmp[100], out[16];
		size_t v, tag_len;

		key_len = hextobin(key, KAT_GCM[u]);
		plain_len = hextobin(plain, KAT_GCM[u + 1]);
		aad_len = hextobin(aad, KAT_GCM[u + 2]);
		iv_len = hextobin(iv, KAT_GCM[u + 3]);
		hextobin(cipher, KAT_GCM[u + 4]);
		hextobin(tag, KAT_GCM[u + 5]);

		br_aes_ct_ctr_init(&bc, key, key_len);
		br_gcm_init(&gc, &bc.vtable, br_ghash_ctmul32);

		memset(tmp, 0x54, sizeof tmp);

		/*
		 * Basic operation.
		 */
		memcpy(tmp, plain, plain_len);
		br_gcm_reset(&gc, iv, iv_len);
		br_gcm_aad_inject(&gc, aad, aad_len);
		br_gcm_flip(&gc);
		br_gcm_run(&gc, 1, tmp, plain_len);
		br_gcm_get_tag(&gc, out);
		check_equals("KAT GCM 1", tmp, cipher, plain_len);
		check_equals("KAT GCM 2", out, tag, 16);

		br_gcm_reset(&gc, iv, iv_len);
		br_gcm_aad_inject(&gc, aad, aad_len);
		br_gcm_flip(&gc);
		br_gcm_run(&gc, 0, tmp, plain_len);
		check_equals("KAT GCM 3", tmp, plain, plain_len);
		if (!br_gcm_check_tag(&gc, tag)) {
			fprintf(stderr, "Tag not verified (1)\n");
			exit(EXIT_FAILURE);
		}

		for (v = plain_len; v < sizeof tmp; v ++) {
			if (tmp[v] != 0x54) {
				fprintf(stderr, "overflow on data\n");
				exit(EXIT_FAILURE);
			}
		}

		/*
		 * Byte-by-byte injection.
		 */
		br_gcm_reset(&gc, iv, iv_len);
		for (v = 0; v < aad_len; v ++) {
			br_gcm_aad_inject(&gc, aad + v, 1);
		}
		br_gcm_flip(&gc);
		for (v = 0; v < plain_len; v ++) {
			br_gcm_run(&gc, 1, tmp + v, 1);
		}
		check_equals("KAT GCM 4", tmp, cipher, plain_len);
		if (!br_gcm_check_tag(&gc, tag)) {
			fprintf(stderr, "Tag not verified (2)\n");
			exit(EXIT_FAILURE);
		}

		br_gcm_reset(&gc, iv, iv_len);
		for (v = 0; v < aad_len; v ++) {
			br_gcm_aad_inject(&gc, aad + v, 1);
		}
		br_gcm_flip(&gc);
		for (v = 0; v < plain_len; v ++) {
			br_gcm_run(&gc, 0, tmp + v, 1);
		}
		br_gcm_get_tag(&gc, out);
		check_equals("KAT GCM 5", tmp, plain, plain_len);
		check_equals("KAT GCM 6", out, tag, 16);

		/*
		 * Check that alterations are detected.
		 */
		for (v = 0; v < aad_len; v ++) {
			memcpy(tmp, cipher, plain_len);
			br_gcm_reset(&gc, iv, iv_len);
			aad[v] ^= 0x04;
			br_gcm_aad_inject(&gc, aad, aad_len);
			aad[v] ^= 0x04;
			br_gcm_flip(&gc);
			br_gcm_run(&gc, 0, tmp, plain_len);
			check_equals("KAT GCM 7", tmp, plain, plain_len);
			if (br_gcm_check_tag(&gc, tag)) {
				fprintf(stderr, "Tag should have changed\n");
				exit(EXIT_FAILURE);
			}
		}

		/*
		 * Tag truncation.
		 */
		for (tag_len = 1; tag_len <= 16; tag_len ++) {
			memset(out, 0x54, sizeof out);
			memcpy(tmp, plain, plain_len);
			br_gcm_reset(&gc, iv, iv_len);
			br_gcm_aad_inject(&gc, aad, aad_len);
			br_gcm_flip(&gc);
			br_gcm_run(&gc, 1, tmp, plain_len);
			br_gcm_get_tag_trunc(&gc, out, tag_len);
			check_equals("KAT GCM 8", out, tag, tag_len);
			for (v = tag_len; v < sizeof out; v ++) {
				if (out[v] != 0x54) {
					fprintf(stderr, "overflow on tag\n");
					exit(EXIT_FAILURE);
				}
			}

			memcpy(tmp, plain, plain_len);
			br_gcm_reset(&gc, iv, iv_len);
			br_gcm_aad_inject(&gc, aad, aad_len);
			br_gcm_flip(&gc);
			br_gcm_run(&gc, 1, tmp, plain_len);
			if (!br_gcm_check_tag_trunc(&gc, out, tag_len)) {
				fprintf(stderr, "Tag not verified (3)\n");
				exit(EXIT_FAILURE);
			}
		}

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
	fflush(stdout);
}

/*
 * From "The EAX Mode of Operation (A Two-Pass Authenticated Encryption
 * Scheme Optimized for Simplicity and Efficiency)" (Bellare, Rogaway,
 * Wagner), presented at FSE 2004. Full article is available at:
 *   http://web.cs.ucdavis.edu/~rogaway/papers/eax.html
 *
 * EAX specification concatenates the authentication tag at the end of
 * the ciphertext; in our API and the vectors below, the tag is separate.
 *
 * Order is: plaintext, key, nonce, header, ciphertext, tag.
 */
static const char *const KAT_EAX[] = {
	"",
	"233952dee4d5ed5f9b9c6d6ff80ff478",
	"62ec67f9c3a4a407fcb2a8c49031a8b3",
	"6bfb914fd07eae6b",
	"",
	"e037830e8389f27b025a2d6527e79d01",

	"f7fb",
	"91945d3f4dcbee0bf45ef52255f095a4",
	"becaf043b0a23d843194ba972c66debd",
	"fa3bfd4806eb53fa",
	"19dd",
	"5c4c9331049d0bdab0277408f67967e5",

	"1a47cb4933",
	"01f74ad64077f2e704c0f60ada3dd523",
	"70c3db4f0d26368400a10ed05d2bff5e",
	"234a3463c1264ac6",
	"d851d5bae0",
	"3a59f238a23e39199dc9266626c40f80",

	"481c9e39b1",
	"d07cf6cbb7f313bdde66b727afd3c5e8",
	"8408dfff3c1a2b1292dc199e46b7d617",
	"33cce2eabff5a79d",
	"632a9d131a",
	"d4c168a4225d8e1ff755939974a7bede",

	"40d0c07da5e4",
	"35b6d0580005bbc12b0587124557d2c2",
	"fdb6b06676eedc5c61d74276e1f8e816",
	"aeb96eaebe2970e9",
	"071dfe16c675",
	"cb0677e536f73afe6a14b74ee49844dd",

	"4de3b35c3fc039245bd1fb7d",
	"bd8e6e11475e60b268784c38c62feb22",
	"6eac5c93072d8e8513f750935e46da1b",
	"d4482d1ca78dce0f",
	"835bb4f15d743e350e728414",
	"abb8644fd6ccb86947c5e10590210a4f",

	"8b0a79306c9ce7ed99dae4f87f8dd61636",
	"7c77d6e813bed5ac98baa417477a2e7d",
	"1a8c98dcd73d38393b2bf1569deefc19",
	"65d2017990d62528",
	"02083e3979da014812f59f11d52630da30",
	"137327d10649b0aa6e1c181db617d7f2",

	"1bda122bce8a8dbaf1877d962b8592dd2d56",
	"5fff20cafab119ca2fc73549e20f5b0d",
	"dde59b97d722156d4d9aff2bc7559826",
	"54b9f04e6a09189a",
	"2ec47b2c4954a489afc7ba4897edcdae8cc3",
	"3b60450599bd02c96382902aef7f832a",

	"6cf36720872b8513f6eab1a8a44438d5ef11",
	"a4a4782bcffd3ec5e7ef6d8c34a56123",
	"b781fcf2f75fa5a8de97a9ca48e522ec",
	"899a175897561d7e",
	"0de18fd0fdd91e7af19f1d8ee8733938b1e8",
	"e7f6d2231618102fdb7fe55ff1991700",

	"ca40d7446e545ffaed3bd12a740a659ffbbb3ceab7",
	"8395fcf1e95bebd697bd010bc766aac3",
	"22e7add93cfc6393c57ec0b3c17d6b44",
	"126735fcc320d25a",
	"cb8920f87a6c75cff39627b56e3ed197c552d295a7",
	"cfc46afc253b4652b1af3795b124ab6e",

	NULL
};

static void
test_EAX_inner(const char *name, const br_block_ctrcbc_class *vt)
{
	size_t u;

	printf("Test EAX %s: ", name);
	fflush(stdout);

	for (u = 0; KAT_EAX[u]; u += 6) {
		unsigned char plain[100];
		unsigned char key[32];
		unsigned char nonce[100];
		unsigned char aad[100];
		unsigned char cipher[100];
		unsigned char tag[100];
		size_t plain_len, key_len, nonce_len, aad_len;
		br_aes_gen_ctrcbc_keys bc;
		br_eax_context ec;
		br_eax_state st;
		unsigned char tmp[100], out[16];
		size_t v, tag_len;

		plain_len = hextobin(plain, KAT_EAX[u]);
		key_len = hextobin(key, KAT_EAX[u + 1]);
		nonce_len = hextobin(nonce, KAT_EAX[u + 2]);
		aad_len = hextobin(aad, KAT_EAX[u + 3]);
		hextobin(cipher, KAT_EAX[u + 4]);
		hextobin(tag, KAT_EAX[u + 5]);

		vt->init(&bc.vtable, key, key_len);
		br_eax_init(&ec, &bc.vtable);

		memset(tmp, 0x54, sizeof tmp);

		/*
		 * Basic operation.
		 */
		memcpy(tmp, plain, plain_len);
		br_eax_reset(&ec, nonce, nonce_len);
		br_eax_aad_inject(&ec, aad, aad_len);
		br_eax_flip(&ec);
		br_eax_run(&ec, 1, tmp, plain_len);
		br_eax_get_tag(&ec, out);
		check_equals("KAT EAX 1", tmp, cipher, plain_len);
		check_equals("KAT EAX 2", out, tag, 16);

		br_eax_reset(&ec, nonce, nonce_len);
		br_eax_aad_inject(&ec, aad, aad_len);
		br_eax_flip(&ec);
		br_eax_run(&ec, 0, tmp, plain_len);
		check_equals("KAT EAX 3", tmp, plain, plain_len);
		if (!br_eax_check_tag(&ec, tag)) {
			fprintf(stderr, "Tag not verified (1)\n");
			exit(EXIT_FAILURE);
		}

		for (v = plain_len; v < sizeof tmp; v ++) {
			if (tmp[v] != 0x54) {
				fprintf(stderr, "overflow on data\n");
				exit(EXIT_FAILURE);
			}
		}

		/*
		 * Byte-by-byte injection.
		 */
		br_eax_reset(&ec, nonce, nonce_len);
		for (v = 0; v < aad_len; v ++) {
			br_eax_aad_inject(&ec, aad + v, 1);
		}
		br_eax_flip(&ec);
		for (v = 0; v < plain_len; v ++) {
			br_eax_run(&ec, 1, tmp + v, 1);
		}
		check_equals("KAT EAX 4", tmp, cipher, plain_len);
		if (!br_eax_check_tag(&ec, tag)) {
			fprintf(stderr, "Tag not verified (2)\n");
			exit(EXIT_FAILURE);
		}

		br_eax_reset(&ec, nonce, nonce_len);
		for (v = 0; v < aad_len; v ++) {
			br_eax_aad_inject(&ec, aad + v, 1);
		}
		br_eax_flip(&ec);
		for (v = 0; v < plain_len; v ++) {
			br_eax_run(&ec, 0, tmp + v, 1);
		}
		br_eax_get_tag(&ec, out);
		check_equals("KAT EAX 5", tmp, plain, plain_len);
		check_equals("KAT EAX 6", out, tag, 16);

		/*
		 * Check that alterations are detected.
		 */
		for (v = 0; v < aad_len; v ++) {
			memcpy(tmp, cipher, plain_len);
			br_eax_reset(&ec, nonce, nonce_len);
			aad[v] ^= 0x04;
			br_eax_aad_inject(&ec, aad, aad_len);
			aad[v] ^= 0x04;
			br_eax_flip(&ec);
			br_eax_run(&ec, 0, tmp, plain_len);
			check_equals("KAT EAX 7", tmp, plain, plain_len);
			if (br_eax_check_tag(&ec, tag)) {
				fprintf(stderr, "Tag should have changed\n");
				exit(EXIT_FAILURE);
			}
		}

		/*
		 * Tag truncation.
		 */
		for (tag_len = 1; tag_len <= 16; tag_len ++) {
			memset(out, 0x54, sizeof out);
			memcpy(tmp, plain, plain_len);
			br_eax_reset(&ec, nonce, nonce_len);
			br_eax_aad_inject(&ec, aad, aad_len);
			br_eax_flip(&ec);
			br_eax_run(&ec, 1, tmp, plain_len);
			br_eax_get_tag_trunc(&ec, out, tag_len);
			check_equals("KAT EAX 8", out, tag, tag_len);
			for (v = tag_len; v < sizeof out; v ++) {
				if (out[v] != 0x54) {
					fprintf(stderr, "overflow on tag\n");
					exit(EXIT_FAILURE);
				}
			}

			memcpy(tmp, plain, plain_len);
			br_eax_reset(&ec, nonce, nonce_len);
			br_eax_aad_inject(&ec, aad, aad_len);
			br_eax_flip(&ec);
			br_eax_run(&ec, 1, tmp, plain_len);
			if (!br_eax_check_tag_trunc(&ec, out, tag_len)) {
				fprintf(stderr, "Tag not verified (3)\n");
				exit(EXIT_FAILURE);
			}
		}

		printf(".");
		fflush(stdout);

		/*
		 * For capture tests, we need the message to be non-empty.
		 */
		if (plain_len == 0) {
			continue;
		}

		/*
		 * Captured state, pre-AAD. This requires the AAD and the
		 * message to be non-empty.
		 */
		br_eax_capture(&ec, &st);

		if (aad_len > 0) {
			br_eax_reset_pre_aad(&ec, &st, nonce, nonce_len);
			br_eax_aad_inject(&ec, aad, aad_len);
			br_eax_flip(&ec);
			memcpy(tmp, plain, plain_len);
			br_eax_run(&ec, 1, tmp, plain_len);
			br_eax_get_tag(&ec, out);
			check_equals("KAT EAX 9", tmp, cipher, plain_len);
			check_equals("KAT EAX 10", out, tag, 16);

			br_eax_reset_pre_aad(&ec, &st, nonce, nonce_len);
			br_eax_aad_inject(&ec, aad, aad_len);
			br_eax_flip(&ec);
			br_eax_run(&ec, 0, tmp, plain_len);
			br_eax_get_tag(&ec, out);
			check_equals("KAT EAX 11", tmp, plain, plain_len);
			check_equals("KAT EAX 12", out, tag, 16);
		}

		/*
		 * Captured state, post-AAD. This requires the message to
		 * be non-empty.
		 */
		br_eax_reset(&ec, nonce, nonce_len);
		br_eax_aad_inject(&ec, aad, aad_len);
		br_eax_flip(&ec);
		br_eax_get_aad_mac(&ec, &st);

		br_eax_reset_post_aad(&ec, &st, nonce, nonce_len);
		memcpy(tmp, plain, plain_len);
		br_eax_run(&ec, 1, tmp, plain_len);
		br_eax_get_tag(&ec, out);
		check_equals("KAT EAX 13", tmp, cipher, plain_len);
		check_equals("KAT EAX 14", out, tag, 16);

		br_eax_reset_post_aad(&ec, &st, nonce, nonce_len);
		br_eax_run(&ec, 0, tmp, plain_len);
		br_eax_get_tag(&ec, out);
		check_equals("KAT EAX 15", tmp, plain, plain_len);
		check_equals("KAT EAX 16", out, tag, 16);

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
	fflush(stdout);
}

static void
test_EAX(void)
{
	const br_block_ctrcbc_class *x_ctrcbc;

	test_EAX_inner("aes_big", &br_aes_big_ctrcbc_vtable);
	test_EAX_inner("aes_small", &br_aes_small_ctrcbc_vtable);
	test_EAX_inner("aes_ct", &br_aes_ct_ctrcbc_vtable);
	test_EAX_inner("aes_ct64", &br_aes_ct64_ctrcbc_vtable);

	x_ctrcbc = br_aes_x86ni_ctrcbc_get_vtable();
	if (x_ctrcbc != NULL) {
		test_EAX_inner("aes_x86ni", x_ctrcbc);
	} else {
		printf("Test EAX aes_x86ni: UNAVAILABLE\n");
	}

	x_ctrcbc = br_aes_pwr8_ctrcbc_get_vtable();
	if (x_ctrcbc != NULL) {
		test_EAX_inner("aes_pwr8", x_ctrcbc);
	} else {
		printf("Test EAX aes_pwr8: UNAVAILABLE\n");
	}
}

/*
 * From NIST SP 800-38C, appendix C.
 *
 * CCM specification concatenates the authentication tag at the end of
 * the ciphertext; in our API and the vectors below, the tag is separate.
 *
 * Order is: key, nonce, aad, plaintext, ciphertext, tag.
 */
static const char *const KAT_CCM[] = {
	"404142434445464748494a4b4c4d4e4f",
	"10111213141516",
	"0001020304050607",
	"20212223",
	"7162015b",
	"4dac255d",

	"404142434445464748494a4b4c4d4e4f",
	"1011121314151617",
	"000102030405060708090a0b0c0d0e0f",
	"202122232425262728292a2b2c2d2e2f",
	"d2a1f0e051ea5f62081a7792073d593d",
	"1fc64fbfaccd",

	"404142434445464748494a4b4c4d4e4f",
	"101112131415161718191a1b",
	"000102030405060708090a0b0c0d0e0f10111213",
	"202122232425262728292a2b2c2d2e2f3031323334353637",
	"e3b201a9f5b71a7a9b1ceaeccd97e70b6176aad9a4428aa5",
	"484392fbc1b09951",

	"404142434445464748494a4b4c4d4e4f",
	"101112131415161718191a1b1c",
	NULL,
	"202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f",
	"69915dad1e84c6376a68c2967e4dab615ae0fd1faec44cc484828529463ccf72",
	"b4ac6bec93e8598e7f0dadbcea5b",

	NULL
};

static void
test_CCM_inner(const char *name, const br_block_ctrcbc_class *vt)
{
	size_t u;

	printf("Test CCM %s: ", name);
	fflush(stdout);

	for (u = 0; KAT_CCM[u]; u += 6) {
		unsigned char plain[100];
		unsigned char key[32];
		unsigned char nonce[100];
		unsigned char aad_buf[100], *aad;
		unsigned char cipher[100];
		unsigned char tag[100];
		size_t plain_len, key_len, nonce_len, aad_len, tag_len;
		br_aes_gen_ctrcbc_keys bc;
		br_ccm_context ec;
		unsigned char tmp[100], out[16];
		size_t v;

		key_len = hextobin(key, KAT_CCM[u]);
		nonce_len = hextobin(nonce, KAT_CCM[u + 1]);
		if (KAT_CCM[u + 2] == NULL) {
			aad_len = 65536;
			aad = malloc(aad_len);
			if (aad == NULL) {
				fprintf(stderr, "OOM error\n");
				exit(EXIT_FAILURE);
			}
			for (v = 0; v < 65536; v ++) {
				aad[v] = (unsigned char)v;
			}
		} else {
			aad = aad_buf;
			aad_len = hextobin(aad, KAT_CCM[u + 2]);
		}
		plain_len = hextobin(plain, KAT_CCM[u + 3]);
		hextobin(cipher, KAT_CCM[u + 4]);
		tag_len = hextobin(tag, KAT_CCM[u + 5]);

		vt->init(&bc.vtable, key, key_len);
		br_ccm_init(&ec, &bc.vtable);

		memset(tmp, 0x54, sizeof tmp);

		/*
		 * Basic operation.
		 */
		memcpy(tmp, plain, plain_len);
		if (!br_ccm_reset(&ec, nonce, nonce_len,
			aad_len, plain_len, tag_len))
		{
			fprintf(stderr, "CCM reset failed\n");
			exit(EXIT_FAILURE);
		}
		br_ccm_aad_inject(&ec, aad, aad_len);
		br_ccm_flip(&ec);
		br_ccm_run(&ec, 1, tmp, plain_len);
		if (br_ccm_get_tag(&ec, out) != tag_len) {
			fprintf(stderr, "CCM returned wrong tag length\n");
			exit(EXIT_FAILURE);
		}
		check_equals("KAT CCM 1", tmp, cipher, plain_len);
		check_equals("KAT CCM 2", out, tag, tag_len);

		br_ccm_reset(&ec, nonce, nonce_len,
			aad_len, plain_len, tag_len);
		br_ccm_aad_inject(&ec, aad, aad_len);
		br_ccm_flip(&ec);
		br_ccm_run(&ec, 0, tmp, plain_len);
		check_equals("KAT CCM 3", tmp, plain, plain_len);
		if (!br_ccm_check_tag(&ec, tag)) {
			fprintf(stderr, "Tag not verified (1)\n");
			exit(EXIT_FAILURE);
		}

		for (v = plain_len; v < sizeof tmp; v ++) {
			if (tmp[v] != 0x54) {
				fprintf(stderr, "overflow on data\n");
				exit(EXIT_FAILURE);
			}
		}

		/*
		 * Byte-by-byte injection.
		 */
		br_ccm_reset(&ec, nonce, nonce_len,
			aad_len, plain_len, tag_len);
		for (v = 0; v < aad_len; v ++) {
			br_ccm_aad_inject(&ec, aad + v, 1);
		}
		br_ccm_flip(&ec);
		for (v = 0; v < plain_len; v ++) {
			br_ccm_run(&ec, 1, tmp + v, 1);
		}
		check_equals("KAT CCM 4", tmp, cipher, plain_len);
		if (!br_ccm_check_tag(&ec, tag)) {
			fprintf(stderr, "Tag not verified (2)\n");
			exit(EXIT_FAILURE);
		}

		br_ccm_reset(&ec, nonce, nonce_len,
			aad_len, plain_len, tag_len);
		for (v = 0; v < aad_len; v ++) {
			br_ccm_aad_inject(&ec, aad + v, 1);
		}
		br_ccm_flip(&ec);
		for (v = 0; v < plain_len; v ++) {
			br_ccm_run(&ec, 0, tmp + v, 1);
		}
		br_ccm_get_tag(&ec, out);
		check_equals("KAT CCM 5", tmp, plain, plain_len);
		check_equals("KAT CCM 6", out, tag, tag_len);

		/*
		 * Check that alterations are detected.
		 */
		for (v = 0; v < aad_len; v ++) {
			memcpy(tmp, cipher, plain_len);
			br_ccm_reset(&ec, nonce, nonce_len,
				aad_len, plain_len, tag_len);
			aad[v] ^= 0x04;
			br_ccm_aad_inject(&ec, aad, aad_len);
			aad[v] ^= 0x04;
			br_ccm_flip(&ec);
			br_ccm_run(&ec, 0, tmp, plain_len);
			check_equals("KAT CCM 7", tmp, plain, plain_len);
			if (br_ccm_check_tag(&ec, tag)) {
				fprintf(stderr, "Tag should have changed\n");
				exit(EXIT_FAILURE);
			}

			/*
			 * When the AAD is really big, we don't want to do
			 * the complete quadratic operation.
			 */
			if (v >= 32) {
				break;
			}
		}

		if (aad != aad_buf) {
			free(aad);
		}

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
	fflush(stdout);
}

static void
test_CCM(void)
{
	const br_block_ctrcbc_class *x_ctrcbc;

	test_CCM_inner("aes_big", &br_aes_big_ctrcbc_vtable);
	test_CCM_inner("aes_small", &br_aes_small_ctrcbc_vtable);
	test_CCM_inner("aes_ct", &br_aes_ct_ctrcbc_vtable);
	test_CCM_inner("aes_ct64", &br_aes_ct64_ctrcbc_vtable);

	x_ctrcbc = br_aes_x86ni_ctrcbc_get_vtable();
	if (x_ctrcbc != NULL) {
		test_CCM_inner("aes_x86ni", x_ctrcbc);
	} else {
		printf("Test CCM aes_x86ni: UNAVAILABLE\n");
	}

	x_ctrcbc = br_aes_pwr8_ctrcbc_get_vtable();
	if (x_ctrcbc != NULL) {
		test_CCM_inner("aes_pwr8", x_ctrcbc);
	} else {
		printf("Test CCM aes_pwr8: UNAVAILABLE\n");
	}
}

static void
test_EC_inner(const char *sk, const char *sU,
	const br_ec_impl *impl, int curve)
{
	unsigned char bk[70];
	unsigned char eG[150], eU[150];
	uint32_t n[22], n0i;
	size_t klen, ulen, nlen;
	const br_ec_curve_def *cd;
	br_hmac_drbg_context rng;
	int i;

	klen = hextobin(bk, sk);
	ulen = hextobin(eU, sU);
	switch (curve) {
	case BR_EC_secp256r1:
		cd = &br_secp256r1;
		break;
	case BR_EC_secp384r1:
		cd = &br_secp384r1;
		break;
	case BR_EC_secp521r1:
		cd = &br_secp521r1;
		break;
	default:
		fprintf(stderr, "Unknown curve: %d\n", curve);
		exit(EXIT_FAILURE);
		break;
	}
	if (ulen != cd->generator_len) {
		fprintf(stderr, "KAT vector wrong (%lu / %lu)\n",
			(unsigned long)ulen,
			(unsigned long)cd->generator_len);
	}
	memcpy(eG, cd->generator, ulen);
	if (impl->mul(eG, ulen, bk, klen, curve) != 1) {
		fprintf(stderr, "KAT multiplication failed\n");
		exit(EXIT_FAILURE);
	}
	if (memcmp(eG, eU, ulen) != 0) {
		fprintf(stderr, "KAT mul: mismatch\n");
		exit(EXIT_FAILURE);
	}

	/*
	 * Test the two-point-mul function. We want to test the basic
	 * functionality, and the following special cases:
	 *   x = y
	 *   x + y = curve order
	 */
	nlen = cd->order_len;
	br_i31_decode(n, cd->order, nlen);
	n0i = br_i31_ninv31(n[1]);
	br_hmac_drbg_init(&rng, &br_sha256_vtable, "seed for EC", 11);
	for (i = 0; i < 10; i ++) {
		unsigned char ba[80], bb[80], bx[80], by[80], bz[80];
		uint32_t a[22], b[22], x[22], y[22], z[22], t1[22], t2[22];
		uint32_t r;
		unsigned char eA[160], eB[160], eC[160], eD[160];

		/*
		 * Generate random a and b, and compute A = a*G and B = b*G.
		 */
		br_hmac_drbg_generate(&rng, ba, sizeof ba);
		br_i31_decode_reduce(a, ba, sizeof ba, n);
		br_i31_encode(ba, nlen, a);
		br_hmac_drbg_generate(&rng, bb, sizeof bb);
		br_i31_decode_reduce(b, bb, sizeof bb, n);
		br_i31_encode(bb, nlen, b);
		memcpy(eA, cd->generator, ulen);
		impl->mul(eA, ulen, ba, nlen, cd->curve);
		memcpy(eB, cd->generator, ulen);
		impl->mul(eB, ulen, bb, nlen, cd->curve);

		/*
		 * Generate random x and y (modulo n).
		 */
		br_hmac_drbg_generate(&rng, bx, sizeof bx);
		br_i31_decode_reduce(x, bx, sizeof bx, n);
		br_i31_encode(bx, nlen, x);
		br_hmac_drbg_generate(&rng, by, sizeof by);
		br_i31_decode_reduce(y, by, sizeof by, n);
		br_i31_encode(by, nlen, y);

		/*
		 * Compute z = a*x + b*y (mod n).
		 */
		memcpy(t1, x, sizeof x);
		br_i31_to_monty(t1, n);
		br_i31_montymul(z, a, t1, n, n0i);
		memcpy(t1, y, sizeof y);
		br_i31_to_monty(t1, n);
		br_i31_montymul(t2, b, t1, n, n0i);
		r = br_i31_add(z, t2, 1);
		r |= br_i31_sub(z, n, 0) ^ 1;
		br_i31_sub(z, n, r);
		br_i31_encode(bz, nlen, z);

		/*
		 * Compute C = x*A + y*B with muladd(), and also
		 * D = z*G with mul(). The two points must match.
		 */
		memcpy(eC, eA, ulen);
		if (impl->muladd(eC, eB, ulen,
			bx, nlen, by, nlen, cd->curve) != 1)
		{
			fprintf(stderr, "muladd() failed (1)\n");
			exit(EXIT_FAILURE);
		}
		memcpy(eD, cd->generator, ulen);
		if (impl->mul(eD, ulen, bz, nlen, cd->curve) != 1) {
			fprintf(stderr, "mul() failed (1)\n");
			exit(EXIT_FAILURE);
		}
		if (memcmp(eC, eD, nlen) != 0) {
			fprintf(stderr, "mul() / muladd() mismatch\n");
			exit(EXIT_FAILURE);
		}

		/*
		 * Also recomputed D = z*G with mulgen(). This must
		 * again match.
		 */
		memset(eD, 0, ulen);
		if (impl->mulgen(eD, bz, nlen, cd->curve) != ulen) {
			fprintf(stderr, "mulgen() failed: wrong length\n");
			exit(EXIT_FAILURE);
		}
		if (memcmp(eC, eD, nlen) != 0) {
			fprintf(stderr, "mulgen() / muladd() mismatch\n");
			exit(EXIT_FAILURE);
		}

		/*
		 * Check with x*A = y*B. We do so by setting b = x and y = a.
		 */
		memcpy(b, x, sizeof x);
		br_i31_encode(bb, nlen, b);
		memcpy(eB, cd->generator, ulen);
		impl->mul(eB, ulen, bb, nlen, cd->curve);
		memcpy(y, a, sizeof a);
		br_i31_encode(by, nlen, y);

		memcpy(t1, x, sizeof x);
		br_i31_to_monty(t1, n);
		br_i31_montymul(z, a, t1, n, n0i);
		memcpy(t1, y, sizeof y);
		br_i31_to_monty(t1, n);
		br_i31_montymul(t2, b, t1, n, n0i);
		r = br_i31_add(z, t2, 1);
		r |= br_i31_sub(z, n, 0) ^ 1;
		br_i31_sub(z, n, r);
		br_i31_encode(bz, nlen, z);

		memcpy(eC, eA, ulen);
		if (impl->muladd(eC, eB, ulen,
			bx, nlen, by, nlen, cd->curve) != 1)
		{
			fprintf(stderr, "muladd() failed (2)\n");
			exit(EXIT_FAILURE);
		}
		memcpy(eD, cd->generator, ulen);
		if (impl->mul(eD, ulen, bz, nlen, cd->curve) != 1) {
			fprintf(stderr, "mul() failed (2)\n");
			exit(EXIT_FAILURE);
		}
		if (memcmp(eC, eD, nlen) != 0) {
			fprintf(stderr,
				"mul() / muladd() mismatch (x*A=y*B)\n");
			exit(EXIT_FAILURE);
		}

		/*
		 * Check with x*A + y*B = 0. At that point, b = x, so we
		 * just need to set y = -a (mod n).
		 */
		memcpy(y, n, sizeof n);
		br_i31_sub(y, a, 1);
		br_i31_encode(by, nlen, y);
		memcpy(eC, eA, ulen);
		if (impl->muladd(eC, eB, ulen,
			bx, nlen, by, nlen, cd->curve) != 0)
		{
			fprintf(stderr, "muladd() should have failed\n");
			exit(EXIT_FAILURE);
		}
	}

	printf(".");
	fflush(stdout);
}

static void
test_EC_P256_carry_inner(const br_ec_impl *impl, const char *sP, const char *sQ)
{
	unsigned char P[65], Q[sizeof P], k[1];
	size_t plen, qlen;

	plen = hextobin(P, sP);
	qlen = hextobin(Q, sQ);
	if (plen != sizeof P || qlen != sizeof P) {
		fprintf(stderr, "KAT is incorrect\n");
		exit(EXIT_FAILURE);
	}
	k[0] = 0x10;
	if (impl->mul(P, plen, k, 1, BR_EC_secp256r1) != 1) {
		fprintf(stderr, "P-256 multiplication failed\n");
		exit(EXIT_FAILURE);
	}
	check_equals("P256_carry", P, Q, plen);
	printf(".");
	fflush(stdout);
}

static void
test_EC_P256_carry(const br_ec_impl *impl)
{
	test_EC_P256_carry_inner(impl,
		"0435BAA24B2B6E1B3C88E22A383BD88CC4B9A3166E7BCF94FF6591663AE066B33B821EBA1B4FC8EA609A87EB9A9C9A1CCD5C9F42FA1365306F64D7CAA718B8C978",
		"0447752A76CA890328D34E675C4971EC629132D1FC4863EDB61219B72C4E58DC5E9D51E7B293488CFD913C3CF20E438BB65C2BA66A7D09EABB45B55E804260C5EB");
	test_EC_P256_carry_inner(impl,
		"04DCAE9D9CE211223602024A6933BD42F77B6BF4EAB9C8915F058C149419FADD2CC9FC0707B270A1B5362BA4D249AFC8AC3DA1EFCA8270176EEACA525B49EE19E6",
		"048DAC7B0BE9B3206FCE8B24B6B4AEB122F2A67D13E536B390B6585CA193427E63F222388B5F51D744D6F5D47536D89EEEC89552BCB269E7828019C4410DFE980A");
}

static void
test_EC_KAT(const char *name, const br_ec_impl *impl, uint32_t curve_mask)
{
	printf("Test %s: ", name);
	fflush(stdout);

	if (curve_mask & ((uint32_t)1 << BR_EC_secp256r1)) {
		test_EC_inner(
			"C9AFA9D845BA75166B5C215767B1D6934E50C3DB36E89B127B8A622B120F6721",
			"0460FED4BA255A9D31C961EB74C6356D68C049B8923B61FA6CE669622E60F29FB67903FE1008B8BC99A41AE9E95628BC64F2F1B20C2D7E9F5177A3C294D4462299",
			impl, BR_EC_secp256r1);
		test_EC_P256_carry(impl);
	}
	if (curve_mask & ((uint32_t)1 << BR_EC_secp384r1)) {
		test_EC_inner(
			"6B9D3DAD2E1B8C1C05B19875B6659F4DE23C3B667BF297BA9AA47740787137D896D5724E4C70A825F872C9EA60D2EDF5",
			"04EC3A4E415B4E19A4568618029F427FA5DA9A8BC4AE92E02E06AAE5286B300C64DEF8F0EA9055866064A254515480BC138015D9B72D7D57244EA8EF9AC0C621896708A59367F9DFB9F54CA84B3F1C9DB1288B231C3AE0D4FE7344FD2533264720",
			impl, BR_EC_secp384r1);
	}
	if (curve_mask & ((uint32_t)1 << BR_EC_secp521r1)) {
		test_EC_inner(
			"00FAD06DAA62BA3B25D2FB40133DA757205DE67F5BB0018FEE8C86E1B68C7E75CAA896EB32F1F47C70855836A6D16FCC1466F6D8FBEC67DB89EC0C08B0E996B83538",
			"0401894550D0785932E00EAA23B694F213F8C3121F86DC97A04E5A7167DB4E5BCD371123D46E45DB6B5D5370A7F20FB633155D38FFA16D2BD761DCAC474B9A2F5023A400493101C962CD4D2FDDF782285E64584139C2F91B47F87FF82354D6630F746A28A0DB25741B5B34A828008B22ACC23F924FAAFBD4D33F81EA66956DFEAA2BFDFCF5",
			impl, BR_EC_secp521r1);
	}

	printf(" done.\n");
	fflush(stdout);
}

static void
test_EC_keygen(const char *name, const br_ec_impl *impl, uint32_t curves)
{
	int curve;
	br_hmac_drbg_context rng;

	printf("Test %s keygen: ", name);
	fflush(stdout);

	br_hmac_drbg_init(&rng, &br_sha256_vtable, "seed for EC keygen", 18);
	br_hmac_drbg_update(&rng, name, strlen(name));

	for (curve = -1; curve <= 35; curve ++) {
		br_ec_private_key sk;
		br_ec_public_key pk;
		unsigned char kbuf_priv[BR_EC_KBUF_PRIV_MAX_SIZE];
		unsigned char kbuf_pub[BR_EC_KBUF_PUB_MAX_SIZE];

		if (curve < 0 || curve >= 32 || ((curves >> curve) & 1) == 0) {
			if (br_ec_keygen(&rng.vtable, impl,
				&sk, kbuf_priv, curve) != 0)
			{
				fprintf(stderr, "br_ec_keygen() did not"
					" reject unsupported curve %d\n",
					curve);
				exit(EXIT_FAILURE);
			}
			sk.curve = curve;
			if (br_ec_compute_pub(impl, NULL, NULL, &sk) != 0) {
				fprintf(stderr, "br_ec_keygen() did not"
					" reject unsupported curve %d\n",
					curve);
				exit(EXIT_FAILURE);
			}
		} else {
			size_t len, u;
			unsigned char tmp_priv[sizeof kbuf_priv];
			unsigned char tmp_pub[sizeof kbuf_pub];
			unsigned z;

			len = br_ec_keygen(&rng.vtable, impl,
				NULL, NULL, curve);
			if (len == 0) {
				fprintf(stderr, "br_ec_keygen() rejects"
					" supported curve %d\n", curve);
				exit(EXIT_FAILURE);
			}
			if (len > sizeof kbuf_priv) {
				fprintf(stderr, "oversized kbuf_priv\n");
				exit(EXIT_FAILURE);
			}
			memset(kbuf_priv, 0, sizeof kbuf_priv);
			if (br_ec_keygen(&rng.vtable, impl,
				NULL, kbuf_priv, curve) != len)
			{
				fprintf(stderr, "kbuf_priv length mismatch\n");
				exit(EXIT_FAILURE);
			}
			z = 0;
			for (u = 0; u < len; u ++) {
				z |= kbuf_priv[u];
			}
			if (z == 0) {
				fprintf(stderr, "kbuf_priv not initialized\n");
				exit(EXIT_FAILURE);
			}
			for (u = len; u < sizeof kbuf_priv; u ++) {
				if (kbuf_priv[u] != 0) {
					fprintf(stderr, "kbuf_priv overflow\n");
					exit(EXIT_FAILURE);
				}
			}
			if (br_ec_keygen(&rng.vtable, impl,
				NULL, tmp_priv, curve) != len)
			{
				fprintf(stderr, "tmp_priv length mismatch\n");
				exit(EXIT_FAILURE);
			}
			if (memcmp(kbuf_priv, tmp_priv, len) == 0) {
				fprintf(stderr, "keygen stutter\n");
				exit(EXIT_FAILURE);
			}
			memset(&sk, 0, sizeof sk);
			if (br_ec_keygen(&rng.vtable, impl,
				&sk, kbuf_priv, curve) != len)
			{
				fprintf(stderr,
					"kbuf_priv length mismatch (2)\n");
				exit(EXIT_FAILURE);
			}
			if (sk.curve != curve || sk.x != kbuf_priv
				|| sk.xlen != len)
			{
				fprintf(stderr, "sk not initialized\n");
				exit(EXIT_FAILURE);
			}

			len = br_ec_compute_pub(impl, NULL, NULL, &sk);
			if (len > sizeof kbuf_pub) {
				fprintf(stderr, "oversized kbuf_pub\n");
				exit(EXIT_FAILURE);
			}
			memset(kbuf_pub, 0, sizeof kbuf_pub);
			if (br_ec_compute_pub(impl, NULL,
				kbuf_pub, &sk) != len)
			{
				fprintf(stderr, "kbuf_pub length mismatch\n");
				exit(EXIT_FAILURE);
			}
			for (u = len; u < sizeof kbuf_pub; u ++) {
				if (kbuf_pub[u] != 0) {
					fprintf(stderr, "kbuf_pub overflow\n");
					exit(EXIT_FAILURE);
				}
			}
			memset(&pk, 0, sizeof pk);
			if (br_ec_compute_pub(impl, &pk,
				tmp_pub, &sk) != len)
			{
				fprintf(stderr, "tmp_pub length mismatch\n");
				exit(EXIT_FAILURE);
			}
			if (memcmp(kbuf_pub, tmp_pub, len) != 0) {
				fprintf(stderr, "pubkey mismatch\n");
				exit(EXIT_FAILURE);
			}
			if (pk.curve != curve || pk.q != tmp_pub
				|| pk.qlen != len)
			{
				fprintf(stderr, "pk not initialized\n");
				exit(EXIT_FAILURE);
			}

			if (impl->mulgen(kbuf_pub,
				sk.x, sk.xlen, curve) != len
				|| memcmp(pk.q, kbuf_pub, len) != 0)
			{
				fprintf(stderr, "wrong pubkey\n");
				exit(EXIT_FAILURE);
			}
		}
		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
	fflush(stdout);
}

static void
test_EC_prime_i15(void)
{
	test_EC_KAT("EC_prime_i15", &br_ec_prime_i15,
		(uint32_t)1 << BR_EC_secp256r1
		| (uint32_t)1 << BR_EC_secp384r1
		| (uint32_t)1 << BR_EC_secp521r1);
	test_EC_keygen("EC_prime_i15", &br_ec_prime_i15,
		(uint32_t)1 << BR_EC_secp256r1
		| (uint32_t)1 << BR_EC_secp384r1
		| (uint32_t)1 << BR_EC_secp521r1);
}

static void
test_EC_prime_i31(void)
{
	test_EC_KAT("EC_prime_i31", &br_ec_prime_i31,
		(uint32_t)1 << BR_EC_secp256r1
		| (uint32_t)1 << BR_EC_secp384r1
		| (uint32_t)1 << BR_EC_secp521r1);
	test_EC_keygen("EC_prime_i31", &br_ec_prime_i31,
		(uint32_t)1 << BR_EC_secp256r1
		| (uint32_t)1 << BR_EC_secp384r1
		| (uint32_t)1 << BR_EC_secp521r1);
}

static void
test_EC_p256_m15(void)
{
	test_EC_KAT("EC_p256_m15", &br_ec_p256_m15,
		(uint32_t)1 << BR_EC_secp256r1);
	test_EC_keygen("EC_p256_m15", &br_ec_p256_m15,
		(uint32_t)1 << BR_EC_secp256r1);
}

static void
test_EC_p256_m31(void)
{
	test_EC_KAT("EC_p256_m31", &br_ec_p256_m31,
		(uint32_t)1 << BR_EC_secp256r1);
	test_EC_keygen("EC_p256_m31", &br_ec_p256_m31,
		(uint32_t)1 << BR_EC_secp256r1);
}

const struct {
	const char *scalar;
	const char *u_in;
	const char *u_out;
} C25519_KAT[] = {
	{ "A546E36BF0527C9D3B16154B82465EDD62144C0AC1FC5A18506A2244BA449AC4",
	  "E6DB6867583030DB3594C1A424B15F7C726624EC26B3353B10A903A6D0AB1C4C",
	  "C3DA55379DE9C6908E94EA4DF28D084F32ECCF03491C71F754B4075577A28552" },
	{ "4B66E9D4D1B4673C5AD22691957D6AF5C11B6421E0EA01D42CA4169E7918BA0D",
	  "E5210F12786811D3F4B7959D0538AE2C31DBE7106FC03C3EFC4CD549C715A493",
	  "95CBDE9476E8907D7AADE45CB4B873F88B595A68799FA152E6F8F7647AAC7957" },
	{ 0, 0, 0 }
};

static void
test_EC_c25519(const char *name, const br_ec_impl *iec)
{
	unsigned char bu[32], bk[32], br[32];
	size_t v;
	int i;

	printf("Test %s: ", name);
	fflush(stdout);
	for (v = 0; C25519_KAT[v].scalar; v ++) {
		hextobin(bk, C25519_KAT[v].scalar);
		hextobin(bu, C25519_KAT[v].u_in);
		hextobin(br, C25519_KAT[v].u_out);
		if (!iec->mul(bu, sizeof bu, bk, sizeof bk, BR_EC_curve25519)) {
			fprintf(stderr, "Curve25519 multiplication failed\n");
			exit(EXIT_FAILURE);
		}
		if (memcmp(bu, br, sizeof bu) != 0) {
			fprintf(stderr, "Curve25519 failed KAT\n");
			exit(EXIT_FAILURE);
		}
		printf(".");
		fflush(stdout);
	}
	printf(" ");
	fflush(stdout);

	memset(bu, 0, sizeof bu);
	bu[0] = 0x09;
	memcpy(bk, bu, sizeof bu);
	for (i = 1; i <= 1000; i ++) {
		if (!iec->mul(bu, sizeof bu, bk, sizeof bk, BR_EC_curve25519)) {
			fprintf(stderr, "Curve25519 multiplication failed"
				" (iter=%d)\n", i);
			exit(EXIT_FAILURE);
		}
		for (v = 0; v < sizeof bu; v ++) {
			unsigned t;

			t = bu[v];
			bu[v] = bk[v];
			bk[v] = t;
		}
		if (i == 1 || i == 1000) {
			const char *sref;

			sref = (i == 1)
				? "422C8E7A6227D7BCA1350B3E2BB7279F7897B87BB6854B783C60E80311AE3079"
				: "684CF59BA83309552800EF566F2F4D3C1C3887C49360E3875F2EB94D99532C51";
			hextobin(br, sref);
			if (memcmp(bk, br, sizeof bk) != 0) {
				fprintf(stderr,
					"Curve25519 failed KAT (iter=%d)\n", i);
				exit(EXIT_FAILURE);
			}
		}
		if (i % 100 == 0) {
			printf(".");
			fflush(stdout);
		}
	}

	printf(" done.\n");
	fflush(stdout);
}

static void
test_EC_c25519_i15(void)
{
	test_EC_c25519("EC_c25519_i15", &br_ec_c25519_i15);
	test_EC_keygen("EC_c25519_i15", &br_ec_c25519_i15,
		(uint32_t)1 << BR_EC_curve25519);
}

static void
test_EC_c25519_i31(void)
{
	test_EC_c25519("EC_c25519_i31", &br_ec_c25519_i31);
	test_EC_keygen("EC_c25519_i31", &br_ec_c25519_i31,
		(uint32_t)1 << BR_EC_curve25519);
}

static void
test_EC_c25519_m15(void)
{
	test_EC_c25519("EC_c25519_m15", &br_ec_c25519_m15);
	test_EC_keygen("EC_c25519_m15", &br_ec_c25519_m15,
		(uint32_t)1 << BR_EC_curve25519);
}

static void
test_EC_c25519_m31(void)
{
	test_EC_c25519("EC_c25519_m31", &br_ec_c25519_m31);
	test_EC_keygen("EC_c25519_m31", &br_ec_c25519_m31,
		(uint32_t)1 << BR_EC_curve25519);
}

static const unsigned char EC_P256_PUB_POINT[] = {
	0x04, 0x60, 0xFE, 0xD4, 0xBA, 0x25, 0x5A, 0x9D,
	0x31, 0xC9, 0x61, 0xEB, 0x74, 0xC6, 0x35, 0x6D,
	0x68, 0xC0, 0x49, 0xB8, 0x92, 0x3B, 0x61, 0xFA,
	0x6C, 0xE6, 0x69, 0x62, 0x2E, 0x60, 0xF2, 0x9F,
	0xB6, 0x79, 0x03, 0xFE, 0x10, 0x08, 0xB8, 0xBC,
	0x99, 0xA4, 0x1A, 0xE9, 0xE9, 0x56, 0x28, 0xBC,
	0x64, 0xF2, 0xF1, 0xB2, 0x0C, 0x2D, 0x7E, 0x9F,
	0x51, 0x77, 0xA3, 0xC2, 0x94, 0xD4, 0x46, 0x22,
	0x99
};

static const unsigned char EC_P256_PRIV_X[] = {
	0xC9, 0xAF, 0xA9, 0xD8, 0x45, 0xBA, 0x75, 0x16,
	0x6B, 0x5C, 0x21, 0x57, 0x67, 0xB1, 0xD6, 0x93,
	0x4E, 0x50, 0xC3, 0xDB, 0x36, 0xE8, 0x9B, 0x12,
	0x7B, 0x8A, 0x62, 0x2B, 0x12, 0x0F, 0x67, 0x21
};

static const br_ec_public_key EC_P256_PUB = {
	BR_EC_secp256r1,
	(unsigned char *)EC_P256_PUB_POINT, sizeof EC_P256_PUB_POINT
};

static const br_ec_private_key EC_P256_PRIV = {
	BR_EC_secp256r1,
	(unsigned char *)EC_P256_PRIV_X, sizeof EC_P256_PRIV_X
};

static const unsigned char EC_P384_PUB_POINT[] = {
	0x04, 0xEC, 0x3A, 0x4E, 0x41, 0x5B, 0x4E, 0x19,
	0xA4, 0x56, 0x86, 0x18, 0x02, 0x9F, 0x42, 0x7F,
	0xA5, 0xDA, 0x9A, 0x8B, 0xC4, 0xAE, 0x92, 0xE0,
	0x2E, 0x06, 0xAA, 0xE5, 0x28, 0x6B, 0x30, 0x0C,
	0x64, 0xDE, 0xF8, 0xF0, 0xEA, 0x90, 0x55, 0x86,
	0x60, 0x64, 0xA2, 0x54, 0x51, 0x54, 0x80, 0xBC,
	0x13, 0x80, 0x15, 0xD9, 0xB7, 0x2D, 0x7D, 0x57,
	0x24, 0x4E, 0xA8, 0xEF, 0x9A, 0xC0, 0xC6, 0x21,
	0x89, 0x67, 0x08, 0xA5, 0x93, 0x67, 0xF9, 0xDF,
	0xB9, 0xF5, 0x4C, 0xA8, 0x4B, 0x3F, 0x1C, 0x9D,
	0xB1, 0x28, 0x8B, 0x23, 0x1C, 0x3A, 0xE0, 0xD4,
	0xFE, 0x73, 0x44, 0xFD, 0x25, 0x33, 0x26, 0x47,
	0x20
};

static const unsigned char EC_P384_PRIV_X[] = {
	0x6B, 0x9D, 0x3D, 0xAD, 0x2E, 0x1B, 0x8C, 0x1C,
	0x05, 0xB1, 0x98, 0x75, 0xB6, 0x65, 0x9F, 0x4D,
	0xE2, 0x3C, 0x3B, 0x66, 0x7B, 0xF2, 0x97, 0xBA,
	0x9A, 0xA4, 0x77, 0x40, 0x78, 0x71, 0x37, 0xD8,
	0x96, 0xD5, 0x72, 0x4E, 0x4C, 0x70, 0xA8, 0x25,
	0xF8, 0x72, 0xC9, 0xEA, 0x60, 0xD2, 0xED, 0xF5
};

static const br_ec_public_key EC_P384_PUB = {
	BR_EC_secp384r1,
	(unsigned char *)EC_P384_PUB_POINT, sizeof EC_P384_PUB_POINT
};

static const br_ec_private_key EC_P384_PRIV = {
	BR_EC_secp384r1,
	(unsigned char *)EC_P384_PRIV_X, sizeof EC_P384_PRIV_X
};

static const unsigned char EC_P521_PUB_POINT[] = {
	0x04, 0x01, 0x89, 0x45, 0x50, 0xD0, 0x78, 0x59,
	0x32, 0xE0, 0x0E, 0xAA, 0x23, 0xB6, 0x94, 0xF2,
	0x13, 0xF8, 0xC3, 0x12, 0x1F, 0x86, 0xDC, 0x97,
	0xA0, 0x4E, 0x5A, 0x71, 0x67, 0xDB, 0x4E, 0x5B,
	0xCD, 0x37, 0x11, 0x23, 0xD4, 0x6E, 0x45, 0xDB,
	0x6B, 0x5D, 0x53, 0x70, 0xA7, 0xF2, 0x0F, 0xB6,
	0x33, 0x15, 0x5D, 0x38, 0xFF, 0xA1, 0x6D, 0x2B,
	0xD7, 0x61, 0xDC, 0xAC, 0x47, 0x4B, 0x9A, 0x2F,
	0x50, 0x23, 0xA4, 0x00, 0x49, 0x31, 0x01, 0xC9,
	0x62, 0xCD, 0x4D, 0x2F, 0xDD, 0xF7, 0x82, 0x28,
	0x5E, 0x64, 0x58, 0x41, 0x39, 0xC2, 0xF9, 0x1B,
	0x47, 0xF8, 0x7F, 0xF8, 0x23, 0x54, 0xD6, 0x63,
	0x0F, 0x74, 0x6A, 0x28, 0xA0, 0xDB, 0x25, 0x74,
	0x1B, 0x5B, 0x34, 0xA8, 0x28, 0x00, 0x8B, 0x22,
	0xAC, 0xC2, 0x3F, 0x92, 0x4F, 0xAA, 0xFB, 0xD4,
	0xD3, 0x3F, 0x81, 0xEA, 0x66, 0x95, 0x6D, 0xFE,
	0xAA, 0x2B, 0xFD, 0xFC, 0xF5
};

static const unsigned char EC_P521_PRIV_X[] = {
	0x00, 0xFA, 0xD0, 0x6D, 0xAA, 0x62, 0xBA, 0x3B,
	0x25, 0xD2, 0xFB, 0x40, 0x13, 0x3D, 0xA7, 0x57,
	0x20, 0x5D, 0xE6, 0x7F, 0x5B, 0xB0, 0x01, 0x8F,
	0xEE, 0x8C, 0x86, 0xE1, 0xB6, 0x8C, 0x7E, 0x75,
	0xCA, 0xA8, 0x96, 0xEB, 0x32, 0xF1, 0xF4, 0x7C,
	0x70, 0x85, 0x58, 0x36, 0xA6, 0xD1, 0x6F, 0xCC,
	0x14, 0x66, 0xF6, 0xD8, 0xFB, 0xEC, 0x67, 0xDB,
	0x89, 0xEC, 0x0C, 0x08, 0xB0, 0xE9, 0x96, 0xB8,
	0x35, 0x38
};

static const br_ec_public_key EC_P521_PUB = {
	BR_EC_secp521r1,
	(unsigned char *)EC_P521_PUB_POINT, sizeof EC_P521_PUB_POINT
};

static const br_ec_private_key EC_P521_PRIV = {
	BR_EC_secp521r1,
	(unsigned char *)EC_P521_PRIV_X, sizeof EC_P521_PRIV_X
};

typedef struct {
	const br_ec_public_key *pub;
	const br_ec_private_key *priv;
	const br_hash_class *hf;
	const char *msg;
	const char *sk;
	const char *sraw;
	const char *sasn1;
} ecdsa_kat_vector;

const ecdsa_kat_vector ECDSA_KAT[] = {

	/* Test vectors for P-256, from RFC 6979. */
	{
		&EC_P256_PUB,
		&EC_P256_PRIV,
		&br_sha1_vtable, "sample",
		"882905F1227FD620FBF2ABF21244F0BA83D0DC3A9103DBBEE43A1FB858109DB4",
		"61340C88C3AAEBEB4F6D667F672CA9759A6CCAA9FA8811313039EE4A35471D326D7F147DAC089441BB2E2FE8F7A3FA264B9C475098FDCF6E00D7C996E1B8B7EB",
		"3044022061340C88C3AAEBEB4F6D667F672CA9759A6CCAA9FA8811313039EE4A35471D3202206D7F147DAC089441BB2E2FE8F7A3FA264B9C475098FDCF6E00D7C996E1B8B7EB"
	},
	{
		&EC_P256_PUB,
		&EC_P256_PRIV,
		&br_sha224_vtable, "sample",
		"103F90EE9DC52E5E7FB5132B7033C63066D194321491862059967C715985D473",
		"53B2FFF5D1752B2C689DF257C04C40A587FABABB3F6FC2702F1343AF7CA9AA3FB9AFB64FDC03DC1A131C7D2386D11E349F070AA432A4ACC918BEA988BF75C74C",
		"3045022053B2FFF5D1752B2C689DF257C04C40A587FABABB3F6FC2702F1343AF7CA9AA3F022100B9AFB64FDC03DC1A131C7D2386D11E349F070AA432A4ACC918BEA988BF75C74C"
	},
	{
		&EC_P256_PUB,
		&EC_P256_PRIV,
		&br_sha256_vtable, "sample",
		"A6E3C57DD01ABE90086538398355DD4C3B17AA873382B0F24D6129493D8AAD60",
		"EFD48B2AACB6A8FD1140DD9CD45E81D69D2C877B56AAF991C34D0EA84EAF3716F7CB1C942D657C41D436C7A1B6E29F65F3E900DBB9AFF4064DC4AB2F843ACDA8",
		"3046022100EFD48B2AACB6A8FD1140DD9CD45E81D69D2C877B56AAF991C34D0EA84EAF3716022100F7CB1C942D657C41D436C7A1B6E29F65F3E900DBB9AFF4064DC4AB2F843ACDA8"
	},
	{
		&EC_P256_PUB,
		&EC_P256_PRIV,
		&br_sha384_vtable, "sample",
		"09F634B188CEFD98E7EC88B1AA9852D734D0BC272F7D2A47DECC6EBEB375AAD4",
		"0EAFEA039B20E9B42309FB1D89E213057CBF973DC0CFC8F129EDDDC800EF77194861F0491E6998B9455193E34E7B0D284DDD7149A74B95B9261F13ABDE940954",
		"304402200EAFEA039B20E9B42309FB1D89E213057CBF973DC0CFC8F129EDDDC800EF771902204861F0491E6998B9455193E34E7B0D284DDD7149A74B95B9261F13ABDE940954"
	},
	{
		&EC_P256_PUB,
		&EC_P256_PRIV,
		&br_sha512_vtable, "sample",
		"5FA81C63109BADB88C1F367B47DA606DA28CAD69AA22C4FE6AD7DF73A7173AA5",
		"8496A60B5E9B47C825488827E0495B0E3FA109EC4568FD3F8D1097678EB97F002362AB1ADBE2B8ADF9CB9EDAB740EA6049C028114F2460F96554F61FAE3302FE",
		"30450221008496A60B5E9B47C825488827E0495B0E3FA109EC4568FD3F8D1097678EB97F0002202362AB1ADBE2B8ADF9CB9EDAB740EA6049C028114F2460F96554F61FAE3302FE"
	},
	{
		&EC_P256_PUB,
		&EC_P256_PRIV,
		&br_sha1_vtable, "test",
		"8C9520267C55D6B980DF741E56B4ADEE114D84FBFA2E62137954164028632A2E",
		"0CBCC86FD6ABD1D99E703E1EC50069EE5C0B4BA4B9AC60E409E8EC5910D81A8901B9D7B73DFAA60D5651EC4591A0136F87653E0FD780C3B1BC872FFDEAE479B1",
		"304402200CBCC86FD6ABD1D99E703E1EC50069EE5C0B4BA4B9AC60E409E8EC5910D81A89022001B9D7B73DFAA60D5651EC4591A0136F87653E0FD780C3B1BC872FFDEAE479B1"
	},
	{
		&EC_P256_PUB,
		&EC_P256_PRIV,
		&br_sha224_vtable, "test",
		"669F4426F2688B8BE0DB3A6BD1989BDAEFFF84B649EEB84F3DD26080F667FAA7",
		"C37EDB6F0AE79D47C3C27E962FA269BB4F441770357E114EE511F662EC34A692C820053A05791E521FCAAD6042D40AEA1D6B1A540138558F47D0719800E18F2D",
		"3046022100C37EDB6F0AE79D47C3C27E962FA269BB4F441770357E114EE511F662EC34A692022100C820053A05791E521FCAAD6042D40AEA1D6B1A540138558F47D0719800E18F2D"
	},
	{
		&EC_P256_PUB,
		&EC_P256_PRIV,
		&br_sha256_vtable, "test",
		"D16B6AE827F17175E040871A1C7EC3500192C4C92677336EC2537ACAEE0008E0",
		"F1ABB023518351CD71D881567B1EA663ED3EFCF6C5132B354F28D3B0B7D38367019F4113742A2B14BD25926B49C649155F267E60D3814B4C0CC84250E46F0083",
		"3045022100F1ABB023518351CD71D881567B1EA663ED3EFCF6C5132B354F28D3B0B7D383670220019F4113742A2B14BD25926B49C649155F267E60D3814B4C0CC84250E46F0083"
	},
	{
		&EC_P256_PUB,
		&EC_P256_PRIV,
		&br_sha384_vtable, "test",
		"16AEFFA357260B04B1DD199693960740066C1A8F3E8EDD79070AA914D361B3B8",
		"83910E8B48BB0C74244EBDF7F07A1C5413D61472BD941EF3920E623FBCCEBEB68DDBEC54CF8CD5874883841D712142A56A8D0F218F5003CB0296B6B509619F2C",
		"304602210083910E8B48BB0C74244EBDF7F07A1C5413D61472BD941EF3920E623FBCCEBEB60221008DDBEC54CF8CD5874883841D712142A56A8D0F218F5003CB0296B6B509619F2C"
	},
	{
		&EC_P256_PUB,
		&EC_P256_PRIV,
		&br_sha512_vtable, "test",
		"6915D11632ACA3C40D5D51C08DAF9C555933819548784480E93499000D9F0B7F",
		"461D93F31B6540894788FD206C07CFA0CC35F46FA3C91816FFF1040AD1581A0439AF9F15DE0DB8D97E72719C74820D304CE5226E32DEDAE67519E840D1194E55",
		"30440220461D93F31B6540894788FD206C07CFA0CC35F46FA3C91816FFF1040AD1581A04022039AF9F15DE0DB8D97E72719C74820D304CE5226E32DEDAE67519E840D1194E55"
	},

	/* Test vectors for P-384, from RFC 6979. */
	{
		&EC_P384_PUB,
		&EC_P384_PRIV,
		&br_sha1_vtable, "sample",
		"4471EF7518BB2C7C20F62EAE1C387AD0C5E8E470995DB4ACF694466E6AB096630F29E5938D25106C3C340045A2DB01A7",
		"EC748D839243D6FBEF4FC5C4859A7DFFD7F3ABDDF72014540C16D73309834FA37B9BA002899F6FDA3A4A9386790D4EB2A3BCFA947BEEF4732BF247AC17F71676CB31A847B9FF0CBC9C9ED4C1A5B3FACF26F49CA031D4857570CCB5CA4424A443",
		"3066023100EC748D839243D6FBEF4FC5C4859A7DFFD7F3ABDDF72014540C16D73309834FA37B9BA002899F6FDA3A4A9386790D4EB2023100A3BCFA947BEEF4732BF247AC17F71676CB31A847B9FF0CBC9C9ED4C1A5B3FACF26F49CA031D4857570CCB5CA4424A443"
	},

	{
		&EC_P384_PUB,
		&EC_P384_PRIV,
		&br_sha224_vtable, "sample",
		"A4E4D2F0E729EB786B31FC20AD5D849E304450E0AE8E3E341134A5C1AFA03CAB8083EE4E3C45B06A5899EA56C51B5879",
		"42356E76B55A6D9B4631C865445DBE54E056D3B3431766D0509244793C3F9366450F76EE3DE43F5A125333A6BE0601229DA0C81787064021E78DF658F2FBB0B042BF304665DB721F077A4298B095E4834C082C03D83028EFBF93A3C23940CA8D",
		"3065023042356E76B55A6D9B4631C865445DBE54E056D3B3431766D0509244793C3F9366450F76EE3DE43F5A125333A6BE0601220231009DA0C81787064021E78DF658F2FBB0B042BF304665DB721F077A4298B095E4834C082C03D83028EFBF93A3C23940CA8D"
	},
	{
		&EC_P384_PUB,
		&EC_P384_PRIV,
		&br_sha256_vtable, "sample",
		"180AE9F9AEC5438A44BC159A1FCB277C7BE54FA20E7CF404B490650A8ACC414E375572342863C899F9F2EDF9747A9B60",
		"21B13D1E013C7FA1392D03C5F99AF8B30C570C6F98D4EA8E354B63A21D3DAA33BDE1E888E63355D92FA2B3C36D8FB2CDF3AA443FB107745BF4BD77CB3891674632068A10CA67E3D45DB2266FA7D1FEEBEFDC63ECCD1AC42EC0CB8668A4FA0AB0",
		"3065023021B13D1E013C7FA1392D03C5F99AF8B30C570C6F98D4EA8E354B63A21D3DAA33BDE1E888E63355D92FA2B3C36D8FB2CD023100F3AA443FB107745BF4BD77CB3891674632068A10CA67E3D45DB2266FA7D1FEEBEFDC63ECCD1AC42EC0CB8668A4FA0AB0"
	},
	{
		&EC_P384_PUB,
		&EC_P384_PRIV,
		&br_sha384_vtable, "sample",
		"94ED910D1A099DAD3254E9242AE85ABDE4BA15168EAF0CA87A555FD56D10FBCA2907E3E83BA95368623B8C4686915CF9",
		"94EDBB92A5ECB8AAD4736E56C691916B3F88140666CE9FA73D64C4EA95AD133C81A648152E44ACF96E36DD1E80FABE4699EF4AEB15F178CEA1FE40DB2603138F130E740A19624526203B6351D0A3A94FA329C145786E679E7B82C71A38628AC8",
		"306602310094EDBB92A5ECB8AAD4736E56C691916B3F88140666CE9FA73D64C4EA95AD133C81A648152E44ACF96E36DD1E80FABE4602310099EF4AEB15F178CEA1FE40DB2603138F130E740A19624526203B6351D0A3A94FA329C145786E679E7B82C71A38628AC8"
	},
	{
		&EC_P384_PUB,
		&EC_P384_PRIV,
		&br_sha512_vtable, "sample",
		"92FC3C7183A883E24216D1141F1A8976C5B0DD797DFA597E3D7B32198BD35331A4E966532593A52980D0E3AAA5E10EC3",
		"ED0959D5880AB2D869AE7F6C2915C6D60F96507F9CB3E047C0046861DA4A799CFE30F35CC900056D7C99CD7882433709512C8CCEEE3890A84058CE1E22DBC2198F42323CE8ACA9135329F03C068E5112DC7CC3EF3446DEFCEB01A45C2667FDD5",
		"3065023100ED0959D5880AB2D869AE7F6C2915C6D60F96507F9CB3E047C0046861DA4A799CFE30F35CC900056D7C99CD78824337090230512C8CCEEE3890A84058CE1E22DBC2198F42323CE8ACA9135329F03C068E5112DC7CC3EF3446DEFCEB01A45C2667FDD5"
	},
	{
		&EC_P384_PUB,
		&EC_P384_PRIV,
		&br_sha1_vtable, "test",
		"66CC2C8F4D303FC962E5FF6A27BD79F84EC812DDAE58CF5243B64A4AD8094D47EC3727F3A3C186C15054492E30698497",
		"4BC35D3A50EF4E30576F58CD96CE6BF638025EE624004A1F7789A8B8E43D0678ACD9D29876DAF46638645F7F404B11C7D5A6326C494ED3FF614703878961C0FDE7B2C278F9A65FD8C4B7186201A2991695BA1C84541327E966FA7B50F7382282",
		"306502304BC35D3A50EF4E30576F58CD96CE6BF638025EE624004A1F7789A8B8E43D0678ACD9D29876DAF46638645F7F404B11C7023100D5A6326C494ED3FF614703878961C0FDE7B2C278F9A65FD8C4B7186201A2991695BA1C84541327E966FA7B50F7382282"
	},
	{
		&EC_P384_PUB,
		&EC_P384_PRIV,
		&br_sha224_vtable, "test",
		"18FA39DB95AA5F561F30FA3591DC59C0FA3653A80DAFFA0B48D1A4C6DFCBFF6E3D33BE4DC5EB8886A8ECD093F2935726",
		"E8C9D0B6EA72A0E7837FEA1D14A1A9557F29FAA45D3E7EE888FC5BF954B5E62464A9A817C47FF78B8C11066B24080E7207041D4A7A0379AC7232FF72E6F77B6DDB8F09B16CCE0EC3286B2BD43FA8C6141C53EA5ABEF0D8231077A04540A96B66",
		"3065023100E8C9D0B6EA72A0E7837FEA1D14A1A9557F29FAA45D3E7EE888FC5BF954B5E62464A9A817C47FF78B8C11066B24080E72023007041D4A7A0379AC7232FF72E6F77B6DDB8F09B16CCE0EC3286B2BD43FA8C6141C53EA5ABEF0D8231077A04540A96B66"
	},
	{
		&EC_P384_PUB,
		&EC_P384_PRIV,
		&br_sha256_vtable, "test",
		"0CFAC37587532347DC3389FDC98286BBA8C73807285B184C83E62E26C401C0FAA48DD070BA79921A3457ABFF2D630AD7",
		"6D6DEFAC9AB64DABAFE36C6BF510352A4CC27001263638E5B16D9BB51D451559F918EEDAF2293BE5B475CC8F0188636B2D46F3BECBCC523D5F1A1256BF0C9B024D879BA9E838144C8BA6BAEB4B53B47D51AB373F9845C0514EEFB14024787265",
		"306402306D6DEFAC9AB64DABAFE36C6BF510352A4CC27001263638E5B16D9BB51D451559F918EEDAF2293BE5B475CC8F0188636B02302D46F3BECBCC523D5F1A1256BF0C9B024D879BA9E838144C8BA6BAEB4B53B47D51AB373F9845C0514EEFB14024787265"
	},
	{
		&EC_P384_PUB,
		&EC_P384_PRIV,
		&br_sha384_vtable, "test",
		"015EE46A5BF88773ED9123A5AB0807962D193719503C527B031B4C2D225092ADA71F4A459BC0DA98ADB95837DB8312EA",
		"8203B63D3C853E8D77227FB377BCF7B7B772E97892A80F36AB775D509D7A5FEB0542A7F0812998DA8F1DD3CA3CF023DBDDD0760448D42D8A43AF45AF836FCE4DE8BE06B485E9B61B827C2F13173923E06A739F040649A667BF3B828246BAA5A5",
		"30660231008203B63D3C853E8D77227FB377BCF7B7B772E97892A80F36AB775D509D7A5FEB0542A7F0812998DA8F1DD3CA3CF023DB023100DDD0760448D42D8A43AF45AF836FCE4DE8BE06B485E9B61B827C2F13173923E06A739F040649A667BF3B828246BAA5A5"
	},
	{
		&EC_P384_PUB,
		&EC_P384_PRIV,
		&br_sha512_vtable, "test",
		"3780C4F67CB15518B6ACAE34C9F83568D2E12E47DEAB6C50A4E4EE5319D1E8CE0E2CC8A136036DC4B9C00E6888F66B6C",
		"A0D5D090C9980FAF3C2CE57B7AE951D31977DD11C775D314AF55F76C676447D06FB6495CD21B4B6E340FC236584FB277976984E59B4C77B0E8E4460DCA3D9F20E07B9BB1F63BEEFAF576F6B2E8B224634A2092CD3792E0159AD9CEE37659C736",
		"3066023100A0D5D090C9980FAF3C2CE57B7AE951D31977DD11C775D314AF55F76C676447D06FB6495CD21B4B6E340FC236584FB277023100976984E59B4C77B0E8E4460DCA3D9F20E07B9BB1F63BEEFAF576F6B2E8B224634A2092CD3792E0159AD9CEE37659C736"
	},

	/* Test vectors for P-521, from RFC 6979. */
	{
		&EC_P521_PUB,
		&EC_P521_PRIV,
		&br_sha1_vtable, "sample",
		"0089C071B419E1C2820962321787258469511958E80582E95D8378E0C2CCDB3CB42BEDE42F50E3FA3C71F5A76724281D31D9C89F0F91FC1BE4918DB1C03A5838D0F9",
		"00343B6EC45728975EA5CBA6659BBB6062A5FF89EEA58BE3C80B619F322C87910FE092F7D45BB0F8EEE01ED3F20BABEC079D202AE677B243AB40B5431D497C55D75D00E7B0E675A9B24413D448B8CC119D2BF7B2D2DF032741C096634D6D65D0DBE3D5694625FB9E8104D3B842C1B0E2D0B98BEA19341E8676AEF66AE4EBA3D5475D5D16",
		"3081870241343B6EC45728975EA5CBA6659BBB6062A5FF89EEA58BE3C80B619F322C87910FE092F7D45BB0F8EEE01ED3F20BABEC079D202AE677B243AB40B5431D497C55D75D024200E7B0E675A9B24413D448B8CC119D2BF7B2D2DF032741C096634D6D65D0DBE3D5694625FB9E8104D3B842C1B0E2D0B98BEA19341E8676AEF66AE4EBA3D5475D5D16"
	},
	{
		&EC_P521_PUB,
		&EC_P521_PRIV,
		&br_sha224_vtable, "sample",
		"0121415EC2CD7726330A61F7F3FA5DE14BE9436019C4DB8CB4041F3B54CF31BE0493EE3F427FB906393D895A19C9523F3A1D54BB8702BD4AA9C99DAB2597B92113F3",
		"01776331CFCDF927D666E032E00CF776187BC9FDD8E69D0DABB4109FFE1B5E2A30715F4CC923A4A5E94D2503E9ACFED92857B7F31D7152E0F8C00C15FF3D87E2ED2E0050CB5265417FE2320BBB5A122B8E1A32BD699089851128E360E620A30C7E17BA41A666AF126CE100E5799B153B60528D5300D08489CA9178FB610A2006C254B41F",
		"308187024201776331CFCDF927D666E032E00CF776187BC9FDD8E69D0DABB4109FFE1B5E2A30715F4CC923A4A5E94D2503E9ACFED92857B7F31D7152E0F8C00C15FF3D87E2ED2E024150CB5265417FE2320BBB5A122B8E1A32BD699089851128E360E620A30C7E17BA41A666AF126CE100E5799B153B60528D5300D08489CA9178FB610A2006C254B41F"
	},
	{
		&EC_P521_PUB,
		&EC_P521_PRIV,
		&br_sha256_vtable, "sample",
		"00EDF38AFCAAECAB4383358B34D67C9F2216C8382AAEA44A3DAD5FDC9C32575761793FEF24EB0FC276DFC4F6E3EC476752F043CF01415387470BCBD8678ED2C7E1A0",
		"01511BB4D675114FE266FC4372B87682BAECC01D3CC62CF2303C92B3526012659D16876E25C7C1E57648F23B73564D67F61C6F14D527D54972810421E7D87589E1A7004A171143A83163D6DF460AAF61522695F207A58B95C0644D87E52AA1A347916E4F7A72930B1BC06DBE22CE3F58264AFD23704CBB63B29B931F7DE6C9D949A7ECFC",
		"308187024201511BB4D675114FE266FC4372B87682BAECC01D3CC62CF2303C92B3526012659D16876E25C7C1E57648F23B73564D67F61C6F14D527D54972810421E7D87589E1A702414A171143A83163D6DF460AAF61522695F207A58B95C0644D87E52AA1A347916E4F7A72930B1BC06DBE22CE3F58264AFD23704CBB63B29B931F7DE6C9D949A7ECFC"
	},
	{
		&EC_P521_PUB,
		&EC_P521_PRIV,
		&br_sha384_vtable, "sample",
		"01546A108BC23A15D6F21872F7DED661FA8431DDBD922D0DCDB77CC878C8553FFAD064C95A920A750AC9137E527390D2D92F153E66196966EA554D9ADFCB109C4211",
		"01EA842A0E17D2DE4F92C15315C63DDF72685C18195C2BB95E572B9C5136CA4B4B576AD712A52BE9730627D16054BA40CC0B8D3FF035B12AE75168397F5D50C6745101F21A3CEE066E1961025FB048BD5FE2B7924D0CD797BABE0A83B66F1E35EEAF5FDE143FA85DC394A7DEE766523393784484BDF3E00114A1C857CDE1AA203DB65D61",
		"308188024201EA842A0E17D2DE4F92C15315C63DDF72685C18195C2BB95E572B9C5136CA4B4B576AD712A52BE9730627D16054BA40CC0B8D3FF035B12AE75168397F5D50C67451024201F21A3CEE066E1961025FB048BD5FE2B7924D0CD797BABE0A83B66F1E35EEAF5FDE143FA85DC394A7DEE766523393784484BDF3E00114A1C857CDE1AA203DB65D61"
	},
	{
		&EC_P521_PUB,
		&EC_P521_PRIV,
		&br_sha512_vtable, "sample",
		"01DAE2EA071F8110DC26882D4D5EAE0621A3256FC8847FB9022E2B7D28E6F10198B1574FDD03A9053C08A1854A168AA5A57470EC97DD5CE090124EF52A2F7ECBFFD3",
		"00C328FAFCBD79DD77850370C46325D987CB525569FB63C5D3BC53950E6D4C5F174E25A1EE9017B5D450606ADD152B534931D7D4E8455CC91F9B15BF05EC36E377FA00617CCE7CF5064806C467F678D3B4080D6F1CC50AF26CA209417308281B68AF282623EAA63E5B5C0723D8B8C37FF0777B1A20F8CCB1DCCC43997F1EE0E44DA4A67A",
		"308187024200C328FAFCBD79DD77850370C46325D987CB525569FB63C5D3BC53950E6D4C5F174E25A1EE9017B5D450606ADD152B534931D7D4E8455CC91F9B15BF05EC36E377FA0241617CCE7CF5064806C467F678D3B4080D6F1CC50AF26CA209417308281B68AF282623EAA63E5B5C0723D8B8C37FF0777B1A20F8CCB1DCCC43997F1EE0E44DA4A67A"
	},
	{
		&EC_P521_PUB,
		&EC_P521_PRIV,
		&br_sha1_vtable, "test",
		"00BB9F2BF4FE1038CCF4DABD7139A56F6FD8BB1386561BD3C6A4FC818B20DF5DDBA80795A947107A1AB9D12DAA615B1ADE4F7A9DC05E8E6311150F47F5C57CE8B222",
		"013BAD9F29ABE20DE37EBEB823C252CA0F63361284015A3BF430A46AAA80B87B0693F0694BD88AFE4E661FC33B094CD3B7963BED5A727ED8BD6A3A202ABE009D036701E9BB81FF7944CA409AD138DBBEE228E1AFCC0C890FC78EC8604639CB0DBDC90F717A99EAD9D272855D00162EE9527567DD6A92CBD629805C0445282BBC916797FF",
		"3081880242013BAD9F29ABE20DE37EBEB823C252CA0F63361284015A3BF430A46AAA80B87B0693F0694BD88AFE4E661FC33B094CD3B7963BED5A727ED8BD6A3A202ABE009D0367024201E9BB81FF7944CA409AD138DBBEE228E1AFCC0C890FC78EC8604639CB0DBDC90F717A99EAD9D272855D00162EE9527567DD6A92CBD629805C0445282BBC916797FF"
	},
	{
		&EC_P521_PUB,
		&EC_P521_PRIV,
		&br_sha224_vtable, "test",
		"0040D09FCF3C8A5F62CF4FB223CBBB2B9937F6B0577C27020A99602C25A01136987E452988781484EDBBCF1C47E554E7FC901BC3085E5206D9F619CFF07E73D6F706",
		"01C7ED902E123E6815546065A2C4AF977B22AA8EADDB68B2C1110E7EA44D42086BFE4A34B67DDC0E17E96536E358219B23A706C6A6E16BA77B65E1C595D43CAE17FB0177336676304FCB343CE028B38E7B4FBA76C1C1B277DA18CAD2A8478B2A9A9F5BEC0F3BA04F35DB3E4263569EC6AADE8C92746E4C82F8299AE1B8F1739F8FD519A4",
		"308188024201C7ED902E123E6815546065A2C4AF977B22AA8EADDB68B2C1110E7EA44D42086BFE4A34B67DDC0E17E96536E358219B23A706C6A6E16BA77B65E1C595D43CAE17FB02420177336676304FCB343CE028B38E7B4FBA76C1C1B277DA18CAD2A8478B2A9A9F5BEC0F3BA04F35DB3E4263569EC6AADE8C92746E4C82F8299AE1B8F1739F8FD519A4"
	},
	{
		&EC_P521_PUB,
		&EC_P521_PRIV,
		&br_sha256_vtable, "test",
		"001DE74955EFAABC4C4F17F8E84D881D1310B5392D7700275F82F145C61E843841AF09035BF7A6210F5A431A6A9E81C9323354A9E69135D44EBD2FCAA7731B909258",
		"000E871C4A14F993C6C7369501900C4BC1E9C7B0B4BA44E04868B30B41D8071042EB28C4C250411D0CE08CD197E4188EA4876F279F90B3D8D74A3C76E6F1E4656AA800CD52DBAA33B063C3A6CD8058A1FB0A46A4754B034FCC644766CA14DA8CA5CA9FDE00E88C1AD60CCBA759025299079D7A427EC3CC5B619BFBC828E7769BCD694E86",
		"30818702410E871C4A14F993C6C7369501900C4BC1E9C7B0B4BA44E04868B30B41D8071042EB28C4C250411D0CE08CD197E4188EA4876F279F90B3D8D74A3C76E6F1E4656AA8024200CD52DBAA33B063C3A6CD8058A1FB0A46A4754B034FCC644766CA14DA8CA5CA9FDE00E88C1AD60CCBA759025299079D7A427EC3CC5B619BFBC828E7769BCD694E86"
	},
	{
		&EC_P521_PUB,
		&EC_P521_PRIV,
		&br_sha384_vtable, "test",
		"01F1FC4A349A7DA9A9E116BFDD055DC08E78252FF8E23AC276AC88B1770AE0B5DCEB1ED14A4916B769A523CE1E90BA22846AF11DF8B300C38818F713DADD85DE0C88",
		"014BEE21A18B6D8B3C93FAB08D43E739707953244FDBE924FA926D76669E7AC8C89DF62ED8975C2D8397A65A49DCC09F6B0AC62272741924D479354D74FF6075578C0133330865C067A0EAF72362A65E2D7BC4E461E8C8995C3B6226A21BD1AA78F0ED94FE536A0DCA35534F0CD1510C41525D163FE9D74D134881E35141ED5E8E95B979",
		"3081880242014BEE21A18B6D8B3C93FAB08D43E739707953244FDBE924FA926D76669E7AC8C89DF62ED8975C2D8397A65A49DCC09F6B0AC62272741924D479354D74FF6075578C02420133330865C067A0EAF72362A65E2D7BC4E461E8C8995C3B6226A21BD1AA78F0ED94FE536A0DCA35534F0CD1510C41525D163FE9D74D134881E35141ED5E8E95B979"
	},
	{
		&EC_P521_PUB,
		&EC_P521_PRIV,
		&br_sha512_vtable, "test",
		"016200813020EC986863BEDFC1B121F605C1215645018AEA1A7B215A564DE9EB1B38A67AA1128B80CE391C4FB71187654AAA3431027BFC7F395766CA988C964DC56D",
		"013E99020ABF5CEE7525D16B69B229652AB6BDF2AFFCAEF38773B4B7D08725F10CDB93482FDCC54EDCEE91ECA4166B2A7C6265EF0CE2BD7051B7CEF945BABD47EE6D01FBD0013C674AA79CB39849527916CE301C66EA7CE8B80682786AD60F98F7E78A19CA69EFF5C57400E3B3A0AD66CE0978214D13BAF4E9AC60752F7B155E2DE4DCE3",
		"3081880242013E99020ABF5CEE7525D16B69B229652AB6BDF2AFFCAEF38773B4B7D08725F10CDB93482FDCC54EDCEE91ECA4166B2A7C6265EF0CE2BD7051B7CEF945BABD47EE6D024201FBD0013C674AA79CB39849527916CE301C66EA7CE8B80682786AD60F98F7E78A19CA69EFF5C57400E3B3A0AD66CE0978214D13BAF4E9AC60752F7B155E2DE4DCE3"
	},

	/* Terminator for list of test vectors. */
	{
		0, 0, 0, 0, 0, 0, 0
	}
};

static void
test_ECDSA_KAT(const br_ec_impl *iec,
	br_ecdsa_sign sign, br_ecdsa_vrfy vrfy, int asn1)
{
	size_t u;

	for (u = 0;; u ++) {
		const ecdsa_kat_vector *kv;
		unsigned char hash[64];
		size_t hash_len;
		unsigned char sig[150], sig2[150];
		size_t sig_len, sig2_len;
		br_hash_compat_context hc;

		kv = &ECDSA_KAT[u];
		if (kv->pub == 0) {
			break;
		}
		kv->hf->init(&hc.vtable);
		kv->hf->update(&hc.vtable, kv->msg, strlen(kv->msg));
		kv->hf->out(&hc.vtable, hash);
		hash_len = (kv->hf->desc >> BR_HASHDESC_OUT_OFF)
			& BR_HASHDESC_OUT_MASK;
		if (asn1) {
			sig_len = hextobin(sig, kv->sasn1);
		} else {
			sig_len = hextobin(sig, kv->sraw);
		}

		if (vrfy(iec, hash, hash_len,
			kv->pub, sig, sig_len) != 1)
		{
			fprintf(stderr, "ECDSA KAT verify failed (1)\n");
			exit(EXIT_FAILURE);
		}
		hash[0] ^= 0x80;
		if (vrfy(iec, hash, hash_len,
			kv->pub, sig, sig_len) != 0)
		{
			fprintf(stderr, "ECDSA KAT verify shoud have failed\n");
			exit(EXIT_FAILURE);
		}
		hash[0] ^= 0x80;
		if (vrfy(iec, hash, hash_len,
			kv->pub, sig, sig_len) != 1)
		{
			fprintf(stderr, "ECDSA KAT verify failed (2)\n");
			exit(EXIT_FAILURE);
		}

		sig2_len = sign(iec, kv->hf, hash, kv->priv, sig2);
		if (sig2_len == 0) {
			fprintf(stderr, "ECDSA KAT sign failed\n");
			exit(EXIT_FAILURE);
		}
		if (sig2_len != sig_len || memcmp(sig, sig2, sig_len) != 0) {
			fprintf(stderr, "ECDSA KAT wrong signature value\n");
			exit(EXIT_FAILURE);
		}

		printf(".");
		fflush(stdout);
	}
}

static void
test_ECDSA_i31(void)
{
	printf("Test ECDSA/i31: ");
	fflush(stdout);
	printf("[raw]");
	fflush(stdout);
	test_ECDSA_KAT(&br_ec_prime_i31,
		&br_ecdsa_i31_sign_raw, &br_ecdsa_i31_vrfy_raw, 0);
	printf(" [asn1]");
	fflush(stdout);
	test_ECDSA_KAT(&br_ec_prime_i31,
		&br_ecdsa_i31_sign_asn1, &br_ecdsa_i31_vrfy_asn1, 1);
	printf(" done.\n");
	fflush(stdout);
}

static void
test_ECDSA_i15(void)
{
	printf("Test ECDSA/i15: ");
	fflush(stdout);
	printf("[raw]");
	fflush(stdout);
	test_ECDSA_KAT(&br_ec_prime_i15,
		&br_ecdsa_i15_sign_raw, &br_ecdsa_i15_vrfy_raw, 0);
	printf(" [asn1]");
	fflush(stdout);
	test_ECDSA_KAT(&br_ec_prime_i31,
		&br_ecdsa_i15_sign_asn1, &br_ecdsa_i15_vrfy_asn1, 1);
	printf(" done.\n");
	fflush(stdout);
}

static void
test_modpow_i31(void)
{
	br_hmac_drbg_context hc;
	int k;

	printf("Test ModPow/i31: ");

	br_hmac_drbg_init(&hc, &br_sha256_vtable, "seed modpow", 11);
	for (k = 10; k <= 500; k ++) {
		size_t blen;
		unsigned char bm[128], bx[128], bx1[128], bx2[128];
		unsigned char be[128];
		unsigned mask;
		uint32_t x1[35], m1[35];
		uint16_t x2[70], m2[70];
		uint32_t tmp1[1000];
		uint16_t tmp2[2000];

		blen = (k + 7) >> 3;
		br_hmac_drbg_generate(&hc, bm, blen);
		br_hmac_drbg_generate(&hc, bx, blen);
		br_hmac_drbg_generate(&hc, be, blen);
		bm[blen - 1] |= 0x01;
		mask = 0xFF >> ((int)(blen << 3) - k);
		bm[0] &= mask;
		bm[0] |= (mask - (mask >> 1));
		bx[0] &= (mask >> 1);

		br_i31_decode(m1, bm, blen);
		br_i31_decode_mod(x1, bx, blen, m1);
		br_i31_modpow_opt(x1, be, blen, m1, br_i31_ninv31(m1[1]),
			tmp1, (sizeof tmp1) / (sizeof tmp1[0]));
		br_i31_encode(bx1, blen, x1);

		br_i15_decode(m2, bm, blen);
		br_i15_decode_mod(x2, bx, blen, m2);
		br_i15_modpow_opt(x2, be, blen, m2, br_i15_ninv15(m2[1]),
			tmp2, (sizeof tmp2) / (sizeof tmp2[0]));
		br_i15_encode(bx2, blen, x2);

		check_equals("ModPow i31/i15", bx1, bx2, blen);

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
	fflush(stdout);
}

static void
test_modpow_i62(void)
{
	br_hmac_drbg_context hc;
	int k;

	printf("Test ModPow/i62: ");

	br_hmac_drbg_init(&hc, &br_sha256_vtable, "seed modpow", 11);
	for (k = 10; k <= 500; k ++) {
		size_t blen;
		unsigned char bm[128], bx[128], bx1[128], bx2[128];
		unsigned char be[128];
		unsigned mask;
		uint32_t x1[35], m1[35];
		uint16_t x2[70], m2[70];
		uint64_t tmp1[500];
		uint16_t tmp2[2000];

		blen = (k + 7) >> 3;
		br_hmac_drbg_generate(&hc, bm, blen);
		br_hmac_drbg_generate(&hc, bx, blen);
		br_hmac_drbg_generate(&hc, be, blen);
		bm[blen - 1] |= 0x01;
		mask = 0xFF >> ((int)(blen << 3) - k);
		bm[0] &= mask;
		bm[0] |= (mask - (mask >> 1));
		bx[0] &= (mask >> 1);

		br_i31_decode(m1, bm, blen);
		br_i31_decode_mod(x1, bx, blen, m1);
		br_i62_modpow_opt(x1, be, blen, m1, br_i31_ninv31(m1[1]),
			tmp1, (sizeof tmp1) / (sizeof tmp1[0]));
		br_i31_encode(bx1, blen, x1);

		br_i15_decode(m2, bm, blen);
		br_i15_decode_mod(x2, bx, blen, m2);
		br_i15_modpow_opt(x2, be, blen, m2, br_i15_ninv15(m2[1]),
			tmp2, (sizeof tmp2) / (sizeof tmp2[0]));
		br_i15_encode(bx2, blen, x2);

		check_equals("ModPow i62/i15", bx1, bx2, blen);

		printf(".");
		fflush(stdout);
	}

	printf(" done.\n");
	fflush(stdout);
}

static int
eq_name(const char *s1, const char *s2)
{
	for (;;) {
		int c1, c2;

		for (;;) {
			c1 = *s1 ++;
			if (c1 >= 'A' && c1 <= 'Z') {
				c1 += 'a' - 'A';
			} else {
				switch (c1) {
				case '-': case '_': case '.': case ' ':
					continue;
				}
			}
			break;
		}
		for (;;) {
			c2 = *s2 ++;
			if (c2 >= 'A' && c2 <= 'Z') {
				c2 += 'a' - 'A';
			} else {
				switch (c2) {
				case '-': case '_': case '.': case ' ':
					continue;
				}
			}
			break;
		}
		if (c1 != c2) {
			return 0;
		}
		if (c1 == 0) {
			return 1;
		}
	}
}

#define STU(x)   { &test_ ## x, #x }

static const struct {
	void (*fn)(void);
	const char *name;
} tfns[] = {
	STU(MD5),
	STU(SHA1),
	STU(SHA224),
	STU(SHA256),
	STU(SHA384),
	STU(SHA512),
	STU(MD5_SHA1),
	STU(multihash),
	STU(HMAC),
	STU(HKDF),
	STU(HMAC_DRBG),
	STU(AESCTR_DRBG),
	STU(PRF),
	STU(AES_big),
	STU(AES_small),
	STU(AES_ct),
	STU(AES_ct64),
	STU(AES_pwr8),
	STU(AES_x86ni),
	STU(AES_CTRCBC_big),
	STU(AES_CTRCBC_small),
	STU(AES_CTRCBC_ct),
	STU(AES_CTRCBC_ct64),
	STU(AES_CTRCBC_x86ni),
	STU(AES_CTRCBC_pwr8),
	STU(DES_tab),
	STU(DES_ct),
	STU(ChaCha20_ct),
	STU(ChaCha20_sse2),
	STU(Poly1305_ctmul),
	STU(Poly1305_ctmul32),
	STU(Poly1305_ctmulq),
	STU(Poly1305_i15),
	STU(RSA_i15),
	STU(RSA_i31),
	STU(RSA_i32),
	STU(RSA_i62),
	STU(GHASH_ctmul),
	STU(GHASH_ctmul32),
	STU(GHASH_ctmul64),
	STU(GHASH_pclmul),
	STU(GHASH_pwr8),
	STU(CCM),
	STU(EAX),
	STU(GCM),
	STU(EC_prime_i15),
	STU(EC_prime_i31),
	STU(EC_p256_m15),
	STU(EC_p256_m31),
	STU(EC_c25519_i15),
	STU(EC_c25519_i31),
	STU(EC_c25519_m15),
	STU(EC_c25519_m31),
	STU(ECDSA_i15),
	STU(ECDSA_i31),
	STU(modpow_i31),
	STU(modpow_i62),
	{ 0, 0 }
};

int
main(int argc, char *argv[])
{
	size_t u;

	if (argc <= 1) {
		printf("usage: testcrypto all | name...\n");
		printf("individual test names:\n");
		for (u = 0; tfns[u].name; u ++) {
			printf("   %s\n", tfns[u].name);
		}
	} else {
		for (u = 0; tfns[u].name; u ++) {
			int i;

			for (i = 1; i < argc; i ++) {
				if (eq_name(argv[i], tfns[u].name)
					|| eq_name(argv[i], "all"))
				{
					tfns[u].fn();
					break;
				}
			}
		}
	}
	return 0;
}
