#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#define VIDEO_SCALE_PACK(w,h) (((w)<<16)|(h))
#define VIDEO_SCALE_W(v) ((v)>>16)
#define VIDEO_SCALE_H(v) ((v)&65535u)
#define VIDEO_POS_X(v) ((int)((v)>>16))
#define VIDEO_POS_Y(v) ((int)((v)&65535u))
#define INLINE
#define D3D11_RECORD_RING 2
#define D3D11_ST_FLAG_READBACK_PENDING 1
#define D3D11_MAP_READ 1
#define D3D11_MAP_FLAG_DO_NOT_WAIT 2
#define DXGI_ERROR_WAS_STILL_DRAWING 9
#define FAILED(hr) ((hr)<0)
#define DXGI_FORMAT_R8G8B8A8_UNORM 1
#define DXGI_FORMAT_B8G8R8A8_UNORM 2
#define RARCH_ERR printf

typedef long HRESULT;
typedef struct { unsigned Width,Height,Format; } D3D11_TEXTURE2D_DESC;
typedef struct { void *pData;unsigned RowPitch; } D3D11_MAPPED_SUBRESOURCE;
typedef void *D3D11Resource;
struct texture;
struct texture_methods { void (*GetDesc)(struct texture*,D3D11_TEXTURE2D_DESC*); };
struct texture { struct texture_methods *lpVtbl; };
typedef struct texture *D3D11Texture2D;
struct context;
struct context_methods {
 HRESULT (*Map)(struct context*,D3D11Resource,unsigned,unsigned,unsigned,D3D11_MAPPED_SUBRESOURCE*);
 void (*Unmap)(struct context*,D3D11Resource,unsigned);
};
struct context { struct context_methods *lpVtbl; };
typedef struct {
 struct {unsigned dims,full_dims,pos;} vp;
 struct {bool enable;unsigned index;D3D11Texture2D staging[2];bool valid[2];} record;
 unsigned flags;
 struct context *context;
} d3d11_video_t;
static d3d11_video_t state;
static unsigned next_dims,maps,redraws,captures,map_flags;
static HRESULT map_result;
static uint8_t pixels[8*8*4];
static void desc(struct texture *t,D3D11_TEXTURE2D_DESC *d)
{ (void)t;d->Width=VIDEO_SCALE_W(state.vp.dims);d->Height=VIDEO_SCALE_H(state.vp.dims);d->Format=DXGI_FORMAT_B8G8R8A8_UNORM; }
static HRESULT map(struct context *c,D3D11Resource r,unsigned sub,unsigned kind,unsigned flags,D3D11_MAPPED_SUBRESOURCE *m)
{ (void)c;(void)r;(void)sub;(void)kind;maps++;map_flags=flags;m->pData=pixels;m->RowPitch=VIDEO_SCALE_W(state.vp.dims)*4;return map_result; }
static void unmap(struct context *c,D3D11Resource r,unsigned sub){(void)c;(void)r;(void)sub;}
static struct texture_methods texture_vtable={desc};
static struct texture texture={&texture_vtable};
static struct context_methods context_vtable={map,unmap};
static struct context context={&context_vtable};
static void d3d11_record_capture(d3d11_video_t *s)
{ captures++;s->record.enable=true;s->record.staging[1]=&texture;s->record.valid[1]=true;s->record.index=0; }
static void video_driver_cached_frame(void)
{ redraws++;state.vp.dims=state.vp.full_dims=next_dims;d3d11_record_capture(&state);state.flags&=~D3D11_ST_FLAG_READBACK_PENDING; }
#include "readback.inc"
static void reset(unsigned before,unsigned after)
{ memset(&state,0,sizeof(state));state.vp.dims=state.vp.full_dims=before;state.context=&context;next_dims=after;maps=redraws=captures=map_flags=0;map_result=0;memset(pixels,0x44,sizeof(pixels)); }
static void resized(unsigned before,unsigned after)
{
 uint8_t destination[8*8*3+16];unsigned i;bool copied;
 reset(before,after);memset(destination,0xa5,sizeof(destination));
 copied=d3d11_gfx_read_viewport(&state,destination,false);
 for(i=0;i<sizeof(destination);i++)assert(destination[i]==0xa5);
 assert(!copied && redraws==1 && maps==0 && !(state.flags&D3D11_ST_FLAG_READBACK_PENDING));
 state.record.enable=false;
 assert(d3d11_gfx_read_viewport(&state,destination,true));
}
int main(void)
{
 uint8_t destination[8*8*3+16];unsigned i;
 resized(VIDEO_SCALE_PACK(2,2),VIDEO_SCALE_PACK(4,4));
 resized(VIDEO_SCALE_PACK(4,4),VIDEO_SCALE_PACK(2,2));
 resized(VIDEO_SCALE_PACK(2,4),VIDEO_SCALE_PACK(4,2));
 reset(VIDEO_SCALE_PACK(2,2),VIDEO_SCALE_PACK(2,2));memset(destination,0xa5,sizeof(destination));
 assert(d3d11_gfx_read_viewport(&state,destination,false));
 for(i=0;i<12;i++)assert(destination[i]==0x44);
 for(;i<sizeof(destination);i++)assert(destination[i]==0xa5);
 assert(redraws==1 && maps==1 && map_flags==0);
 reset(VIDEO_SCALE_PACK(2,2),VIDEO_SCALE_PACK(2,2));
 state.record.enable=true;state.record.staging[0]=&texture;state.record.valid[0]=true;
 assert(d3d11_gfx_read_viewport(&state,destination,false));
 assert(redraws==0 && maps==1 && map_flags==D3D11_MAP_FLAG_DO_NOT_WAIT);
 state.record.valid[0]=true;map_result=DXGI_ERROR_WAS_STILL_DRAWING;
 assert(!d3d11_gfx_read_viewport(&state,destination,false));
 assert(!d3d11_gfx_read_viewport(NULL,destination,false));
 puts("D3D11 resized readback: passed");return 0;
}
