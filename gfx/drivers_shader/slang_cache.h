#ifndef SLANG_CACHE_H
#define SLANG_CACHE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of glslang_output structure */
struct glslang_output;

/**
 * Compute SHA256 hash of preprocessed shader source code
 *
 * @param vertex_source   Preprocessed vertex stage source (null-terminated string)
 * @param fragment_source Preprocessed fragment stage source (null-terminated string)
 * @param hash_out        Output buffer for hex-encoded hash (must be at least 65 bytes: 64 hex chars + null terminator)
 * @return true on success, false on error
 */
bool spirv_cache_compute_hash(const char *vertex_source, const char *fragment_source, char *hash_out);

/**
 * Load cached SPIR-V output from disk
 *
 * @param hash      Hex-encoded SHA256 hash (64 characters)
 * @param output    Output structure to populate with cached data
 * @return true if cache hit and successfully loaded, false otherwise
 */
bool spirv_cache_load(const char *hash, struct glslang_output *output);

/**
 * Save compiled SPIR-V output to disk cache
 *
 * @param hash      Hex-encoded SHA256 hash (64 characters)
 * @param output    Compiled output structure to cache
 * @return true on success, false on error
 */
bool spirv_cache_save(const char *hash, const struct glslang_output *output);

#ifdef __cplusplus
}
#endif

#endif /* SLANG_CACHE_H */
