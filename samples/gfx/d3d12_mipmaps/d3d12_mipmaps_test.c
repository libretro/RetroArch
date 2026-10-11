/* The shipping mip generator against a subresource-state model. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <float.h>

#define D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE 1
#define D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE 2
#define D3D12_RESOURCE_STATE_UNORDERED_ACCESS 4
#define D3D12_RESOURCE_BARRIER_TYPE_TRANSITION 0
#define D3D12_RESOURCE_BARRIER_TYPE_UAV 1
#define ALL_MIPS UINT_MAX
#define CS_ROOT_ID_TEXTURE_T 0
#define CS_ROOT_ID_UAV_T 1
#define CS_ROOT_ID_CONSTANTS 2
#define MAX_MIPS 15
#define D3D12_FILTER_MIN_MAG_MIP_POINT 0
#define D3D12_TEXTURE_ADDRESS_MODE_CLAMP 3
#define D3D12_FLOAT32_MAX FLT_MAX

typedef struct
{
   unsigned Filter, AddressU, AddressV, AddressW;
   float MinLOD, MaxLOD;
} D3D12_STATIC_SAMPLER_DESC;

typedef unsigned UINT;
typedef unsigned D3D12PipelineState;
struct resource { unsigned levels, state[MAX_MIPS], value[MAX_MIPS]; };
typedef struct
{
   unsigned Type;
   struct { struct resource *pResource; unsigned StateBefore, StateAfter, Subresource; } Transition;
   struct { struct resource *pResource; } UAV;
} D3D12_RESOURCE_BARRIER;
typedef struct
{
   struct resource *handle;
   struct { unsigned Width, Height, MipLevels; } desc;
   unsigned gpu_descriptor[MAX_MIPS];
} d3d12_texture_t;
typedef struct { struct { unsigned cs_rootSignature; } desc; unsigned mipmapgen_pipe; } d3d12_video_t;
struct command;
typedef struct command *D3D12GraphicsCommandList;
struct vtable
{
   void (*SetComputeRootSignature)(struct command *, unsigned);
   void (*SetPipelineState)(struct command *, unsigned);
   void (*SetComputeRootDescriptorTable)(struct command *, unsigned, unsigned);
   void (*SetComputeRoot32BitConstants)(struct command *, unsigned, unsigned, const void *, unsigned);
   void (*Dispatch)(struct command *, unsigned, unsigned, unsigned);
   void (*ResourceBarrier)(struct command *, unsigned, const D3D12_RESOURCE_BARRIER *);
};
struct command
{
   struct vtable *lpVtbl;
   d3d12_texture_t *texture;
   unsigned src, dst, dispatches, barriers, root, pipeline;
   float tx, ty;
   D3D12_STATIC_SAMPLER_DESC sampler;
};
static unsigned failures;
#define CHECK(x) do { if (!(x)) { failures++; fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); } } while(0)
static void barrier(struct command *cmd, unsigned n, const D3D12_RESOURCE_BARRIER *b)
{
   unsigned k, first, last;
   CHECK(n == 1);
   cmd->barriers++;
   if (b->Type == D3D12_RESOURCE_BARRIER_TYPE_UAV)
   {
      CHECK(b->UAV.pResource == cmd->texture->handle);
      return;
   }
   first = b->Transition.Subresource == ALL_MIPS ? 0 : b->Transition.Subresource;
   last = b->Transition.Subresource == ALL_MIPS ? cmd->texture->desc.MipLevels : first + 1;
   CHECK(last <= MAX_MIPS);
   for (k = first; k < last; k++)
   {
      CHECK(b->Transition.pResource->state[k] == b->Transition.StateBefore);
      b->Transition.pResource->state[k] = b->Transition.StateAfter;
   }
}
#define D3D12_RESOURCE_TRANSITION(cmd,resource,before,after) do { \
   D3D12_RESOURCE_BARRIER b; memset(&b,0,sizeof(b)); \
   b.Transition.pResource=resource; b.Transition.StateBefore=before; \
   b.Transition.StateAfter=after; b.Transition.Subresource=ALL_MIPS; \
   (cmd)->lpVtbl->ResourceBarrier(cmd,1,&b); } while(0)
static void root(struct command *cmd, unsigned v) { cmd->root=v; }
static void pipeline(struct command *cmd, unsigned v) { cmd->pipeline=v; }
static void table(struct command *cmd, unsigned slot, unsigned v)
{
   if (slot == CS_ROOT_ID_UAV_T) cmd->dst=v;
   else CHECK(slot == CS_ROOT_ID_TEXTURE_T && v == 0);
}
static void constants(struct command *cmd, unsigned slot, unsigned n, const void *data, unsigned offset)
{
   struct params { uint32_t level; float texel[2]; } p;
   CHECK(slot == CS_ROOT_ID_CONSTANTS && n == 3 && offset == 0);
   memcpy(&p,data,sizeof(p)); cmd->src=p.level; cmd->tx=p.texel[0]; cmd->ty=p.texel[1];
}
static void dispatch(struct command *cmd, unsigned x, unsigned y, unsigned z)
{
   unsigned w=cmd->texture->desc.Width >> cmd->dst;
   unsigned h=cmd->texture->desc.Height >> cmd->dst;
   if (!w) w=1;
   if (!h) h=1;
   CHECK(cmd->root == 7 && cmd->pipeline == 8);
   CHECK(cmd->dst == cmd->src + 1);
   CHECK(cmd->src >= cmd->sampler.MinLOD && cmd->src <= cmd->sampler.MaxLOD);
   CHECK(cmd->texture->handle->state[cmd->src] == D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   CHECK(cmd->texture->handle->state[cmd->dst] == D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   CHECK(x == (w+7)/8 && y == (h+7)/8 && z == 1);
   CHECK(cmd->tx == 1.0f/w && cmd->ty == 1.0f/h);
   cmd->texture->handle->value[cmd->dst]=cmd->texture->handle->value[cmd->src];
   cmd->dispatches++;
}
#include "driver_code.inc"
int main(void)
{
   static const unsigned sizes[][2]={{1,1},{2,1},{1,256},{17,33},{256,256},{16384,1},{1,16384}};
   struct vtable vt={root,pipeline,table,constants,dispatch,barrier};
   unsigned c, req, frame, k;
   for(c=0;c<sizeof(sizes)/sizeof(sizes[0]);c++) for(req=0;req<3;req++)
   {
      struct resource r;
      d3d12_texture_t tex;
      d3d12_video_t video;
      struct command cmd;
      unsigned span=sizes[c][0]>sizes[c][1]?sizes[c][0]:sizes[c][1];
      unsigned levels=1;
      while(span>1) { levels++; span/=2; }
      memset(&r,0,sizeof(r)); memset(&tex,0,sizeof(tex)); memset(&cmd,0,sizeof(cmd));
      video.desc.cs_rootSignature=7; video.mipmapgen_pipe=8;
      tex.handle=&r; tex.desc.Width=sizes[c][0]; tex.desc.Height=sizes[c][1];
      tex.desc.MipLevels=d3d12_mip_levels(tex.desc.Width,tex.desc.Height,req==2?65535:req);
      CHECK(tex.desc.MipLevels == (req==2?levels:1));
      CHECK(d3d12_mip_levels(tex.desc.Width,tex.desc.Height,2) == (levels>1?2:1));
      cmd.lpVtbl=&vt; cmd.texture=&tex; r.levels=tex.desc.MipLevels;
      d3d12_init_compute_sampler(&cmd.sampler);
      CHECK(cmd.sampler.Filter == D3D12_FILTER_MIN_MAG_MIP_POINT);
      CHECK(cmd.sampler.AddressU == D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
      CHECK(cmd.sampler.AddressV == cmd.sampler.AddressU && cmd.sampler.AddressW == cmd.sampler.AddressU);
      for(k=0;k<r.levels;k++) { r.state[k]=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; tex.gpu_descriptor[k]=k; }
      for(frame=1;frame<=3;frame++)
      {
         unsigned before=cmd.dispatches;
         r.value[0]=frame;
         d3d12_generate_mipmaps(&cmd,&tex,&video);
         CHECK(cmd.dispatches-before == r.levels-1);
         for(k=0;k<r.levels;k++)
         {
            CHECK(r.state[k] == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            CHECK(r.value[k] == frame);
         }
      }
   }
   if(!failures) puts("ok: D3D12 mip counts, compute states, dispatch sizes and repeated generation");
   return failures?1:0;
}
