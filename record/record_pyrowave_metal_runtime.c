/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_metal_internal.h"
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__) || defined(RECORD_PYROWAVE_METAL_FORCE_NATIVE)
#include <dlfcn.h>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#elif !defined(TARGET_OS_OSX)
#define TARGET_OS_OSX 1
#endif

typedef struct record_pyrowave_metal_size
{
   size_t width, height, depth;
} record_pyrowave_metal_size_t;

typedef struct record_pyrowave_metal_range
{
   size_t location, length;
} record_pyrowave_metal_range_t;

enum record_pyrowave_metal_selector
{
   RPY_MTL_RETAIN, RPY_MTL_RELEASE, RPY_MTL_ALLOC, RPY_MTL_NEW,
   RPY_MTL_UTF8, RPY_MTL_LANGUAGE, RPY_MTL_LIBRARY, RPY_MTL_FUNCTION,
   RPY_MTL_PIPELINE, RPY_MTL_THREADS, RPY_MTL_STATIC_MEMORY, RPY_MTL_MAX_MEMORY,
   RPY_MTL_BUFFER_LIMIT, RPY_MTL_UNIFIED, RPY_MTL_RESPONDS,
   RPY_MTL_BUFFER, RPY_MTL_CONTENTS, RPY_MTL_MODIFY, RPY_MTL_DESCRIPTOR,
   RPY_MTL_STORAGE, RPY_MTL_USAGE_SET, RPY_MTL_TEXTURE, RPY_MTL_DEVICE,
   RPY_MTL_WIDTH, RPY_MTL_HEIGHT, RPY_MTL_FORMAT, RPY_MTL_USAGE,
   RPY_MTL_TYPE, RPY_MTL_SAMPLES, RPY_MTL_ARRAY,
   RPY_MTL_QUEUE, RPY_MTL_STATUS, RPY_MTL_RETAINED,
   RPY_MTL_COMPUTE, RPY_MTL_SET_PIPELINE, RPY_MTL_SET_BUFFER,
   RPY_MTL_SET_TEXTURE, RPY_MTL_DISPATCH,
   RPY_MTL_END, RPY_MTL_BLIT, RPY_MTL_COPY, RPY_MTL_SYNCHRONIZE,
   RPY_MTL_SELECTORS
};

typedef struct record_pyrowave_metal_runtime
{
   void *objc_library, *metal_library, *foundation_library;
   void *selectors[RPY_MTL_SELECTORS];
   void *string_class, *options_class, *descriptor_class, *entry_name;
   void *(*get_class)(const char*);
   void *(*get_selector)(const char*);
   void *(*push_pool)(void);
   void (*pop_pool)(void*);
   void *(*object)(void*, void*);
   void (*nothing)(void*, void*);
   size_t (*number)(void*, void*);
   unsigned char (*boolean)(void*, void*);
   unsigned char (*responds)(void*, void*, void*);
   void *(*object_arg)(void*, void*, const void*);
   void (*void_object)(void*, void*, void*);
   void (*number_arg)(void*, void*, size_t);
   void *(*new_buffer)(void*, void*, size_t, size_t);
   void (*range_arg)(void*, void*, record_pyrowave_metal_range_t);
   void *(*new_library)(void*, void*, void*, void*, void**);
   void *(*new_pipeline)(void*, void*, void*, void**);
   void *(*new_descriptor)(void*, void*, size_t, size_t, size_t, unsigned char);
   void (*buffer_arg)(void*, void*, void*, size_t, size_t);
   void (*texture_arg)(void*, void*, void*, size_t);
   void (*dispatch_arg)(void*, void*, record_pyrowave_metal_size_t, record_pyrowave_metal_size_t);
   void (*copy_arg)(void*, void*, void*, size_t, void*, size_t, size_t);
   bool managed;
} record_pyrowave_metal_runtime_t;

static void *record_pyrowave_metal_native_retain(void *user, void *object)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   return object ? r->object(object, r->selectors[RPY_MTL_RETAIN]) : NULL;
}

static void record_pyrowave_metal_native_release(void *user, void *object)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   if (object) r->nothing(object, r->selectors[RPY_MTL_RELEASE]);
}

static void record_pyrowave_metal_native_destroy(void *user)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   record_pyrowave_metal_native_release(r, r->entry_name);
   if (r->foundation_library) dlclose(r->foundation_library);
   if (r->metal_library) dlclose(r->metal_library);
   if (r->objc_library) dlclose(r->objc_library);
   free(r);
}

static size_t record_pyrowave_metal_native_limit(void *user, void *device)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   return r->number(device, r->selectors[RPY_MTL_BUFFER_LIMIT]);
}

static void *record_pyrowave_metal_native_buffer(void *user, void *device, size_t bytes, bool cpu)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   /* MTLResourceStorageMode: shared=0, managed=1<<4, private=2<<4. */
   return r->new_buffer(device, r->selectors[RPY_MTL_BUFFER], bytes, cpu ? (r->managed ? 16 : 0) : 32);
}

static void *record_pyrowave_metal_native_data(void *user, void *buffer)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   return r->object(buffer, r->selectors[RPY_MTL_CONTENTS]);
}

static void record_pyrowave_metal_native_flush(void *user, void *buffer, size_t bytes)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   record_pyrowave_metal_range_t range;
   if (!r->managed) return;
   range.location = 0;
   range.length = bytes;
   r->range_arg(buffer, r->selectors[RPY_MTL_MODIFY], range);
}

static void *record_pyrowave_metal_native_texture(void *user, void *device, unsigned width, unsigned height)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   void *descriptor = r->new_descriptor(r->descriptor_class, r->selectors[RPY_MTL_DESCRIPTOR], 115, width, height, 0);
   if (!descriptor) return NULL;
   r->number_arg(descriptor, r->selectors[RPY_MTL_STORAGE], 2);
   r->number_arg(descriptor, r->selectors[RPY_MTL_USAGE_SET], 3);
   return r->object_arg(device, r->selectors[RPY_MTL_TEXTURE], descriptor);
}

static void *record_pyrowave_metal_native_pipeline(void *user, void *device, const char *source, unsigned threads)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   void *text = r->object_arg(r->object(r->string_class, r->selectors[RPY_MTL_ALLOC]),
         r->selectors[RPY_MTL_UTF8], source);
   void *options = r->object(r->options_class, r->selectors[RPY_MTL_NEW]);
   void *library;
   void *function;
   void *pipeline = NULL;
   if (!text || !options) goto done;
   r->number_arg(options, r->selectors[RPY_MTL_LANGUAGE], 131073);
   library = r->new_library(device, r->selectors[RPY_MTL_LIBRARY], text, options, NULL);
   if (!library) goto done;
   function = r->object_arg(library, r->selectors[RPY_MTL_FUNCTION], r->entry_name);
   if (function) pipeline = r->new_pipeline(device, r->selectors[RPY_MTL_PIPELINE], function, NULL);
   record_pyrowave_metal_native_release(r, function);
   record_pyrowave_metal_native_release(r, library);
   if (pipeline && (r->number(pipeline, r->selectors[RPY_MTL_THREADS]) < threads
            || r->number(pipeline, r->selectors[RPY_MTL_STATIC_MEMORY])
            > r->number(device, r->selectors[RPY_MTL_MAX_MEMORY])))
   {
      record_pyrowave_metal_native_release(r, pipeline);
      pipeline = NULL;
   }
done:
   record_pyrowave_metal_native_release(r, text);
   record_pyrowave_metal_native_release(r, options);
   return pipeline;
}

static bool record_pyrowave_metal_native_source(void *user, void *source, void *device, unsigned width, unsigned height)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   size_t format;
   if (!source || r->object(source, r->selectors[RPY_MTL_DEVICE]) != device
         || r->number(source, r->selectors[RPY_MTL_WIDTH]) < width
         || r->number(source, r->selectors[RPY_MTL_HEIGHT]) < height
         || r->number(source, r->selectors[RPY_MTL_TYPE]) != 2
         || r->number(source, r->selectors[RPY_MTL_SAMPLES]) != 1
         || r->number(source, r->selectors[RPY_MTL_ARRAY]) != 1
         || !(r->number(source, r->selectors[RPY_MTL_USAGE]) & 1)) return false;
   format = r->number(source, r->selectors[RPY_MTL_FORMAT]);
   return format == 70 || format == 80 || format == 90 || format == 115 || format == 125;
}

static bool record_pyrowave_metal_native_commands(void *user, void *commands, void *device, void *queue)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   return commands && r->object(commands, r->selectors[RPY_MTL_DEVICE]) == device
      && r->object(commands, r->selectors[RPY_MTL_QUEUE]) == queue
      && r->number(commands, r->selectors[RPY_MTL_STATUS]) <= 1
      && r->boolean(commands, r->selectors[RPY_MTL_RETAINED]);
}

static unsigned record_pyrowave_metal_native_status(void *user, void *commands)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   return (unsigned)r->number(commands, r->selectors[RPY_MTL_STATUS]);
}

static void *record_pyrowave_metal_native_compute(void *user, void *commands)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   return r->object(commands, r->selectors[RPY_MTL_COMPUTE]);
}

static void record_pyrowave_metal_native_set_pipeline(void *user, void *encoder, void *pipeline)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   r->void_object(encoder, r->selectors[RPY_MTL_SET_PIPELINE], pipeline);
}

static void record_pyrowave_metal_native_set_buffer(void *user, void *encoder, void *buffer, size_t offset, unsigned index)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   r->buffer_arg(encoder, r->selectors[RPY_MTL_SET_BUFFER], buffer, offset, index);
}

static void record_pyrowave_metal_native_set_texture(void *user, void *encoder, void *texture, unsigned index)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   r->texture_arg(encoder, r->selectors[RPY_MTL_SET_TEXTURE], texture, index);
}

static void record_pyrowave_metal_native_dispatch(void *user, void *encoder,
      unsigned x, unsigned y, unsigned z, unsigned tx, unsigned ty)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   record_pyrowave_metal_size_t groups;
   record_pyrowave_metal_size_t threads;
   groups.width = x; groups.height = y; groups.depth = z;
   threads.width = tx; threads.height = ty; threads.depth = 1;
   r->dispatch_arg(encoder, r->selectors[RPY_MTL_DISPATCH], groups, threads);
}

static void record_pyrowave_metal_native_barrier(void *user, void *encoder, bool textures)
{
   /* Serial compute and directly bound, tracked resources synchronize here. */
   (void)user;
   (void)encoder;
   (void)textures;
}

static void record_pyrowave_metal_native_end(void *user, void *encoder)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   r->nothing(encoder, r->selectors[RPY_MTL_END]);
}

static void *record_pyrowave_metal_native_blit(void *user, void *commands)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   return r->object(commands, r->selectors[RPY_MTL_BLIT]);
}

static void record_pyrowave_metal_native_copy(void *user, void *encoder,
      void *source, size_t source_offset, void *destination, size_t destination_offset, size_t bytes)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   r->copy_arg(encoder, r->selectors[RPY_MTL_COPY], source, source_offset, destination, destination_offset, bytes);
}

static void record_pyrowave_metal_native_synchronize(void *user, void *encoder, void *buffer)
{
   record_pyrowave_metal_runtime_t *r = (record_pyrowave_metal_runtime_t*)user;
   if (r->managed) r->void_object(encoder, r->selectors[RPY_MTL_SYNCHRONIZE], buffer);
}

static void *record_pyrowave_metal_native_pool_push(void *user)
{
   return ((record_pyrowave_metal_runtime_t*)user)->push_pool();
}

static void record_pyrowave_metal_native_pool_pop(void *user, void *pool)
{
   ((record_pyrowave_metal_runtime_t*)user)->pop_pool(pool);
}

bool record_pyrowave_metal_native_api(record_pyrowave_metal_api_t *api, void *device, void *queue)
{
   static const char *const names[RPY_MTL_SELECTORS] =
   {
      "retain", "release", "alloc", "new", "initWithUTF8String:", "setLanguageVersion:",
      "newLibraryWithSource:options:error:", "newFunctionWithName:",
      "newComputePipelineStateWithFunction:error:", "maxTotalThreadsPerThreadgroup",
      "staticThreadgroupMemoryLength", "maxThreadgroupMemoryLength", "maxBufferLength",
      "hasUnifiedMemory", "respondsToSelector:", "newBufferWithLength:options:", "contents",
      "didModifyRange:", "texture2DDescriptorWithPixelFormat:width:height:mipmapped:",
      "setStorageMode:", "setUsage:", "newTextureWithDescriptor:", "device", "width", "height",
      "pixelFormat", "usage", "textureType", "sampleCount", "arrayLength", "commandQueue",
      "status", "retainedReferences", "computeCommandEncoder", "setComputePipelineState:",
      "setBuffer:offset:atIndex:", "setTexture:atIndex:", "dispatchThreadgroups:threadsPerThreadgroup:",
      "endEncoding", "blitCommandEncoder",
      "copyFromBuffer:sourceOffset:toBuffer:destinationOffset:size:", "synchronizeResource:"
   };
   record_pyrowave_metal_runtime_t *r;
   void *symbol;
   void *pool;
   unsigned i;
   if (!api || !device || !queue) return false;
   r = (record_pyrowave_metal_runtime_t*)calloc(1, sizeof(*r));
   if (!r) return false;
   r->objc_library = dlopen("/usr/lib/libobjc.A.dylib", RTLD_LAZY | RTLD_LOCAL);
   r->metal_library = dlopen("/System/Library/Frameworks/Metal.framework/Metal", RTLD_LAZY | RTLD_LOCAL);
   r->foundation_library = dlopen("/System/Library/Frameworks/Foundation.framework/Foundation", RTLD_LAZY | RTLD_LOCAL);
   if (!r->objc_library || !r->metal_library || !r->foundation_library) goto error;
#define RPY_MTL_LOAD(member, name) do { \
   symbol = dlsym(r->objc_library, name); \
   if (!symbol || sizeof(r->member) != sizeof(symbol)) goto error; \
   memcpy(&r->member, &symbol, sizeof(symbol)); \
} while (0)
   RPY_MTL_LOAD(get_class, "objc_getClass");
   RPY_MTL_LOAD(get_selector, "sel_registerName");
   RPY_MTL_LOAD(push_pool, "objc_autoreleasePoolPush");
   RPY_MTL_LOAD(pop_pool, "objc_autoreleasePoolPop");
   RPY_MTL_LOAD(object, "objc_msgSend");
   RPY_MTL_LOAD(nothing, "objc_msgSend");
   RPY_MTL_LOAD(number, "objc_msgSend");
   RPY_MTL_LOAD(boolean, "objc_msgSend");
   RPY_MTL_LOAD(responds, "objc_msgSend");
   RPY_MTL_LOAD(object_arg, "objc_msgSend");
   RPY_MTL_LOAD(void_object, "objc_msgSend");
   RPY_MTL_LOAD(number_arg, "objc_msgSend");
   RPY_MTL_LOAD(new_buffer, "objc_msgSend");
   RPY_MTL_LOAD(range_arg, "objc_msgSend");
   RPY_MTL_LOAD(new_library, "objc_msgSend");
   RPY_MTL_LOAD(new_pipeline, "objc_msgSend");
   RPY_MTL_LOAD(new_descriptor, "objc_msgSend");
   RPY_MTL_LOAD(buffer_arg, "objc_msgSend");
   RPY_MTL_LOAD(texture_arg, "objc_msgSend");
   RPY_MTL_LOAD(dispatch_arg, "objc_msgSend");
   RPY_MTL_LOAD(copy_arg, "objc_msgSend");
#undef RPY_MTL_LOAD
   for (i = 0; i < RPY_MTL_SELECTORS; i++)
      if (!(r->selectors[i] = r->get_selector(names[i]))) goto error;
   r->string_class = r->get_class("NSString");
   r->options_class = r->get_class("MTLCompileOptions");
   r->descriptor_class = r->get_class("MTLTextureDescriptor");
   if (!r->string_class || !r->options_class || !r->descriptor_class
         || r->object(queue, r->selectors[RPY_MTL_DEVICE]) != device
         || !r->responds(device, r->selectors[RPY_MTL_RESPONDS], r->selectors[RPY_MTL_BUFFER_LIMIT])) goto error;
   pool = r->push_pool();
   r->entry_name = r->object_arg(r->object(r->string_class, r->selectors[RPY_MTL_ALLOC]),
         r->selectors[RPY_MTL_UTF8], "rpyrowave_main");
#if TARGET_OS_OSX
   r->managed = !r->responds(device, r->selectors[RPY_MTL_RESPONDS], r->selectors[RPY_MTL_UNIFIED])
      || !r->boolean(device, r->selectors[RPY_MTL_UNIFIED]);
#endif
   r->pop_pool(pool);
   if (!r->entry_name) goto error;
   memset(api, 0, sizeof(*api));
   api->user = r;
   api->retain = record_pyrowave_metal_native_retain;
   api->release = record_pyrowave_metal_native_release;
   api->destroy = record_pyrowave_metal_native_destroy;
   api->buffer_limit = record_pyrowave_metal_native_limit;
   api->buffer_new = record_pyrowave_metal_native_buffer;
   api->buffer_data = record_pyrowave_metal_native_data;
   api->buffer_flush = record_pyrowave_metal_native_flush;
   api->texture_new = record_pyrowave_metal_native_texture;
   api->pipeline_new = record_pyrowave_metal_native_pipeline;
   api->source_valid = record_pyrowave_metal_native_source;
   api->commands_valid = record_pyrowave_metal_native_commands;
   api->status = record_pyrowave_metal_native_status;
   api->compute_begin = record_pyrowave_metal_native_compute;
   api->compute_pipeline = record_pyrowave_metal_native_set_pipeline;
   api->compute_buffer = record_pyrowave_metal_native_set_buffer;
   api->compute_texture = record_pyrowave_metal_native_set_texture;
   api->compute_dispatch = record_pyrowave_metal_native_dispatch;
   api->compute_barrier = record_pyrowave_metal_native_barrier;
   api->end = record_pyrowave_metal_native_end;
   api->blit_begin = record_pyrowave_metal_native_blit;
   api->blit_copy = record_pyrowave_metal_native_copy;
   api->synchronize = record_pyrowave_metal_native_synchronize;
   api->pool_push = record_pyrowave_metal_native_pool_push;
   api->pool_pop = record_pyrowave_metal_native_pool_pop;
   return true;
error:
   record_pyrowave_metal_native_destroy(r);
   return false;
}
#else
bool record_pyrowave_metal_native_api(record_pyrowave_metal_api_t *api, void *device, void *queue)
{
   (void)api;
   (void)device;
   (void)queue;
   return false;
}
#endif
