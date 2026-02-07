/**************************** hmac.c ****************************/
/******************** See RFC 4634 for details ******************/
/*
 *  Description:
 *      This file implements the HMAC algorithm (Keyed-Hashing for
 *      Message Authentication, RFC2104), expressed in terms of the
 *      various SHA algorithms.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "compat.h"

#include "sha.h"

/*
 *  hmac
 *
 *  Description:
 *      This function will compute an HMAC message digest.
 *
 *  Parameters:
 *      whichSha: [in]
 *          One of SHA1, SHA224, SHA256, SHA384, SHA512
 *      key: [in]
 *          The secret shared key.
 *      key_len: [in]
 *          The length of the secret shared key.
 *      message_array: [in]
 *          An array of characters representing the message.
 *      length: [in]
 *          The length of the message in message_array
 *      digest: [out]
 *          Where the digest is returned.
 *          NOTE: The length of the digest is determined by
 *              the value of whichSha.
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int
hmac (SHAversion whichSha, const unsigned char *text, size_t text_len,
      const unsigned char *key, size_t key_len, uint8_t digest[USHAMaxHashSize])
{
  HMACContext ctx;
  return hmacReset (&ctx, whichSha, key, key_len) ||
    hmacInput (&ctx, text, text_len) || hmacResult (&ctx, digest);
}

/*
 *  hmacReset
 *
 *  Description:
 *      This function will initialize the hmacContext in preparation
 *      for computing a new HMAC message digest.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to reset.
 *      whichSha: [in]
 *          One of SHA1, SHA224, SHA256, SHA384, SHA512
 *      key: [in]
 *          The secret shared key.
 *      key_len: [in]
 *          The length of the secret shared key.
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int
hmacReset (HMACContext * ctx, enum SHAversion whichSha,
	   const unsigned char *key, size_t key_len)
{
  int i, blocksize, hashsize;

  /* inner padding - key XORd with ipad */
  unsigned char k_ipad[USHA_Max_Message_Block_Size];

  /* temporary buffer when keylen > blocksize */
  unsigned char tempkey[USHAMaxHashSize];

  if (!ctx)
    return shaNull;

  blocksize = ctx->blockSize = USHABlockSize (whichSha);
  hashsize = ctx->hashSize = USHAHashSize (whichSha);

  ctx->whichSha = whichSha;

  /*
   * If key is longer than the hash blocksize,
   * reset it to key = HASH(key).
   */
  if (key_len > (size_t)blocksize)
    {
      USHAContext tctx;
      int err = USHAReset (&tctx, whichSha) ||
	USHAInput (&tctx, key, key_len) || USHAResult (&tctx, tempkey);
      if (err != shaSuccess)
	return err;

      key = tempkey;
      key_len = hashsize;
    }

  /*
   * The HMAC transform looks like:
   *
   * SHA(K XOR opad, SHA(K XOR ipad, text))
   *
   * where K is an n byte key.
   * ipad is the byte 0x36 repeated blocksize times
   * opad is the byte 0x5c repeated blocksize times
   * and text is the data being protected.
   */

  /* store key into the pads, XOR'd with ipad and opad values */
  for (i = 0; i < (int)key_len; i++)
    {
      k_ipad[i] = key[i] ^ 0x36;
      ctx->k_opad[i] = key[i] ^ 0x5c;
    }
  /* remaining pad bytes are '\0' XOR'd with ipad and opad values */
  for (; i < blocksize; i++)
    {
      k_ipad[i] = 0x36;
      ctx->k_opad[i] = 0x5c;
    }

  /* perform inner hash */
  /* init context for 1st pass */
  return USHAReset (&ctx->shaContext, whichSha) ||
    /* and start with inner pad */
    USHAInput (&ctx->shaContext, k_ipad, blocksize);
}

/*
 *  hmacInput
 *
 *  Description:
 *      This function accepts an array of octets as the next portion
 *      of the message.
 *
 *  Parameters:
 *      context: [in/out]
 *          The HMAC context to update
 *      message_array: [in]
 *          An array of characters representing the next portion of
 *          the message.
 *      length: [in]
 *          The length of the message in message_array
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int
hmacInput (HMACContext * ctx, const unsigned char *text, size_t text_len)
{
  if (!ctx)
    return shaNull;
  /* then text of datagram */
  return USHAInput (&ctx->shaContext, text, text_len);
}

/*
 * HMACFinalBits
 *
 * Description:
 *   This function will add in any final bits of the message.
 *
 * Parameters:
 *   context: [in/out]
 *     The HMAC context to update
 *   message_bits: [in]
 *     The final bits of the message, in the upper portion of the
 *     byte. (Use 0b###00000 instead of 0b00000### to input the
 *     three bits ###.)
 *   length: [in]
 *     The number of bits in message_bits, between 1 and 7.
 *
 * Returns:
 *   sha Error Code.
 */
int
hmacFinalBits (HMACContext * ctx, const uint8_t bits, size_t bitcount)
{
  if (!ctx)
    return shaNull;
  /* then final bits of datagram */
  return USHAFinalBits (&ctx->shaContext, bits, bitcount);
}

/*
 * HMACResult
 *
 * Description:
 *   This function will return the N-byte message digest into the
 *   Message_Digest array provided by the caller.
 *   NOTE: The first octet of hash is stored in the 0th element,
 *      the last octet of hash in the Nth element.
 *
 * Parameters:
 *   context: [in/out]
 *     The context to use to calculate the HMAC hash.
 *   digest: [out]
 *     Where the digest is returned.
 *   NOTE 2: The length of the hash is determined by the value of
 *      whichSha that was passed to hmacReset().
 *
 * Returns:
 *   sha Error Code.
 *
 */
int
hmacResult (HMACContext * ctx, uint8_t * digest)
{
  if (!ctx)
    return shaNull;

  /* finish up 1st pass */
  /* (Use digest here as a temporary buffer.) */
  return USHAResult (&ctx->shaContext, digest) ||
    /* perform outer SHA */
    /* init context for 2nd pass */
    USHAReset (&ctx->shaContext, ctx->whichSha) ||
    /* start with outer pad */
    USHAInput (&ctx->shaContext, ctx->k_opad, ctx->blockSize) ||
    /* then results of 1st hash */
    USHAInput (&ctx->shaContext, digest, ctx->hashSize) ||
    /* finish up 2nd pass */
    USHAResult (&ctx->shaContext, digest);
}
