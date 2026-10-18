#!/usr/bin/env python3
"""Model queued draws using the shipping Metal constant-binding code."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/metal.m").read_text(encoding="utf-8")
match = re.search(r"^static bool metal_bind_slang_constants\(", source, re.M)
assert match, "Metal per-draw constant binding is missing"
brace = source.index("{", match.start())
end, depth = brace + 1, 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
binding = source[match.start():end]
assert "if (![self _chainBind:e pass:i encoder:rce])" in source
assert "else if (![vs->frameView bindViewChainLast:c encoder:d->rce])" in source
assert source.index("if (!data || buffer.length < buffer_sem->size)") < source.index("while (uniform->size)", source.index("- (BOOL)_chainBind:"))
assert "commandBufferWithUnretainedReferences" not in source

for metal, native in (("id<MTLRenderCommandEncoder>", "struct encoder *"),
                      ("id<MTLDevice>", "struct device *"),
                      ("id<MTLBuffer>", "struct buffer *")):
    binding = binding.replace(metal, native)
binding = re.sub(r"\[rce set(Vertex|Fragment)Bytes:data\s+length:size atIndex:buffer_sem->binding\]",
                 lambda m: f"set_bytes(rce, {0 if m[1] == 'Vertex' else 1}, data, size, buffer_sem->binding)", binding)
binding = re.sub(r"\[rce set(Vertex|Fragment)Buffer:(buffer|snapshot) offset:0 atIndex:buffer_sem->binding\]",
                 lambda m: f"set_buffer(rce, {0 if m[1] == 'Vertex' else 1}, {m[2]}, buffer_sem->binding)", binding)
binding = re.sub(r"\[device newBufferWithBytes:data\s+length:size options:PLATFORM_METAL_RESOURCE_STORAGE_MODE\]",
                 "new_buffer(device, data, size)", binding)
binding = re.sub(r"\[(buffer|snapshot) didModifyRange:NSMakeRange\(0, (size|buffer.length)\)\]",
                 lambda m: f"modify_buffer({m[1]}, {m[2]})", binding)
binding = re.sub(r"\b(buffer|snapshot)\.(length|contents)", r"\1->\2", binding)
binding = binding.replace("nil", "NULL")
assert "[rce " not in binding and "[device " not in binding, binding

fixture = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
typedef int bool;
typedef size_t NSUInteger;
#define true 1
#define false 0
#define SLANG_STAGE_VERTEX_MASK 1
#define SLANG_STAGE_FRAGMENT_MASK 2
#define RARCH_AUTORELEASE_R(x) (x)
typedef struct { unsigned stage_mask, binding, size; } cbuffer_sem_t;
struct buffer { unsigned char *contents; size_t length; unsigned modified; struct buffer *next; };
struct device { struct buffer *buffers; unsigned large_calls; int fail; };
struct encoder { struct device *device; struct buffer *stage[2]; unsigned index[2]; unsigned bytes_calls, buffer_calls; };
static struct buffer *copy_buffer(struct device *d, const void *data, size_t length)
{
   struct buffer *b = (struct buffer*)calloc(1, sizeof(*b));
   if (!b || !(b->contents = (unsigned char*)malloc(length)))
      exit(2);
   b->length = length;
   memcpy(b->contents, data, length);
   b->next = d->buffers;
   d->buffers = b;
   return b;
}
static struct buffer *new_buffer(struct device *d, const void *data, size_t length)
{
   if (!d)
      return NULL;
   d->large_calls++;
   return d->fail ? NULL : copy_buffer(d, data, length);
}
static void set_bytes(struct encoder *e, unsigned stage, const void *data, size_t length, unsigned index)
{
   e->stage[stage] = copy_buffer(e->device, data, length);
   e->index[stage] = index;
   e->bytes_calls++;
}
static void set_buffer(struct encoder *e, unsigned stage, struct buffer *b, unsigned index)
{
   e->stage[stage] = b;
   e->index[stage] = index;
   e->buffer_calls++;
}
static void modify_buffer(struct buffer *b, size_t length)
{
   if (length != b->length)
      exit(2);
   b->modified++;
}
static void cleanup(struct device *d)
{
   while (d->buffers)
   {
      struct buffer *b = d->buffers;
      d->buffers = b->next;
      free(b->contents);
      free(b);
   }
}
BINDING
int main(void)
{
   const unsigned sizes[] = { 16, 4095, 4096, 8192 };
   struct device device = { 0 };
   struct buffer staging = { 0 };
   unsigned n, mask, stage;
   (void)modify_buffer;
   staging.contents = (unsigned char*)malloc(8192);
   staging.length = 8192;
   CHECK(staging.contents);
   for (n = 0; n < sizeof(sizes) / sizeof(sizes[0]); n++)
   {
      for (mask = 1; mask <= 3; mask++)
      {
         cbuffer_sem_t semantic = { 0 };
         struct encoder first = { 0 }, second = { 0 };
         unsigned stages = mask == 3 ? 2 : 1;
         first.device = second.device = &device;
         semantic.stage_mask = mask;
         semantic.binding = 7;
         semantic.size = sizes[n];
         device.large_calls = 0;
         memset(staging.contents, 0x11, staging.length);
         CHECK(metal_bind_slang_constants(&first, &device, &semantic, &staging));
         memset(staging.contents, 0x22, staging.length);
         CHECK(metal_bind_slang_constants(&second, &device, &semantic, &staging));
         memset(staging.contents, 0x33, staging.length);
         for (stage = 0; stage < 2; stage++)
         {
            if (mask & (1u << stage))
            {
               CHECK(first.stage[stage] && second.stage[stage]);
               CHECK(first.stage[stage]->length == semantic.size);
               CHECK(first.stage[stage]->contents[0] == 0x11);
               CHECK(first.stage[stage]->contents[semantic.size - 1] == 0x11);
               CHECK(second.stage[stage]->contents[0] == 0x22);
               CHECK(first.index[stage] == 7 && second.index[stage] == 7);
#ifdef HAVE_COCOATOUCH
               CHECK(first.stage[stage]->modified == 0);
#else
               CHECK(first.stage[stage]->modified == (semantic.size < 4096 ? 0u : 1u));
#endif
            }
            else
               CHECK(!first.stage[stage] && !second.stage[stage]);
         }
         CHECK(device.large_calls == (semantic.size < 4096 ? 0u : 2u));
         CHECK(first.bytes_calls == (semantic.size < 4096 ? stages : 0u));
         CHECK(first.buffer_calls == (semantic.size < 4096 ? 0u : stages));
         if (semantic.size >= 4096 && mask == 3)
            CHECK(first.stage[0] == first.stage[1]);
         cleanup(&device);
      }
   }
   {
      cbuffer_sem_t semantic = { 3, 7, 8192 };
      struct encoder encoder = { 0 };
      encoder.device = &device;
      device.fail = 1;
      CHECK(!metal_bind_slang_constants(&encoder, &device, &semantic, &staging));
      CHECK(!encoder.stage[0] && !encoder.stage[1]);
      device.fail = 0;
      CHECK(!metal_bind_slang_constants(NULL, &device, &semantic, &staging));
      CHECK(!metal_bind_slang_constants(&encoder, &device, &semantic, NULL));
      staging.length = 16;
      CHECK(!metal_bind_slang_constants(&encoder, &device, &semantic, &staging));
      staging.length = 8192;
      semantic.size = 16;
      free(staging.contents);
      staging.contents = NULL;
      CHECK(!metal_bind_slang_constants(&encoder, &device, &semantic, &staging));
      semantic.size = 0;
      CHECK(metal_bind_slang_constants(&encoder, &device, &semantic, NULL));
   }
   cleanup(&device);
   puts("PASS: queued Metal constants survive staging reuse, boundaries and allocation refusal");
   return 0;
}
'''
code = fixture.replace("BINDING", binding)
cc = shlex.split(os.environ.get("CC", "cc"))
flags = shlex.split(os.environ.get("CFLAGS", ""))
with tempfile.TemporaryDirectory(prefix="retroarch-metal-constants-") as directory:
    directory = Path(directory)
    src = directory / "constants.c"
    exe = directory / ("constants.exe" if os.name == "nt" else "constants")
    src.write_text(code, encoding="utf-8")
    for cocoa in (False, True):
        defines = ["-DHAVE_COCOATOUCH"] if cocoa else []
        subprocess.run(cc + flags + defines + ["-std=gnu89", "-O1", "-Wall", "-Wextra",
                       "-Werror", "-Werror=declaration-after-statement", str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
