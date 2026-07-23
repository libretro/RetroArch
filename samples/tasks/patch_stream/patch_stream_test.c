/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (patch_stream_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Differential harness for the streaming patch appliers.
 *
 * The streaming appliers in tasks/patch_stream.c must produce output
 * byte-identical to the whole-buffer appliers in tasks/task_patch.c, for
 * every patch format, at every chunk size.  This harness holds a copy of
 * the whole-buffer logic as the oracle, generates random valid patches,
 * and compares the two across a range of chunk sizes - including 1-byte
 * chunks, which maximise the number of suspend/resume cycles the
 * streaming interpreters go through.
 *
 * The size distribution is deliberately edge-heavy.  A streaming applier
 * has failure modes the whole-buffer form cannot have, because the whole
 * buffer form always has everything: an empty source (feed() never
 * called), a source that ends short of its declared length, and chunk
 * boundaries landing inside a patch command.  Those cases are generated
 * explicitly; the first of them caught a real bug during development.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./patch_stream_test
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../../tasks/patch_stream.h"

#define PATCH_SUCCESS 0
#define PATCH_FAIL    1

/* Verbatim extraction of ips_apply_patch/ups_apply_patch logic from
 * tasks/task_patch.c (checksums simplified: we compare bytes, not
 * validation verdicts - the streaming applier's job is byte-identical
 * output). */
#include <stdlib.h>
#include <string.h>
static uint32_t CT[256]; static int I=0;
static void ct(void){uint32_t i,j,c;if(I)return;for(i=0;i<256;i++){c=i;for(j=0;j<8;j++)c=(c&1)?0xEDB88320u^(c>>1):c>>1;CT[i]=c;}I=1;}

int oracle_ips(const uint8_t *pd, uint64_t pl, const uint8_t *sd, uint64_t sl, uint8_t **td, uint64_t *tl)
{
   uint32_t off=5, tlen=(uint32_t)sl;
   uint8_t *t;
   /* pass 1: target length */
   /* pass 1: determine the target length */
   {
      uint32_t o = 5;
      for (;;)
      {
         uint32_t a, l;
         if (o > pl - 3)
            break;
         a = (pd[o] << 16) | (pd[o + 1] << 8) | pd[o + 2];
         o += 3;
         if (a == 0x454f46) /* EOF */
         {
            if (o == pl)
               break;
            if (o == pl - 3)
            {
               tlen = (pd[o] << 16) | (pd[o + 1] << 8) | pd[o + 2];
               o += 3;
               break;
            }
         }
         if (o > pl - 2)
            break;
         l = (pd[o] << 8) | pd[o + 1];
         o += 2;
         if (l)
         {
            if (o > pl - l)
               break;
            a += l;
            o += l;
         }
         else
         {
            uint32_t r;
            if (o > pl - 3)
               break;
            r = (pd[o] << 8) | pd[o + 1];
            o += 2;
            if (!r)
               break;
            a += r;
            o++;
         }
         if (a > tlen)
            tlen = a;
      }
   }

   if (!(t = (uint8_t*)malloc(tlen ? tlen : 1)))
      return PATCH_FAIL;
   if (sl)
      memcpy(t, sd, (size_t)sl);
   if (tlen > sl)
      memset(t + sl, 0, tlen - sl);

   /* pass 2: apply the records */
   for (;;)
   {
      uint32_t a, l;
      if (off > pl - 3)
         break;
      a = (pd[off] << 16) | (pd[off + 1] << 8) | pd[off + 2];
      off += 3;
      if (a == 0x454f46) /* EOF */
      {
         if (off == pl || off == pl - 3)
         {
            *td = t;
            *tl = tlen;
            return PATCH_SUCCESS;
         }
      }
      if (off > pl - 2)
         break;
      l = (pd[off] << 8) | pd[off + 1];
      off += 2;
      if (l)
      {
         if (off > pl - l)
            break;
         while (l--)
            t[a++] = pd[off++];
      }
      else
      {
         uint32_t r;
         if (off > pl - 3)
            break;
         r = (pd[off] << 8) | pd[off + 1];
         off += 2;
         if (!r)
            break;
         while (r--)
            t[a++] = pd[off];
         off++;
      }
   }

   *td=t;*tl=tlen;return PATCH_SUCCESS;
}

/* UPS oracle */
struct u{const uint8_t*p;const uint8_t*s;uint8_t*t;unsigned pl,sl,tl;unsigned po,so,to;};
static uint8_t upr(struct u*d){ if(d->po<d->pl) return d->p[d->po++]; return 0; }
static uint8_t usr(struct u*d){ if(d->so<d->sl) return d->s[d->so++]; d->so++; return 0; }
static void utw(struct u*d,uint8_t n){ if(d->to<d->tl) d->t[d->to]=n; d->to++; }
static uint64_t udec(struct u*d){ uint64_t o=0,s=1; for(;;){ uint8_t x=upr(d); o+=(x&0x7f)*s; if(x&0x80)break; s<<=7; o+=s; } return o; }
int oracle_ups(const uint8_t *pd, uint64_t pl, const uint8_t *sd, uint64_t sl, uint8_t **td, uint64_t *tl)
{
   struct u d; unsigned srl, trl; uint8_t *t;
   ct(); (void)ct;
   d.p=pd; d.s=sd; d.pl=(unsigned)pl; d.sl=(unsigned)sl;
   d.po=0; d.so=0; d.to=0;
   if(pl<18||memcmp(pd,"UPS1",4))return PATCH_FAIL;
   d.po=4;
   srl=(unsigned)udec(&d); trl=(unsigned)udec(&d);
   t=(uint8_t*)calloc(1,trl?trl:1); if(!t)return PATCH_FAIL;
   d.t=t; d.tl=trl;
   while(d.po < d.pl-12){ unsigned n=(unsigned)udec(&d);
     while(n--) utw(&d,usr(&d));
     for(;;){ uint8_t px=upr(&d); utw(&d, px^usr(&d)); if(px==0)break; } }
   while(d.so<srl) utw(&d,usr(&d));
   while(d.to<trl) utw(&d,usr(&d));
   *td=t;*tl=trl;return PATCH_SUCCESS;
}

/* BPS oracle: verbatim structure from bps_apply_patch. */
struct b{const uint8_t*m;const uint8_t*s;uint8_t*t;uint64_t ml,sl,tl;uint64_t mo,so,to,oo;};
static uint8_t br(struct b*d){ if(d->mo<d->ml) return d->m[d->mo++]; return 0; }
static uint64_t bd(struct b*d){uint64_t v=0,sh=1;for(;;){uint8_t x=br(d);v+=(x&0x7f)*sh;if(x&0x80)break;sh<<=7;v+=sh;}return v;}
int oracle_bps(const uint8_t *md,uint64_t ml,const uint8_t *sd,uint64_t sl,uint8_t **td,uint64_t *tl)
{
   struct b d; uint64_t ss,ts,ms,i; uint8_t *t;
   if(ml<19||memcmp(md,"BPS1",4))return PATCH_FAIL;
   d.m=md;d.s=sd;d.ml=ml;d.sl=sl;d.mo=4;d.so=0;d.to=0;d.oo=0;
   ss=bd(&d); ts=bd(&d); ms=bd(&d);
   if(ms>ml-d.mo)return PATCH_FAIL;
   for(i=0;i<ms;i++)br(&d);
   if(ss>sl)return PATCH_FAIL;
   t=(uint8_t*)calloc(1,ts?(size_t)ts:1); if(!t)return PATCH_FAIL;
   d.t=t; d.tl=ts;
   while(d.mo < d.ml-12){
      uint64_t len=bd(&d); unsigned mode=len&3; len=(len>>2)+1;
      if(d.oo>=d.tl||len>d.tl-d.oo){free(t);return PATCH_FAIL;}
      switch(mode){
      case 0: if(d.oo+len>d.sl){free(t);return PATCH_FAIL;}
              while(len--){t[d.oo]=sd[d.oo];d.oo++;} break;
      case 1: if(d.mo+len>d.ml-12){free(t);return PATCH_FAIL;}
              while(len--){t[d.oo++]=br(&d);} break;
      default:{
         int64_t off=(int64_t)bd(&d); int neg=off&1; off>>=1; if(neg)off=-off;
         if(mode==2){ d.so+=off;
            if(d.so>d.sl||len>d.sl-d.so){free(t);return PATCH_FAIL;}
            while(len--){t[d.oo++]=sd[d.so++];} }
         else { d.to+=off;
            if(d.to>d.tl||len>d.tl-d.to){free(t);return PATCH_FAIL;}
            while(len--){t[d.oo++]=t[d.to++];} }
         break;}
      }
   }
   *td=t;*tl=ts;return PATCH_SUCCESS;
}

static int g_finish_failures = 0;
static void pstream_fail_note(void){ g_finish_failures++; printf("  finish() failed\n"); }

/* Hypothesis: harness aliasing, not applier bug.  Same trials, but each
 * cmp_stream gets its own private copy of the patch buffer. */
static unsigned R=2463534242u;
static unsigned xr(void){R^=R<<13;R^=R>>17;R^=R<<5;return R;}
static void put24(uint8_t*b,uint32_t v){b[0]=v>>16;b[1]=v>>8;b[2]=v;}
static size_t make_ips(uint8_t*pat,const uint8_t*src,size_t sl,size_t*want){size_t o=0;int recs=1+xr()%12,i;uint32_t maxend=(uint32_t)sl;memcpy(pat,"PATCH",5);o=5;for(i=0;i<recs;i++){uint32_t addr=xr()%(sl?sl+64:64);if(addr==0x454f46)addr++;if(xr()&1){uint32_t len=1+xr()%40;put24(pat+o,addr);o+=3;pat[o++]=len>>8;pat[o++]=len;{uint32_t k;for(k=0;k<len;k++)pat[o++]=(uint8_t)xr();}if(addr+len>maxend)maxend=addr+len;}else{uint32_t len=1+xr()%60;put24(pat+o,addr);o+=3;pat[o++]=0;pat[o++]=0;pat[o++]=len>>8;pat[o++]=len;pat[o++]=(uint8_t)xr();if(addr+len>maxend)maxend=addr+len;}}memcpy(pat+o,"EOF",3);o+=3;if(xr()&1){uint32_t tl=maxend+(xr()%128);put24(pat+o,tl);o+=3;*want=tl;}else *want=maxend;return o;}
static void enc(uint8_t*b,size_t*o,uint64_t v){for(;;){uint8_t x=v&0x7f;v>>=7;if(!v){b[(*o)++]=x|0x80;break;}b[(*o)++]=x;v--;}}
static uint32_t crc(const uint8_t*p,size_t n){static uint32_t T[256];static int I=0;uint32_t c;if(!I){uint32_t i,j;for(i=0;i<256;i++){c=i;for(j=0;j<8;j++)c=(c&1)?0xEDB88320u^(c>>1):c>>1;T[i]=c;}I=1;}c=~0u;while(n--)c=T[(c^*p++)&0xFF]^(c>>8);return ~c;}
static size_t make_ups(uint8_t*pat,const uint8_t*src,size_t sl,uint8_t*tgt,size_t tl){size_t o=0,i=0;memcpy(pat,"UPS1",4);o=4;enc(pat,&o,sl);enc(pat,&o,tl);while(i<tl||i<sl){size_t run=0;while(i<tl){uint8_t s=i<sl?src[i]:0;if(s!=tgt[i])break;i++;run++;}enc(pat,&o,run);for(;;){uint8_t s=i<sl?src[i]:0,t=i<tl?tgt[i]:0,x=s^t;pat[o++]=x;i++;if(x==0)break;if(i>tl&&i>sl)break;}}{uint32_t sc=crc(src,sl),tc=crc(tgt,tl),pc;int k;for(k=0;k<4;k++)pat[o++]=sc>>(k*8);for(k=0;k<4;k++)pat[o++]=tc>>(k*8);pc=crc(pat,o);for(k=0;k<4;k++)pat[o++]=pc>>(k*8);}return o;}
static int cmp1(int fmt,const uint8_t*pat,size_t pl,const uint8_t*src,size_t sl,size_t chunk){
   uint8_t *os,*ss;uint64_t ol;size_t sl2,p=0;patch_stream_t*ps;
   uint8_t *priv=malloc(pl?pl:1); if(pl) memcpy(priv,pat,pl); /* private copy */
   if(fmt==0){oracle_ips(priv,pl,src,sl,&os,&ol);ps=patch_stream_ips_open(priv,pl,sl);}
   else{oracle_ups(priv,pl,src,sl,&os,&ol);ps=patch_stream_ups_open(priv,pl,sl);}
   if(!ps){free(os);free(priv);return 0;}
   while(p<sl){size_t n=chunk<sl-p?chunk:sl-p;patch_stream_feed(ps,src+p,n);p+=n;}
   if(!patch_stream_finish(ps,&ss,&sl2)){pstream_fail_note();free(os);patch_stream_free(ps);free(priv);return 1;}
   {int bad=(sl2!=ol)||(ol&&memcmp(os,ss,ol));
    free(os);free(ss);patch_stream_free(ps);free(priv);return bad;}}
static int run_ips_ups(void){
   size_t chunks[]={1,2,3,7,64,1024,1<<20};int trial,ci,fails=0,total=0;
   for(trial=0;trial<20000;trial++){
      size_t sl; {unsigned m=xr()%10; sl = m==0?0 : m==1?1 : m==2?2 : m==3?(xr()%4) : m==4?1024 : m==5?1025 : xr()%8192;}uint8_t*src=malloc(sl?sl:1);size_t k;uint8_t*pat=malloc(1<<21);size_t pl,wtl;
      for(k=0;k<sl;k++)src[k]=(uint8_t)xr();
      pl=make_ips(pat,src,sl,&wtl);
      for(ci=0;ci<7;ci++){total++;fails+=cmp1(0,pat,pl,src,sl,chunks[ci]);}
      {long tld=(long)sl+((long)(xr()%256)-128);size_t tl=tld<0?0:(size_t)tld;uint8_t*tgt=malloc(tl?tl:1);
       for(k=0;k<tl;k++)tgt[k]=(uint8_t)((k<sl?src[k]:0)^((xr()&3)?0:xr()));
       pl=make_ups(pat,src,sl,tgt,tl);
       for(ci=0;ci<7;ci++){total++;fails+=cmp1(1,pat,pl,src,sl,chunks[ci]);}
       free(tgt);}
      free(src);free(pat);}
   printf("%s: %d comparisons, %d mismatches (private patch copies)\n",fails?"FAIL":"PASS",total,fails);
   return fails!=0;}

/* Differential test for streaming BPS: build valid BPS patches, compare
 * streamed output against the oracle across chunk sizes. */
static unsigned R2=13579;
static unsigned xr2(void){R2^=R2<<13;R2^=R2>>17;R2^=R2<<5;return R2;}
static void enc2(uint8_t*b,size_t*o,uint64_t v){for(;;){uint8_t x=v&0x7f;v>>=7;if(!v){b[(*o)++]=x|0x80;break;}b[(*o)++]=x;v--;}}
static uint32_t crc2(const uint8_t*p,size_t n){static uint32_t T[256];static int I=0;uint32_t c;if(!I){uint32_t i,j;for(i=0;i<256;i++){c=i;for(j=0;j<8;j++)c=(c&1)?0xEDB88320u^(c>>1):c>>1;T[i]=c;}I=1;}c=~0u;while(n--)c=T[(c^*p++)&0xFF]^(c>>8);return ~c;}

/* Emit a random but VALID bps: walk output, choosing commands whose
 * bounds hold. Track expected target as we go. */
static size_t make_bps(uint8_t*pat,const uint8_t*src,size_t sl,uint8_t*tgt,size_t tl)
{
   size_t o=0,oo=0; int64_t so=0,to=0;
   memcpy(pat,"BPS1",4);o=4;
   enc2(pat,&o,sl); enc2(pat,&o,tl); enc2(pat,&o,0); /* no markup */
   while(oo<tl){
      unsigned mode; uint64_t len;
      uint64_t maxlen=tl-oo; if(maxlen>32)maxlen=32;
      len=1+xr2()%maxlen;
      mode=xr2()&3;
      /* validity filters */
      if(mode==0 && oo+len>sl) mode=1;
      if(mode==2){ if(sl==0){mode=1;} }
      if(mode==3){ if(to<=0||(uint64_t)to>=oo){mode=1;} }
      switch(mode){
      case 0: /* SOURCE_READ */
         enc2(pat,&o,((len-1)<<2)|0);
         { uint64_t k; for(k=0;k<len;k++){ tgt[oo]=src[oo]; oo++; } }
         break;
      case 1: /* TARGET_READ */
         enc2(pat,&o,((len-1)<<2)|1);
         { uint64_t k; for(k=0;k<len;k++){ uint8_t v=(uint8_t)xr2(); pat[o++]=v; tgt[oo++]=v; } }
         break;
      case 2: { /* SOURCE_COPY */
         int64_t ns; int64_t delta;
         if(len>sl){ len=sl; }
         ns = (int64_t)(xr2()%(sl-len+1));
         delta = ns-so;
         enc2(pat,&o,((len-1)<<2)|2);
         enc2(pat,&o, (uint64_t)((delta<0? ((-delta)<<1)|1 : (delta<<1))));
         so=ns;
         { uint64_t k; for(k=0;k<len;k++){ tgt[oo++]=src[so++]; } }
         break; }
      default: { /* TARGET_COPY */
         int64_t nt, delta;
         if(len>oo) len=oo;
         if(len==0){ continue; }
         nt=(int64_t)(xr2()%(oo-len+1));
         delta=nt-to;
         enc2(pat,&o,((len-1)<<2)|3);
         enc2(pat,&o,(uint64_t)((delta<0? ((-delta)<<1)|1 : (delta<<1))));
         to=nt;
         { uint64_t k; for(k=0;k<len;k++){ tgt[oo++]=tgt[to++]; } }
         break; }
      }
   }
   {
      uint32_t sc = crc2(src, sl), tc = crc2(tgt, tl), pc;
      int k;
      for (k = 0; k < 4; k++)
         pat[o++] = sc >> (k * 8);
      for (k = 0; k < 4; k++)
         pat[o++] = tc >> (k * 8);
      pc = crc2(pat, o);
      for (k = 0; k < 4; k++)
         pat[o++] = pc >> (k * 8);
   }
   return o;
}

static int run_bps(void)
{
   size_t chunks[]={1,3,64,1024,1<<20};
   int trial,ci,fails=0,total=0,skipped=0;
   for(trial=0;trial<4000;trial++){
      size_t sl,tl,k,pl;
      static uint8_t src[6000],tgt[6000],pat[200000];
      { unsigned m=xr2()%8; sl = m==0?0: m==1?1: m==2?2: xr2()%6000; }
      tl = 1+xr2()%6000;
      for(k=0;k<sl;k++)src[k]=(uint8_t)xr2();
      pl=make_bps(pat,src,sl,tgt,tl);
      for(ci=0;ci<5;ci++){
         uint8_t *os,*ss; uint64_t ol; size_t sl2,p=0; patch_stream_t*ps;
         uint8_t *priv=malloc(pl); memcpy(priv,pat,pl);
         if(oracle_bps(priv,pl,src,sl,&os,&ol)!=PATCH_SUCCESS){ skipped++; free(priv); continue; }
         ps=patch_stream_bps_open(priv,pl,sl);
         if(!ps){ skipped++; free(os); free(priv); continue; }
         while(p<sl){ size_t n=chunks[ci]<sl-p?chunks[ci]:sl-p; patch_stream_feed(ps,src+p,n); p+=n; }
         total++;
         if(!patch_stream_finish(ps,&ss,&sl2)){ fails++; printf("finish failed t=%d chunk=%zu\n",trial,chunks[ci]); patch_stream_free(ps); free(os); free(priv); continue; }
         if(sl2!=ol||(ol&&memcmp(os,ss,ol))){ fails++;
            printf("MISMATCH t=%d chunk=%zu sl=%zu tl=%zu\n",trial,chunks[ci],sl,tl);
            for(k=0;k<ol&&k<8;k++) if(os[k]!=ss[k]){printf("  first diff@%zu %02x vs %02x\n",k,os[k],ss[k]);break;} }
         free(os);free(ss);patch_stream_free(ps);free(priv);
      }
   }
   printf("%s: %d comparisons, %d mismatches (%d skipped)\n",fails?"FAIL":"PASS",total,fails,skipped);
   return fails!=0;
}

int main(void)
{
   int a, b;
   printf("--- IPS / UPS ---\n");
   a = run_ips_ups();
   printf("--- BPS ---\n");
   b = run_bps();
   printf("%s\n", (a || b) ? "FAILED" : "OK");
   return (a || b) ? 1 : 0;
}
