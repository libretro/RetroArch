/* Reads every hunk of an image through rchd and compares against
 * another reader's decode of the same hunk.
 *
 * Reading to the end rather than stopping at a round number matters:
 * an image whose size is not a multiple of the hunk size ends in a
 * hunk that is partly padding, and that hunk exercises a path no other
 * one does.
 *
 *   cc -I libretro-common/include -I libretro-common/formats/libchdr \
 *      -DHAVE_RCHD_DEFLATE -DHAVE_RCHD_LZMA -DHAVE_RCHD_FLAC \
 *      -DHAVE_7ZIP -DHAVE_RFLAC -DWANT_SUBCODE -DWANT_RAW_DATA_SECTOR \
 *      -o rchd_compare_test <this> <rchd.c and its deps> <libchdr>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rchd.h>
#include <libchdr/chd.h>
static FILE *g;
static int pull(rchd_t *c, rchd_request_t *r)
{ static uint8_t b[262144]; size_t t = r->length>sizeof(b)?sizeof(b):r->length, n;
  if (fseek(g,(long)r->offset,SEEK_SET)) return 0; n=fread(b,1,t,g);
  return n && rchd_feed(c,b,n)==RCHD_OK; }
int main(int argc,char**argv)
{ rchd_t*c; rchd_request_t rq; chd_file*ch=NULL; const chd_header*h;
  const rchd_info_t*i; uint8_t*a,*b2; uint32_t n,ok=0,bad=0,lim;
  if(argc<2)return 2; if(!(g=fopen(argv[1],"rb")))return 1;
  if(chd_open(argv[1],CHD_OPEN_READ,NULL,&ch)!=CHDERR_NONE){printf("  libchdr open failed\n");return 1;}
  h=chd_get_header(ch); c=rchd_new();
  { int e; while((e=rchd_open_step(c,&rq))==RCHD_PENDING) if(!pull(c,&rq)){e=-1;break;}
    if(e!=RCHD_OK){printf("  rchd open failed %d\n",e);return 1;} }
  i=rchd_info(c); a=malloc(i->hunk_bytes); b2=malloc(h->hunkbytes);
  lim = i->hunk_count;
  for(n=0;n<lim;n++)
  { int e; if(chd_read(ch,n,b2)!=CHDERR_NONE){bad++;continue;}
    if(rchd_read_hunk_begin(c,n,a)!=RCHD_OK){bad++;continue;}
    while((e=rchd_read_step(c,&rq))==RCHD_PENDING) if(!pull(c,&rq))break;
    if(e!=RCHD_OK||memcmp(a,b2,i->hunk_bytes)){bad++;continue;} ok++; }
  printf("  %-44s hunks ok=%-6u bad=%-4u %s\n",argv[1],ok,bad,bad?"FAIL":"PASS");
  free(a);free(b2);rchd_free(c);chd_close(ch);fclose(g);return bad?1:0; }
int path_is_directory(const char*p){(void)p;return 0;}
int path_mkdir(const char*d){(void)d;return 0;}
