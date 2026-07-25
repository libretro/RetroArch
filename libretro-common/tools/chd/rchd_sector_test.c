/* Checks that a sector-addressed read yields the same bytes as a byte
 * read of the same frames.
 *
 * The two paths differ: one pulls whole 2448-byte frames, the other
 * emits each sector at its own track size and appends subchannel only
 * where a track declares it. They must still agree.
 *
 *   rchd_sector_test <image.chd> <lba>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rchd.h>
static FILE *g;
static int pull(rchd_t *c, rchd_request_t *r)
{ static uint8_t b[262144]; size_t t=r->length>sizeof(b)?sizeof(b):r->length,n;
  if(fseek(g,(long)r->offset,SEEK_SET))return 0; n=fread(b,1,t,g);
  return n && rchd_feed(c,b,n)==RCHD_OK; }
int main(int argc,char**argv)
{ rchd_t*c; rchd_request_t rq; const rchd_info_t*i; int e; size_t need;
  uint8_t *sec,*raw; uint32_t n,bad=0, lba0=(uint32_t)atoi(argv[2]);
  g=fopen(argv[1],"rb"); c=rchd_new();
  while((e=rchd_open_step(c,&rq))==RCHD_PENDING) if(!pull(c,&rq))break;
  i=rchd_info(c);
  if(rchd_read_extent(c,lba0,16,&need)!=RCHD_OK){printf("  extent failed\n");return 1;}
  sec=malloc(need); raw=malloc(16*i->unit_bytes);
  if(rchd_read_sectors_begin(c,lba0,16,sec,need,0)!=RCHD_OK)return 1;
  while((e=rchd_read_step(c,&rq))==RCHD_PENDING) if(!pull(c,&rq))break;
  if(e!=RCHD_OK){printf("  sector read failed\n");return 1;}
  if(rchd_read_begin(c,(uint64_t)lba0*i->unit_bytes,raw,16*i->unit_bytes)!=RCHD_OK)return 1;
  while((e=rchd_read_step(c,&rq))==RCHD_PENDING) if(!pull(c,&rq))break;
  if(e!=RCHD_OK){printf("  byte read failed\n");return 1;}
  { const rchd_track_t*t=rchd_track_for_lba(c,lba0);
    size_t off=0;
    for(n=0;n<16;n++)
    { if(memcmp(sec+off, raw+(size_t)n*i->unit_bytes, t->data_size)) bad++;
      off+=t->data_size;
      if(t->sub_size)
      { if(memcmp(sec+off, raw+(size_t)n*i->unit_bytes+2352, t->sub_size)) bad++;
        off+=t->sub_size; } } }
  printf("  %-40s 16 sectors from LBA %u: %s (%lu bytes)\n",
         argv[1], lba0, bad?"MISMATCH":"identical to a byte read",
         (unsigned long)need);
  return bad?1:0; }
int path_is_directory(const char*p){(void)p;return 0;}
int path_mkdir(const char*d){(void)d;return 0;}
