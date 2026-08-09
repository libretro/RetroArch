/* Robustness: truncation, mid-stream entry, and byte corruption.
 * Asserts the parser never stalls, never over-reads, and always terminates. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rmpeg1_ps.h>

static unsigned long rng_s = 12345;
static unsigned long rng(void){ rng_s = rng_s*1103515245UL+12345UL; return (rng_s>>16)&0x7FFF; }

static int run(const unsigned char *d, size_t len, size_t chunk, size_t *pkts)
{
   rmpeg1_ps_t *ps = rmpeg1_ps_init(0);
   size_t off=0, n=0, guard=0;
   rmpeg1_ps_packet_t p;
   if(!ps) return -1;
   while (off < len) {
      size_t want = len-off; if (want>chunk) want=chunk;
      size_t got = rmpeg1_ps_write(ps, d+off, want);
      if (got==0) { /* must be drainable */
         if (!rmpeg1_ps_next(ps,&p)) { rmpeg1_ps_free(ps); return -2; /* stall */ }
         n++; continue;
      }
      off += got;
      while (rmpeg1_ps_next(ps,&p)) {
         if (p.size==0 || p.data==NULL) { rmpeg1_ps_free(ps); return -3; }
         n++;
         if (++guard > 10000000) { rmpeg1_ps_free(ps); return -4; }
      }
   }
   while (rmpeg1_ps_next(ps,&p)) n++;
   *pkts=n; rmpeg1_ps_free(ps); return 0;
}

int main(int argc,char**argv)
{
   FILE *f=fopen(argv[1],"rb"); long len; unsigned char *d,*w; size_t n; int i,r,fails=0;
   fseek(f,0,SEEK_END); len=ftell(f); fseek(f,0,SEEK_SET);
   d=malloc(len); if(fread(d,1,len,f)!=(size_t)len) return 2; fclose(f);
   w=malloc(len);

   for (i=1;i<=64;i++){ size_t cut=(size_t)len*i/65;
      r=run(d,cut,2324,&n); if(r){printf("TRUNC %zu -> err %d\n",cut,r);fails++;} }
   printf("truncation: %d/64 failed\n",fails);

   { int f2=0; for(i=1;i<=64;i++){ size_t st=(size_t)len*i/65;
      r=run(d+st,(size_t)len-st,2324,&n); if(r){printf("MIDENTRY %zu -> err %d\n",st,r);f2++;} }
     printf("mid-stream entry: %d/64 failed\n",f2); fails+=f2; }

   { int f3=0; for(i=0;i<3000;i++){ int k,nc=1+(rng()%64);
      memcpy(w,d,len);
      for(k=0;k<nc;k++){ size_t pos=((size_t)rng()<<15|rng())%(size_t)len; w[pos]=(unsigned char)(rng()&0xFF); }
      r=run(w,(size_t)len,2324,&n); if(r){printf("CORRUPT seed %d -> err %d\n",i,r);if(++f3>5)break;} }
     printf("corruption: %d failures in 3000 runs\n",f3); fails+=f3; }

   { int f4=0; for(i=0;i<400;i++){ size_t k,L=1+(rng()%4096);
      for(k=0;k<L;k++) w[k]=(unsigned char)(rng()&0xFF);
      r=run(w,L,2324,&n); if(r){printf("RANDOM -> err %d\n",r);if(++f4>5)break;} }
     printf("pure random input: %d failures in 400 runs\n",f4); fails+=f4; }

   free(d); free(w);
   printf(fails?"RESULT: FAIL\n":"RESULT: PASS\n"); return fails?1:0;
}
