/* Decodes every 'huff' hunk of a CHD through rhuff_decode_block and
 * compares against the original uncompressed source. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <encodings/huffman.h>

static uint32_t rd32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t rdbe(const uint8_t*p,int n){uint64_t v=0;int i;for(i=0;i<n;i++)v=(v<<8)|p[i];return v;}

int main(int argc,char**argv)
{
   FILE *cf,*sf; long clen; uint8_t *chd,*src;
   uint64_t mo,logical,ds; uint32_t hb,nh,ml,lb; uint32_t n;
   static uint16_t maplut[1<<8];
   static uint16_t biglut[1<<16];
   rhuff_dec_t mapdec, big; rhuff_bits_t bits;
   uint8_t *codes, *out; uint64_t cur; uint32_t rep=0,lastc=0;
   unsigned ok=0,tot=0;

   if (argc<3) return 2;
   cf=fopen(argv[1],"rb"); sf=fopen(argv[2],"rb"); if(!cf||!sf) return 1;
   fseek(cf,0,SEEK_END); clen=ftell(cf); fseek(cf,0,SEEK_SET);
   chd=malloc((size_t)clen); if(fread(chd,1,(size_t)clen,cf)!=(size_t)clen)return 1; fclose(cf);
   fseek(sf,0,SEEK_END); { long sl=ftell(sf); fseek(sf,0,SEEK_SET);
     src=malloc((size_t)sl); if(fread(src,1,(size_t)sl,sf)!=(size_t)sl)return 1; } fclose(sf);

   logical=rdbe(chd+32,8); mo=rdbe(chd+40,8); hb=rd32(chd+56);
   nh=(uint32_t)((logical+hb-1)/hb);
   ml=rd32(chd+mo); ds=rdbe(chd+mo+4,6); lb=chd[mo+12];

   rhuff_dec_init(&mapdec,16,8,maplut,RHUFF_LOOKUP_ENTRIES(8));
   rhuff_bits_init(&bits,chd+mo+16,ml);
   if (rhuff_read_tree_rle(&mapdec,&bits)!=RHUFF_OK){printf("map tree failed\n");return 1;}
   codes=malloc(nh);
   for(n=0;n<nh;n++){
      uint32_t v;
      if(rep){codes[n]=(uint8_t)lastc;rep--;continue;}
      v=rhuff_dec_decode_one(&mapdec,&bits);
      if(v==7){codes[n]=(uint8_t)lastc;rep=2+rhuff_dec_decode_one(&mapdec,&bits);}
      else if(v==8){uint32_t hi=rhuff_dec_decode_one(&mapdec,&bits);
                    uint32_t lo=rhuff_dec_decode_one(&mapdec,&bits);
                    codes[n]=(uint8_t)lastc; rep=2+16+(hi<<4)+lo;}
      else {lastc=v; codes[n]=(uint8_t)v;}
   }
   cur=ds; out=malloc(hb);
   rhuff_dec_init(&big,256,16,biglut,RHUFF_LOOKUP_ENTRIES(16));
   for(n=0;n<nh;n++){
      uint32_t code=codes[n], len=0;
      if(code<=3){len=rhuff_bits_read(&bits,(int)lb); rhuff_bits_read(&bits,16);}
      else if(code==4){len=hb; rhuff_bits_read(&bits,16);}
      else if(code==5){rhuff_bits_read(&bits,(int)chd[mo+13]); continue;}
      else if(code==6){rhuff_bits_read(&bits,(int)chd[mo+14]); continue;}
      else continue;
      if(code<=3){
         rhuff_bits_t hb2;
         tot++;
         rhuff_bits_init(&hb2,chd+cur,len);
         if(rhuff_decode_block(&big,&hb2,out,hb)==RHUFF_OK
            && memcmp(out,src+(size_t)n*hb,hb)==0) ok++;
      }
      cur+=len;
   }
   printf("%-14s huff hunks byte-exact: %u/%u %s\n",argv[1],ok,tot,ok==tot?"PASS":"FAIL");
   free(codes); free(out); free(chd); free(src);
   return ok==tot?0:1;
}
