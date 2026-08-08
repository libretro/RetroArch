/* Differential test: rmpeg1_video vs pl_mpeg, I-pictures only.
 * Compares geometry and per-plane pixel hashes frame by frame, and reports
 * worst-case per-pixel deviation (IEEE 1180 allows IDCT implementations to
 * differ slightly, so an exact hash match is not required -- but it is what
 * we want to see, and any large deviation is a real bug). */
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rmpeg1_ps.h>
#include <formats/rmpeg1_video.h>

#define PLM_NO_STDIO 1
#define PL_MPEG_IMPLEMENTATION
#include <stddef.h>
#include "pl_mpeg.h"

static unsigned long fnv(const unsigned char *p, size_t n)
{ unsigned long h=2166136261UL; size_t i; for(i=0;i<n;i++){h^=p[i];h*=16777619UL;} return h; }

typedef struct { unsigned char type; unsigned w,h; unsigned long hy,hcb,hcr; unsigned char *y,*cb,*cr; unsigned ys,cs; } shot_t;

static void grab_t(shot_t *s, unsigned char ty){ s->type=ty; }
static void grab(shot_t *s, const unsigned char *y,const unsigned char *cb,const unsigned char *cr,
                 unsigned w,unsigned h,unsigned ys,unsigned cs)
{
   unsigned r; size_t yn=(size_t)w*h, cn=(size_t)(w/2)*(h/2);
   s->w=w; s->h=h; s->ys=w; s->cs=w/2;
   s->y=malloc(yn); s->cb=malloc(cn); s->cr=malloc(cn);
   for(r=0;r<h;r++) memcpy(s->y+(size_t)r*w, y+(size_t)r*ys, w);
   for(r=0;r<h/2;r++){ memcpy(s->cb+(size_t)r*(w/2), cb+(size_t)r*cs, w/2);
                       memcpy(s->cr+(size_t)r*(w/2), cr+(size_t)r*cs, w/2); }
   s->hy=fnv(s->y,yn); s->hcb=fnv(s->cb,cn); s->hcr=fnv(s->cr,cn);
}

int main(int argc, char **argv)
{
   FILE *f; long len; unsigned char *data;
   rmpeg1_ps_t *ps; rmpeg1_ps_packet_t pkt;
   rmpeg1_video_t *vid; rmpeg1_video_frame_t fr;
   plm_t *plm;
   shot_t mine[4096], ref[4096]; size_t nm=0, nr=0, i, off, chunk;
   int fails=0;

   if (argc<3){fprintf(stderr,"usage: %s file.mpg chunk\n",argv[0]);return 2;}
   chunk=(size_t)atoi(argv[2]);
   f=fopen(argv[1],"rb"); if(!f){perror("open");return 2;}
   fseek(f,0,SEEK_END); len=ftell(f); fseek(f,0,SEEK_SET);
   data=malloc((size_t)len);
   if(fread(data,1,(size_t)len,f)!=(size_t)len) return 2; fclose(f);

   /* ---- ours: demux then decode ---- */
   ps=rmpeg1_ps_init(0); vid=rmpeg1_video_init();
   off=0;
   while(off<(size_t)len){
      size_t want=(size_t)len-off; if(want>chunk) want=chunk;
      size_t got=rmpeg1_ps_write(ps,data+off,want); off+=got;
      while(rmpeg1_ps_next(ps,&pkt)){
         if(pkt.type!=RMPEG1_PS_VIDEO) continue;
         size_t p=0;
         while(p<pkt.size){
            size_t w=rmpeg1_video_write(vid,pkt.data+p,pkt.size-p);
            p+=w;
            while(rmpeg1_video_decode(vid,&fr)){
               if(nm<4096)
                  { grab(&mine[nm],fr.y,fr.cb,fr.cr,fr.width,fr.height,fr.y_stride,fr.c_stride); grab_t(&mine[nm],fr.coding_type); nm++; }
            }
            if(w==0) break;
         }
      }
      if(got==0) break;
   }
   rmpeg1_video_flush(vid);
   while(rmpeg1_video_decode(vid,&fr))
      if(nm<4096)
         { grab(&mine[nm],fr.y,fr.cb,fr.cr,fr.width,fr.height,fr.y_stride,fr.c_stride); grab_t(&mine[nm],fr.coding_type); nm++; }

   /* ---- reference ---- */
   plm=plm_create_with_memory(data,(size_t)len,0);
   plm_set_audio_enabled(plm,0);
   {
      plm_frame_t *pf;
      while((pf=plm_decode_video(plm))!=NULL && nr<4096)
         grab(&ref[nr++],pf->y.data,pf->cb.data,pf->cr.data,pf->width,pf->height,
              pf->y.width,pf->cb.width);
   }

   printf("I-frames ours=%zu   all-frames ref=%zu   skipped(non-I)=%u errors=%u\n",
          nm,nr,rmpeg1_video_skipped(vid),rmpeg1_video_errors(vid));
   printf("geometry: %ux%u  fps=", rmpeg1_video_width(vid), rmpeg1_video_height(vid));
   { unsigned n,d; rmpeg1_video_framerate(vid,&n,&d); printf("%u/%u\n",n,d); }

   if(nm==0){ printf("RESULT: FAIL (no I-frames decoded)\n"); return 1; }

   /* Compare every frame, not just the first. A P picture is built on the
    * previous one, so an error in motion compensation or in the residual
    * does not stay put -- it accumulates down the GOP. Checking frame 0
    * alone would pass a decoder whose prediction is subtly wrong.
    *
    * With no B pictures in the stream, coded order and display order agree,
    * so index i lines up between the two decoders. */
   {
      long worst = 0; size_t worst_i = 0; double worst_mean = 0;
      size_t n_cmp = nm < nr ? nm : nr;
      for (i = 0; i < n_cmp; i++) {
         shot_t *a2 = &mine[i], *b2 = &ref[i];
         long maxd = 0; double sum = 0; size_t k, n;
         if (a2->w != b2->w || a2->h != b2->h) {
            printf("MISMATCH geometry at frame %zu\n", i); fails++; break; }
         n = (size_t)a2->w * a2->h;
         for (k = 0; k < n; k++) {
            long d = (long)a2->y[k] - (long)b2->y[k]; if (d < 0) d = -d;
            if (d > maxd) maxd = d; sum += d; }
         if (getenv("RMPEG1_LOCATE") && maxd > 16) {
            size_t kk; long bd=0; size_t bk=0;
            for (kk=0;kk<n;kk++){ long d=(long)a2->y[kk]-(long)b2->y[kk]; if(d<0)d=-d;
                                  if(d>bd){bd=d;bk=kk;} }
            printf("  frame %zu worst at x=%zu y=%zu (mb %zu,%zu) ours=%u ref=%u\n",
                   i, bk%a2->w, bk/a2->w, (bk%a2->w)/16, (bk/a2->w)/16,
                   a2->y[bk], b2->y[bk]);
            { size_t mbx=((bk%a2->w)/16)*16, mby=((bk/a2->w)/16)*16, r2,c2;
              printf("  16x16 diff block at (%zu,%zu):\n",mbx,mby);
              for(r2=0;r2<16;r2++){ printf("   ");
                for(c2=0;c2<16;c2++){ long d=(long)a2->y[(mby+r2)*a2->w+mbx+c2]
                                        -(long)b2->y[(mby+r2)*a2->w+mbx+c2];
                                      printf("%4ld",d); }
                printf("\n"); } }
            return 1;
         }
         if (getenv("RMPEG1_PERFRAME"))
            printf("  frame %3zu type=%c maxdiff=%3ld mean=%.4f\n",
                   i, a2->type==1?'I':(a2->type==2?'P':'B'), maxd, sum/(double)n);
         if (maxd > worst) { worst = maxd; worst_i = i; worst_mean = sum/(double)n; }
         if (maxd > 16) { printf("frame %zu: Y maxdiff=%ld mean=%.4f\n",
                                 i, maxd, sum/(double)n); fails++; }
      }
      printf("frames compared: %zu   worst Y maxdiff=%ld at frame %zu (mean %.4f)\n",
             n_cmp, worst, worst_i, worst_mean);
   }
   printf(fails?"RESULT: FAIL\n":"RESULT: PASS\n");
   return fails?1:0;
}
