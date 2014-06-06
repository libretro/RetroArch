/****************************************************************************
 * TinySMB
 * Nintendo Wii/GameCube SMB implementation
 *
 * NTLM authentication
 ****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ogc/machine/processor.h>
#include <stdint.h>

typedef struct
{
	uint32_t encrypt_subkeys[32];
	uint32_t decrypt_subkeys[32];
} gl_des_ctx;

extern void *md4_buffer(const char *buffer, size_t len, void *resblock);
extern void gl_des_setkey(gl_des_ctx *ctx, const char * key);
extern void gl_des_ecb_encrypt(gl_des_ctx *ctx, const char * from, char * to);

/* C89 compliant way to cast 'char' to 'unsigned char'. */
static inline unsigned char to_uchar(char ch)
{
	return ch;
}

/*
 * turns a 56 bit key into the 64 bit, and sets the key schedule ks.
 */
static void ntlm_convert_key(char *key_56, gl_des_ctx * ks)
{
	char key[8];

	key[0] = to_uchar(key_56[0]);
	key[1] = ((to_uchar(key_56[0]) << 7) & 0xFF) | (to_uchar(key_56[1]) >> 1);
	key[2] = ((to_uchar(key_56[1]) << 6) & 0xFF) | (to_uchar(key_56[2]) >> 2);
	key[3] = ((to_uchar(key_56[2]) << 5) & 0xFF) | (to_uchar(key_56[3]) >> 3);
	key[4] = ((to_uchar(key_56[3]) << 4) & 0xFF) | (to_uchar(key_56[4]) >> 4);
	key[5] = ((to_uchar(key_56[4]) << 3) & 0xFF) | (to_uchar(key_56[5]) >> 5);
	key[6] = ((to_uchar(key_56[5]) << 2) & 0xFF) | (to_uchar(key_56[6]) >> 6);
	key[7] = (to_uchar(key_56[6]) << 1) & 0xFF;

	gl_des_setkey(ks, key);

	memset(&key, 0, sizeof(key));
}

/*
 * takes a 21 byte array and treats it as 3 56-bit DES keys. The
 * 8 byte plaintext is encrypted with each key and the resulting 24
 * bytes are stored in the results array.
 */
static void ntlm_encrypt_answer(char *hash, const char *challenge, char *answer)
{
	gl_des_ctx ks;

	ntlm_convert_key(hash, &ks);
	gl_des_ecb_encrypt(&ks, challenge, answer);

	ntlm_convert_key(&hash[7], &ks);
	gl_des_ecb_encrypt(&ks, challenge, &answer[8]);

	ntlm_convert_key(&hash[14], &ks);
	gl_des_ecb_encrypt(&ks, challenge, &answer[16]);

	memset(&ks, 0, sizeof(ks));
}

void ntlm_smb_nt_encrypt(const char *passwd, const u8 * challenge, u8 * answer)
{
	size_t len, i;
	unsigned char hash[24];
	unsigned char nt_pw[256];

	/* NT resp */
	len = strlen(passwd);
	if (len > 128)
		len = 128;
	for (i = 0; i < len; ++i)
	{
		nt_pw[2 * i] = passwd[i];
		nt_pw[2 * i + 1] = 0;
	}

	md4_buffer((const char *) nt_pw, len * 2, hash);

	memset(hash + 16, 0, 5);
	ntlm_encrypt_answer((char *) hash, (const char *) challenge,
			(char *) answer);

	/* with security is best be pedantic */
	memset(hash, 0, sizeof(hash));
	memset(nt_pw, 0, sizeof(nt_pw));
}
