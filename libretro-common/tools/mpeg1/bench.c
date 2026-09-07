/* Throughput: rmpeg1 stack vs pl_mpeg on the same stream.
 * Measures decode only; the file is read once up front. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <formats/rmpeg1_ps.h>
#include <formats/rmpeg1_video.h>

#define PLM_NO_STDIO 1
#define PL_MPEG_IMPLEMENTATION
#include <stddef.h>
#include "pl_mpeg.h"

static double now(void)
{ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
  return t.tv_sec + t.tv_nsec*1e-9; }

static unsigned char *data; static long len;

static size_t bench_ours_demux(void)
{
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0); rmpeg1_ps_packet_t p;
   size_t off=0, n=0;
   while(off<(size_t)len){
      size_t want=(size_t)len-off; if(want>2324) want=2324;
      size_t got=rmpeg1_ps_write(ps,data+off,want); off+=got;
      while(rmpeg1_ps_next(ps,&p)) n+=p.size;
      if(!got) break;
   }
   rmpeg1_ps_free(ps); return n;
}

static size_t bench_ref_demux(void)
{
   plm_buffer_t *b = plm_buffer_create_with_memory(data,(size_t)len,0);
   plm_demux_t *d = plm_demux_create(b,0); plm_packet_t *p; size_t n=0;
   while((p=plm_demux_decode(d))!=NULL) n+=p->length;
   plm_demux_destroy(d); plm_buffer_destroy(b); return n;
}

static size_t bench_ours_full(void)
{
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0); rmpeg1_ps_packet_t p;
   rmpeg1_video_t *v = rmpeg1_video_init(); rmpeg1_video_frame_t fr;
   size_t off=0, frames=0;
   while(off<(size_t)len){
      size_t want=(size_t)len-off; if(want>2324) want=2324;
      size_t got=rmpeg1_ps_write(ps,data+off,want); off+=got;
      while(rmpeg1_ps_next(ps,&p)){
         size_t q=0;
         if(p.type!=RMPEG1_PS_VIDEO) continue;
         while(q<p.size){
            size_t w=rmpeg1_video_write(v,p.data+q,p.size-q); q+=w;
            while(rmpeg1_video_decode(v,&fr)) frames++;
            if(!w) break;
         }
      }
      if(!got) break;
   }
   rmpeg1_video_flush(v);
   while(rmpeg1_video_decode(v,&fr)) frames++;
   rmpeg1_video_free(v); rmpeg1_ps_free(ps); return frames;
}

static size_t bench_ref_full(void)
{
   plm_t *plm = plm_create_with_memory(data,(size_t)len,0);
   plm_frame_t *f; size_t frames=0;
   plm_set_audio_enabled(plm,0);
   while((f=plm_decode_video(plm))!=NULL) frames++;
   plm_destroy(plm); return frames;
}

static void run(const char *name, size_t (*fn)(void), int iters)
{
   double t0, t1; size_t r=0; int i;
   fn();                                  /* warm */
   t0=now(); for(i=0;i<iters;i++) r=fn(); t1=now();
   printf("  %-22s %7.1f ms/iter   (%zu)   %6.1f MB/s\n",
          name, (t1-t0)*1000.0/iters, r,
          ((double)len*iters)/((t1-t0)*1e6));
}

int main(int argc, char **argv)
{
   FILE *f = fopen(argv[1],"rb"); int iters = argc>2?atoi(argv[2]):5;
   fseek(f,0,SEEK_END); len=ftell(f); fseek(f,0,SEEK_SET);
   data=malloc(len); if(fread(data,1,len,f)!=(size_t)len) return 2; fclose(f);
   printf("%s (%ld bytes, %d iters)\n", argv[1], len, iters);
   printf(" demux only:\n");
   run("rmpeg1_ps", bench_ours_demux, iters);
   run("pl_mpeg demux", bench_ref_demux, iters);
   printf(" demux + video decode:\n");
   run("rmpeg1_ps+video", bench_ours_full, iters);
   run("pl_mpeg", bench_ref_full, iters);
   return 0;
}
