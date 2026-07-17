/* rh264 -- clean-room H.264 baseline intra decoder (amalgamated single TU).
 * Public API: include/formats/rh264.h. CAVLC tables extracted from libopenh264
 * encoder rodata (verified prefix-free). Baseline intra only. */
#include <formats/rh264.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ==================== rh264_bits.h ==================== */
/* rh264 -- bitstream layer: NAL RBSP + Exp-Golomb. */
typedef struct { const uint8_t *buf; size_t size; size_t bitpos; } rh264_bits;
static void rh264_bits_init(rh264_bits *b, const uint8_t *buf, size_t size){ b->buf=buf; b->size=size; b->bitpos=0; }
static int rh264_more_data(const rh264_bits *b){ return b->bitpos < b->size*8; }
static uint32_t rh264_u1(rh264_bits *b){
   size_t byte=b->bitpos>>3; int off=7-(int)(b->bitpos&7); uint32_t v;
   if(byte>=b->size){ b->bitpos++; return 0; }
   v=(b->buf[byte]>>off)&1; b->bitpos++; return v;
}
static uint32_t rh264_un(rh264_bits *b,int n){ uint32_t v=0; int i; for(i=0;i<n;i++) v=(v<<1)|rh264_u1(b); return v; }
static uint32_t rh264_ue(rh264_bits *b){
   int z=0; while(rh264_more_data(b)&&rh264_u1(b)==0) z++;
   if(z==0) return 0;
   if(z>=32) return 0xFFFFFFFFu;
   return ((1u<<z)-1u)+rh264_un(b,z);
}
static int32_t rh264_se(rh264_bits *b){ uint32_t k=rh264_ue(b); uint32_t m=(k+1)>>1; return (k&1)?(int32_t)m:-(int32_t)m; }
static uint8_t *rh264_unescape(const uint8_t *nal,size_t len,size_t *out_size){
   uint8_t *r; size_t i,o=0; if(!(r=(uint8_t*)malloc(len?len:1))) return NULL;
   for(i=0;i<len;i++){
      if(i>=2&&nal[i]==0x03&&nal[i-1]==0x00&&nal[i-2]==0x00&&i+1<len&&nal[i+1]<=0x03) continue;
      r[o++]=nal[i];
   }
   *out_size=o; return r;
}


/* ==================== rh264_ps.h ==================== */
typedef struct { int valid,profile_idc,level_idc,log2_max_frame_num,pic_order_cnt_type,
   log2_max_poc_lsb,frame_mbs_only_flag,pic_width_in_mbs,pic_height_in_map_units,
   frame_width,frame_height,chroma_format_idc; } rh264_sps;
typedef struct { int valid,entropy_coding_mode_flag,pic_init_qp,
   deblocking_filter_control_present,constrained_intra_pred_flag,chroma_qp_index_offset; } rh264_pps;
static void rh264_skip_scaling_list(rh264_bits *b,int size){
   int last=8,next=8,j; for(j=0;j<size;j++){ if(next){int d=rh264_se(b); next=(last+d+256)&255;} last=next?next:last; }
}
static int rh264_parse_sps(const uint8_t *rbsp,size_t size,rh264_sps *s){
   rh264_bits b; int i; memset(s,0,sizeof(*s)); rh264_bits_init(&b,rbsp,size);
   s->profile_idc=rh264_un(&b,8); rh264_un(&b,8); s->level_idc=rh264_un(&b,8); rh264_ue(&b);
   s->chroma_format_idc=1;
   if(s->profile_idc==100||s->profile_idc==110||s->profile_idc==122||s->profile_idc==244||
      s->profile_idc==44||s->profile_idc==83||s->profile_idc==86||s->profile_idc==118||
      s->profile_idc==128||s->profile_idc==138||s->profile_idc==139||s->profile_idc==134){
      s->chroma_format_idc=rh264_ue(&b); if(s->chroma_format_idc==3) rh264_u1(&b);
      rh264_ue(&b); rh264_ue(&b); rh264_u1(&b);
      if(rh264_u1(&b)){ int lists=(s->chroma_format_idc!=3)?8:12; for(i=0;i<lists;i++) if(rh264_u1(&b)) rh264_skip_scaling_list(&b,i<6?16:64); }
   }
   s->log2_max_frame_num=rh264_ue(&b)+4; s->pic_order_cnt_type=rh264_ue(&b);
   if(s->pic_order_cnt_type==0) s->log2_max_poc_lsb=rh264_ue(&b)+4;
   else if(s->pic_order_cnt_type==1){ int n; rh264_u1(&b); rh264_se(&b); rh264_se(&b); n=rh264_ue(&b); for(i=0;i<n;i++) rh264_se(&b); }
   rh264_ue(&b); rh264_u1(&b);
   s->pic_width_in_mbs=rh264_ue(&b)+1; s->pic_height_in_map_units=rh264_ue(&b)+1;
   s->frame_mbs_only_flag=rh264_u1(&b); if(!s->frame_mbs_only_flag) rh264_u1(&b);
   rh264_u1(&b);
   { int cl=0,cr=0,ct=0,cb=0; int mbh=(2-s->frame_mbs_only_flag)*s->pic_height_in_map_units;
     if(rh264_u1(&b)){ cl=rh264_ue(&b); cr=rh264_ue(&b); ct=rh264_ue(&b); cb=rh264_ue(&b); }
     { int sw=2, sh=2*(2-s->frame_mbs_only_flag);
       s->frame_width=s->pic_width_in_mbs*16-sw*(cl+cr);
       s->frame_height=mbh*16-sh*(ct+cb); } }
   s->valid=1; return 1;
}
static int rh264_parse_pps(const uint8_t *rbsp,size_t size,rh264_pps *p){
   rh264_bits b; memset(p,0,sizeof(*p)); rh264_bits_init(&b,rbsp,size);
   rh264_ue(&b); rh264_ue(&b); p->entropy_coding_mode_flag=rh264_u1(&b); rh264_u1(&b);
   if(rh264_ue(&b)!=0) return 0;
   rh264_ue(&b); rh264_ue(&b); rh264_u1(&b); rh264_un(&b,2);
   p->pic_init_qp=rh264_se(&b)+26; rh264_se(&b); p->chroma_qp_index_offset=rh264_se(&b);
   p->deblocking_filter_control_present=rh264_u1(&b); p->constrained_intra_pred_flag=rh264_u1(&b);
   p->valid=1; return 1;
}


/* ==================== rh264_slice.h ==================== */
enum { RH264_SLICE_P=0,RH264_SLICE_B=1,RH264_SLICE_I=2,RH264_SLICE_SP=3,RH264_SLICE_SI=4 };
typedef struct { int first_mb_in_slice,slice_type,pic_parameter_set_id,frame_num,
   idr_pic_id,poc_lsb,slice_qp,disable_deblocking_filter_idc,is_idr,
   slice_alpha_c0_offset,slice_beta_offset; } rh264_slice_hdr;
/* advancing variant: parses from an existing reader b (leaves it at slice data) */
static int rh264_parse_slice_header_adv(rh264_bits *b,int nal_unit_type,int nal_ref_idc,
      const rh264_sps *sps,const rh264_pps *pps,rh264_slice_hdr *sh){
   int st; memset(sh,0,sizeof(*sh)); sh->is_idr=(nal_unit_type==5);
   sh->first_mb_in_slice=rh264_ue(b);
   st=rh264_ue(b); sh->slice_type=st%5; sh->pic_parameter_set_id=rh264_ue(b);
   sh->frame_num=rh264_un(b,sps->log2_max_frame_num);
   if(!sps->frame_mbs_only_flag){ if(rh264_u1(b)) rh264_u1(b); }
   if(sh->is_idr) sh->idr_pic_id=rh264_ue(b);
   if(sps->pic_order_cnt_type==0) sh->poc_lsb=rh264_un(b,sps->log2_max_poc_lsb);
   if(sh->slice_type!=RH264_SLICE_I&&sh->slice_type!=RH264_SLICE_SI) return 0;
   /* dec_ref_pic_marking (7.3.3.3): only when nal_ref_idc != 0. */
   if(nal_ref_idc){
      if(sh->is_idr){ rh264_u1(b); rh264_u1(b); } /* no_output_prior + long_term_ref */
      else {
         if(rh264_u1(b)){ /* adaptive_ref_pic_marking_mode_flag */
            int op; do{ op=rh264_ue(b);
               if(op==1||op==3) rh264_ue(b);
               if(op==2) rh264_ue(b);
               if(op==3||op==6) rh264_ue(b);
               if(op==4) rh264_ue(b);
            } while(op!=0);
         }
      }
   }
   sh->slice_qp=pps->pic_init_qp+rh264_se(b);
   if(pps->deblocking_filter_control_present){
      sh->disable_deblocking_filter_idc=rh264_ue(b);
      if(sh->disable_deblocking_filter_idc!=1){
         sh->slice_alpha_c0_offset=rh264_se(b)*2;
         sh->slice_beta_offset=rh264_se(b)*2; }
   }
   return 1;
}


/* ==================== rh264_xform.h ==================== */
static const uint8_t rh264_zigzag4[16]={0,1,4,8,5,2,3,6,9,12,13,10,7,11,14,15};
static const int rh264_dequant4_v[6][3]={{10,16,13},{11,18,14},{13,20,16},{14,23,18},{16,25,20},{18,29,23}};
static const uint8_t rh264_dequant4_idx[16]={0,2,0,2,2,1,2,1,0,2,0,2,2,1,2,1};
static void rh264_dequant4x4(int32_t *blk,int qP,int has_dc_sep){
   int i,per=qP/6,rem=qP%6;
   for(i=0;i<16;i++){ int sc=16*rh264_dequant4_v[rem][rh264_dequant4_idx[i]];
      if(has_dc_sep&&i==0) continue;
      if(per>=4) blk[i]=(blk[i]*sc)<<(per-4); else blk[i]=(blk[i]*sc+(1<<(3-per)))>>(4-per); }
}
static void rh264_itransform4x4(const int32_t *d,int32_t *r){
   int32_t e[16]; int i;
   for(i=0;i<4;i++){ const int32_t*s=d+i*4;
      int32_t z0=s[0]+s[2],z1=s[0]-s[2],z2=(s[1]>>1)-s[3],z3=s[1]+(s[3]>>1);
      e[i*4+0]=z0+z3;e[i*4+1]=z1+z2;e[i*4+2]=z1-z2;e[i*4+3]=z0-z3; }
   for(i=0;i<4;i++){ int32_t z0=e[0*4+i]+e[2*4+i],z1=e[0*4+i]-e[2*4+i],z2=(e[1*4+i]>>1)-e[3*4+i],z3=e[1*4+i]+(e[3*4+i]>>1);
      r[0*4+i]=z0+z3;r[1*4+i]=z1+z2;r[2*4+i]=z1-z2;r[3*4+i]=z0-z3; }
}
static void rh264_ihadamard4x4(const int32_t *in,int32_t *out){
   int32_t e[16]; int i;
   for(i=0;i<4;i++){ const int32_t*s=in+i*4;
      int32_t z0=s[0]+s[2],z1=s[0]-s[2],z2=s[1]-s[3],z3=s[1]+s[3];
      e[i*4+0]=z0+z3;e[i*4+1]=z1+z2;e[i*4+2]=z1-z2;e[i*4+3]=z0-z3; }
   for(i=0;i<4;i++){ int32_t z0=e[0*4+i]+e[2*4+i],z1=e[0*4+i]-e[2*4+i],z2=e[1*4+i]-e[3*4+i],z3=e[1*4+i]+e[3*4+i];
      out[0*4+i]=z0+z3;out[1*4+i]=z1+z2;out[2*4+i]=z1-z2;out[3*4+i]=z0-z3; }
}


/* ==================== rh264_cavlc_ct.h ==================== */
/* CAVLC VLC tables extracted from openh264 WelsEnc forward tables,
 * verified prefix-free. All {len,code}. */
static const uint8_t rh264_ct_len[4][17][4]={
 {{1,0,0,0},{6,2,0,0},{8,6,3,0},{9,8,7,5},{10,9,8,6},{11,10,9,7},{13,11,10,8},{13,13,11,9},{13,13,13,10},{14,14,13,11},{14,14,14,13},{15,15,14,14},{15,15,15,14},{16,15,15,15},{16,16,16,15},{16,16,16,16},{16,16,16,16},},
 {{2,0,0,0},{6,2,0,0},{6,5,3,0},{7,6,6,4},{8,6,6,4},{8,7,7,5},{9,8,8,6},{11,9,9,6},{11,11,11,7},{12,11,11,9},{12,12,12,11},{12,12,12,11},{13,13,13,12},{13,13,13,13},{13,14,13,13},{14,14,14,13},{14,14,14,14},},
 {{4,0,0,0},{6,4,0,0},{6,5,4,0},{6,5,5,4},{7,5,5,4},{7,5,5,4},{7,6,6,4},{7,6,6,4},{8,7,7,5},{8,8,7,6},{9,8,8,7},{9,9,8,8},{9,9,9,8},{10,9,9,9},{10,10,10,10},{10,10,10,10},{10,10,10,10},},
 {{6,0,0,0},{6,6,0,0},{6,6,6,0},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},},
};
static const uint8_t rh264_ct_code[4][17][4]={
 {{1,0,0,0},{5,1,0,0},{7,4,1,0},{7,6,5,3},{7,6,5,3},{7,6,5,4},{15,6,5,4},{11,14,5,4},{8,10,13,4},{15,14,9,4},{11,10,13,12},{15,14,9,12},{11,10,13,8},{15,1,9,12},{11,14,13,8},{7,10,9,12},{4,6,5,8},},
 {{3,0,0,0},{11,2,0,0},{7,7,3,0},{7,10,9,5},{7,6,5,4},{4,6,5,6},{7,6,5,8},{15,6,5,4},{11,14,13,4},{15,10,9,4},{11,14,13,12},{8,10,9,8},{15,14,13,12},{11,10,9,12},{7,11,6,8},{9,8,10,1},{7,6,5,4},},
 {{15,0,0,0},{15,14,0,0},{11,15,13,0},{8,12,14,12},{15,10,11,11},{11,8,9,10},{9,14,13,9},{8,10,9,8},{15,14,13,13},{11,14,10,12},{15,10,13,12},{11,14,9,12},{8,10,13,8},{13,7,9,12},{9,12,11,10},{5,8,7,6},{1,4,3,2},},
 {{3,0,0,0},{0,1,0,0},{4,5,6,0},{8,9,10,11},{12,13,14,15},{16,17,18,19},{20,21,22,23},{24,25,26,27},{28,29,30,31},{32,33,34,35},{36,37,38,39},{40,41,42,43},{44,45,46,47},{48,49,50,51},{52,53,54,55},{56,57,58,59},{60,61,62,63},},
};
static const uint8_t rh264_ctc_len[17][4]={
 {2,0,0,0},
 {6,1,0,0},
 {6,6,3,0},
 {6,7,7,6},
 {6,8,8,7},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
};
static const uint8_t rh264_ctc_code[17][4]={
 {1,0,0,0},
 {7,1,0,0},
 {4,6,1,0},
 {3,3,2,5},
 {2,3,2,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
};
static const uint8_t rh264_tz_len[16][16]={
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,3,3,4,4,5,5,6,6,7,7,8,8,9,9,9},
 {3,3,3,3,3,4,4,4,4,5,5,6,6,6,6,0},
 {4,3,3,3,4,4,3,3,4,5,5,6,5,6,0,0},
 {5,3,4,4,3,3,3,4,3,4,5,5,5,0,0,0},
 {4,4,4,3,3,3,3,3,4,5,4,5,0,0,0,0},
 {6,5,3,3,3,3,3,3,4,3,6,0,0,0,0,0},
 {6,5,3,3,3,2,3,4,3,6,0,0,0,0,0,0},
 {6,4,5,3,2,2,3,3,6,0,0,0,0,0,0,0},
 {6,6,4,2,2,3,2,5,0,0,0,0,0,0,0,0},
 {5,5,3,2,2,2,4,0,0,0,0,0,0,0,0,0},
 {4,4,3,3,1,3,0,0,0,0,0,0,0,0,0,0},
 {4,4,2,1,3,0,0,0,0,0,0,0,0,0,0,0},
 {3,3,1,2,0,0,0,0,0,0,0,0,0,0,0,0},
 {2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};
static const uint8_t rh264_tz_code[16][16]={
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,3,2,3,2,3,2,3,2,3,2,3,2,3,2,1},
 {7,6,5,4,3,5,4,3,2,3,2,3,2,1,0,0},
 {5,7,6,5,4,3,4,3,2,3,2,1,1,0,0,0},
 {3,7,5,4,6,5,4,3,3,2,2,1,0,0,0,0},
 {5,4,3,7,6,5,4,3,2,1,1,0,0,0,0,0},
 {1,1,7,6,5,4,3,2,1,1,0,0,0,0,0,0},
 {1,1,5,4,3,3,2,1,1,0,0,0,0,0,0,0},
 {1,1,1,3,3,2,2,1,0,0,0,0,0,0,0,0},
 {1,0,1,3,2,1,1,1,0,0,0,0,0,0,0,0},
 {1,0,1,3,2,1,1,0,0,0,0,0,0,0,0,0},
 {0,1,1,2,1,3,0,0,0,0,0,0,0,0,0,0},
 {0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
 {0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
 {0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};
static const uint8_t rh264_rb_len[8][15]={
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,2,2,0,0,0,0,0,0,0,0,0,0,0,0},
 {2,2,2,2,0,0,0,0,0,0,0,0,0,0,0},
 {2,2,2,3,3,0,0,0,0,0,0,0,0,0,0},
 {2,2,3,3,3,3,0,0,0,0,0,0,0,0,0},
 {2,3,3,3,3,3,3,0,0,0,0,0,0,0,0},
 {3,3,3,3,3,3,3,4,5,6,7,8,9,10,11},
};
static const uint8_t rh264_rb_code[8][15]={
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {3,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
 {3,2,1,1,0,0,0,0,0,0,0,0,0,0,0},
 {3,2,3,2,1,0,0,0,0,0,0,0,0,0,0},
 {3,0,1,3,2,5,4,0,0,0,0,0,0,0,0},
 {7,6,5,4,3,2,1,1,1,1,1,1,1,1,1},
};
static const uint8_t rh264_cdctz_len[4][4]={
 {0,0,0,0},
 {1,2,3,3},
 {1,2,2,0},
 {1,1,0,0},
};
static const uint8_t rh264_cdctz_code[4][4]={
 {0,0,0,0},
 {1,1,1,0},
 {1,1,0,0},
 {1,0,0,0},
};


/* ==================== rh264_cavlc.h ==================== */
/* rh264 -- CAVLC residual block decode (H.264 clause 9.2), using tables
 * extracted+verified from openh264. Decodes one block's coefficients into
 * coeffLevel[maxNumCoeff] in zig-zag order (caller applies inverse scan). */


/* Match a prefix-free VLC: read bits MSB-first until (len,code) hits a
 * table row whose value index is returned. tab_len/tab_code are [N]; the
 * decoded symbol is the matching index. Returns -1 on failure. */
static int rh264_vlc_match1(rh264_bits *b, const uint8_t *len_row,
      const uint8_t *code_row, int n)
{
   uint32_t acc = 0; int L = 0, i;
   while (L < 24)
   {
      acc = (acc << 1) | rh264_u1(b);
      L++;
      for (i = 0; i < n; i++)
         if (len_row[i] == L && code_row[i] == acc)
            return i;
   }
   return -1;
}

/* coeff_token: choose nC table, return total_coeff & trailing_ones. */
static int rh264_coeff_token(rh264_bits *b, int nC,
      int *total_coeff, int *trailing_ones)
{
   int t, tc, t1;
   if (nC >= 8)      t = 3;         /* FLC path baked into table 3       */
   else if (nC >= 4) t = 2;
   else if (nC >= 2) t = 1;
   else              t = 0;
   /* Search the [17][4] (tc,t1) grid for a matching prefix. */
   {
      uint32_t acc = 0; int L = 0;
      while (L < 16)
      {
         acc = (acc << 1) | rh264_u1(b);
         L++;
         for (tc = 0; tc < 17; tc++)
            for (t1 = 0; t1 <= (tc < 3 ? tc : 3); t1++)
               if (rh264_ct_len[t][tc][t1] == L
                && rh264_ct_code[t][tc][t1] == acc)
               {
                  *total_coeff   = tc;
                  *trailing_ones = t1;
                  return 1;
               }
      }
   }
   return 0;
}

/* level_prefix: count of leading zeros terminated by a 1. */
static int rh264_level_prefix(rh264_bits *b)
{
   int lz = 0;
   while (rh264_more_data(b) && rh264_u1(b) == 0)
      lz++;
   return lz;
}

/* Decode one residual block. nC: predicted non-zero count (context).
 * maxNumCoeff: 16 (luma/AC), 15 (Intra16 AC after DC), 4 (chroma DC).
 * out[]: coefficients in scan order, zero-filled. Returns total_coeff. */
static int rh264_residual_block(rh264_bits *b, int nC, int maxNumCoeff,
      int32_t *out)
{
   int total_coeff = 0, trailing_ones = 0;
   int level[16], run[16];
   int i, is_chroma_dc = (nC == -1);
   for (i = 0; i < maxNumCoeff; i++) out[i] = 0;

   if (is_chroma_dc)
   {
      /* chroma-DC coeff_token (nC == -1), dedicated VLC table. */
      uint32_t acc = 0; int L = 0, tc, t1;
      total_coeff = -1;
      while (L < 8 && total_coeff < 0)
      {
         acc = (acc << 1) | rh264_u1(b); L++;
         for (tc = 0; tc < 5 && total_coeff < 0; tc++)
            for (t1 = 0; t1 <= (tc < 3 ? tc : 3); t1++)
               if (rh264_ctc_len[tc][t1] == L && rh264_ctc_code[tc][t1] == acc)
               {
                  total_coeff = tc; trailing_ones = t1; break;
               }
      }
      if (total_coeff < 0) return -1;
      if (total_coeff == 0) return 0;
      /* fall through to shared level/run decode with maxNumCoeff==4 */
   }
   else
   {
      if (!rh264_coeff_token(b, nC, &total_coeff, &trailing_ones))
         return -1;
      if (total_coeff == 0)
         return 0;
   }

   /* Levels. */
   {
      int suffix_length = (total_coeff > 10 && trailing_ones < 3) ? 1 : 0;
      for (i = 0; i < total_coeff; i++)
      {
         if (i < trailing_ones)
         {
            level[i] = rh264_u1(b) ? -1 : 1;
         }
         else
         {
            int level_prefix = rh264_level_prefix(b);
            int level_code, level_suffix, suffix_size = suffix_length;
            if (level_prefix == 14 && suffix_length == 0) suffix_size = 4;
            else if (level_prefix >= 15) suffix_size = level_prefix - 3;
            level_suffix = suffix_size ? (int)rh264_un(b, suffix_size) : 0;
            level_code = (level_prefix < 15 ? (level_prefix << suffix_length)
                                            : ((15 << suffix_length)))
                         + level_suffix;
            if (level_prefix >= 15 && suffix_length == 0)
               level_code += 15;
            if (level_prefix >= 16)
               level_code += (1 << (level_prefix - 3)) - 4096;
            if (i == trailing_ones && trailing_ones < 3)
               level_code += 2;
            if (level_code & 1)
               level[i] = (-level_code - 1) >> 1;
            else
               level[i] = (level_code + 2) >> 1;
            if (suffix_length == 0)
               suffix_length = 1;
            if ((level[i] > (3 << (suffix_length - 1)) ||
                 level[i] < -(3 << (suffix_length - 1)))
                && suffix_length < 6)
               suffix_length++;
         }
      }
   }

   /* total_zeros. */
   {
      int zeros_left = 0;
      if (total_coeff < maxNumCoeff)
      {
         int idx;
         if (is_chroma_dc)
            idx = rh264_vlc_match1(b, rh264_cdctz_len[total_coeff],
                  rh264_cdctz_code[total_coeff], 4);
         else
            idx = rh264_vlc_match1(b, rh264_tz_len[total_coeff],
                  rh264_tz_code[total_coeff], 16);
         if (idx < 0) return -1;
         zeros_left = idx;
      }
      /* run_before. */
      for (i = 0; i < total_coeff - 1; i++)
      {
         int rbv = 0;
         if (zeros_left > 0)
         {
            int bucket = zeros_left > 6 ? 7 : zeros_left;
            int idx = rh264_vlc_match1(b, rh264_rb_len[bucket],
                  rh264_rb_code[bucket], 15);
            if (idx < 0) return -1;
            rbv = idx;
         }
         run[i] = rbv;
         zeros_left -= rbv;
      }
      run[total_coeff - 1] = zeros_left;
   }

   /* Place coefficients into scan positions (high-freq first). */
   {
      int coeff_num = -1;
      for (i = total_coeff - 1; i >= 0; i--)
      {
         coeff_num += run[i] + 1;
         if (coeff_num < maxNumCoeff)
            out[coeff_num] = level[i];
      }
   }
   return total_coeff;
}



/* ==================== rh264_intra.h ==================== */
/* rh264 -- intra prediction (H.264 clause 8.3), baseline modes. */
#define RH264_CLIP(v) ((v)<0?0:((v)>255?255:(v)))

static void rh264_intra16x16(uint8_t *dst,int stride,int mode,int have_up,int have_left){
   int x,y; const uint8_t *up=dst-stride;
   switch(mode){
   case 0: for(y=0;y<16;y++)for(x=0;x<16;x++)dst[y*stride+x]=up[x]; break;
   case 1: for(y=0;y<16;y++)for(x=0;x<16;x++)dst[y*stride+x]=dst[y*stride-1]; break;
   case 2:{int sum=0,cnt=0,dc;
      if(have_up){for(x=0;x<16;x++)sum+=up[x];cnt+=16;}
      if(have_left){for(y=0;y<16;y++)sum+=dst[y*stride-1];cnt+=16;}
      dc=(cnt==32)?(sum+16)>>5:(cnt==16)?(sum+8)>>4:128;
      for(y=0;y<16;y++)for(x=0;x<16;x++)dst[y*stride+x]=(uint8_t)dc;
      break;}
   default:{int H=0,V=0,a,bb,c,i,val; const uint8_t *tl=dst-stride-1;
      for(i=0;i<7;i++)H+=(i+1)*(up[8+i]-up[6-i]);
      H+=8*(up[15]-tl[0]);
      for(i=0;i<7;i++)V+=(i+1)*(dst[(8+i)*stride-1]-dst[(6-i)*stride-1]);
      V+=8*(dst[15*stride-1]-tl[0]);
      a=16*(up[15]+dst[15*stride-1]); bb=(5*H+32)>>6; c=(5*V+32)>>6;
      for(y=0;y<16;y++)for(x=0;x<16;x++){val=(a+bb*(x-7)+c*(y-7)+16)>>5;dst[y*stride+x]=(uint8_t)RH264_CLIP(val);}
      break;}
   }
}
static void rh264_intra_chroma(uint8_t *dst,int stride,int mode,int have_up,int have_left){
   int x,y; const uint8_t *up=dst-stride;
   switch(mode){
   case 1: for(y=0;y<8;y++)for(x=0;x<8;x++)dst[y*stride+x]=dst[y*stride-1]; break;
   case 2: for(y=0;y<8;y++)for(x=0;x<8;x++)dst[y*stride+x]=up[x]; break;
   case 3:{int H=0,V=0,a,bb,c,i,val; const uint8_t *tl=dst-stride-1;
      for(i=0;i<3;i++)H+=(i+1)*(up[4+i]-up[2-i]);
      H+=4*(up[7]-tl[0]);
      for(i=0;i<3;i++)V+=(i+1)*(dst[(4+i)*stride-1]-dst[(2-i)*stride-1]);
      V+=4*(dst[7*stride-1]-tl[0]);
      a=16*(up[7]+dst[7*stride-1]); bb=(17*H+16)>>5; c=(17*V+16)>>5;
      for(y=0;y<8;y++)for(x=0;x<8;x++){val=(a+bb*(x-3)+c*(y-3)+16)>>5;dst[y*stride+x]=(uint8_t)RH264_CLIP(val);}
      break;}
   default:{int bx,by;
      for(by=0;by<2;by++)for(bx=0;bx<2;bx++){
         int sum=0,cnt=0,dc,i,ux=bx*4,uy=by*4,use_up=have_up,use_left=have_left;
         if(bx==1&&by==0){use_left=use_up?0:have_left;}
         else if(bx==0&&by==1){use_up=have_left?0:have_up;}
         if(use_up){for(i=0;i<4;i++)sum+=up[ux+i];cnt+=4;}
         if(use_left){for(i=0;i<4;i++)sum+=dst[(uy+i)*stride-1];cnt+=4;}
         dc=(cnt==8)?(sum+4)>>3:(cnt==4)?(sum+2)>>2:128;
         for(y=0;y<4;y++)for(x=0;x<4;x++)dst[(uy+y)*stride+ux+x]=(uint8_t)dc;
      } break;}
   }
}
/* Intra_4x4 (8.3.1.2). p[] samples: p[-1,-1]=C, p[x,-1]=T[0..7] (top+topright),
 * p[-1,y]=L[0..3]. avail flags gate which samples are valid. */
static void rh264_intra4x4(uint8_t *dst,int stride,int mode,int have_up,int have_left,int have_up_right){
   const uint8_t *up=dst-stride; uint8_t L[4],C,T[8]; int x,y;
   for(y=0;y<4;y++)L[y]=have_left?dst[y*stride-1]:0;
   C=(have_up&&have_left)?dst[-stride-1]:0;
   for(x=0;x<4;x++)T[x]=have_up?up[x]:0;
   for(x=4;x<8;x++)T[x]=have_up_right?up[x]:(have_up?T[3]:0);
   #define PT(k) ((k)==-1?C:T[k])
   #define PL(k) ((k)==-1?C:L[k])
   #define P(xx,yy) dst[(yy)*stride+(xx)]
   for(y=0;y<4;y++)for(x=0;x<4;x++){
      int v=0;
      switch(mode){
      case 0: v=PT(x); break;
      case 1: v=PL(y); break;
      case 2: { int s=0,cnt=0; if(have_up){s+=T[0]+T[1]+T[2]+T[3];cnt+=4;}
               if(have_left){s+=L[0]+L[1]+L[2]+L[3];cnt+=4;}
               v=(cnt==8)?(s+4)>>3:(cnt==4)?(s+2)>>2:128; } break;
      case 3: { int i=x+y; v=(i==6)?((T[6]+3*T[7]+2)>>2):((T[i]+2*T[i+1]+T[i+2]+2)>>2); } break;
      case 4:
         if(x>y) v=(PT(x-y-2)+2*PT(x-y-1)+PT(x-y)+2)>>2;
         else if(x<y) v=(PL(y-x-2)+2*PL(y-x-1)+PL(y-x)+2)>>2;
         else v=(PT(0)+2*C+PL(0)+2)>>2;
         break;
      case 5: { int z=2*x-y;
         if(z>=0){ if((z&1)==0) v=(PT(x-(y>>1)-1)+PT(x-(y>>1))+1)>>1;
                   else v=(PT(x-(y>>1)-2)+2*PT(x-(y>>1)-1)+PT(x-(y>>1))+2)>>2; }
         else if(z==-1) v=(PL(0)+2*C+PT(0)+2)>>2;
         else v=(PL(y-1)+2*PL(y-2)+PL(y-3)+2)>>2; } break;
      case 6: { int z=2*y-x;
         if(z>=0){ if((z&1)==0) v=(PL(y-(x>>1)-1)+PL(y-(x>>1))+1)>>1;
                   else v=(PL(y-(x>>1)-2)+2*PL(y-(x>>1)-1)+PL(y-(x>>1))+2)>>2; }
         else if(z==-1) v=(PT(0)+2*C+PL(0)+2)>>2;
         else v=(PT(x-1)+2*PT(x-2)+PT(x-3)+2)>>2; } break;
      case 7:
         if((y&1)==0) v=(PT(x+(y>>1))+PT(x+(y>>1)+1)+1)>>1;
         else v=(PT(x+(y>>1))+2*PT(x+(y>>1)+1)+PT(x+(y>>1)+2)+2)>>2;
         break;
      default: { int z=x+2*y;
         if(z<5){ if((z&1)==0) v=(PL(y+(x>>1))+PL(y+(x>>1)+1)+1)>>1;
                  else v=(PL(y+(x>>1))+2*PL(y+(x>>1)+1)+PL(y+(x>>1)+2)+2)>>2; }
         else if(z==5) v=(PL(2)+3*PL(3)+2)>>2;
         else v=PL(3); } break;
      }
      P(x,y)=(uint8_t)v;
   }
   #undef PT
   #undef PL
   #undef P
}



/* ==================== rh264_mb.h ==================== */
/* rh264 -- macroblock layer + reconstruction (I slices, baseline). */

typedef struct {
   int w,h,mbw,mbh,ystride,cstride;
   uint8_t *Y,*U,*V;
   int qp;
   int chroma_qp_offset;
   uint8_t *i4mode;   /* per-4x4-block intra mode, raster mbw*4 x mbh*4 */
   uint8_t *nzL;      /* per-4x4 luma nonzero count, same grid          */
   uint8_t *nzC[2];   /* per-4x4 chroma nonzero (mbw*2 x mbh*2)         */
   uint8_t *mbqp;     /* per-MB luma QP (mbw x mbh)                     */
} rh264_frame;

/* 4x4 luma block scan order within an MB (raster of the 4x4 blocks in the
 * standard zig-zag/Z order used by CAVLC block indexing, 8.4.x). */
static const uint8_t rh264_blk_x[16]={0,1,0,1, 2,3,2,3, 0,1,0,1, 2,3,2,3};
static const uint8_t rh264_blk_y[16]={0,0,1,1, 0,0,1,1, 2,2,3,3, 2,2,3,3};

/* CBP mapping for Intra: codeNum -> cbp via Table 9-4 (intra column). */
static const uint8_t rh264_cbp_intra[48]={
   47,31,15,0,23,27,29,30,7,11,13,14,39,43,45,46,16,3,5,10,12,19,21,26,28,35,
   37,42,44,1,2,4,8,17,18,20,24,6,9,22,25,32,33,34,36,40,38,41};

/* nC from left & top 4x4 neighbour nonzero counts (9.2.1). gx,gy are the
 * 4x4-block grid coords. */
static int rh264_nC(const uint8_t *nz,int gw,int gh,int gx,int gy){
   int have_l=gx>0, have_t=gy>0, nA=0,nB=0,n=0,cnt=0;
   (void)gh;
   if(have_l){nA=nz[gy*gw+(gx-1)];cnt++;}
   if(have_t){nB=nz[(gy-1)*gw+gx];cnt++;}
   if(cnt==2)n=(nA+nB+1)>>1; else if(cnt==1)n=have_l?nA:nB; else n=0;
   return n;
}

/* Chroma QP derivation (8.5.8): map qPI -> QPc. */
static int rh264_chroma_qp(int qpy, int offset)
{
   static const int m[22]={29,30,31,32,32,33,34,34,35,35,36,36,37,37,37,38,38,38,39,39,39,39};
   int qpi = qpy + offset;
   if (qpi < 0) qpi = 0; else if (qpi > 51) qpi = 51;
   if (qpi < 30) return qpi;
   return m[qpi-30];
}

/* chroma DC 2x2 inverse Hadamard (8.5.11.1). */
static void rh264_chroma_dc_idct(int32_t *c)
{
   int32_t a=c[0],b=c[1],cc=c[2],d=c[3];
   c[0]=a+b+cc+d; c[1]=a-b+cc-d; c[2]=a+b-cc-d; c[3]=a-b-cc+d;
}

/* Decode chroma residual for one MB (4:2:0). cbp_chroma: 1=DC only,
 * 2=DC+AC. Returns 0 on success. */
static int rh264_decode_chroma_residual(rh264_bits *b, rh264_frame *f,
      int mbx, int mby, uint8_t *u, uint8_t *v, int cbp_chroma)
{
   uint8_t *planes[2]; int comp;
   int32_t cdc[2][4];
   planes[0]=u; planes[1]=v;
   /* chroma DC blocks (both components), nC == -1 */
   for (comp=0; comp<2; comp++)
   {
      int32_t scan[4]; int k;
      for (k=0;k<4;k++) cdc[comp][k]=0;
      {
         int tc=rh264_residual_block(b,-1,4,scan);
         if (tc<0) return -1;
         for (k=0;k<4;k++) cdc[comp][k]=scan[k];
      }
      rh264_chroma_dc_idct(cdc[comp]);
      /* scale (8.5.11.2): dcC = ((f * LevelScale[qpc%6][0]) << (qpc/6)) >> 5 */
      {
         int qpc=rh264_chroma_qp(f->qp, f->chroma_qp_offset);
         int per=qpc/6, rem=qpc%6, LS=16*rh264_dequant4_v[rem][0];
         for (k=0;k<4;k++)
            cdc[comp][k]=((cdc[comp][k]*LS)<<per)>>5;
      }
   }
   /* chroma AC blocks (only if cbp_chroma==2) + reconstruct */
   for (comp=0; comp<2; comp++)
   {
      uint8_t *p=planes[comp]; int blk;
      int cgw=f->mbw*2;
      for (blk=0; blk<4; blk++)
      {
         int bx=blk&1, by=blk>>1, k;
         int cgx=mbx*2+bx, cgy=mby*2+by;
         int32_t ac[16],r[16];
         int nzc=0;
         for (k=0;k<16;k++) ac[k]=0;
         if (cbp_chroma==2)
         {
            /* chroma AC nC from left/top chroma-block nonzero counts */
            int nA=0,nB=0,cnt=0,nC;
            if (cgx>0){ nA=f->nzC[comp][cgy*cgw+(cgx-1)]; cnt++; }
            if (cgy>0){ nB=f->nzC[comp][(cgy-1)*cgw+cgx]; cnt++; }
            nC=(cnt==2)?((nA+nB+1)>>1):(cnt==1?(cgx>0?nA:nB):0);
            {
               int32_t scan[16];
               int tc=rh264_residual_block(b,nC,15,scan);
               if (tc<0) return -1;
               nzc=tc;
               for (k=0;k<15;k++) ac[rh264_zigzag4[k+1]]=scan[k];
            }
         }
         f->nzC[comp][cgy*cgw+cgx]=(uint8_t)nzc;
         ac[0]=cdc[comp][blk];
         rh264_dequant4x4(ac,rh264_chroma_qp(f->qp,f->chroma_qp_offset),1);
         rh264_itransform4x4(ac,r);
         {
            uint8_t *d=p+by*4*f->cstride+bx*4; int xx,yy,val;
            for (yy=0;yy<4;yy++) for(xx=0;xx<4;xx++)
            { val=d[yy*f->cstride+xx]+((r[yy*4+xx]+32)>>6);
              d[yy*f->cstride+xx]=(uint8_t)RH264_CLIP(val); }
         }
      }
   }
   (void)mbx; (void)mby;
   return 0;
}

#ifndef RH264_ABS
#define RH264_ABS(x) ((x)<0?-(x):(x))
#endif
static const int rh264_alpha[52]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,5,6,7,8,9,10,12,13,15,17,20,22,25,28,32,36,40,45,50,56,63,71,80,90,101,113,127,144,162,182,203,226,255,255};
static const int rh264_beta[52]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,3,3,3,3,4,4,4,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,17,17,18,18};
static const int rh264_tc0[3][52]={
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,3,3,3,4,4,4,5,6,6,7,8},
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,2,2,2,2,3,3,3,4,4,5,6,6,7,8,8,10,11,12,13},
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,2,2,2,3,3,3,4,4,4,5,6,6,7,8,9,10,11,13,14,16,18,20,23,25,27},
};

/* Filter one luma sample line across an edge (p3..p0 | q0..q3), stride s.
 * bS in 1..4. a=alpha, be=beta, tc0v from table (unused for bS==4). */
static void rh264_filter_luma_edge(uint8_t *e,int s,int bS,int a,int be,int tc0v)
{
   int p0=e[-1*s],p1=e[-2*s],p2=e[-3*s],p3=e[-4*s];
   int q0=e[0],q1=e[1*s],q2=e[2*s],q3=e[3*s];
   if(RH264_ABS(p0-q0)>=a||RH264_ABS(p1-p0)>=be||RH264_ABS(q1-q0)>=be) return;
   if(bS<4){
      int tc=tc0v, ap=RH264_ABS(p2-p0), aq=RH264_ABS(q2-q0), d;
      if(ap<be) tc++;
      if(aq<be) tc++;
      d=(((q0-p0)<<2)+(p1-q1)+4)>>3;
      if(d<-tc)d=-tc; else if(d>tc)d=tc;
      e[-1*s]=(uint8_t)RH264_CLIP(p0+d);
      e[0]   =(uint8_t)RH264_CLIP(q0-d);
      if(ap<be){ int dp=(p2+((p0+q0+1)>>1)-2*p1)>>1; if(dp<-tc0v)dp=-tc0v;else if(dp>tc0v)dp=tc0v; e[-2*s]=(uint8_t)(p1+dp); }
      if(aq<be){ int dq=(q2+((p0+q0+1)>>1)-2*q1)>>1; if(dq<-tc0v)dq=-tc0v;else if(dq>tc0v)dq=tc0v; e[1*s]=(uint8_t)(q1+dq); }
   } else {
      int ap=RH264_ABS(p2-p0), aq=RH264_ABS(q2-q0);
      if(RH264_ABS(p0-q0)<((a>>2)+2)){
         if(ap<be){
            e[-1*s]=(uint8_t)((p2+2*p1+2*p0+2*q0+q1+4)>>3);
            e[-2*s]=(uint8_t)((p2+p1+p0+q0+2)>>2);
            e[-3*s]=(uint8_t)((2*p3+3*p2+p1+p0+q0+4)>>3);
         } else e[-1*s]=(uint8_t)((2*p1+p0+q1+2)>>2);
         if(aq<be){
            e[0]   =(uint8_t)((q2+2*q1+2*q0+2*p0+p1+4)>>3);
            e[1*s] =(uint8_t)((q2+q1+q0+p0+2)>>2);
            e[2*s] =(uint8_t)((2*q3+3*q2+q1+q0+p0+4)>>3);
         } else e[0]=(uint8_t)((2*q1+q0+p1+2)>>2);
      } else {
         e[-1*s]=(uint8_t)((2*p1+p0+q1+2)>>2);
         e[0]   =(uint8_t)((2*q1+q0+p1+2)>>2);
      }
   }
}

/* Filter one chroma sample across an edge. */
static void rh264_filter_chroma_edge(uint8_t *e,int s,int bS,int a,int be,int tc0v)
{
   int p1=e[-2*s],p0=e[-1*s],q0=e[0],q1=e[1*s];
   if(RH264_ABS(p0-q0)>=a||RH264_ABS(p1-p0)>=be||RH264_ABS(q1-q0)>=be) return;
   if(bS<4){
      int tc=tc0v+1, d=(((q0-p0)<<2)+(p1-q1)+4)>>3;
      if(d<-tc)d=-tc; else if(d>tc)d=tc;
      e[-1*s]=(uint8_t)RH264_CLIP(p0+d);
      e[0]   =(uint8_t)RH264_CLIP(q0-d);
   } else {
      e[-1*s]=(uint8_t)((2*p1+p0+q1+2)>>2);
      e[0]   =(uint8_t)((2*q1+q0+p1+2)>>2);
   }
}

/* In-loop deblocking (8.7) for an all-intra frame. */
static void rh264_deblock(rh264_frame *f, const rh264_slice_hdr *sh)
{
   int mbx,mby,edge,i;
   int oA=sh->slice_alpha_c0_offset, oB=sh->slice_beta_offset;
   if(sh->disable_deblocking_filter_idc==1) return;
   for(mby=0;mby<f->mbh;mby++)
   for(mbx=0;mbx<f->mbw;mbx++)
   {
      int qp=f->mbqp?f->mbqp[mby*f->mbw+mbx]:f->qp;
      int qpc=rh264_chroma_qp(qp,f->chroma_qp_offset);
      /* ---- vertical edges (filter columns), left to right ---- */
      for(edge=0;edge<4;edge++)
      {
         int x=mbx*16+edge*4;
         int bS,qpavg,a,be,idxA,idxB,t;
         if(edge==0){ if(mbx==0) continue; bS=4;
            qpavg=(qp+(f->mbqp?f->mbqp[mby*f->mbw+mbx-1]:qp)+1)>>1; }
         else { bS=3; qpavg=qp; }
         idxA=qpavg+oA; if(idxA<0)idxA=0; else if(idxA>51)idxA=51;
         idxB=qpavg+oB; if(idxB<0)idxB=0; else if(idxB>51)idxB=51;
         a=rh264_alpha[idxA]; be=rh264_beta[idxB]; t=rh264_tc0[bS==4?2:bS-1][idxA];
         for(i=0;i<16;i++){ uint8_t *e=f->Y+(mby*16+i)*f->ystride+x; rh264_filter_luma_edge(e,1,bS,a,be,t); }
         /* chroma: only on even edges (0 and 8 luma -> chroma 0,4) */
         if((edge&1)==0){
            int cx=mbx*8+(edge>>1)*4, ci;
            int cqpavg = (edge==0)? ((qpc+rh264_chroma_qp(f->mbqp?f->mbqp[mby*f->mbw+mbx-1]:qp,f->chroma_qp_offset)+1)>>1) : qpc;
            int cA=cqpavg+oA,cB=cqpavg+oB,ca,cbe,ct;
            if(cA<0)cA=0;else if(cA>51)cA=51; if(cB<0)cB=0;else if(cB>51)cB=51;
            ca=rh264_alpha[cA];cbe=rh264_beta[cB];ct=rh264_tc0[bS==4?2:bS-1][cA];
            for(ci=0;ci<8;ci++){ rh264_filter_chroma_edge(f->U+(mby*8+ci)*f->cstride+cx,1,bS,ca,cbe,ct);
                                 rh264_filter_chroma_edge(f->V+(mby*8+ci)*f->cstride+cx,1,bS,ca,cbe,ct); }
         }
      }
      /* ---- horizontal edges (filter rows), top to bottom ---- */
      for(edge=0;edge<4;edge++)
      {
         int y=mby*16+edge*4;
         int bS,qpavg,a,be,t,idxA,idxB;
         if(edge==0){ if(mby==0) continue; bS=4;
            qpavg=(qp+(f->mbqp?f->mbqp[(mby-1)*f->mbw+mbx]:qp)+1)>>1; }
         else { bS=3; qpavg=qp; }
         idxA=qpavg+oA; if(idxA<0)idxA=0; else if(idxA>51)idxA=51;
         idxB=qpavg+oB; if(idxB<0)idxB=0; else if(idxB>51)idxB=51;
         a=rh264_alpha[idxA]; be=rh264_beta[idxB]; t=rh264_tc0[bS==4?2:bS-1][idxA];
         for(i=0;i<16;i++){ uint8_t *e=f->Y+y*f->ystride+(mbx*16+i); rh264_filter_luma_edge(e,f->ystride,bS,a,be,t); }
         if((edge&1)==0){
            int cy=mby*8+(edge>>1)*4, ci;
            int cqpavg = (edge==0)? ((qpc+rh264_chroma_qp(f->mbqp?f->mbqp[(mby-1)*f->mbw+mbx]:qp,f->chroma_qp_offset)+1)>>1) : qpc;
            int cA=cqpavg+oA,cB=cqpavg+oB,ca,cbe,ct;
            if(cA<0)cA=0;else if(cA>51)cA=51; if(cB<0)cB=0;else if(cB>51)cB=51;
            ca=rh264_alpha[cA];cbe=rh264_beta[cB];ct=rh264_tc0[bS==4?2:bS-1][cA];
            for(ci=0;ci<8;ci++){ rh264_filter_chroma_edge(f->U+cy*f->cstride+(mbx*8+ci),f->cstride,bS,ca,cbe,ct);
                                 rh264_filter_chroma_edge(f->V+cy*f->cstride+(mbx*8+ci),f->cstride,bS,ca,cbe,ct); }
         }
      }
   }
}

static int rh264_decode_islice(rh264_bits *b,const rh264_sps *sps,
      const rh264_pps *pps,rh264_slice_hdr *sh,rh264_frame *f){
   int mbaddr=sh->first_mb_in_slice, total=f->mbw*f->mbh;
   int gw=f->mbw*4;        /* luma 4x4 grid width */
   int cgw=f->mbw*2;       /* chroma 4x4 grid width */
   f->qp=sh->slice_qp;
   f->chroma_qp_offset=pps->chroma_qp_index_offset;
   (void)pps;
   while(mbaddr<total){
      int mbx=mbaddr%f->mbw, mby=mbaddr/f->mbw;
      int mb_type=rh264_ue(b);
      int have_up=(mby>0), have_left=(mbx>0);
      uint8_t *y=f->Y+(mby*16)*f->ystride+mbx*16;
      uint8_t *u=f->U+(mby*8)*f->cstride+mbx*8;
      uint8_t *v=f->V+(mby*8)*f->cstride+mbx*8;

      if(mb_type==0){
         /* I_4x4 */
         int modes[16], i, chroma_mode, cbp, cbp_luma, cbp_chroma;
         for(i=0;i<16;i++){
            int bx=rh264_blk_x[i], by=rh264_blk_y[i];
            int gx=mbx*4+bx, gy=mby*4+by;
            int predm;
            int prev=rh264_u1(b);
            /* most-probable mode = min of left & top block modes (8.3.1.1) */
            int la= (gx>0)? f->i4mode[gy*gw+(gx-1)] : -1;
            int ta= (gy>0)? f->i4mode[(gy-1)*gw+gx] : -1;
            int mpm;
            if(la<0||ta<0) mpm=2; else mpm=(la<ta?la:ta);
            if(prev) predm=mpm;
            else { int rem=rh264_un(b,3); predm=(rem<mpm)?rem:rem+1; }
            modes[i]=predm; f->i4mode[gy*gw+gx]=(uint8_t)predm;
         }
         chroma_mode=rh264_ue(b);
         cbp=rh264_ue(b); if(cbp>=48)return -3; cbp=rh264_cbp_intra[cbp];
         cbp_luma=cbp&15; cbp_chroma=cbp>>4;
         if(cbp_luma||cbp_chroma){ int d=rh264_se(b); f->qp=(f->qp+d+52)%52; }

         /* per-4x4 luma: predict then (if cbp bit) residual+reconstruct */
         for(i=0;i<16;i++){
            int bx=rh264_blk_x[i], by=rh264_blk_y[i];
            int gx=mbx*4+bx, gy=mby*4+by;
            uint8_t *d=y+by*4*f->ystride+bx*4;
            int hu=(by>0)||have_up, hl=(bx>0)||have_left;
            int hur;
            /* Top-right (up-right) 4x4 availability per H.264 block scan.
             * Within-MB blocks whose TR block is decoded later are
             * unavailable; MB-edge blocks depend on the top / top-right MB. */
            switch(i){
               /* within-MB, TR decoded earlier -> available */
               case 2: case 6: case 8: case 9: case 10: case 12: case 14:
                  hur=1; break;
               /* within-MB, TR decoded later -> unavailable */
               case 3: case 11: case 13: case 15:
                  hur=0; break;
               /* top edge of MB (by==0): TR from top / top-right MB */
               case 0: case 1: case 4:
                  hur=have_up; break;
               case 5:
                  /* blk5 top-right leaves the MB to the top-right MB */
                  hur=have_up && (mbx+1 < f->mbw); break;
               case 7:
                  /* blk7 TR is to the right MB's top -> unavailable (right
                   * MB not yet decoded in this row) */
                  hur=0; break;
               default: hur=0; break;
            }
            rh264_intra4x4(d,f->ystride,modes[i],hu,hl,hur);
            {
               int32_t coef[16],r[16]; int k,nzc=0; for(k=0;k<16;k++)coef[k]=0;
               if(cbp_luma&(1<<(i>>2))){
                  int nC=rh264_nC(f->nzL,gw,f->mbh*4,gx,gy);
                  int32_t scan[16];
                  int tc=rh264_residual_block(b,nC,16,scan);
                  if(tc<0)return -1;
                  nzc=tc;
                  for(k=0;k<16;k++)coef[rh264_zigzag4[k]]=scan[k];
                  rh264_dequant4x4(coef,f->qp,0);
                  rh264_itransform4x4(coef,r);
                  {int xx,yy,val;for(yy=0;yy<4;yy++)for(xx=0;xx<4;xx++){
                     val=d[yy*f->ystride+xx]+((r[yy*4+xx]+32)>>6);
                     d[yy*f->ystride+xx]=(uint8_t)RH264_CLIP(val);}}
               }
               f->nzL[gy*gw+gx]=(uint8_t)nzc;
            }
         }
         /* chroma predict */
         rh264_intra_chroma(u,f->cstride,chroma_mode,have_up,have_left);
         rh264_intra_chroma(v,f->cstride,chroma_mode,have_up,have_left);
         if(cbp_chroma) {
            if(rh264_decode_chroma_residual(b,f,mbx,mby,u,v,cbp_chroma)<0)
               return -1;
         }
         /* mark chroma nz zero when no chroma residual */
         if(!cbp_chroma){ int cx,cy; for(cy=0;cy<2;cy++)for(cx=0;cx<2;cx++){
            f->nzC[0][(mby*2+cy)*cgw+mbx*2+cx]=0;
            f->nzC[1][(mby*2+cy)*cgw+mbx*2+cx]=0; } }
      }
      else if(mb_type>=1&&mb_type<=24){
         int m=mb_type-1, pred=m%4, cbp_chroma=(m/4)%3, cbp_luma=(m>=12)?15:0;
         int32_t dc[16],tmp[16]; int i,bx,by,k;
         int gx0=mbx*4, gy0=mby*4;
         int chroma_mode;
         rh264_intra16x16(y,f->ystride,pred,have_up,have_left);
         chroma_mode=rh264_ue(b);
         { int d=rh264_se(b); f->qp=(f->qp+d+52)%52; }
         /* luma DC (Hadamard) */
         {  int nC=rh264_nC(f->nzL,gw,f->mbh*4,gx0,gy0);
            int32_t scan[16]; int tc=rh264_residual_block(b,nC,16,scan);
            if(tc<0)return -1;
            for(i=0;i<16;i++)dc[rh264_zigzag4[i]]=scan[i];
            rh264_ihadamard4x4(dc,tmp);
            /* I_16x16 luma DC scaling (8.5.10): LevelScale = 16*normAdjust
             * (flat weightScale matrix); shift per qP. */
            { int per=f->qp/6,rem=f->qp%6,LS=16*rh264_dequant4_v[rem][0];
              for(i=0;i<16;i++){ if(f->qp>=36)dc[i]=(tmp[i]*LS)<<(per-6);
                 else dc[i]=(tmp[i]*LS+(1<<(5-per)))>>(6-per); } }
         }
         /* luma AC per 4x4 in block scan order */
         for(i=0;i<16;i++){
            int bxx=rh264_blk_x[i], byy=rh264_blk_y[i];
            int gx=gx0+bxx, gy=gy0+byy;
            int32_t ac[16],r[16]; int nzc=0;
            for(k=0;k<16;k++)ac[k]=0;
            if(cbp_luma){
               int nC=rh264_nC(f->nzL,gw,f->mbh*4,gx,gy);
               int32_t scan[16]; int tc=rh264_residual_block(b,nC,15,scan);
               if(tc<0)return -1;
                  nzc=tc;
               for(k=0;k<15;k++)ac[rh264_zigzag4[k+1]]=scan[k];
            }
            ac[0]=dc[(byy*4+bxx)]; /* raster DC index within 4x4 grid */
            rh264_dequant4x4(ac,f->qp,1);
            rh264_itransform4x4(ac,r);
            { uint8_t *bd=y+byy*4*f->ystride+bxx*4; int xx,yy,val;
              for(yy=0;yy<4;yy++)for(xx=0;xx<4;xx++){
                 val=bd[yy*f->ystride+xx]+((r[yy*4+xx]+32)>>6);
                 bd[yy*f->ystride+xx]=(uint8_t)RH264_CLIP(val);} }
            f->nzL[gy*gw+gx]=(uint8_t)nzc;
            f->i4mode[gy*gw+gx]=2;  /* I_16x16 -> DC for neighbour MPM (8.3.1.1) */
         }
         (void)bx;(void)by;
         rh264_intra_chroma(u,f->cstride,chroma_mode,have_up,have_left);
         rh264_intra_chroma(v,f->cstride,chroma_mode,have_up,have_left);
         if(cbp_chroma){ if(rh264_decode_chroma_residual(b,f,mbx,mby,u,v,cbp_chroma)<0)return -1; }
      }
      else return -2;
      if(f->mbqp) f->mbqp[mby*f->mbw+mbx]=(uint8_t)f->qp;
      mbaddr++;
   }
   return 0;
}


/* ==================== rh264_video.h ==================== */
/* rh264_video -- persistent H.264 intra decoder wrapper (rvp8_video-style).
 * Included after rh264_mb.h; provides the public rh264_video_* API. */



struct rh264_video
{
   rh264_sps sps;
   rh264_pps pps;
   int       have_sps, have_pps;
   int       nal_length_size;   /* from avcC; 0 => Annex-B input */
   rh264_frame f;
   int        alloc_w, alloc_h; /* geometry the plane buffers were sized for */
};

/* ---- allocation helpers ---- */

static void rh264_frame_free(rh264_frame *f)
{
   free(f->Y);    free(f->U);      free(f->V);
   free(f->i4mode); free(f->nzL);
   free(f->nzC[0]); free(f->nzC[1]);
   free(f->mbqp);
   memset(f, 0, sizeof(*f));
}

static int rh264_frame_alloc(rh264_frame *f, const rh264_sps *sps)
{
   int mbw = sps->pic_width_in_mbs;
   int mbh = (sps->frame_height + 15) / 16;
   rh264_frame_free(f);
   f->w = sps->frame_width;  f->h = sps->frame_height;
   f->mbw = mbw;             f->mbh = mbh;
   f->ystride = mbw * 16;    f->cstride = mbw * 8;
   f->Y = (uint8_t*)calloc((size_t)f->ystride * mbh * 16, 1);
   f->U = (uint8_t*)calloc((size_t)f->cstride * mbh * 8, 1);
   f->V = (uint8_t*)calloc((size_t)f->cstride * mbh * 8, 1);
   f->i4mode = (uint8_t*)calloc((size_t)mbw * 4 * mbh * 4, 1);
   f->nzL    = (uint8_t*)calloc((size_t)mbw * 4 * mbh * 4, 1);
   f->nzC[0] = (uint8_t*)calloc((size_t)mbw * 2 * mbh * 2, 1);
   f->nzC[1] = (uint8_t*)calloc((size_t)mbw * 2 * mbh * 2, 1);
   f->mbqp   = (uint8_t*)calloc((size_t)mbw * mbh, 1);
   if (!f->Y || !f->U || !f->V || !f->i4mode || !f->nzL
         || !f->nzC[0] || !f->nzC[1] || !f->mbqp)
   {
      rh264_frame_free(f);
      return -1;
   }
   return 0;
}

static void rh264_frame_reset(rh264_frame *f)
{
   int mbw = f->mbw, mbh = f->mbh;
   memset(f->Y, 0, (size_t)f->ystride * mbh * 16);
   memset(f->U, 0, (size_t)f->cstride * mbh * 8);
   memset(f->V, 0, (size_t)f->cstride * mbh * 8);
   memset(f->i4mode, 0xff, (size_t)mbw * 4 * mbh * 4);
   memset(f->nzL, 0, (size_t)mbw * 4 * mbh * 4);
   memset(f->nzC[0], 0, (size_t)mbw * 2 * mbh * 2);
   memset(f->nzC[1], 0, (size_t)mbw * 2 * mbh * 2);
   memset(f->mbqp, 0, (size_t)mbw * mbh);
}

/* (Re)allocate the frame buffers if the SPS geometry changed. */
static int rh264_frame_alloc_if_needed(rh264_video *v)
{
   if (!v->have_sps) return -1;
   if (v->f.Y && v->alloc_w == v->sps.frame_width
              && v->alloc_h == v->sps.frame_height)
      return 0;
   if (rh264_frame_alloc(&v->f, &v->sps) != 0) return -1;
   v->alloc_w = v->sps.frame_width;
   v->alloc_h = v->sps.frame_height;
   return 0;
}

/* ---- public API ---- */

rh264_video *rh264_video_open(void)
{
   rh264_video *v = (rh264_video*)calloc(1, sizeof(*v));
   if (!v) return NULL;
   v->nal_length_size = 4;
   return v;
}

void rh264_video_close(rh264_video *v)
{
   if (!v) return;
   rh264_frame_free(&v->f);
   free(v);
}

/* parse one SPS or PPS NAL (RBSP already unescaped elsewhere). */
static void rh264_video_take_ps(rh264_video *v, const uint8_t *nal, size_t len)
{
   int type;
   size_t rl;
   uint8_t *rbsp;
   if (len < 1) return;
   type = nal[0] & 0x1f;
   rbsp = rh264_unescape(nal + 1, len - 1, &rl);
   if (!rbsp) return;
   if (type == 7) { if (rh264_parse_sps(rbsp, rl, &v->sps)) v->have_sps = 1; }
   else if (type == 8) { if (rh264_parse_pps(rbsp, rl, &v->pps)) v->have_pps = 1; }
   free(rbsp);
}

int rh264_video_set_extradata(rh264_video *v, const uint8_t *avcc, size_t len)
{
   size_t p = 0;
   int i, n;
   if (!v || !avcc || len < 7) return -1;
   /* AVCDecoderConfigurationRecord:
    * [0]=1 [1]=profile [2]=compat [3]=level
    * [4]=0xFC|(lengthSizeMinusOne) [5]=0xE0|numSPS ... */
   v->nal_length_size = (avcc[4] & 0x03) + 1;
   n = avcc[5] & 0x1f;
   p = 6;
   for (i = 0; i < n; i++)
   {
      size_t sl;
      if (p + 2 > len) return -1;
      sl = ((size_t)avcc[p] << 8) | avcc[p+1]; p += 2;
      if (p + sl > len) return -1;
      rh264_video_take_ps(v, avcc + p, sl);
      p += sl;
   }
   if (p + 1 > len) return -1;
   n = avcc[p++]; /* numPPS */
   for (i = 0; i < n; i++)
   {
      size_t sl;
      if (p + 2 > len) return -1;
      sl = ((size_t)avcc[p] << 8) | avcc[p+1]; p += 2;
      if (p + sl > len) return -1;
      rh264_video_take_ps(v, avcc + p, sl);
      p += sl;
   }
   return (v->have_sps && v->have_pps) ? 0 : -1;
}

/* Decode a single IDR NAL into v->f (already sized). Returns 0 on success. */
static int rh264_video_decode_idr(rh264_video *v, const uint8_t *nal, size_t len)
{
   int nut, nri, rc;
   size_t rl;
   uint8_t *rbsp;
   rh264_bits b;
   rh264_slice_hdr sh;
   if (len < 1) return -1;
   nut = nal[0] & 0x1f;
   nri = (nal[0] >> 5) & 3;
   rbsp = rh264_unescape(nal + 1, len - 1, &rl);
   if (!rbsp) return -1;
   rh264_bits_init(&b, rbsp, rl);
   if (!rh264_parse_slice_header_adv(&b, nut, nri, &v->sps, &v->pps, &sh))
   { free(rbsp); return -1; }
   rh264_frame_reset(&v->f);
   rc = rh264_decode_islice(&b, &v->sps, &v->pps, &sh, &v->f);
   if (rc == 0) rh264_deblock(&v->f, &sh);
   free(rbsp);
   return rc;
}

/* Walk NAL units in either length-prefixed (AVCC) or Annex-B form and act on
 * SPS/PPS + the first IDR slice. */
int rh264_video_decode(rh264_video *v, const uint8_t *data, size_t len)
{
   size_t p = 0;
   int got_idr = 0;
   if (!v || !data) return -1;

   if (v->nal_length_size > 0 && len >= (size_t)v->nal_length_size
         && !(data[0] == 0 && data[1] == 0
              && (data[2] == 1 || (data[2] == 0 && data[3] == 1))))
   {
      /* length-prefixed AVCC */
      int ls = v->nal_length_size, i;
      while (p + ls <= len)
      {
         size_t nl = 0;
         for (i = 0; i < ls; i++) nl = (nl << 8) | data[p + i];
         p += ls;
         if (nl == 0 || p + nl > len) break;
         {
            const uint8_t *nal = data + p;
            int type = nal[0] & 0x1f;
            if (type == 7 || type == 8) rh264_video_take_ps(v, nal, nl);
            else if (type == 5)
            {
               if (!v->have_sps || !v->have_pps) return -1;
               if (rh264_frame_alloc_if_needed(v) != 0) return -1;
               if (rh264_video_decode_idr(v, nal, nl) != 0) return -1;
               got_idr = 1;
               break;   /* one coded picture per call */
            }
         }
         p += nl;
      }
   }
   else
   {
      /* Annex-B: scan for 00 00 01 start codes */
      while (p + 3 <= len)
      {
         size_t s, e;
         if (!(data[p] == 0 && data[p+1] == 0 && data[p+2] == 1))
         { p++; continue; }
         s = p + 3;
         e = s;
         while (e + 3 <= len
                && !(data[e] == 0 && data[e+1] == 0 && data[e+2] == 1)) e++;
         if (e + 3 > len) e = len;
         if (s < e)
         {
            const uint8_t *nal = data + s;
            int type = nal[0] & 0x1f;
            size_t nl = e - s;
            if (type == 7 || type == 8) rh264_video_take_ps(v, nal, nl);
            else if (type == 5)
            {
               if (!v->have_sps || !v->have_pps) return -1;
               if (rh264_frame_alloc_if_needed(v) != 0) return -1;
               if (rh264_video_decode_idr(v, nal, nl) != 0) return -1;
               got_idr = 1;
               break;   /* one coded picture per call */
            }
         }
         p = e;
      }
   }
   return got_idr ? 0 : -1;
}

const uint8_t *rh264_video_plane(const rh264_video *v, int plane,
      int *stride, int *width, int *height)
{
   const uint8_t *p;
   int st, w, h;
   if (!v) return NULL;
   if (plane == 0)      { st = v->f.ystride; w = v->f.w;         h = v->f.h;         p = v->f.Y; }
   else                 { st = v->f.cstride; w = (v->f.w+1)/2;   h = (v->f.h+1)/2;
                          p = (plane == 1) ? v->f.U : v->f.V; }
   if (stride) *stride = st;
   if (width)  *width  = w;
   if (height) *height = h;
   return p;
}
