#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>

#define FAILED(hr) ((hr) < 0)
#define SUCCEEDED(hr) (!FAILED(hr))
#define VIDEO_SCALE_PACK(w,h) (((w) << 16) | (h))
#define VIDEO_SCALE_W(d) ((d) >> 16)
#define VIDEO_SCALE_H(d) ((d) & 65535)
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define D3DLOCK_NOSYSLOCK 1
#define D3DLOCK_READONLY 2
#define D3DBACKBUFFER_TYPE_MONO 0
#define D3D9_XRGB8888_FORMAT 0
#define D3D9_ARGB8888_FORMAT 0
#define D3DPOOL_SYSTEMMEM 0
#define D3DUSAGE_AUTOGENMIPMAP 1
#define TEXTURE_FILTER_MIPMAP_LINEAR 1
#define TEXTURE_FILTER_MIPMAP_NEAREST 2
typedef int D3DFORMAT;
typedef struct object { unsigned refs; } *LPDIRECT3DTEXTURE9;
typedef LPDIRECT3DTEXTURE9 LPDIRECT3DSURFACE9;
typedef void *LPDIRECT3DDEVICE9;
typedef struct { int Pitch; void *pBits; } D3DLOCKED_RECT;
struct Vertex { float x,y,z,u,v; uint32_t color; };
struct shader_pass
{
   unsigned last_dims;
   struct { unsigned tex_dims; } info;
   void *vertex_buf;
};
typedef struct
{
   LPDIRECT3DDEVICE9 dev;
   struct menu { LPDIRECT3DTEXTURE9 tex; float alpha_mod; } *menu;
   struct { unsigned full_dims; } vp;
   struct { unsigned X,Y,Width,Height; } out_vp;
} d3d9_video_t;
struct texture_image { unsigned width,height; const uint32_t *pixels; };
struct d3d9_texture_info { unsigned type; void *userdata; void *data; };
static unsigned locks, unlocks, draws, fail_lock;
static uint32_t memory[16];
static struct Vertex vertices[4];
static struct object tex, back_surface, read_surface;
static int vertex_lock(void *vb, unsigned off, unsigned size, void **p, unsigned flags)
{
   (void)vb; (void)off; (void)size; (void)flags;
   locks++; *p=vertices; return fail_lock ? -1 : 0;
}
#define IDirect3DVertexBuffer9_Lock vertex_lock
#define IDirect3DVertexBuffer9_Unlock(vb) (++unlocks)
static int d3d9_tex_lock(LPDIRECT3DTEXTURE9 t, unsigned level, D3DLOCKED_RECT *r, const void *rect, unsigned flags)
{
   (void)t; (void)level; (void)rect; (void)flags;
   locks++; r->pBits=memory; r->Pitch=16; return fail_lock ? -1 : 0;
}
static void d3d9_tex_unlock(LPDIRECT3DTEXTURE9 t, unsigned level, bool wrote)
{ (void)t; (void)level; assert(wrote); unlocks++; }
static int d3d9_tex_create(LPDIRECT3DDEVICE9 dev, unsigned w, unsigned h,
      unsigned levels, unsigned usage, unsigned format, LPDIRECT3DTEXTURE9 *out)
{ (void)dev; (void)w; (void)h; (void)levels; (void)usage; (void)format; tex.refs++; *out=&tex; return 0; }
static void release(LPDIRECT3DTEXTURE9 t) { assert(t->refs); t->refs--; }
#define IDirect3DTexture9_Release release
#define IDirect3DSurface9_Release release
static int acquire(LPDIRECT3DSURFACE9 *out, LPDIRECT3DSURFACE9 t) { t->refs++; *out=t; return 0; }
#define IDirect3DDevice9_GetBackBuffer(d,a,b,c,out) acquire(out,&back_surface)
#define IDirect3DDevice9_CreateOffscreenPlainSurface(d,w,h,f,p,out,s) acquire(out,&read_surface)
static bool d3d9_hlsl_device_get_render_target_data(LPDIRECT3DDEVICE9 d, LPDIRECT3DSURFACE9 a, LPDIRECT3DSURFACE9 b)
{ (void)d; (void)a; (void)b; return true; }
#define IDirect3DSurface9_LockRect(t,r,rect,flags) d3d9_tex_lock(t,0,r,rect,flags)
#define IDirect3DSurface9_UnlockRect(t) (++unlocks)
static void render(struct shader_pass *pass, unsigned width, unsigned height)
{
#include "vertex.inc"
   draws++;
}
static void upload_menu(d3d9_video_t *d3d, unsigned dims, bool rgb32, float alpha, const void *frame)
{
   D3DLOCKED_RECT d3dlr;
#include "menu.inc"
}
#include "texture.inc"
#include "readback.inc"
int main(void)
{
   struct shader_pass pass;
   d3d9_video_t drv;
   struct menu menu;
   uint32_t pixels[4]={0xff102030,0xff405060,0xff708090,0xffa0b0c0};
   uint16_t pixels16[4]={0xf123,0xf456,0xf789,0xfabc};
   struct texture_image image={2,2,pixels};
   struct d3d9_texture_info info;
   uintptr_t id=0;
   uint8_t out[12], saved[sizeof(memory)];
   unsigned mode;
   memset(&pass,0,sizeof(pass)); pass.info.tex_dims=VIDEO_SCALE_PACK(4,4);
   memset(&drv,0,sizeof(drv)); drv.dev=&drv; drv.menu=&menu;
   menu.tex=&tex; tex.refs=1;
   drv.vp.full_dims=VIDEO_SCALE_PACK(2,2); drv.out_vp.Width=drv.out_vp.Height=2;
   info.type=0; info.userdata=&drv; info.data=&image;
   fail_lock=1;
   render(&pass,2,2);
   assert(locks==1 && unlocks==0 && draws==0 && pass.last_dims==0);
   fail_lock=0; render(&pass,2,2);
   assert(locks==2 && unlocks==1 && draws==1 && pass.last_dims==VIDEO_SCALE_PACK(2,2));
   render(&pass,2,2); assert(locks==2 && draws==2);
   for(mode=0;mode<2;mode++)
   {
      memset(memory,0xa5,sizeof(memory)); memcpy(saved,memory,sizeof(saved));
      locks=unlocks=0; fail_lock=1;
      upload_menu(&drv,VIDEO_SCALE_PACK(2,2),mode!=0,1.0f,mode ? (void*)pixels : (void*)pixels16);
      assert(locks==1 && unlocks==0 && memcmp(memory,saved,sizeof(saved))==0);
      fail_lock=0;
      upload_menu(&drv,VIDEO_SCALE_PACK(2,2),mode!=0,1.0f,mode ? (void*)pixels : (void*)pixels16);
      assert(locks==2 && unlocks==1);
      assert(memcmp(memory,mode ? (void*)pixels : (void*)pixels16,mode ? 8 : 4)==0);
   }
   locks=unlocks=0; fail_lock=1;
   d3d9_hlsl_video_texture_load_d3d(&info,&id);
   assert(locks==1 && unlocks==0 && id==0 && tex.refs==1);
   fail_lock=0; d3d9_hlsl_video_texture_load_d3d(&info,&id);
   assert(locks==2 && unlocks==1 && id==(uintptr_t)&tex && tex.refs==2);
   release((LPDIRECT3DTEXTURE9)id);
   memset(out,0xa5,sizeof(out)); locks=unlocks=0; fail_lock=1;
   assert(!d3d9_hlsl_read_viewport(&drv,out,false));
   for(mode=0;mode<sizeof(out);mode++) assert(out[mode]==0xa5);
   assert(locks==1 && unlocks==0 && back_surface.refs==0 && read_surface.refs==0);
   fail_lock=0; assert(d3d9_hlsl_read_viewport(&drv,out,false));
   assert(locks==2 && unlocks==1 && back_surface.refs==0 && read_surface.refs==0);
   assert(out[0]==0x90 && out[1]==0x80 && out[2]==0x70);
   release(menu.tex); assert(tex.refs==0);
   puts("D3D9 failed locks: vertex retry, 16/32-bit menu, texture cleanup and readback PASS");
   return 0;
}
