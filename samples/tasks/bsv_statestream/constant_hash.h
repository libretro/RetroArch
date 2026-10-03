/* Forced in with -include by the collision build of this sample: every
 * object hashes alike, so each insert lands in one bucket and the
 * buckets' own growth, and its failure, is exercised. */
#define uint32s_hash_bytes(bytes, len) ((void)(bytes), (void)(len), 0u)
