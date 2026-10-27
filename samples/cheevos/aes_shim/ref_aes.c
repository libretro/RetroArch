/* rcheevos' own AES, under other names, as the reference */
#define AES_init_ctx           ref_AES_init_ctx
#define AES_init_ctx_iv        ref_AES_init_ctx_iv
#define AES_ctx_set_iv         ref_AES_ctx_set_iv
#define AES_CBC_decrypt_buffer ref_AES_CBC_decrypt_buffer
#define AES_CTR_xcrypt_buffer  ref_AES_CTR_xcrypt_buffer
#include "../../../deps/rcheevos/src/rhash/aes.c"
