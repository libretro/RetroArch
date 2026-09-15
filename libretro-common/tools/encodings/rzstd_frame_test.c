/* Decodes a Zstandard frame through rzstd and compares against a
 * reference decode of the same frame.
 *
 * Frames extracted from a Zstandard-compressed CHD image are a good
 * source: they are small, self-contained, and their decoded size is
 * the hunk size, so a reference is easy to produce.
 *
 *   rzstd_frame_test <frame.zst> <frame.raw>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
int rzstd_probe_frame(const uint8_t*,size_t,uint8_t*,size_t,size_t*,size_t*);
int main(int argc,char**argv)
{ FILE*f; long n,m; uint8_t*b,*t; static uint8_t o[1<<20]; size_t w=0,u=0; int e;
  f=fopen(argv[1],"rb"); fseek(f,0,SEEK_END); n=ftell(f); fseek(f,0,SEEK_SET);
  b=malloc((size_t)n); if(fread(b,1,(size_t)n,f)!=(size_t)n) return 1; fclose(f);
  f=fopen(argv[2],"rb"); fseek(f,0,SEEK_END); m=ftell(f); fseek(f,0,SEEK_SET);
  t=malloc((size_t)m); if(fread(t,1,(size_t)m,f)!=(size_t)m) return 1; fclose(f);
  e=rzstd_probe_frame(b,(size_t)n,o,sizeof(o),&w,&u);
  if(e){printf("  %-14s decode failed %d\n",argv[1],e);return 1;}
  printf("  %-14s %lu bytes -> %s\n",argv[1],(unsigned long)w,
    (w==(size_t)m && !memcmp(o,t,w)) ? "BYTE-EXACT vs reference" : "DIFFERS");
  if(w!=(size_t)m||memcmp(o,t,w)){size_t i;for(i=0;i<w&&i<(size_t)m;i++)
    if(o[i]!=t[i]){printf("    first difference at %lu: got %02x want %02x\n",
      (unsigned long)i,o[i],t[i]);break;}}
  return 0; }
