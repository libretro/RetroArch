#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int GLint;
typedef unsigned GLuint;
#define GL_TRUE 1
#define GL_TEXTURE_2D 1
#define GL_TEXTURE_BINDING_2D 2
#define RA_WGL_DRAW_FRAMEBUFFER 3
#define RA_WGL_DRAW_FRAMEBUFFER_BINDING 4
#define RA_WGL_COLOR_ATTACHMENT0 5
#define RA_WGL_DEDICATED_MEMORY 6
#define RA_WGL_D3D12_RESOURCE 7
#define RA_WGL_FRAMEBUFFER_COMPLETE 8
#define GL_NO_ERROR 0
typedef struct {
   void *bridge;
   GLuint memory[2], texture[2], framebuffer[2], ready, done;
   unsigned width, height, internal_format;
   void (*create_memory)(int, GLuint *);
   void (*gen_framebuffer)(int, GLuint *);
   void (*delete_memory)(int, const GLuint *);
   void (*delete_framebuffer)(int, const GLuint *);
   void (*delete_semaphore)(int, const GLuint *);
   void (*memory_parameter)(GLuint, unsigned, const GLint *);
   void (*import_memory)(GLuint, unsigned, unsigned, unsigned);
   void (*texture_storage)(unsigned, unsigned, unsigned, unsigned, unsigned, GLuint, unsigned);
   void (*bind_framebuffer)(unsigned, GLuint);
   void (*attach_texture)(unsigned, unsigned, unsigned, GLuint, GLint);
   unsigned (*check_framebuffer)(unsigned);
} ra_wgl_dxgi_t;
static unsigned next_object = 10, framebuffer_creates, framebuffer_deletes;
static unsigned textures_deleted, memories_deleted, finishes, bridge_destroyed;
static unsigned draw = 9, texture = 8, attachments[64], error, incomplete;
static void generate(int count, GLuint *objects)
{ while (count--) *objects++ = next_object++; }
static void generate_framebuffer(int count, GLuint *objects)
{ framebuffer_creates += count; generate(count, objects); }
static void delete_framebuffer(int count, const GLuint *objects)
{
   int i;
   assert(finishes);
   for (i = 0; i < count; i++)
      if (objects[i]) { attachments[objects[i]] = 0; ++framebuffer_deletes; }
}
static void delete_memory(int count, const GLuint *objects)
{ assert(objects[0] && objects[1]); memories_deleted += count; }
static void glGenTextures(int count, GLuint *objects) { generate(count, objects); }
static void glDeleteTextures(int count, const GLuint *objects)
{
   unsigned i;
   int j;
   assert(objects);
   for (i = 0; i < 64; i++)
      for (j = 0; j < count; j++)
         assert(!objects[j] || attachments[i] != objects[j]);
   textures_deleted += count;
}
static void glGetIntegerv(unsigned name, GLint *value)
{ *value = name == GL_TEXTURE_BINDING_2D ? texture : draw; }
static void glBindTexture(unsigned target, GLuint object) { assert(target == 1); texture = object; }
static void bind_framebuffer(unsigned target, GLuint object) { assert(target == 3); draw = object; }
static void attach_texture(unsigned target, unsigned slot, unsigned type, GLuint object, GLint level)
{ assert(target == 3 && slot == 5 && type == 1 && !level); attachments[draw] = object; }
static unsigned check_framebuffer(unsigned target)
{ assert(target == 3 && attachments[draw]); return incomplete ? 0 : 8; }
static void memory_parameter(GLuint object, unsigned name, const GLint *value)
{ assert(object && name == 6 && *value == GL_TRUE); }
static void import_memory(GLuint object, unsigned size, unsigned type, unsigned handle)
{ assert(object && !size && type == 7 && handle); }
static void texture_storage(unsigned target, unsigned levels, unsigned format,
      unsigned width, unsigned height, GLuint memory, unsigned offset)
{ assert(target == 1 && levels == 1 && format == 28 && width && height && memory && !offset); }
static unsigned ra_dxgi_bridge_resource(void *bridge, unsigned i) { assert(bridge); return i + 1; }
static unsigned glGetError(void) { return error; }
static void glFinish(void) { ++finishes; }
static void ra_dxgi_bridge_destroy(void *bridge)
{ assert(finishes); if (bridge) ++bridge_destroyed; }
#include "wgl_images.inc"
int main(void)
{
   ra_wgl_dxgi_t *p = (ra_wgl_dxgi_t *)calloc(1, sizeof(*p));
   ra_wgl_dxgi_t callbacks;
   GLuint first, second;
   assert(p); p->bridge = p;
   p->create_memory = generate; p->gen_framebuffer = generate_framebuffer;
   p->delete_memory = delete_memory; p->delete_framebuffer = delete_framebuffer;
   p->memory_parameter = memory_parameter; p->import_memory = import_memory;
   p->texture_storage = texture_storage; p->bind_framebuffer = bind_framebuffer;
   p->attach_texture = attach_texture; p->check_framebuffer = check_framebuffer;
   callbacks = *p;
   assert(ra_wgl_dxgi_create_images(p, 320, 240, 28));
   first = p->framebuffer[0]; second = p->framebuffer[1];
   assert(first && second && first != second && framebuffer_creates == 2);
   assert(draw == 9 && texture == 8);
   ra_wgl_dxgi_destroy_images(p);
   assert(p->framebuffer[0] == first && p->framebuffer[1] == second && !framebuffer_deletes);
   assert(draw == 9 && textures_deleted == 2 && memories_deleted == 2);
   assert(ra_wgl_dxgi_create_images(p, 640, 480, 28));
   assert(p->framebuffer[0] == first && p->framebuffer[1] == second && framebuffer_creates == 2);
   ra_wgl_dxgi_destroy_images(p);
   error = 1;
   assert(!ra_wgl_dxgi_create_images(p, 320, 240, 28));
   assert(framebuffer_creates == 2 && draw == 9 && texture == 8);
   ra_wgl_dxgi_destroy(p);
   assert(framebuffer_deletes == 2 && bridge_destroyed == 1);
   assert(textures_deleted == 6 && memories_deleted == 6);
   p = (ra_wgl_dxgi_t *)malloc(sizeof(*p)); assert(p);
   *p = callbacks; p->bridge = p; incomplete = 1; error = 0;
   assert(!ra_wgl_dxgi_create_images(p, 320, 240, 28));
   assert(p->framebuffer[0] && !p->framebuffer[1] && framebuffer_creates == 3);
   assert(draw == 9 && texture == 8);
   ra_wgl_dxgi_destroy(p);
   assert(framebuffer_deletes == 3 && bridge_destroyed == 2);
   p = (ra_wgl_dxgi_t *)calloc(1, sizeof(*p)); assert(p);
   ra_wgl_dxgi_destroy(p);
   puts("PASS: GL/DXGI framebuffer reuse, attachment release and teardown");
   return 0;
}
