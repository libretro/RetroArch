#!/usr/bin/env python3
"""The shipping compositor selects HDR sampling without an intermediate image."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "gfx/drivers/d3d12.c").read_text(encoding="utf-8")
start = source.index("static D3D12PipelineState d3d12_media_sprite_pipe(")
function = source[start:source.index("\n}\n", start) + 3]
fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#define DXGI_FORMAT_R16G16B16A16_FLOAT 10
typedef void *D3D12PipelineState;
typedef struct { struct { unsigned Format; } desc; } d3d12_texture_t;
typedef struct { d3d12_texture_t *final; unsigned mode; } d3d12_filter_chain_t;
typedef struct {
   d3d12_filter_chain_t *media_shader_chain;
   struct { void *pipe_pq_hdr,*pipe_pq_f16,*pipe_linear_hdr,*pipe_linear_f16; } sprites;
   unsigned target;
} d3d12_video_t;
static unsigned d3d12_viewer_output_mode(const d3d12_filter_chain_t *chain)
{ return chain->mode; }
static D3D12PipelineState d3d12_rt_pipe(d3d12_video_t *v,
      D3D12PipelineState sdr,D3D12PipelineState hdr,D3D12PipelineState fp16)
{ (void)sdr; return v->target ? fp16 : hdr; }
FUNCTION
int main(void)
{
   d3d12_video_t video;
   d3d12_filter_chain_t chain;
   d3d12_texture_t input,other;
   unsigned i;
   char pipes[4];
   memset(&video,0,sizeof(video));memset(&chain,0,sizeof(chain));
   memset(&input,0,sizeof(input));memset(&other,0,sizeof(other));
   video.sprites.pipe_pq_hdr=&pipes[0];video.sprites.pipe_pq_f16=&pipes[1];
   video.sprites.pipe_linear_hdr=&pipes[2];video.sprites.pipe_linear_f16=&pipes[3];
   video.media_shader_chain=&chain;chain.final=&input;
   for(i=0;i<2;i++)
   {
      video.target=i;
      input.desc.Format=10;
      chain.mode=0;
      assert(!d3d12_media_sprite_pipe(&video,&input));
      chain.mode=1;
      assert(d3d12_media_sprite_pipe(&video,&input)==&pipes[i]);
      chain.mode=2;
      assert(d3d12_media_sprite_pipe(&video,&input)==&pipes[2+i]);
      input.desc.Format=24;
      assert(d3d12_media_sprite_pipe(&video,&input)==&pipes[2+i]);
      chain.mode=0;
      assert(!d3d12_media_sprite_pipe(&video,&input));
      other.desc.Format=10;
      assert(d3d12_media_sprite_pipe(&video,&other)==&pipes[2+i]);
      other.desc.Format=28;
      assert(!d3d12_media_sprite_pipe(&video,&other));
   }
   assert(chain.final==&input);
   video.media_shader_chain=NULL;input.desc.Format=10;
   assert(d3d12_media_sprite_pipe(&video,&input)==&pipes[3]);
   puts("PASS viewer composite: SDR/linear/PQ sampling, native target pipelines, original output texture");
   return 0;
}
'''.replace("FUNCTION", function)
with tempfile.TemporaryDirectory(prefix="viewer-hdr-") as directory:
    unit = Path(directory) / "test.c"
    exe = Path(directory) / ("test.exe" if os.name == "nt" or "mingw" in os.environ.get("CC", "") else "test")
    unit.write_bytes(fixture.encode("utf-8"))
    subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
        *shlex.split(os.environ.get("CFLAGS", "")), "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror",
        str(unit), "-o", str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get("RUNNER", "")) + [str(exe)], check=True)
