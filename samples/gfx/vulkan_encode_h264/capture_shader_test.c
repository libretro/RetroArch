/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Capture shader acceptance requires assertions
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vulkan_encode_capture.h"
static unsigned calls;
static bool fail_new;
static const char *prefix;
static unsigned instructions(const vulkan_encode_capture_config_t *c,
      unsigned opcode, unsigned operand, uint32_t value)
{
   size_t at = 5, words = c->code_bytes / sizeof(uint32_t);
   unsigned count = 0;
   while (at < words)
   {
      uint32_t word = c->code[at];
      size_t length = word >> 16;
      assert(length && length <= words - at);
      if ((word & 65535) == opcode)
      {
         assert(length > operand);
         if (c->code[at + operand] == value)
            count++;
      }
      at += length;
   }
   assert(at == words);
   return count;
}
vulkan_encode_capture_t *vulkan_encode_capture_new(
      const vulkan_encode_capture_config_t *config,
      const vulkan_encode_capture_ops_t *ops)
{
   FILE *file;
   char path[256];
   assert(config && config->pool && ops && config->code &&
         config->code_bytes > 20 && !(config->code_bytes & 3) &&
         config->code[0] == 0x07230203);
   assert(instructions(config, 17, 1, 49) == (config->pool->storage_images ? 1u : 0u));
   assert(instructions(config, 25, 8, 15) == (config->pool->storage_images ? 1u : 0u));
   assert(instructions(config, 25, 8, 13) == (config->pool->storage_images ? 1u : 0u));
   assert(instructions(config, 16, 2, 17) == 1);
   if (prefix && !fail_new)
   {
      assert(strlen(prefix) < sizeof(path) - 32);
      sprintf(path, "%s-%s.spv", prefix,
            config->pool->storage_images ? "direct" : "buffer");
      file = fopen(path, "wb");
      assert(file && fwrite(config->code, 1, config->code_bytes, file) ==
            config->code_bytes && !fclose(file));
   }
   calls++;
   return fail_new ? NULL : (vulkan_encode_capture_t*)(uintptr_t)1;
}
int main(int argc, char **argv)
{
   static vulkan_encode_pool_t pool;
   static vulkan_encode_capture_ops_t ops;
   vulkan_encode_capture_t *capture;
   char error[256];
   unsigned mode;
   assert(argc == 1 || argc == 2);
   prefix = argc == 2 ? argv[1] : NULL;
   memset(&pool, 0, sizeof(pool));
   memset(&ops, 0, sizeof(ops));
   for (mode = 0; mode < 2; mode++)
   {
      pool.storage_images = mode != 0;
      capture = vulkan_encode_capture_new_compiled(&pool, &ops, error, sizeof(error));
#ifdef EXPECT_NO_EXTENDED
      assert(!capture && !calls && error[0]);
#else
      assert(capture == (vulkan_encode_capture_t*)(uintptr_t)1 && !error[0]);
      fail_new = true;
      assert(!vulkan_encode_capture_new_compiled(&pool, &ops, error, sizeof(error)));
      fail_new = false;
#endif
   }
#ifndef EXPECT_NO_EXTENDED
   assert(calls == 4);
#endif
   memset(error, 'x', sizeof(error));
   assert(!vulkan_encode_capture_new_compiled(NULL, &ops, error, sizeof(error)) && !error[0]);
   assert(!vulkan_encode_capture_new_compiled(&pool, NULL, NULL, 0));
   puts("Vulkan capture compiler shaders and startup borrow lifetime PASS");
   return 0;
}
