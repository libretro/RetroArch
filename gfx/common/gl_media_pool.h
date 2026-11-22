/* Stable frontend texture handles with bounded, history-aware GL storage.
 * All calls run on the context owner. Loans remain keyed by the public name. */
#ifndef GL_MEDIA_POOL_H
#define GL_MEDIA_POOL_H

/* Constants may be absent from legacy SDK headers. They are used only
 * when the context reports the corresponding capability. */
#ifndef GL_TEXTURE_SWIZZLE_R
#define GL_TEXTURE_SWIZZLE_R 0x8E42
#endif
#ifndef GL_TEXTURE_SWIZZLE_B
#define GL_TEXTURE_SWIZZLE_B 0x8E44
#endif
#ifndef GL_TEXTURE_SWIZZLE_G
#define GL_TEXTURE_SWIZZLE_G 0x8E43
#endif
#ifndef GL_TEXTURE_SWIZZLE_A
#define GL_TEXTURE_SWIZZLE_A 0x8E45
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER_BINDING
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#endif
#define GL_MEDIA_POOL_SWIZZLE 1u
#define GL_MEDIA_POOL_UNPACK 2u
/* The legacy driver uses mutable storage, where later presets may generate mips. */
#define GL_MEDIA_POOL_MUTABLE_MIPS 4u

typedef struct gl_media_pool
{
   struct gl_media_pool *next;
   GLuint handle,current,versions[GFX_MAX_FRAME_HISTORY + 2];
   unsigned count,width,height,generation,sampler_generation[GFX_MAX_FRAME_HISTORY + 2];
   GLint format,max_level,filter[4],swizzle[4];
#ifdef HAVE_OPENGLES
   GLenum external,type;
#endif
   bool eligible;
} gl_media_pool_t;

typedef bool (*gl_media_pool_reference_t)(void*, GLuint);
typedef bool (*gl_media_pool_retire_t)(void*, GLuint);

static gl_media_pool_t *gl_media_pool_find(gl_media_pool_t *list, GLuint handle)
{
   while (list && list->handle != handle) list = list->next;
   return list;
}

static GLuint gl_media_pool_resolve(gl_media_pool_t *list, GLuint handle)
{
   gl_media_pool_t *pool = gl_media_pool_find(list, handle);
   return pool ? pool->current : handle;
}

#if defined(HAVE_OPENGLES) && !defined(HAVE_PSGL)
/* GLES has no portable texture-level query. The creator supplies its storage. */
static void gl_media_pool_register(gl_media_pool_t **list, GLuint handle,
      unsigned width, unsigned height, GLint format, GLenum external,
      GLenum type, bool single_level, bool mutable_mips, unsigned capabilities)
{
   gl_media_pool_t *pool;
   GLint bound;
   if (!handle || !width || !height || gl_media_pool_find(*list, handle)) return;
   pool = (gl_media_pool_t*)calloc(1, sizeof(*pool));
   if (!pool) return;
   pool->handle = pool->current = pool->versions[0] = handle;
   pool->count = pool->generation = pool->sampler_generation[0] = 1;
   pool->width = width; pool->height = height; pool->format = format;
   pool->external = external; pool->type = type;
   pool->max_level = mutable_mips ? 1000 : 0;
   pool->eligible = single_level;
   glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
   glBindTexture(GL_TEXTURE_2D, handle);
   glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &pool->filter[0]);
   glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &pool->filter[1]);
   glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &pool->filter[2]);
   glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &pool->filter[3]);
   if (capabilities & GL_MEDIA_POOL_SWIZZLE)
   {
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, &pool->swizzle[0]);
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, &pool->swizzle[1]);
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, &pool->swizzle[2]);
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, &pool->swizzle[3]);
   }
   glBindTexture(GL_TEXTURE_2D, (GLuint)bound);
   pool->next = *list; *list = pool;
}
#endif

/* Merely reserve an unused destination. A refused/dropped upload does not
 * publish it, and no existing image is copied to initialize it. */
static GLuint gl_media_pool_prepare_ex(gl_media_pool_t **list, GLuint handle,
      unsigned width, unsigned height, gl_media_pool_reference_t referenced,
      void *context, unsigned capabilities)
{
#if !defined(HAVE_PSGL)
   gl_media_pool_t *pool = gl_media_pool_find(*list, handle);
   GLuint current = pool ? pool->current : handle;
   GLint old_texture,old_unpack;
   unsigned i;
   if (!referenced(context,current)) return current;
   if (!pool)
   {
#ifdef HAVE_OPENGLES
      return current;
#else
      GLint w,h,mip,format;
      glGetIntegerv(GL_TEXTURE_BINDING_2D,&old_texture);
      glBindTexture(GL_TEXTURE_2D,handle);
      glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);
      glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);
      glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);
      glGetTexLevelParameteriv(GL_TEXTURE_2D,1,GL_TEXTURE_WIDTH,&mip);
      glBindTexture(GL_TEXTURE_2D,(GLuint)old_texture);
      if (w<=0 || h<=0) return current;
      pool=(gl_media_pool_t*)calloc(1,sizeof(*pool));
      if (!pool) return 0;
      pool->handle=pool->current=handle;pool->versions[0]=handle;pool->count=1;
      pool->generation=pool->sampler_generation[0]=1;
      pool->width=(unsigned)w;pool->height=(unsigned)h;pool->format=format;
      glBindTexture(GL_TEXTURE_2D,handle);
      glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&pool->filter[0]);
      glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&pool->filter[1]);
      glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,&pool->filter[2]);
      glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,&pool->filter[3]);
      if (capabilities & GL_MEDIA_POOL_MUTABLE_MIPS)
         glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,&pool->max_level);
      if (capabilities & GL_MEDIA_POOL_SWIZZLE)
      {
         glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_R,&pool->swizzle[0]);
         glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_G,&pool->swizzle[1]);
         glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_B,&pool->swizzle[2]);
         glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_A,&pool->swizzle[3]);
      }
      glBindTexture(GL_TEXTURE_2D,(GLuint)old_texture);
      pool->eligible=!mip && (format==GL_RGBA || format==GL_RGBA8 || format==GL_RGBA16F);
      pool->next=*list;*list=pool;
#endif
   }
   if (!pool->eligible || width!=pool->width || height!=pool->height)
      return current; /* Preserve the established partial/mipmap/format fallback. */
   for (i=0;i<pool->count;i++)
      if (!referenced(context,pool->versions[i]))
      {
         if (pool->sampler_generation[i]!=pool->generation)
         {
            glGetIntegerv(GL_TEXTURE_BINDING_2D,&old_texture);
            glBindTexture(GL_TEXTURE_2D,pool->versions[i]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,pool->filter[0]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,pool->filter[1]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,pool->filter[2]);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,pool->filter[3]);
            glBindTexture(GL_TEXTURE_2D,(GLuint)old_texture);
            pool->sampler_generation[i]=pool->generation;
         }
         return pool->versions[i];
      }
   if (pool->count==GFX_MAX_FRAME_HISTORY+2) return 0;
   glGetIntegerv(GL_TEXTURE_BINDING_2D,&old_texture);
   if (capabilities & GL_MEDIA_POOL_UNPACK)
   {
      glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&old_unpack);
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
   }
   {
      GLuint image;
      GLint w=0,h=0;
      glGenTextures(1,&image);
      if (!image)
      {
         if (capabilities & GL_MEDIA_POOL_UNPACK)
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER,(GLuint)old_unpack);
         return 0;
      }
      glBindTexture(GL_TEXTURE_2D,image);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,pool->filter[0]);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,pool->filter[1]);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,pool->filter[2]);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,pool->filter[3]);
      if (capabilities & GL_MEDIA_POOL_SWIZZLE)
      {
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_R,pool->swizzle[0]);
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_G,pool->swizzle[1]);
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_B,pool->swizzle[2]);
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_SWIZZLE_A,pool->swizzle[3]);
      }
#if !defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3)
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,pool->max_level);
#endif
#ifdef HAVE_OPENGLES
      {
         GLuint fbo = 0;
         GLint previous;
#ifdef HAVE_OPENGLES3
         GLenum target = GL_DRAW_FRAMEBUFFER;
         glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous);
#else
         GLenum target = GL_FRAMEBUFFER;
         glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
#endif
         glTexImage2D(GL_TEXTURE_2D,0,pool->format,width,height,0,
               pool->external,pool->type,NULL);
         glGenFramebuffers(1, &fbo);
         if (fbo)
         {
            glBindFramebuffer(target, fbo);
            glFramebufferTexture2D(target, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
            if (glCheckFramebufferStatus(target) == GL_FRAMEBUFFER_COMPLETE)
            { w = (GLint)width; h = (GLint)height; }
            glBindFramebuffer(target, (GLuint)previous);
         }
         glDeleteFramebuffers(1, &fbo);
      }
#else
      glTexImage2D(GL_TEXTURE_2D,0,pool->format,width,height,0,GL_RGBA,
            pool->format==GL_RGBA16F ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE,NULL);
      glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);
      glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);
#endif
      if ((unsigned)w!=width || (unsigned)h!=height)
      {
         glDeleteTextures(1,&image);image=0;
      }
      else
      {
         pool->sampler_generation[pool->count]=pool->generation;
         pool->versions[pool->count++]=image;
      }
      if (capabilities & GL_MEDIA_POOL_UNPACK)
         glBindBuffer(GL_PIXEL_UNPACK_BUFFER,(GLuint)old_unpack);
      glBindTexture(GL_TEXTURE_2D,(GLuint)old_texture);
      return image;
   }
#else
   (void)list;(void)width;(void)height;(void)referenced;(void)context;(void)capabilities;
   return handle;
#endif
}

static INLINE GLuint gl_media_pool_prepare(gl_media_pool_t **list, GLuint handle,
      unsigned width, unsigned height, gl_media_pool_reference_t referenced,
      void *context)
{
   return gl_media_pool_prepare_ex(list,handle,width,height,referenced,context,
         GL_MEDIA_POOL_SWIZZLE | GL_MEDIA_POOL_UNPACK);
}

/* Slang changes producer sampler state while drawing. Cache its known
 * state without a GL query; old versions catch up once when reused. */
static INLINE void gl_media_pool_sync_sampler(gl_media_pool_t *pool, const GLint filter[4])
{
   unsigned i;
   if (!pool || !memcmp(pool->filter,filter,sizeof(pool->filter))) return;
   memcpy(pool->filter,filter,sizeof(pool->filter));
   pool->generation++;
   if (!pool->generation)
   {
      pool->generation=1;
      memset(pool->sampler_generation,0,sizeof(pool->sampler_generation));
   }
   for (i=0;i<pool->count;i++)
      if (pool->versions[i]==pool->current) pool->sampler_generation[i]=pool->generation;
}

static void gl_media_pool_publish(gl_media_pool_t *list, GLuint handle, GLuint image)
{
   gl_media_pool_t *pool=gl_media_pool_find(list,handle);
   if (pool) pool->current=image;
}

static bool gl_media_pool_forget(gl_media_pool_t **list, GLuint handle,
      gl_media_pool_retire_t retire, void *context)
{
   gl_media_pool_t **at=list,*pool;
   unsigned i;
   while (*at && (*at)->handle!=handle) at=&(*at)->next;
   if (!(pool=*at)) return false;
   *at=pool->next;
   for (i=0;i<pool->count;i++)
      if (!retire || !retire(context,pool->versions[i]))
         glDeleteTextures(1,&pool->versions[i]);
   free(pool);
   return true;
}

static void gl_media_pool_free(gl_media_pool_t **list)
{
   while (*list) gl_media_pool_forget(list,(*list)->handle,NULL,NULL);
}
#endif
