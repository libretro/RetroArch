/* rh264 -- clean-room H.264 I/P decoder (amalgamated single TU).
 * Public API: include/formats/rh264.h. CAVLC tables extracted from libopenh264
 * encoder rodata (verified prefix-free). I and CAVLC P pictures. */
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
   num_ref_idx_l0_default,
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
   p->num_ref_idx_l0_default=rh264_ue(&b)+1; rh264_ue(&b); rh264_u1(&b); rh264_un(&b,2);
   p->pic_init_qp=rh264_se(&b)+26; rh264_se(&b); p->chroma_qp_index_offset=rh264_se(&b);
   p->deblocking_filter_control_present=rh264_u1(&b); p->constrained_intra_pred_flag=rh264_u1(&b);
   p->valid=1; return 1;
}


/* ==================== rh264_slice.h ==================== */
enum { RH264_SLICE_P=0,RH264_SLICE_B=1,RH264_SLICE_I=2,RH264_SLICE_SP=3,RH264_SLICE_SI=4 };
typedef struct { int first_mb_in_slice,slice_type,pic_parameter_set_id,frame_num,
   idr_pic_id,poc_lsb,slice_qp,disable_deblocking_filter_idc,is_idr,
   num_ref_idx_l0,
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
   sh->num_ref_idx_l0=1;
   if(sh->slice_type==RH264_SLICE_P||sh->slice_type==RH264_SLICE_SP){
      /* num_ref_idx_active_override_flag + ref_pic_list_modification (7.3.3). */
      sh->num_ref_idx_l0=pps->num_ref_idx_l0_default;
      if(sh->num_ref_idx_l0<1) sh->num_ref_idx_l0=1;
      if(rh264_u1(b)) sh->num_ref_idx_l0=rh264_ue(b)+1;
      if(rh264_u1(b)){ /* ref_pic_list_modification_flag_l0 */
         int op; do{ op=rh264_ue(b);
            if(op==0||op==1) rh264_ue(b);          /* abs_diff_pic_num_minus1 */
            else if(op==2) rh264_ue(b);            /* long_term_pic_num */
         } while(op!=3);
      }
   } else if(sh->slice_type!=RH264_SLICE_I&&sh->slice_type!=RH264_SLICE_SI)
      return 0; /* B and other slice types not yet supported */
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
   /* Vertical(0)/Horizontal(1)/Plane(3) require the corresponding neighbours;
    * fall back to DC(2) if unavailable rather than read out of bounds. */
   if((mode==0&&!have_up)||(mode==1&&!have_left)||(mode==3&&!(have_up&&have_left)))
      mode=2;
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
   /* Horizontal(1)/Vertical(2)/Plane(3) require the corresponding neighbours.
    * A conformant stream never signals them when unavailable, but arbitrary
    * (e.g. thumbnail) input might; fall back to DC rather than read out of
    * bounds. */
   if((mode==1&&!have_left)||(mode==2&&!have_up)||(mode==3&&!(have_up&&have_left)))
      mode=0;
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
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,2,2,2,2,3,3,3,4,4,4,5,6,6,7,8,9,10,11,13},
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,2,2,2,2,3,3,3,4,4,5,5,6,7,8,8,10,11,12,13,15,17},
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,2,2,2,2,3,3,3,4,4,4,5,6,6,7,8,9,10,11,13,14,16,18,20,23,25},
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

static int rh264_decode_intra_mb_cavlc(rh264_bits *b, rh264_frame *f,
      int mbx, int mby, int mb_type)
{
   int have_up=(mby>0), have_left=(mbx>0);
   int gw=f->mbw*4, cgw=f->mbw*2;
   uint8_t *y=f->Y+(mby*16)*f->ystride+mbx*16;
   uint8_t *u=f->U+(mby*8)*f->cstride+mbx*8;
   uint8_t *v=f->V+(mby*8)*f->cstride+mbx*8;
   (void)cgw;
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
            /* 0xff marks an inter-coded neighbour: per 8.3.1.1 (with
             * constrained_intra_pred_flag == 0) it contributes Intra_4x4 mode
             * 2 (DC) to the most-probable-mode derivation, whereas a neighbour
             * off the frame/slice edge is unavailable (-1 -> forces DC). */
            if(la==0xff) la=2;
            if(ta==0xff) ta=2;
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
   return 0;
}


/* ==================== rh264_inter.h ==================== */
/* Inter prediction: luma sub-pel interpolation (all 15 fractional
 * positions, 8.4.2.2.1) and chroma 1/8-pel bilinear (8.4.2.2.2). */
#ifndef RH264_INLINE
#define RH264_INLINE
#endif
/* rh264 inter prediction (motion compensation) for P/B slices, 4:2:0.
 * Luma uses the 6-tap half-pel filter (8.4.2.2.1) plus bilinear quarter-pel.
 * Chroma uses 1/8-pel bilinear (8.4.2.2.2). Reference samples are clamped to
 * the picture edge (unrestricted MV / edge replication). */

/* Clamp a source coordinate to [0, max-1] for edge extension. */
#define RH264_MC_CLAMP(v, max) ((v) < 0 ? 0 : ((v) >= (max) ? (max)-1 : (v)))

/* Fetch a luma sample from the reference with edge clamping. */
static RH264_INLINE int rh264_ref_luma(const uint8_t *ref, int stride,
      int w, int h, int x, int y)
{
   x = RH264_MC_CLAMP(x, w);
   y = RH264_MC_CLAMP(y, h);
   return ref[y * stride + x];
}

/* 6-tap filter kernel: (E - 5F + 20G + 20H - 5I + J). */
static RH264_INLINE int rh264_tap6(int a, int b, int c, int d, int e, int f)
{
   return a - 5 * b + 20 * c + 20 * d - 5 * e + f;
}

/* Luma motion compensation for a bw x bh block at integer position (bx,by)
 * within the destination, with a quarter-pel MV (mvx,mvy) in 1/4 units.
 * Writes bw x bh predicted samples to dst (dstride). ref is the full ref
 * plane (rw x rh, stride rstride). (ox,oy) is the block's integer luma
 * position in the picture. */
static void rh264_mc_luma(uint8_t *dst, int dstride,
      const uint8_t *ref, int rstride, int rw, int rh,
      int ox, int oy, int bw, int bh, int mvx, int mvy)
{
   int fx = mvx & 3, fy = mvy & 3;
   int ix = ox + (mvx >> 2);
   int iy = oy + (mvy >> 2);
   int x, y;

   if (fx == 0 && fy == 0)
   {
      /* full-pel: straight copy */
      for (y = 0; y < bh; y++)
         for (x = 0; x < bw; x++)
            dst[y * dstride + x] =
               (uint8_t)rh264_ref_luma(ref, rstride, rw, rh, ix + x, iy + y);
      return;
   }

   for (y = 0; y < bh; y++)
   {
      for (x = 0; x < bw; x++)
      {
         int sx = ix + x, sy = iy + y;
         int val;
#define P(dx,dy) rh264_ref_luma(ref, rstride, rw, rh, sx + (dx), sy + (dy))
         /* Per 8.4.2.2.1, name the samples by their quarter-pel position
          * inside the unit cell:
          *      G  a  b  c  H
          *      d  e  f  g
          *      h  i  j  k  m
          *      n  p  q  r
          *      M  .  s  .  N
          * G,H,M,N are integer samples; b,h,j,m,s are half-pels; the rest are
          * quarter-pels formed by averaging an adjacent half or integer pair. */
         int G = P(0,0);
         /* b: horizontal half-pel at (0,0) */
         int b1 = rh264_tap6(P(-2,0),P(-1,0),P(0,0),P(1,0),P(2,0),P(3,0));
         int b  = RH264_CLIP((b1 + 16) >> 5);
         /* h: vertical half-pel at (0,0) */
         int h1 = rh264_tap6(P(0,-2),P(0,-1),P(0,0),P(0,1),P(0,2),P(0,3));
         int hh = RH264_CLIP((h1 + 16) >> 5);

         if (fy == 0)
         {
            if (fx == 0)      val = G;
            else if (fx == 2) val = b;
            else              val = (b + ((fx == 1) ? G : P(1,0)) + 1) >> 1; /* a / c */
         }
         else if (fx == 0)
         {
            if (fy == 2) val = hh;
            else         val = (hh + ((fy == 1) ? G : P(0,1)) + 1) >> 1;    /* d / n */
         }
         else if (fx == 2 && fy == 2)
         {
            /* j: centre half-pel = 6-tap over vertical half-pels of the row */
            int t0=rh264_tap6(P(-2,-2),P(-1,-2),P(0,-2),P(1,-2),P(2,-2),P(3,-2));
            int t1=rh264_tap6(P(-2,-1),P(-1,-1),P(0,-1),P(1,-1),P(2,-1),P(3,-1));
            int t2=rh264_tap6(P(-2, 0),P(-1, 0),P(0, 0),P(1, 0),P(2, 0),P(3, 0));
            int t3=rh264_tap6(P(-2, 1),P(-1, 1),P(0, 1),P(1, 1),P(2, 1),P(3, 1));
            int t4=rh264_tap6(P(-2, 2),P(-1, 2),P(0, 2),P(1, 2),P(2, 2),P(3, 2));
            int t5=rh264_tap6(P(-2, 3),P(-1, 3),P(0, 3),P(1, 3),P(2, 3),P(3, 3));
            int j = rh264_tap6(t0,t1,t2,t3,t4,t5);
            val = RH264_CLIP((j + 512) >> 10);
         }
         else if (fx == 2)
         {
            /* column b half-pel; fy 1 or 3 -> f / q: average b with j */
            int t0=rh264_tap6(P(-2,-2),P(-1,-2),P(0,-2),P(1,-2),P(2,-2),P(3,-2));
            int t1=rh264_tap6(P(-2,-1),P(-1,-1),P(0,-1),P(1,-1),P(2,-1),P(3,-1));
            int t2=rh264_tap6(P(-2, 0),P(-1, 0),P(0, 0),P(1, 0),P(2, 0),P(3, 0));
            int t3=rh264_tap6(P(-2, 1),P(-1, 1),P(0, 1),P(1, 1),P(2, 1),P(3, 1));
            int t4=rh264_tap6(P(-2, 2),P(-1, 2),P(0, 2),P(1, 2),P(2, 2),P(3, 2));
            int t5=rh264_tap6(P(-2, 3),P(-1, 3),P(0, 3),P(1, 3),P(2, 3),P(3, 3));
            int j = RH264_CLIP((rh264_tap6(t0,t1,t2,t3,t4,t5) + 512) >> 10);
            /* b half-pel of the adjacent row (y=0 for fy=1, y=1 for fy=3) */
            int brow = (fy == 1) ? t2 : t3;
            int bc = RH264_CLIP((brow + 16) >> 5);
            val = (bc + j + 1) >> 1;
         }
         else if (fy == 2)
         {
            /* row h half-pel; fx 1 or 3 -> i / k: average h with j */
            int t0=rh264_tap6(P(-2,-2),P(-1,-2),P(0,-2),P(1,-2),P(2,-2),P(3,-2));
            int t1=rh264_tap6(P(-2,-1),P(-1,-1),P(0,-1),P(1,-1),P(2,-1),P(3,-1));
            int t2=rh264_tap6(P(-2, 0),P(-1, 0),P(0, 0),P(1, 0),P(2, 0),P(3, 0));
            int t3=rh264_tap6(P(-2, 1),P(-1, 1),P(0, 1),P(1, 1),P(2, 1),P(3, 1));
            int t4=rh264_tap6(P(-2, 2),P(-1, 2),P(0, 2),P(1, 2),P(2, 2),P(3, 2));
            int t5=rh264_tap6(P(-2, 3),P(-1, 3),P(0, 3),P(1, 3),P(2, 3),P(3, 3));
            int j = RH264_CLIP((rh264_tap6(t0,t1,t2,t3,t4,t5) + 512) >> 10);
            /* h half-pel of the adjacent column (x=0 for fx=1, x=1 for fx=3) */
            int hcol;
            if (fx == 1)
               hcol = rh264_tap6(P(0,-2),P(0,-1),P(0,0),P(0,1),P(0,2),P(0,3));
            else
               hcol = rh264_tap6(P(1,-2),P(1,-1),P(1,0),P(1,1),P(1,2),P(1,3));
            { int hc = RH264_CLIP((hcol + 16) >> 5);
              val = (hc + j + 1) >> 1; }
         }
         else
         {
            /* Corner quarter-pels e,g,p,r (fx in {1,3}, fy in {1,3}):
             * average the nearest horizontal half-pel (b, on the top or bottom
             * edge) with the nearest vertical half-pel (h, on the left or right
             * edge), per 8.4.2.2.1. */
            int bx, hy;
            /* b half-pel on the row toward fy: y=0 (fy=1) or y=1 (fy=3) */
            if (fy == 1)
               bx = b;   /* b at (0,0) already computed */
            else
            {
               int bb = rh264_tap6(P(-2,1),P(-1,1),P(0,1),P(1,1),P(2,1),P(3,1));
               bx = RH264_CLIP((bb + 16) >> 5);
            }
            /* h half-pel on the column toward fx: x=0 (fx=1) or x=1 (fx=3) */
            if (fx == 1)
               hy = hh;  /* h at (0,0) already computed */
            else
            {
               int hb = rh264_tap6(P(1,-2),P(1,-1),P(1,0),P(1,1),P(1,2),P(1,3));
               hy = RH264_CLIP((hb + 16) >> 5);
            }
            val = (bx + hy + 1) >> 1;
         }
#undef P
         dst[y * dstride + x] = (uint8_t)val;
      }
   }
}

/* Chroma 1/8-pel bilinear MC. mv is the LUMA quarter-pel MV; chroma uses
 * mv directly as 1/8-pel offsets (4:2:0 -> chroma mv = luma mv, 1/8 units). */
static void rh264_mc_chroma(uint8_t *dst, int dstride,
      const uint8_t *ref, int rstride, int rw, int rh,
      int ox, int oy, int bw, int bh, int mvx, int mvy)
{
   int ix = ox + (mvx >> 3);
   int iy = oy + (mvy >> 3);
   int fx = mvx & 7, fy = mvy & 7;
   int x, y;
   for (y = 0; y < bh; y++)
   {
      for (x = 0; x < bw; x++)
      {
         int sx = ix + x, sy = iy + y;
         int a = rh264_ref_luma(ref, rstride, rw, rh, sx,     sy);
         int b = rh264_ref_luma(ref, rstride, rw, rh, sx + 1, sy);
         int c = rh264_ref_luma(ref, rstride, rw, rh, sx,     sy + 1);
         int d = rh264_ref_luma(ref, rstride, rw, rh, sx + 1, sy + 1);
         int val = ((8 - fx) * (8 - fy) * a + fx * (8 - fy) * b
                  + (8 - fx) * fy * c + fx * fy * d + 32) >> 6;
         dst[y * dstride + x] = (uint8_t)val;
      }
   }
}

/* ==================== rh264_pslice.h ==================== */
/* rh264 P-slice decoding (single reference, CAVLC), 4:2:0 Main/Baseline.
 * Depends on the merged decoder's residual/transform/reconstruct helpers and
 * on rh264_inter.h for motion compensation. Motion vectors are stored on a
 * per-4x4 grid so neighbour prediction (median) works across MB boundaries. */

/* Per-4x4 motion state, carried on the frame for neighbour MV prediction and
 * inter deblock. mvx/mvy are quarter-pel; ref is the ref_idx (0 for single
 * ref); intra marks intra-coded 4x4 blocks (unavailable as MV predictors). */
typedef struct {
   int16_t mvx, mvy;
   int8_t  ref;   /* -1 = intra / not available */
} rh264_mv;

/* median of three */
static RH264_INLINE int rh264_median3(int a, int b, int c)
{
   int mx = a > b ? a : b; int mn = a < b ? a : b;
   int t = c > mx ? mx : (c < mn ? mn : c);
   return t;
}

/* Fetch neighbour MV cell at grid (gx,gy); returns availability. */
static RH264_INLINE const rh264_mv *rh264_mv_at(const rh264_mv *grid,
      int gw, int gx, int gy, int gwmax, int ghmax)
{
   if (gx < 0 || gy < 0 || gx >= gwmax || gy >= ghmax) return NULL;
   return &grid[gy * gw + gx];
}

/* Directional-predictor hint (8.4.1.3.2):
 *   0 = median (default)
 *   1 = prefer B  (16x8 top partition)
 *   2 = prefer A  (16x8 bottom, 8x16 left partition)
 *   3 = prefer C  (8x16 right partition) */
#define RH264_MVP_MEDIAN 0
#define RH264_MVP_B      1
#define RH264_MVP_A      2
#define RH264_MVP_C      3

/* Predict the MV for a partition at 4x4-grid position (gx,gy) of size
 * (bw4 x bh4) 4x4 blocks, given the current partition's ref_idx and a
 * directional hint for 16x8 / 8x16 partitions (8.4.1.3). */
static void rh264_pred_mv_dir(const rh264_mv *grid, int gw, int gwmax, int ghmax,
      int gx, int gy, int bw4, int ref, int hint, int *pmvx, int *pmvy)
{
   const rh264_mv *A = rh264_mv_at(grid, gw, gx - 1,     gy,     gwmax, ghmax);
   const rh264_mv *B = rh264_mv_at(grid, gw, gx,         gy - 1, gwmax, ghmax);
   const rh264_mv *C = rh264_mv_at(grid, gw, gx + bw4,   gy - 1, gwmax, ghmax);
   const rh264_mv *D = rh264_mv_at(grid, gw, gx - 1,     gy - 1, gwmax, ghmax);
   int axx, axy, bxx, bxy, cxx, cxy;
   int aok, bok, cok;
   /* C falls back to D only for GEOMETRIC unavailability -- the top-right
    * neighbour is out of bounds or belongs to a macroblock not yet decoded
    * (ref == -2). An intra neighbour (ref == -1) is geometrically present, so
    * it is NOT substituted; it simply contributes an unavailable predictor
    * (mv 0, ref -1). (6.4.11.7 / 8.4.1.3.) */
   if (!C || C->ref == -2) C = D;

   aok = A && A->ref >= 0; bok = B && B->ref >= 0; cok = C && C->ref >= 0;
   axx = aok ? A->mvx : 0; axy = aok ? A->mvy : 0;
   bxx = bok ? B->mvx : 0; bxy = bok ? B->mvy : 0;
   cxx = cok ? C->mvx : 0; cxy = cok ? C->mvy : 0;

   /* 8.4.1.3.2 directional prediction for 16x8 / 8x16 takes priority. */
   if (hint == RH264_MVP_B && bok && B->ref == ref) { *pmvx = bxx; *pmvy = bxy; return; }
   if (hint == RH264_MVP_A && aok && A->ref == ref) { *pmvx = axx; *pmvy = axy; return; }
   if (hint == RH264_MVP_C && cok && C->ref == ref) { *pmvx = cxx; *pmvy = cxy; return; }

   /* 8.4.1.3.1: if exactly one neighbour has the same ref, use it. */
   {
      int nsame = (aok && A->ref == ref)
                + (bok && B->ref == ref)
                + (cok && C->ref == ref);
      if (nsame == 1)
      {
         if (aok && A->ref == ref) { *pmvx = axx; *pmvy = axy; return; }
         if (bok && B->ref == ref) { *pmvx = bxx; *pmvy = bxy; return; }
         { *pmvx = cxx; *pmvy = cxy; return; }
      }
   }
   /* If B and C are both unavailable but A is available, use A. */
   if (!bok && !cok && aok) { *pmvx = axx; *pmvy = axy; return; }

   *pmvx = rh264_median3(axx, bxx, cxx);
   *pmvy = rh264_median3(axy, bxy, cxy);
}

/* Median-only convenience wrapper. */
static void rh264_pred_mv(const rh264_mv *grid, int gw, int gwmax, int ghmax,
      int gx, int gy, int bw4, int ref, int *pmvx, int *pmvy)
{
   rh264_pred_mv_dir(grid, gw, gwmax, ghmax, gx, gy, bw4, ref,
         RH264_MVP_MEDIAN, pmvx, pmvy);
}

/* Store an MV into all cells of a partition and mark ref. */
static void rh264_mv_fill(rh264_mv *grid, int gw, int gx, int gy,
      int bw4, int bh4, int mvx, int mvy, int ref)
{
   int x, y;
   for (y = 0; y < bh4; y++)
      for (x = 0; x < bw4; x++)
      {
         rh264_mv *m = &grid[(gy + y) * gw + (gx + x)];
         m->mvx = (int16_t)mvx; m->mvy = (int16_t)mvy; m->ref = (int8_t)ref;
      }
}

/* Mark an inter MB's 4x4 luma-mode cells as unavailable (0xff) so a later
 * intra4x4 MB in the same P-slice computes its most-probable-mode with the
 * inter neighbour treated as unavailable (8.3.1.1). */
static void rh264_inter_clear_i4mode(rh264_frame *f, int mbx, int mby)
{
   int gw = f->mbw * 4, cx, cy;
   for (cy = 0; cy < 4; cy++)
      for (cx = 0; cx < 4; cx++)
         f->i4mode[(mby * 4 + cy) * gw + mbx * 4 + cx] = 0xff;
}

/* Motion-compensate a luma partition (bw x bh px at MB-relative bx,by) from
 * the reference frame into the current frame, using quarter-pel MV. Also does
 * the matching chroma (half size). */
static void rh264_inter_pred_block(rh264_frame *f, const rh264_frame *ref,
      int mbx, int mby, int bx, int by, int bw, int bh, int mvx, int mvy)
{
   int ox = mbx * 16 + bx, oy = mby * 16 + by;
   uint8_t *dY = f->Y + oy * f->ystride + ox;
   rh264_mc_luma(dY, f->ystride, ref->Y, ref->ystride, ref->w, ref->h,
         ox, oy, bw, bh, mvx, mvy);
   {
      int cox = (mbx * 16 + bx) >> 1, coy = (mby * 16 + by) >> 1;
      int cbw = bw >> 1, cbh = bh >> 1;
      uint8_t *dU = f->U + coy * f->cstride + cox;
      uint8_t *dV = f->V + coy * f->cstride + cox;
      rh264_mc_chroma(dU, f->cstride, ref->U, ref->cstride,
            ref->w >> 1, ref->h >> 1, cox, coy, cbw, cbh, mvx, mvy);
      rh264_mc_chroma(dV, f->cstride, ref->V, ref->cstride,
            ref->w >> 1, ref->h >> 1, cox, coy, cbw, cbh, mvx, mvy);
   }
}

/* ==================== rh264_pdrive.h ==================== */
/* CBP mapping for Inter macroblocks: codeNum -> cbp (Table 9-4, inter col). */
static const uint8_t rh264_cbp_inter[48]={
   0,16,1,2,4,8,32,3,5,10,12,15,47,7,11,13,14,6,9,31,35,37,42,44,33,34,36,40,
   39,43,45,46,17,18,20,24,19,21,26,28,23,27,29,30,22,25,38,41};

/* Reconstruct the 16 luma 4x4 residual blocks of an inter MB on top of the
 * already motion-compensated prediction in the current frame. */
static int rh264_inter_luma_residual(rh264_bits *b, rh264_frame *f,
      int mbx, int mby, int cbp_luma)
{
   int gw = f->mbw * 4;
   uint8_t *y = f->Y + (mby * 16) * f->ystride + mbx * 16;
   int i;
   for (i = 0; i < 16; i++)
   {
      int bx = rh264_blk_x[i], by = rh264_blk_y[i];
      int gx = mbx * 4 + bx, gy = mby * 4 + by;
      uint8_t *d = y + by * 4 * f->ystride + bx * 4;
      int nzc = 0;
      if (cbp_luma & (1 << (i >> 2)))
      {
         int nC = rh264_nC(f->nzL, gw, f->mbh * 4, gx, gy);
         int32_t scan[16], coef[16], r[16]; int k, tc;
         for (k = 0; k < 16; k++) coef[k] = 0;
         tc = rh264_residual_block(b, nC, 16, scan);
         if (tc < 0) return -1;
         nzc = tc;

         for (k = 0; k < 16; k++) coef[rh264_zigzag4[k]] = scan[k];
         rh264_dequant4x4(coef, f->qp, 0);
         rh264_itransform4x4(coef, r);
         {
            int xx, yy, val;
            for (yy = 0; yy < 4; yy++) for (xx = 0; xx < 4; xx++)
            {
               val = d[yy * f->ystride + xx] + ((r[yy * 4 + xx] + 32) >> 6);
               d[yy * f->ystride + xx] = (uint8_t)RH264_CLIP(val);
            }
         }
      }
      f->nzL[gy * gw + gx] = (uint8_t)nzc;
   }
   return 0;
}

/* Decode one P macroblock partition's motion: predict MV, read mvd, store,
 * and motion-compensate. gx,gy = 4x4 grid pos of the partition top-left;
 * bw,bh = pixel size; bx,by = MB-relative pixel offset. */
static void rh264_p_part_hint(rh264_bits *b, rh264_frame *f, const rh264_frame *ref,
      rh264_mv *mvg, int gwmax, int ghmax,
      int mbx, int mby, int bx, int by, int bw, int bh, int refidx, int hint)
{
   int gx = mbx * 4 + (bx >> 2), gy = mby * 4 + (by >> 2);
   int bw4 = bw >> 2, bh4 = bh >> 2;
   int pmvx, pmvy, mvx, mvy, mvdx, mvdy;
   rh264_pred_mv_dir(mvg, gwmax, gwmax, ghmax, gx, gy, bw4, refidx, hint, &pmvx, &pmvy);
   mvdx = rh264_se(b); mvdy = rh264_se(b);

   mvx = pmvx + mvdx; mvy = pmvy + mvdy;
   rh264_mv_fill(mvg, gwmax, gx, gy, bw4, bh4, mvx, mvy, refidx);
   rh264_inter_pred_block(f, ref, mbx, mby, bx, by, bw, bh, mvx, mvy);
}

/* Median-predictor partition (8x8 and sub-8x8 use plain median). */
static void rh264_p_part(rh264_bits *b, rh264_frame *f, const rh264_frame *ref,
      rh264_mv *mvg, int gwmax, int ghmax,
      int mbx, int mby, int bx, int by, int bw, int bh, int refidx)
{
   rh264_p_part_hint(b, f, ref, mvg, gwmax, ghmax, mbx, mby,
         bx, by, bw, bh, refidx, RH264_MVP_MEDIAN);
}

/* Full P-slice decode (single reference, CAVLC). ref is the previously
 * decoded frame. mvg is a per-4x4 MV grid (gwmax x ghmax) that must persist
 * for the whole slice. Returns 0 on success. */
static int g_frameno=0;
static int rh264_decode_pslice(rh264_bits *b, const rh264_sps *sps,
      const rh264_pps *pps, rh264_slice_hdr *sh, rh264_frame *f,
      const rh264_frame *ref, rh264_mv *mvg)
{
   int mbaddr = sh->first_mb_in_slice, total = f->mbw * f->mbh;
   int gw = f->mbw * 4, cgw = f->mbw * 2;
   int gwmax = f->mbw * 4, ghmax = f->mbh * 4;
   int skip_run = -1;
   int gi;
   g_frameno++;
   f->qp = sh->slice_qp;
   f->chroma_qp_offset = pps->chroma_qp_index_offset;
   (void)sps;

   /* Reset the MV grid so cells belonging to not-yet-decoded macroblocks are
    * treated as unavailable predictors (ref = -1). Decoded partitions fill
    * their cells as the slice progresses, giving correct neighbour
    * availability in raster order (6.4.11.7 / 8.4.1.3). */
   for (gi = 0; gi < gwmax * ghmax; gi++)
   {
      mvg[gi].mvx = 0; mvg[gi].mvy = 0; mvg[gi].ref = -2;
   }

   /* Reset the per-frame coefficient/mode context. These arrays feed the nC
    * neighbour derivation for CAVLC coeff_token (9.2.1) and the intra-4x4
    * most-probable-mode derivation; they describe the frame currently being
    * decoded, not the reference, so they must start clean for every slice.
    * (Without this they retain values from whichever frame last occupied this
    * buffer, corrupting nC at macroblocks whose neighbours were coded there.) */
   memset(f->nzL, 0, (size_t)gw * f->mbh * 4);
   memset(f->nzC[0], 0, (size_t)cgw * f->mbh * 2);
   memset(f->nzC[1], 0, (size_t)cgw * f->mbh * 2);
   memset(f->i4mode, 0xff, (size_t)gw * f->mbh * 4);

   while (mbaddr < total)
   {
      int mbx = mbaddr % f->mbw, mby = mbaddr / f->mbw;
      int mb_type;

      /* mb_skip_run */
      if (skip_run < 0) skip_run = rh264_ue(b);
      if (skip_run > 0)
      {
         /* P_Skip: predicted MV with the skip special case (8.4.1.1). */
         int gx = mbx * 4, gy = mby * 4;
         const rh264_mv *A = rh264_mv_at(mvg, gwmax, gx - 1, gy, gwmax, ghmax);
         const rh264_mv *B = rh264_mv_at(mvg, gwmax, gx, gy - 1, gwmax, ghmax);
         int mvx, mvy;
         /* 8.4.1.1: P_Skip MV is zero when the left or top neighbour is not
          * available (off-frame or not-yet-decoded -> ref == -2; an intra
          * neighbour is "available" per 6.4.9 and does not trigger this), or
          * when either has refIdxL0 == 0 with a zero MV. */
         if (!A || A->ref == -2 || !B || B->ref == -2
               || (A->ref == 0 && A->mvx == 0 && A->mvy == 0)
               || (B->ref == 0 && B->mvx == 0 && B->mvy == 0))
         { mvx = 0; mvy = 0; }
         else
            rh264_pred_mv(mvg, gwmax, gwmax, ghmax, gx, gy, 4, 0, &mvx, &mvy);
         rh264_mv_fill(mvg, gwmax, gx, gy, 4, 4, mvx, mvy, 0);
         rh264_inter_pred_block(f, ref, mbx, mby, 0, 0, 16, 16, mvx, mvy);
         rh264_inter_clear_i4mode(f, mbx, mby);
         /* skip MBs carry no residual */
         { int cx, cy; for (cy = 0; cy < 4; cy++) for (cx = 0; cx < 4; cx++)
              f->nzL[(mby * 4 + cy) * gw + mbx * 4 + cx] = 0; }
         { int cx, cy; for (cy = 0; cy < 2; cy++) for (cx = 0; cx < 2; cx++) {
              f->nzC[0][(mby * 2 + cy) * cgw + mbx * 2 + cx] = 0;
              f->nzC[1][(mby * 2 + cy) * cgw + mbx * 2 + cx] = 0; } }
         f->mbqp[mby * f->mbw + mbx] = (uint8_t)f->qp;
         skip_run--; mbaddr++;
         continue;
      }
      skip_run = -1;
      mb_type = rh264_ue(b);

      if (mb_type >= 5)
      {
         /* Intra MB inside a P-slice: mb_type is offset by 5 (Table 7-11). */
         int imb = mb_type - 5;
         int cx, cy;
         if (rh264_decode_intra_mb_cavlc(b, f, mbx, mby, imb) < 0) return -1;
         /* mark these 4x4 cells intra (unavailable as MV predictors) */
         for (cy = 0; cy < 4; cy++) for (cx = 0; cx < 4; cx++)
         {
            rh264_mv *m = &mvg[(mby * 4 + cy) * gwmax + mbx * 4 + cx];
            m->mvx = 0; m->mvy = 0; m->ref = -1;
         }
         f->mbqp[mby * f->mbw + mbx] = (uint8_t)f->qp;
      }
      else
      {
         /* Inter P MB. */
         int cbp, cbp_luma, cbp_chroma;
         uint8_t *u = f->U + (mby * 8) * f->cstride + mbx * 8;
         uint8_t *v = f->V + (mby * 8) * f->cstride + mbx * 8;
         rh264_inter_clear_i4mode(f, mbx, mby);

      if (mb_type == 0)
         {
            /* P_L0_16x16 */
            int refidx = 0; /* num_ref_idx_l0 == 1 for our target */
            rh264_p_part(b, f, ref, mvg, gwmax, ghmax, mbx, mby, 0, 0, 16, 16, refidx);
         }
         else if (mb_type == 1)
         {
            /* P_L0_L0_16x8: top prefers B, bottom prefers A (8.4.1.3.2). */
            rh264_p_part_hint(b, f, ref, mvg, gwmax, ghmax, mbx, mby, 0, 0, 16, 8, 0, RH264_MVP_B);
            rh264_p_part_hint(b, f, ref, mvg, gwmax, ghmax, mbx, mby, 0, 8, 16, 8, 0, RH264_MVP_A);
         }
         else if (mb_type == 2)
         {
            /* P_L0_L0_8x16: left prefers A, right prefers C (8.4.1.3.2). */
            rh264_p_part_hint(b, f, ref, mvg, gwmax, ghmax, mbx, mby, 0, 0, 8, 16, 0, RH264_MVP_A);
            rh264_p_part_hint(b, f, ref, mvg, gwmax, ghmax, mbx, mby, 8, 0, 8, 16, 0, RH264_MVP_C);
         }
         else
         {
            /* P_8x8 / P_8x8ref0: four 8x8 sub-partitions. */
            int sub[4], p;
            for (p = 0; p < 4; p++) sub[p] = rh264_ue(b);
            for (p = 0; p < 4; p++)
            {
               int px = (p & 1) * 8, py = (p >> 1) * 8;
               int st = sub[p];
               if (st == 0)       /* 8x8 */
                  rh264_p_part(b, f, ref, mvg, gwmax, ghmax, mbx, mby, px, py, 8, 8, 0);
               else if (st == 1)  /* 8x4 */
               {
                  rh264_p_part(b, f, ref, mvg, gwmax, ghmax, mbx, mby, px, py,   8, 4, 0);
                  rh264_p_part(b, f, ref, mvg, gwmax, ghmax, mbx, mby, px, py+4, 8, 4, 0);
               }
               else if (st == 2)  /* 4x8 */
               {
                  rh264_p_part(b, f, ref, mvg, gwmax, ghmax, mbx, mby, px,   py, 4, 8, 0);
                  rh264_p_part(b, f, ref, mvg, gwmax, ghmax, mbx, mby, px+4, py, 4, 8, 0);
               }
               else               /* 4x4 */
               {
                  rh264_p_part(b, f, ref, mvg, gwmax, ghmax, mbx, mby, px,   py,   4, 4, 0);
                  rh264_p_part(b, f, ref, mvg, gwmax, ghmax, mbx, mby, px+4, py,   4, 4, 0);
                  rh264_p_part(b, f, ref, mvg, gwmax, ghmax, mbx, mby, px,   py+4, 4, 4, 0);
                  rh264_p_part(b, f, ref, mvg, gwmax, ghmax, mbx, mby, px+4, py+4, 4, 4, 0);
               }
            }
         }

         /* coded_block_pattern */
         cbp = rh264_ue(b);
         if (cbp >= 48) { return -3; }
         cbp = rh264_cbp_inter[cbp];
         cbp_luma = cbp & 15; cbp_chroma = cbp >> 4;
         if (cbp_luma || cbp_chroma)
         {
            int d = rh264_se(b); f->qp = (f->qp + d + 52) % 52;
         }
         if (rh264_inter_luma_residual(b, f, mbx, mby, cbp_luma) < 0) return -1;
         if (cbp_chroma)
         {
            if (rh264_decode_chroma_residual(b, f, mbx, mby, u, v, cbp_chroma) < 0)
               return -1;
         }
         else
         {
            int cx, cy; for (cy = 0; cy < 2; cy++) for (cx = 0; cx < 2; cx++) {
               f->nzC[0][(mby * 2 + cy) * cgw + mbx * 2 + cx] = 0;
               f->nzC[1][(mby * 2 + cy) * cgw + mbx * 2 + cx] = 0; }
         }
         f->mbqp[mby * f->mbw + mbx] = (uint8_t)f->qp;
      }
      mbaddr++;
   }

   return 0;
}

/* Boundary strength for a 4x4 edge between block P (side with lower coord) and
 * block Q, per 8.7.2.1. mbedge != 0 when the edge is a macroblock boundary.
 * intra_p/intra_q: the block belongs to an intra MB. cbf_p/cbf_q: the block has
 * non-zero transform coefficients. The two MVs (single reference list, one ref)
 * decide bS 1 vs 0 by the quarter-pel >= 4 rule. */
static RH264_INLINE int rh264_inter_bs(int mbedge, int intra_p, int intra_q,
      int cbf_p, int cbf_q,
      int mvpx, int mvpy, int refp, int mvqx, int mvqy, int refq)
{
   if (intra_p || intra_q)
      return mbedge ? 4 : 3;
   if (cbf_p || cbf_q)
      return 2;
   if (refp != refq)
      return 1;
   if (mvpx - mvqx >= 4 || mvqx - mvpx >= 4 ||
       mvpy - mvqy >= 4 || mvqy - mvpy >= 4)
      return 1;
   return 0;
}

/* In-loop deblocking for a P-slice: identical filter kernels to the intra
 * rh264_deblock, but the boundary strength is derived per 4-sample edge segment
 * from the macroblock/partition geometry carried in the MV grid (mvg) and the
 * per-4x4 non-zero-coefficient flags (f->nzL / f->nzC). */
static void rh264_deblock_pslice(rh264_frame *f, const rh264_slice_hdr *sh,
      const rh264_mv *mvg)
{
   int mbx, mby, edge, seg;
   int oA = sh->slice_alpha_c0_offset, oB = sh->slice_beta_offset;
   int gw = f->mbw * 4, cgw = f->mbw * 2;
   if (sh->disable_deblocking_filter_idc == 1) return;

   for (mby = 0; mby < f->mbh; mby++)
   for (mbx = 0; mbx < f->mbw; mbx++)
   {
      int qp  = f->mbqp ? f->mbqp[mby*f->mbw+mbx] : f->qp;
      int qpc = rh264_chroma_qp(qp, f->chroma_qp_offset);

      /* ---- vertical edges (filter columns), left to right ---- */
      for (edge = 0; edge < 4; edge++)
      {
         int x = mbx*16 + edge*4;
         int mbedge = (edge == 0);
         int qpavg, qpp;
         if (mbedge && mbx == 0) continue;
         qpp = mbedge ? (f->mbqp ? f->mbqp[mby*f->mbw+mbx-1] : qp) : qp;
         qpavg = mbedge ? ((qp + qpp + 1) >> 1) : qp;
         /* four 4-sample segments down the edge, each its own bS */
         for (seg = 0; seg < 4; seg++)
         {
            int gy = mby*4 + seg;
            int gxq = mbx*4 + edge;           /* Q block column   */
            int gxp = gxq - 1;                /* P block (left)   */
            int rp = mvg[gy*gw + gxp].ref, rq = mvg[gy*gw + gxq].ref;
            int bS = rh264_inter_bs(mbedge,
                  rp == -1, rq == -1,
                  f->nzL[gy*gw+gxp] != 0, f->nzL[gy*gw+gxq] != 0,
                  mvg[gy*gw+gxp].mvx, mvg[gy*gw+gxp].mvy, rp<0?-1:rp,
                  mvg[gy*gw+gxq].mvx, mvg[gy*gw+gxq].mvy, rq<0?-1:rq);
            int a, be, t, idxA, idxB, i;
            if (bS == 0) continue;
            idxA = qpavg+oA; if(idxA<0)idxA=0; else if(idxA>51)idxA=51;
            idxB = qpavg+oB; if(idxB<0)idxB=0; else if(idxB>51)idxB=51;
            a = rh264_alpha[idxA]; be = rh264_beta[idxB];
            t = rh264_tc0[bS==4?2:bS-1][idxA];
            for (i = 0; i < 4; i++)
            { uint8_t *e = f->Y + (mby*16+seg*4+i)*f->ystride + x;
              rh264_filter_luma_edge(e, 1, bS, a, be, t); }
            /* chroma on even luma edges; two chroma rows per luma segment */
            if ((edge&1)==0)
            {
               int cx = mbx*8 + (edge>>1)*4;
               int cqpavg = mbedge ?
                  ((qpc + rh264_chroma_qp(qpp,f->chroma_qp_offset) + 1)>>1) : qpc;
               int cA=cqpavg+oA, cB=cqpavg+oB, ca, cbe, ct, ci;
               if(cA<0)cA=0; else if(cA>51)cA=51;
               if(cB<0)cB=0; else if(cB>51)cB=51;
               ca=rh264_alpha[cA]; cbe=rh264_beta[cB]; ct=rh264_tc0[bS==4?2:bS-1][cA];
               for (ci = 0; ci < 2; ci++)
               { int cy = mby*8 + seg*2 + ci;
                 rh264_filter_chroma_edge(f->U+cy*f->cstride+cx,1,bS,ca,cbe,ct);
                 rh264_filter_chroma_edge(f->V+cy*f->cstride+cx,1,bS,ca,cbe,ct); }
               (void)cgw;
            }
         }
      }

      /* ---- horizontal edges (filter rows), top to bottom ---- */
      for (edge = 0; edge < 4; edge++)
      {
         int y = mby*16 + edge*4;
         int mbedge = (edge == 0);
         int qpavg, qpp;
         if (mbedge && mby == 0) continue;
         qpp = mbedge ? (f->mbqp ? f->mbqp[(mby-1)*f->mbw+mbx] : qp) : qp;
         qpavg = mbedge ? ((qp + qpp + 1) >> 1) : qp;
         for (seg = 0; seg < 4; seg++)
         {
            int gx = mbx*4 + seg;
            int gyq = mby*4 + edge;           /* Q block row    */
            int gyp = gyq - 1;                /* P block (above)*/
            int rp = mvg[gyp*gw + gx].ref, rq = mvg[gyq*gw + gx].ref;
            int bS = rh264_inter_bs(mbedge,
                  rp == -1, rq == -1,
                  f->nzL[gyp*gw+gx] != 0, f->nzL[gyq*gw+gx] != 0,
                  mvg[gyp*gw+gx].mvx, mvg[gyp*gw+gx].mvy, rp<0?-1:rp,
                  mvg[gyq*gw+gx].mvx, mvg[gyq*gw+gx].mvy, rq<0?-1:rq);
            int a, be, t, idxA, idxB, i;
            if (bS == 0) continue;
            idxA = qpavg+oA; if(idxA<0)idxA=0; else if(idxA>51)idxA=51;
            idxB = qpavg+oB; if(idxB<0)idxB=0; else if(idxB>51)idxB=51;
            a = rh264_alpha[idxA]; be = rh264_beta[idxB];
            t = rh264_tc0[bS==4?2:bS-1][idxA];
            for (i = 0; i < 4; i++)
            { uint8_t *e = f->Y + y*f->ystride + (mbx*16+seg*4+i);
              rh264_filter_luma_edge(e, f->ystride, bS, a, be, t); }
            if ((edge&1)==0)
            {
               int cy = mby*8 + (edge>>1)*4;
               int cqpavg = mbedge ?
                  ((qpc + rh264_chroma_qp(qpp,f->chroma_qp_offset) + 1)>>1) : qpc;
               int cA=cqpavg+oA, cB=cqpavg+oB, ca, cbe, ct, ci;
               if(cA<0)cA=0; else if(cA>51)cA=51;
               if(cB<0)cB=0; else if(cB>51)cB=51;
               ca=rh264_alpha[cA]; cbe=rh264_beta[cB]; ct=rh264_tc0[bS==4?2:bS-1][cA];
               for (ci = 0; ci < 2; ci++)
               { int cx = mbx*8 + seg*2 + ci;
                 rh264_filter_chroma_edge(f->U+cy*f->cstride+cx,f->cstride,bS,ca,cbe,ct);
                 rh264_filter_chroma_edge(f->V+cy*f->cstride+cx,f->cstride,bS,ca,cbe,ct); }
            }
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

      if(rh264_decode_intra_mb_cavlc(b,f,mbx,mby,mb_type)<0) return -1;
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
   rh264_frame ref;             /* previous decoded frame, used as P-slice ref */
   rh264_mv  *mvg;              /* motion-vector grid for the frame being decoded */
   int        have_ref;         /* a reference frame is available (post-IDR)     */
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
   if (rh264_frame_alloc(&v->ref, &v->sps) != 0) return -1;
   free(v->mvg);
   {
      int gwmax = v->f.mbw * 4;
      int ghmax = ((v->sps.frame_height + 15) / 16) * 4;
      v->mvg = (rh264_mv*)calloc((size_t)gwmax * ghmax, sizeof(rh264_mv));
      if (!v->mvg) return -1;
   }
   v->have_ref = 0;
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
   rh264_frame_free(&v->ref);
   free(v->mvg);
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
/* ==================== CABAC (Main-profile I-slice) ==================== */
/* Clean-room CABAC entropy decoder for Main-profile intra frames.
 * Byte-exact vs openh264 for I_4x4 / I_16x16 macroblocks (4:2:0). */

/* ---- rh264_cabac_tables.h ---- */
/* rh264 CABAC core tables (H.264 clause 9.3.3.2). These are the universal
 * spec tables, identical across all decoders. */

/* Table 9-33: rangeTabLPS[pStateIdx][qCodIRangeIdx] */
static const uint8_t rh264_rangeTabLPS[64][4]={
 {128,176,208,240},{128,167,197,227},{128,158,187,216},{123,150,178,205},
 {116,142,169,195},{111,135,160,185},{105,128,152,175},{100,122,144,166},
 { 95,116,137,158},{ 90,110,130,150},{ 85,104,123,142},{ 81, 99,117,135},
 { 77, 94,111,128},{ 73, 89,105,122},{ 69, 85,100,116},{ 66, 80, 95,110},
 { 62, 76, 90,104},{ 59, 72, 86, 99},{ 56, 69, 81, 94},{ 53, 65, 77, 89},
 { 51, 62, 73, 85},{ 48, 59, 69, 80},{ 46, 56, 66, 76},{ 43, 53, 63, 72},
 { 41, 50, 59, 69},{ 39, 48, 56, 65},{ 37, 45, 54, 62},{ 35, 43, 51, 59},
 { 33, 41, 48, 56},{ 32, 39, 46, 53},{ 30, 37, 43, 50},{ 29, 35, 41, 48},
 { 27, 33, 39, 45},{ 26, 31, 37, 43},{ 24, 30, 35, 41},{ 23, 28, 33, 39},
 { 22, 27, 32, 37},{ 21, 26, 30, 35},{ 20, 24, 29, 33},{ 19, 23, 27, 31},
 { 18, 22, 26, 30},{ 17, 21, 25, 28},{ 16, 20, 23, 27},{ 15, 19, 22, 25},
 { 14, 18, 21, 24},{ 14, 17, 20, 23},{ 13, 16, 19, 22},{ 12, 15, 18, 21},
 { 12, 14, 17, 20},{ 11, 14, 16, 19},{ 11, 13, 15, 18},{ 10, 12, 15, 17},
 { 10, 12, 14, 16},{  9, 11, 13, 15},{  9, 11, 12, 14},{  8, 10, 12, 14},
 {  8,  9, 11, 13},{  7,  9, 11, 12},{  7,  9, 10, 12},{  7,  8, 10, 11},
 {  6,  8,  9, 11},{  6,  7,  9, 10},{  6,  7,  8,  9},{  2,  2,  2,  2}};

/* Table 9-34: transIdxLPS / transIdxMPS (pStateIdx transitions) */
static const uint8_t rh264_transIdxLPS[64]={
  0, 0, 1, 2, 2, 4, 4, 5, 6, 7, 8, 9, 9,11,11,12,
 13,13,15,15,16,16,18,18,19,19,21,21,22,22,23,24,
 24,25,26,26,27,27,28,29,29,30,30,30,31,32,32,33,
 33,33,34,34,35,35,35,36,36,36,37,37,37,38,38,63};
static const uint8_t rh264_transIdxMPS[64]={
  1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,
 17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
 33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
 49,50,51,52,53,54,55,56,57,58,59,60,61,62,62,63};

/* ---- rh264_cabac_init.h ---- */
/* rh264 CABAC I-slice context init table (m,n), spec Tables 9-12..9-24,
   extracted from libopenh264 g_kiCabacGlobalContextIdx variant 0. */
#define RH264_CABAC_NCTX 460
static const int8_t rh264_cabac_init_I[460][2]={
  {20,-15},{2,54},{3,74},{20,-15},{2,54},{3,74},{-28,127},{-23,104},
  {-6,53},{-1,54},{7,51},{0,0},{0,0},{0,0},{0,0},{0,0},
  {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
  {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
  {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
  {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
  {0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
  {0,0},{0,0},{0,0},{0,0},{0,41},{0,63},{0,63},{0,63},
  {-9,83},{4,86},{0,97},{-7,72},{13,41},{3,62},{0,11},{1,55},
  {0,69},{-17,127},{-13,102},{0,82},{-7,74},{-21,107},{-27,127},{-31,127},
  {-24,127},{-18,95},{-27,127},{-21,114},{-30,127},{-17,123},{-12,115},{-16,122},
  {-11,115},{-12,63},{-2,68},{-15,84},{-13,104},{-3,70},{-8,93},{-10,90},
  {-30,127},{-1,74},{-6,97},{-7,91},{-20,127},{-4,56},{-5,82},{-7,76},
  {-22,125},{-7,93},{-11,87},{-3,77},{-5,71},{-4,63},{-4,68},{-12,84},
  {-7,62},{-7,65},{8,61},{5,56},{-2,66},{1,64},{0,61},{-2,78},
  {1,50},{7,52},{10,35},{0,44},{11,38},{1,45},{0,46},{5,44},
  {31,17},{1,51},{7,50},{28,19},{16,33},{14,62},{-13,108},{-15,100},
  {-13,101},{-13,91},{-12,94},{-10,88},{-16,84},{-10,86},{-7,83},{-13,87},
  {-19,94},{1,70},{0,72},{-5,74},{18,59},{-8,102},{-15,100},{0,95},
  {-4,75},{2,72},{-11,75},{-3,71},{15,46},{-13,69},{0,62},{0,65},
  {21,37},{-15,72},{9,57},{16,54},{0,62},{12,72},{24,0},{15,9},
  {8,25},{13,18},{15,9},{13,19},{10,37},{12,18},{6,29},{20,33},
  {15,30},{4,45},{1,58},{0,62},{7,61},{12,38},{11,45},{15,39},
  {11,42},{13,44},{16,45},{12,41},{10,49},{30,34},{18,42},{10,55},
  {17,51},{17,46},{0,89},{26,-19},{22,-17},{26,-17},{30,-25},{28,-20},
  {33,-23},{37,-27},{33,-23},{40,-28},{38,-17},{33,-11},{40,-15},{41,-6},
  {38,1},{41,17},{30,-6},{27,3},{26,22},{37,-16},{35,-4},{38,-8},
  {38,-3},{37,3},{38,5},{42,0},{35,16},{39,22},{14,48},{27,37},
  {21,60},{12,68},{2,97},{-3,71},{-6,42},{-5,50},{-3,54},{-2,62},
  {0,58},{1,63},{-2,72},{-1,74},{-9,91},{-5,67},{-5,27},{-3,39},
  {-2,44},{0,46},{-16,64},{-8,68},{-10,78},{-6,77},{-10,86},{-12,92},
  {-15,55},{-10,60},{-6,62},{-4,65},{-12,73},{-8,76},{-7,80},{-9,88},
  {-17,110},{-11,97},{-20,84},{-11,79},{-6,73},{-4,74},{-13,86},{-13,96},
  {-11,97},{-19,117},{-8,78},{-5,33},{-4,48},{-2,53},{-3,62},{-13,71},
  {-10,79},{-12,86},{-13,90},{-14,97},{0,0},{-6,93},{-6,84},{-8,79},
  {0,66},{-1,71},{0,62},{-2,60},{-2,59},{-5,75},{-3,62},{-4,58},
  {-9,66},{-1,79},{0,71},{3,68},{10,44},{-7,62},{15,36},{14,40},
  {16,27},{12,29},{1,44},{20,36},{18,32},{5,42},{1,48},{10,62},
  {17,46},{9,64},{-12,104},{-11,97},{-16,96},{-7,88},{-8,85},{-7,85},
  {-9,85},{-13,88},{4,66},{-3,77},{-3,76},{-6,76},{10,58},{-1,76},
  {-1,83},{-7,99},{-14,95},{2,95},{0,76},{-5,74},{0,70},{-11,75},
  {1,68},{0,65},{-14,73},{3,62},{4,62},{-1,68},{-13,75},{11,55},
  {5,64},{12,70},{15,6},{6,19},{7,16},{12,14},{18,13},{13,11},
  {13,15},{15,16},{12,23},{13,23},{15,20},{14,26},{14,44},{17,40},
  {17,47},{24,17},{21,21},{25,22},{31,27},{22,29},{19,35},{14,50},
  {10,57},{7,63},{-2,77},{-4,82},{-3,94},{9,69},{-12,109},{36,-35},
  {36,-34},{32,-26},{37,-30},{44,-32},{34,-18},{34,-15},{40,-15},{33,-7},
  {35,-5},{33,0},{38,2},{33,13},{23,35},{13,58},{29,-3},{26,0},
  {22,30},{31,-7},{35,-15},{34,-3},{34,3},{36,-1},{34,5},{32,11},
  {35,5},{34,12},{39,11},{30,29},{34,26},{29,39},{19,66},{31,21},
  {31,31},{25,50},{-17,120},{-20,112},{-18,114},{-11,85},{-15,92},{-14,89},
  {-26,71},{-15,81},{-14,80},{0,68},{-14,70},{-24,56},{-23,68},{-24,50},
  {-11,74},{23,-13},{26,-13},{40,-15},{49,-14},{44,3},{45,6},{44,34},
  {33,54},{19,82},{-3,75},{-1,23},{1,34},{1,43},{0,54},{-2,55},
  {0,61},{1,64},{0,68},{-9,92},{-14,106},{-13,97},{-15,90},{-12,90},
  {-18,88},{-10,73},{-9,79},{-14,86},{-10,73},{-10,70},{-10,69},{-5,66},
  {-9,64},{-5,58},{2,59},{21,-10},{24,-11},{28,-8},{28,-1},{29,3},
  {29,9},{35,20},{29,36},{14,67}
};

/* ---- rh264_cabac.h ---- */
/* rh264 CABAC arithmetic decoding engine (H.264 clause 9.3). I-slice only. */

typedef struct {
   const uint8_t *buf, *end;
   uint32_t range;      /* codIRange */
   uint32_t offset;     /* codIOffset */
   uint32_t bitbuf;
   int      bitcnt;
   uint8_t  state[1024];
   uint8_t  mps[1024];
} rh264_cabac;

static int rh264_cb_bit(rh264_cabac *c)
{
   int b;
   if (c->bitcnt == 0)
   {
      c->bitbuf = (uint32_t)((c->buf < c->end) ? *c->buf++ : 0);
      c->bitcnt = 8;
   }
   c->bitcnt--;
   b = (int)((c->bitbuf >> c->bitcnt) & 1);
   return b;
}

static void rh264_cabac_init_engine(rh264_cabac *c,
      const uint8_t *buf, const uint8_t *end)
{
   int i;
   c->buf = buf; c->end = end; c->bitbuf = 0; c->bitcnt = 0;
   c->range = 510;
   c->offset = 0;
   for (i = 0; i < 9; i++) c->offset = (c->offset << 1) | (uint32_t)rh264_cb_bit(c);
}

/* 9.3.1.1 context variable initialisation for an I slice at SliceQPy. */
static void rh264_cabac_init_contexts(rh264_cabac *c, int sliceQP)
{
   int i;
   if (sliceQP < 0) sliceQP = 0; else if (sliceQP > 51) sliceQP = 51;
   for (i = 0; i < RH264_CABAC_NCTX; i++)
   {
      int m = rh264_cabac_init_I[i][0];
      int n = rh264_cabac_init_I[i][1];
      int pre = ((m * sliceQP) >> 4) + n;
      if (pre < 1) pre = 1; else if (pre > 126) pre = 126;
      if (pre <= 63) { c->state[i] = (uint8_t)(63 - pre); c->mps[i] = 0; }
      else           { c->state[i] = (uint8_t)(pre - 64); c->mps[i] = 1; }
   }
   /* contexts above the extracted range default to a neutral state; they are
    * only used by High-profile 8x8 residual, which this decoder rejects. */
   for (; i < 1024; i++) { c->state[i] = 0; c->mps[i] = 0; }
}

static int rh264_cabac_decode(rh264_cabac *c, int ctxIdx)
{
   int pState = c->state[ctxIdx];
   int valMPS = c->mps[ctxIdx];
   uint32_t qIdx = (c->range >> 6) & 3;
   uint32_t rLPS = rh264_rangeTabLPS[pState][qIdx];
   int bin;
   c->range -= rLPS;
   if (c->offset >= c->range)
   {
      bin = 1 - valMPS;
      c->offset -= c->range;
      c->range   = rLPS;
      if (pState == 0) c->mps[ctxIdx] = (uint8_t)(1 - valMPS);
      c->state[ctxIdx] = rh264_transIdxLPS[pState];
   }
   else
   {
      bin = valMPS;
      c->state[ctxIdx] = rh264_transIdxMPS[pState];
   }
   while (c->range < 256)
   {
      c->range  <<= 1;
      c->offset  = (c->offset << 1) | (uint32_t)rh264_cb_bit(c);
   }
   return bin;
}

static int rh264_cabac_bypass(rh264_cabac *c)
{
   c->offset = (c->offset << 1) | (uint32_t)rh264_cb_bit(c);
   if (c->offset >= c->range) { c->offset -= c->range; return 1; }
   return 0;
}

static int rh264_cabac_terminate(rh264_cabac *c)
{
   c->range -= 2;
   if (c->offset >= c->range) return 1;
   while (c->range < 256)
   {
      c->range  <<= 1;
      c->offset  = (c->offset << 1) | (uint32_t)rh264_cb_bit(c);
   }
   return 0;
}

/* ---- rh264_cabac_mb.h ---- */
/* rh264 CABAC I-slice macroblock parsing (H.264 clause 9.3.3.1).
 * Parses I_4x4 / I_16x16 / I_PCM macroblocks and their residual using CABAC,
 * filling the same coefficient path the CAVLC decoder feeds so reconstruction
 * (intra prediction, inverse transform, dequant, deblock) is shared. */

/* ---- ctxIdxOffset anchors (spec Table 9-11, I-slice) ---- */
enum {
   CTX_MB_TYPE_I      = 3,   /* mb_type prefix for I slice (3..10)          */
   CTX_CBP_LUMA       = 73,  /* coded_block_pattern luma  (73..76)          */
   CTX_CBP_CHROMA     = 77,  /* coded_block_pattern chroma(77..84)          */
   CTX_QP_DELTA       = 60,  /* mb_qp_delta               (60..63)          */
   CTX_CHROMA_PRED    = 64,  /* intra_chroma_pred_mode    (64..67)          */
   CTX_PREV_I4        = 68,  /* prev_intra4x4_pred_mode_flag(68)            */
   CTX_REM_I4         = 69,  /* rem_intra4x4_pred_mode      (69)            */
   CTX_CBF            = 85,  /* coded_block_flag base       (85..104)       */
   CTX_SIG            = 105, /* significant_coeff_flag frame(105..165)      */
   CTX_LAST           = 166, /* last_significant_coeff_flag (166..226)      */
   CTX_ABS            = 227   /* coeff_abs_level_minus1      (227..275)      */
};

/* coded_block_flag ctxBlockCat base offsets into the 85.. range (spec
 * Table 9-40 style; ctxIdxBlockCatOffset for coded_block_flag). */
/* cat: 0=lumaDC(I16) 1=lumaAC(I16) 2=luma4x4 3=chromaDC 4=chromaAC */
static const int rh264_cbf_catoff[5]  = {0,4,8,12,16};
static const int rh264_sig_catoff[5]  = {0,15,29,44,47};
static const int rh264_last_catoff[5] = {0,15,29,44,47};
static const int rh264_abs_catoff[5]  = {0,10,20,30,39};

/* significant_coeff_flag ctxIdxInc = scan position (0..14) for 4x4 blocks.
  * last_significant likewise. abs_level uses its own numDecodAbsLevel logic. */

/* ===== residual block via CABAC (9.3.2.3, 9.3.3.1.3) ===== */
/* Decode one residual block into coef[] (zig-zag order, length maxNumCoeff).
 * cat = ctxBlockCat (0=I16 lumaDC,1=I16 lumaAC,2=luma4x4,3=chromaDC,4=chromaAC).
 * cbf_ctxinc = coded_block_flag ctxIdxInc from neighbours. Returns coeff count. */
static int rh264_cabac_residual(rh264_cabac *c, int cat, int cbf_ctxinc,
      int maxNumCoeff, int32_t *coef)
{
   int i, coded, nsig=0, sig[16];
   int cbf_ctx = CTX_CBF + rh264_cbf_catoff[cat] + cbf_ctxinc;
   for (i = 0; i < maxNumCoeff; i++) coef[i] = 0;
   coded = rh264_cabac_decode(c, cbf_ctx);
   if (!coded) return 0;
   /* significance map (forward scan) */
   for (i = 0; i < maxNumCoeff - 1; i++)
   {
      int sinc = (cat==3 && i>2) ? 2 : i;
      if (rh264_cabac_decode(c, CTX_SIG + rh264_sig_catoff[cat] + sinc))
      {
         int linc = (cat==3 && i>2) ? 2 : i;
         sig[nsig++] = i;
         if (rh264_cabac_decode(c, CTX_LAST + rh264_last_catoff[cat] + linc))
            break;
      }
   }
   if (i == maxNumCoeff - 1) sig[nsig++] = maxNumCoeff - 1;
   /* levels (reverse scan) — coeff_abs_level_minus1 via the c1/c2 running
    * context scheme (openh264 ParseSignificantCoeffCabac + DecodeUEGLevelCabac).
    * oneCtx (227+catoff) indexed by c1: starts 1, +1 (cap 4) while all decoded
    * levels==1, ->0 permanently after the first level>1. absCtx (232+catoff)
    * indexed by c2: count of level>1 coeffs so far (cap g_kMaxC2). */
   {
      int k;
      int c1 = 1, c2 = 0;
      int oneoff = CTX_ABS + rh264_abs_catoff[cat];       /* 227 + catoff */
      int absoff = CTX_ABS + 5 + rh264_abs_catoff[cat];   /* 232 + catoff */
      int maxc2  = (cat==3) ? 3 : 4;                       /* g_kMaxC2      */
      for (k = nsig-1; k >= 0; k--)
      {
         int lvl = 1, sign;
         if (rh264_cabac_decode(c, oneoff + c1))
         {
            /* level >= 2: DecodeUEGLevel(absCtx + c2) returns the extra amount */
            int add = 0;
            if (rh264_cabac_decode(c, absoff + c2))
            {
               int cnt = 1, tmp = 1;
               add = 0;
               do {
                  tmp = rh264_cabac_decode(c, absoff + c2);
                  add++;
                  cnt++;
               } while (tmp != 0 && cnt != 13);
               if (tmp != 0)
               {
                  /* DecodeExpBypass(iCount=0): unary-length exp-golomb bypass */
                  int icount = 0, s1 = 0, s2 = 0, b;
                  do {
                     b = rh264_cabac_bypass(c);
                     if (b) { s1 += (1 << icount); icount++; }
                  } while (b != 0 && icount != 16);
                  while (icount--) {
                     b = rh264_cabac_bypass(c);
                     if (b) s2 |= (1 << icount);
                  }
                  add += (s1 + s2) + 1;
               }
            }
            lvl = 2 + add;
            c2++; if (c2 > maxc2) c2 = maxc2;
            c1 = 0;
         }
         else if (c1)
         {
            c1++; if (c1 > 4) c1 = 4;
         }
         sign = rh264_cabac_bypass(c);
         coef[sig[k]] = sign ? -lvl : lvl;
      }
   }
   return nsig;
}

/* ---- rh264_cabac_slice.h ---- */
/* rh264 CABAC I-slice decode (clause 9.3.3.1). Parses I_4x4 / I_16x16
 * macroblocks + residual via CABAC and reconstructs with the shared intra /
 * transform / dequant path. 4:2:0, 4x4 transform (Main profile). */

/* per-MB coded_block_flag state (for neighbour ctx of the NEXT block/MB) */
typedef struct {
   int avail;      /* MB decoded                                  */
   int is_i16;     /* I_16x16 (vs I_4x4)                          */
   int chroma_nz;  /* chroma_pred_mode != 0 (for chroma-mode ctx) */
   int cbpLuma;    /* coded_block_pattern luma (4 bits)           */
   int cbpChroma;  /* coded_block_pattern chroma (0/1/2)          */
   int lumaDC;     /* I16 luma-DC cbf                             */
   int luma[16];   /* per-4x4 luma cbf (raster in-MB)             */
   int cDC[2];     /* chroma DC cbf [cb,cr]                       */
   int cAC[2][4];  /* chroma AC cbf [cb,cr][blk]                  */
} rh264_cbf;

/* chroma AC 4x4 block idx 0..3 (2x2), comp 0/1 */
static int rh264_cbf_cac_ctx(int comp,int idx,rh264_cbf *cur,rh264_cbf *L,rh264_cbf *U)
{
   int bx=idx&1, by=idx>>1, a, b;
   if (bx>0) a = cur->cAC[comp][idx-1];
   else      a = L->avail ? L->cAC[comp][by*2+1] : 1;
   if (by>0) b = cur->cAC[comp][idx-2];
   else      b = U->avail ? U->cAC[comp][2+bx] : 1;
   return a + 2*b;
}

/* Decode one I MB. Returns 0 ok, <0 error/unsupported. */
static int rh264_cabac_decode_mb(rh264_cabac *cb, const rh264_sps *sps,
      const rh264_pps *pps, rh264_frame *f, int mbx, int mby,
      int *prevQpDeltaNZ, rh264_cbf *cur, rh264_cbf *L, rh264_cbf *U)
{
   int have_up=(mby>0), have_left=(mbx>0);
   int gw=f->mbw*4, cgw=f->mbw*2, gx0=mbx*4, gy0=mby*4;
   int is_i16, i16mode=0, cbp_luma=0, cbp_chroma=0, chroma_mode=0, ctxInc, i;
   int modes[16];
   uint8_t *Y=f->Y+(mby*16)*f->ystride+mbx*16;
   uint8_t *U8=f->U+(mby*8)*f->cstride+mbx*8;
   uint8_t *V8=f->V+(mby*8)*f->cstride+mbx*8;
   memset(cur,0,sizeof(*cur)); cur->avail=1;

   /* mb_type bin0 ctxIdxInc: neighbours available & NOT I_4x4 */
   ctxInc = (have_left && L->avail && L->is_i16 ? 1:0)
          + (have_up   && U->avail && U->is_i16 ? 1:0);
   if (!rh264_cabac_decode(cb, CTX_MB_TYPE_I + ctxInc))
      is_i16 = 0;
   else {
      if (rh264_cabac_terminate(cb)) return -100;   /* I_PCM unsupported */
      is_i16 = 1;
      cbp_luma   = rh264_cabac_decode(cb, CTX_MB_TYPE_I+3) ? 15 : 0;
      if (rh264_cabac_decode(cb, CTX_MB_TYPE_I+4))
         cbp_chroma = rh264_cabac_decode(cb, CTX_MB_TYPE_I+5) ? 2 : 1;
      i16mode  = rh264_cabac_decode(cb, CTX_MB_TYPE_I+6) << 1;
      i16mode |= rh264_cabac_decode(cb, CTX_MB_TYPE_I+7);
   }
   cur->is_i16 = is_i16;

   if (!is_i16) {
      /* prev/rem intra4x4 modes in block scan order (zig) */
      for (i=0;i<16;i++) {
         int prev = rh264_cabac_decode(cb, CTX_PREV_I4);
         int rmode = -1;
         if (!prev) {
            rmode  = rh264_cabac_decode(cb, CTX_REM_I4);
            rmode |= rh264_cabac_decode(cb, CTX_REM_I4)<<1;
            rmode |= rh264_cabac_decode(cb, CTX_REM_I4)<<2;
         }
         modes[i] = rmode;   /* -1 = use predicted */
      }
   }
   /* intra_chroma_pred_mode: TU cMax=3 (decoded for BOTH I_4x4 and I_16x16) */
   ctxInc = (have_left && L->avail && L->chroma_nz?1:0)
          + (have_up   && U->avail && U->chroma_nz?1:0);
   if (!rh264_cabac_decode(cb, CTX_CHROMA_PRED + ctxInc))
      chroma_mode = 0;
   else if (!rh264_cabac_decode(cb, CTX_CHROMA_PRED+3))
      chroma_mode = 1;
   else
      chroma_mode = rh264_cabac_decode(cb, CTX_CHROMA_PRED+3) ? 3 : 2;
   cur->chroma_nz = (chroma_mode!=0);
   if (!is_i16) {
      /* CBP: luma 4 bins + chroma */
      { int cbp=0, k;
        for (k=0;k<4;k++) {
           int bx=(k&1)*2, by=(k>>1)*2; /* 8x8 index in 4x4 units */
           int a,b,inc;
           /* condTermN = neighbour 8x8 cbp bit == 0 ? 1 : 0 (avail intra) */
           if (bx>0) a = !((cbp>>(k-1))&1);
           else      a = L->avail ? !((L->cbpLuma>>(k+1))&1) : 0;
           if (by>0) b = !((cbp>>(k-2))&1);
           else      b = U->avail ? !((U->cbpLuma>>(k+2))&1) : 0;
           inc = a + 2*b;
           if (rh264_cabac_decode(cb, CTX_CBP_LUMA+inc)) cbp |= (1<<k);
        }
        cbp_luma = 0;
        { int m; for(m=0;m<4;m++) if(cbp&(1<<m)) cbp_luma |= (1<<m); }
        cur->cbpLuma = cbp_luma;
        /* chroma: bin0 (any) then bin1 (dc+ac) */
        { int a,b,inc;
          a = L->avail ? (L->cbpChroma!=0?1:0):0;
          b = U->avail ? (U->cbpChroma!=0?1:0):0;
          inc = a + 2*b;
          if (rh264_cabac_decode(cb, CTX_CBP_CHROMA+inc)) {
             a = L->avail ? (L->cbpChroma==2?1:0):0;
             b = U->avail ? (U->cbpChroma==2?1:0):0;
             inc = a + 2*b;
             cbp_chroma = rh264_cabac_decode(cb, CTX_CBP_CHROMA+4+inc) ? 2 : 1;
          } else cbp_chroma = 0;
        }
      }
      cur->cbpChroma = cbp_chroma;
   } else {
      cur->cbpLuma = cbp_luma; cur->cbpChroma = cbp_chroma;
   }

   /* mb_qp_delta (only if any residual) */
   { int has_res = is_i16 ? 1 : (cbp_luma||cbp_chroma);
     int dqp=0;
     if (has_res) {
        int inc = (*prevQpDeltaNZ)?1:0;
        if (rh264_cabac_decode(cb, CTX_QP_DELTA+inc)) {
           int k=1;
           if (rh264_cabac_decode(cb, CTX_QP_DELTA+2)) {
              k=2;
              while (rh264_cabac_decode(cb, CTX_QP_DELTA+3)) k++;
           }
           /* codeNum k -> signed: +1,-1,+2,... */
           dqp = (k&1) ? (k+1)/2 : -(k/2);
        }
        *prevQpDeltaNZ = (dqp!=0);
     } else *prevQpDeltaNZ = 0;
     f->qp = (f->qp + dqp + 52) % 52;
   }

   /* ============ reconstruction ============ */
   if (is_i16) {
      int32_t dc[16], tmp[16]; int bx,by,k;
      /* I_16x16 blocks contribute DC (mode 2) to neighbour intra4x4 MPM */
      { int yy,xx; for(yy=0;yy<4;yy++) for(xx=0;xx<4;xx++)
           f->i4mode[(gy0+yy)*gw+(gx0+xx)] = 2; }
      rh264_intra16x16(Y,f->ystride,i16mode,have_up,have_left);
      /* luma DC block (cat0) */
      { int inc, a, b, ndc;
        for(k=0;k<16;k++) dc[k]=0;
        a = have_left ? (L->avail ? L->lumaDC : 1) : 1;
        b = have_up   ? (U->avail ? U->lumaDC : 1) : 1;
        inc = a + 2*b;
        ndc = rh264_cabac_residual(cb, 0, inc, 16, dc);
        cur->lumaDC = ndc ? 1 : 0;
      }
      { int per=f->qp/6,rem=f->qp%6,LS=16*rh264_dequant4_v[rem][0];
        int32_t hin[16],hout[16];
        for(k=0;k<16;k++) hin[rh264_zigzag4[k]]=dc[k];
        rh264_ihadamard4x4(hin,hout);
        for(k=0;k<16;k++){ int32_t val=hout[k];
           if(per>=6) val=(val*LS)<<(per-6); else val=(val*LS+(1<<(5-per)))>>(6-per);
           tmp[k]=val; }
      }
      { int bi;
      for (bi=0; bi<16; bi++){
         int32_t ac[16],r[16];
         int raster, inc, a, b, nz=0;
         bx=rh264_blk_x[bi]; by=rh264_blk_y[bi]; raster=by*4+bx;
         { uint8_t *d=Y+(by*4)*f->ystride+bx*4;
         for(k=0;k<16;k++) ac[k]=0;
         if (cbp_luma) {
            int32_t scan[16];
            a = (bx>0)? cur->luma[raster-1] : (L->avail? L->luma[by*4+3]:1);
            b = (by>0)? cur->luma[raster-4] : (U->avail? U->luma[12+bx]:1);
            inc = a + 2*b;
            nz = rh264_cabac_residual(cb, 1, inc, 15, scan); /* AC only, scan order */
            for(k=0;k<15;k++) ac[rh264_zigzag4[k+1]] = scan[k]; /* de-zigzag */
            cur->luma[raster] = nz?1:0;
         }
         ac[0]=tmp[raster]; /* DC from hadamard, raster order */
         { int32_t q[16]; for(k=0;k<16;k++)q[k]=ac[k];
           rh264_dequant4x4(q,f->qp,1); q[0]=ac[0];
           rh264_itransform4x4(q,r); }
         { int yy,xx; for(yy=0;yy<4;yy++)for(xx=0;xx<4;xx++){
            int val=d[yy*f->ystride+xx]+((r[yy*4+xx]+32)>>6);
            d[yy*f->ystride+xx]=(uint8_t)RH264_CLIP(val);} }
         f->nzL[(gy0+by)*gw+(gx0+bx)] = cur->luma[raster];
         }
      } }
   } else {
      /* I_4x4: predict+residual per 4x4 in zig scan order */
      int bi;
      for (bi=0; bi<16; bi++) {
         int bx=rh264_blk_x[bi], by=rh264_blk_y[bi];
         int raster = by*4+bx;
         uint8_t *d=Y+(by*4)*f->ystride+bx*4;
         int predmode, mode=modes[bi];
         int hu=(by>0)||have_up, hl=(bx>0)||have_left;
         int hur; int32_t coef[16],r[16]; int k,nz=0;
         /* predicted mode = min(modeA,modeB) */
         { int mA,mB;
           if (bx>0||have_left) mA=f->i4mode[(gy0+by)*gw+(gx0+bx-1)];
           else mA=-1;
           if (by>0||have_up) mB=f->i4mode[(gy0+by-1)*gw+(gx0+bx)];
           else mB=-1;
           if (mA<0||mB<0) predmode=2; /* DC */
           else predmode=(mA<mB?mA:mB);
         }
         if (mode<0) mode=predmode;
         else if (mode>=predmode) mode++;
         f->i4mode[(gy0+by)*gw+(gx0+bx)]=(uint8_t)mode;
         /* up-right availability (same rule as CAVLC path), keyed by scan bi */
         switch(bi){
            case 2: case 6: case 8: case 9: case 10: case 12: case 14:
               hur=1; break;
            case 3: case 11: case 13: case 15:
               hur=0; break;
            case 0: case 1: case 4:
               hur=have_up; break;
            case 5:
               hur=have_up && (mbx+1 < f->mbw); break;
            case 7:
               hur=0; break;
            default: hur=0; break;
         }
         rh264_intra4x4(d,f->ystride,mode,hu,hl,hur);
         for(k=0;k<16;k++)coef[k]=0;
         if (cbp_luma & (1<<(((by>>1)*2)+(bx>>1)))) {
            int a = (bx>0)? cur->luma[raster-1] : (L->avail? L->luma[by*4+3]:1);
            int b = (by>0)? cur->luma[raster-4] : (U->avail? U->luma[12+bx]:1);
            int inc = a + 2*b;
            int32_t scan[16];
            nz = rh264_cabac_residual(cb, 2, inc, 16, scan);
            for(k=0;k<16;k++)coef[rh264_zigzag4[k]]=scan[k];
            cur->luma[raster]=nz?1:0;
            rh264_dequant4x4(coef,f->qp,0);
            rh264_itransform4x4(coef,r);
            { int yy,xx; for(yy=0;yy<4;yy++)for(xx=0;xx<4;xx++){
               int val=d[yy*f->ystride+xx]+((r[yy*4+xx]+32)>>6);
               d[yy*f->ystride+xx]=(uint8_t)RH264_CLIP(val);} }
         }
         f->nzL[(gy0+by)*gw+(gx0+bx)]=cur->luma[raster];
      }
   }

   /* chroma prediction + residual (4:2:0). Bitstream order (7.3.5.3.1):
    * both chroma DC blocks first, then all chroma AC blocks. */
   rh264_intra_chroma(U8,f->cstride,chroma_mode,have_up,have_left);
   rh264_intra_chroma(V8,f->cstride,chroma_mode,have_up,have_left);
   { int comp, blk, k;
     int32_t dcs[2][4], cdc[2][4], cac[2][4][16];
     int qpc=rh264_chroma_qp(f->qp,f->chroma_qp_offset);
     /* chroma DC for both components */
     for (comp=0; comp<2; comp++) {
        for(k=0;k<4;k++) dcs[comp][k]=0;
        if (cbp_chroma) {
           int a = have_left ? (L->avail? L->cDC[comp]:1):1;
           int b = have_up   ? (U->avail? U->cDC[comp]:1):1;
           int inc = a + 2*b;
           int ndc = rh264_cabac_residual(cb, 3, inc, 4, dcs[comp]);
           cur->cDC[comp] = ndc ? 1 : 0;
        }
        { int per=qpc/6,rem=qpc%6,LS=16*rh264_dequant4_v[rem][0];
          int32_t e[4];
          e[0]=dcs[comp][0]+dcs[comp][1]+dcs[comp][2]+dcs[comp][3];
          e[1]=dcs[comp][0]-dcs[comp][1]+dcs[comp][2]-dcs[comp][3];
          e[2]=dcs[comp][0]+dcs[comp][1]-dcs[comp][2]-dcs[comp][3];
          e[3]=dcs[comp][0]-dcs[comp][1]-dcs[comp][2]+dcs[comp][3];
          for(k=0;k<4;k++){ int32_t v=e[k]; v=((v*LS)<<per)>>5; cdc[comp][k]=v; }
        }
     }
     /* chroma AC for both components */
     for (comp=0; comp<2; comp++)
        for (blk=0; blk<4; blk++) {
           int bx=blk&1, by=blk>>1;
           for(k=0;k<16;k++) cac[comp][blk][k]=0;
           if (cbp_chroma==2) {
              int inc = rh264_cbf_cac_ctx(comp,blk,cur,L,U);
              int32_t scan[16]; int nz;
              nz = rh264_cabac_residual(cb, 4, inc, 15, scan);
              for(k=0;k<15;k++) cac[comp][blk][rh264_zigzag4[k+1]]=scan[k];
              cur->cAC[comp][blk]=nz?1:0;
              (void)bx;(void)by;
           }
        }
     /* reconstruct both components */
     for (comp=0; comp<2; comp++) {
        uint8_t *P = comp? V8:U8;
        for (blk=0; blk<4; blk++) {
           int bx=blk&1, by=blk>>1;
           uint8_t *d=P+(by*4)*f->cstride+bx*4;
           int32_t q[16],r[16];
           for(k=0;k<16;k++) q[k]=cac[comp][blk][k];
           rh264_dequant4x4(q,qpc,1); q[0]=cdc[comp][blk];
           rh264_itransform4x4(q,r);
           { int yy,xx; for(yy=0;yy<4;yy++)for(xx=0;xx<4;xx++){
              int val=d[yy*f->cstride+xx]+((r[yy*4+xx]+32)>>6);
              d[yy*f->cstride+xx]=(uint8_t)RH264_CLIP(val);} }
           f->nzC[comp][(mby*2+by)*cgw+(mbx*2+bx)]=cur->cAC[comp][blk];
        }
     }
   }
   f->mbqp[mby*f->mbw+mbx]=(uint8_t)f->qp;
   (void)sps;
   return 0;
}


/* Full CABAC I-slice decode. b is positioned right after the slice header.
 * Returns 0 on success. */
static int rh264_cabac_decode_islice(rh264_bits *b, const rh264_sps *sps,
      const rh264_pps *pps, rh264_slice_hdr *sh, rh264_frame *f)
{
   rh264_cabac cb;
   rh264_cbf *row, *cur, *L, *U, dummy;
   int mbw=f->mbw, mbh=f->mbh, mbx, mby, prevQpNZ=0, rc=0;
   size_t bytepos;
   /* cabac_alignment_one_bits: consume 1-bits until byte aligned */
   while (b->bitpos & 7) { if (!rh264_u1(b)) break; }
   bytepos = (b->bitpos + 7) >> 3;
   rh264_cabac_init_engine(&cb, b->buf + bytepos, b->buf + b->size);
   rh264_cabac_init_contexts(&cb, sh->slice_qp);
   f->qp = sh->slice_qp;
   f->chroma_qp_offset = pps->chroma_qp_index_offset;

   /* per-MB cbf caches: a full row for 'up', plus 'left' tracking */
   row = (rh264_cbf*)calloc((size_t)mbw+2, sizeof(rh264_cbf));
   if (!row) return -1;
   memset(&dummy,0,sizeof(dummy));

   for (mby=0; mby<mbh; mby++) {
      rh264_cbf leftcbf; memset(&leftcbf,0,sizeof(leftcbf));
      for (mbx=0; mbx<mbw; mbx++) {
         rh264_cbf tmp;
         cur = &tmp;
         L = (mbx>0) ? &leftcbf : &dummy;
         U = &row[mbx];
         rc = rh264_cabac_decode_mb(&cb, sps, pps, f, mbx, mby,
               &prevQpNZ, cur, L, U);
         if (rc < 0) { free(row); return rc; }
         leftcbf = tmp;      /* becomes left for next MB */
         row[mbx] = tmp;     /* becomes up for next row  */
         /* end_of_slice_flag */
         if (mbx==mbw-1 && mby==mbh-1) break;
         if (rh264_cabac_terminate(&cb)) { mby=mbh; break; }
      }
   }
   free(row);
   return 0;
}

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
   /* Main/High-profile streams use CABAC entropy coding; baseline uses CAVLC.
    * Dispatch on the PPS entropy_coding_mode_flag. Both paths are intra-only. */
   if (v->pps.entropy_coding_mode_flag)
      rc = rh264_cabac_decode_islice(&b, &v->sps, &v->pps, &sh, &v->f);
   else
      rc = rh264_decode_islice(&b, &v->sps, &v->pps, &sh, &v->f);
   if (rc == 0) rh264_deblock(&v->f, &sh);
   free(rbsp);
   return rc;
}

/* Copy the pixel planes of src into dst (both already allocated at the same
 * geometry). Used to snapshot the just-decoded frame as the reference for the
 * next inter picture. */
static void rh264_frame_copy_planes(rh264_frame *dst, const rh264_frame *src)
{
   memcpy(dst->Y, src->Y, (size_t)src->ystride * src->mbh * 16);
   memcpy(dst->U, src->U, (size_t)src->cstride * src->mbh * 8);
   memcpy(dst->V, src->V, (size_t)src->cstride * src->mbh * 8);
   dst->qp = src->qp;
   dst->chroma_qp_offset = src->chroma_qp_offset;
}

/* Decode one non-IDR (type-1) slice. Only P-slices with CAVLC entropy coding
 * are currently supported; the previous decoded frame in v->ref is the single
 * reference. Returns 0 on success, negative otherwise (e.g. B-slice, CABAC P,
 * or no reference yet). */
static int rh264_video_decode_inter(rh264_video *v, const uint8_t *nal, size_t len)
{
   int nut, nri, rc;
   size_t rl;
   uint8_t *rbsp;
   rh264_bits b;
   rh264_slice_hdr sh;
   if (len < 1) return -1;
   if (!v->have_ref) return -1;   /* need a decoded reference first */
   nut = nal[0] & 0x1f;
   nri = (nal[0] >> 5) & 3;
   rbsp = rh264_unescape(nal + 1, len - 1, &rl);
   if (!rbsp) return -1;
   rh264_bits_init(&b, rbsp, rl);
   if (!rh264_parse_slice_header_adv(&b, nut, nri, &v->sps, &v->pps, &sh))
   { free(rbsp); return -1; }   /* unsupported slice type (e.g. B) */
   if (sh.slice_type != RH264_SLICE_P && sh.slice_type != RH264_SLICE_SP)
   { free(rbsp); return -1; }
   if (v->pps.entropy_coding_mode_flag)
   { free(rbsp); return -1; }   /* CABAC P-slices not yet supported */
   /* The reference is the previous output frame (v->ref); decode into v->f. */
   rc = rh264_decode_pslice(&b, &v->sps, &v->pps, &sh, &v->f, &v->ref, v->mvg);
   if (rc == 0) rh264_deblock_pslice(&v->f, &sh, v->mvg);
   free(rbsp);
   return rc;
}

/* Walk NAL units in either length-prefixed (AVCC) or Annex-B form and act on
 * SPS/PPS + the first coded slice (IDR or non-IDR). */
static int rh264_video_handle_slice_nal(rh264_video *v, const uint8_t *nal,
      size_t nl, int *got_pic)
{
   int type = nal[0] & 0x1f;
   if (type == 7 || type == 8) { rh264_video_take_ps(v, nal, nl); return 0; }
   if (type == 5)
   {
      if (!v->have_sps || !v->have_pps) return -1;
      if (rh264_frame_alloc_if_needed(v) != 0) return -1;
      if (rh264_video_decode_idr(v, nal, nl) != 0) return -1;
      /* Snapshot as the reference for subsequent inter pictures. */
      rh264_frame_copy_planes(&v->ref, &v->f);
      v->have_ref = 1;
      *got_pic = 1;
      return 1;   /* one coded picture per call */
   }
   if (type == 1)
   {
      if (!v->have_sps || !v->have_pps) return -1;
      if (rh264_frame_alloc_if_needed(v) != 0) return -1;
      if (rh264_video_decode_inter(v, nal, nl) != 0) return -1;
      /* This frame becomes the reference for the next inter picture. */
      rh264_frame_copy_planes(&v->ref, &v->f);
      v->have_ref = 1;
      *got_pic = 1;
      return 1;
   }
   return 0;
}

int rh264_video_decode(rh264_video *v, const uint8_t *data, size_t len)
{
   size_t p = 0;
   int got_pic = 0;
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
            int r = rh264_video_handle_slice_nal(v, data + p, nl, &got_pic);
            if (r < 0) return -1;
            if (r == 1) break;   /* one coded picture per call */
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
            int r = rh264_video_handle_slice_nal(v, data + s, e - s, &got_pic);
            if (r < 0) return -1;
            if (r == 1) break;   /* one coded picture per call */
         }
         p = e;
      }
   }
   return got_pic ? 0 : -1;
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
