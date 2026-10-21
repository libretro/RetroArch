/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_H
#define RARCH_VAAPI_HEVC_H
#include <va/va.h>
#include <formats/rh265_hw.h>

/* Worker-owned prepared picture and live surface IDs. No VA calls or leases.
 * The initial adapter excludes PCM, tiles and dependent slice segments.
 * False leaves the caller's submission buffer unchanged. */
static INLINE bool vaapi_hevc_picture(VAPictureParameterBufferHEVC *out,
      const rh265_video_params_t *s,const rh265_access_unit_t *au,
      const rh265_hw_picture_t *p,const VASurfaceID *surfaces,unsigned slots)
{
   VAPictureParameterBufferHEVC v;
   const rh265_slice_info_t *h;
   unsigned i,j,list,used=0;
   uint32_t flags[15];
   if (!out || !s || !au || !p || !surfaces || !slots || slots>32
         || p->slot>=slots || p->num_refs>15 || !au->num_slices
         || s->chroma_format_idc!=1 || (s->bit_depth!=8 && s->bit_depth!=10)
         || s->pcm_enabled || s->tiles_enabled || s->dependent_slice_segments_enabled
         || s->width<8 || s->width>65535 || s->height<8 || s->height>65535
         || p->width!=(unsigned)s->width || p->height!=(unsigned)s->height
         || p->bit_depth!=(unsigned)s->bit_depth
         || s->log2_min_cb<3 || s->log2_min_cb>6
         || s->log2_ctb<s->log2_min_cb || s->log2_ctb>6
         || s->log2_min_tb<2 || s->log2_min_tb>5
         || s->log2_max_tb<s->log2_min_tb || s->log2_max_tb>5
         || s->log2_max_tb>s->log2_ctb
         || (s->width&((1<<s->log2_min_cb)-1))
         || (s->height&((1<<s->log2_min_cb)-1))
         || s->max_dec_pic_buffering<1 || s->max_dec_pic_buffering>16
         || p->num_refs>=(unsigned)s->max_dec_pic_buffering
         || s->max_num_reorder_pics<0 || s->max_num_reorder_pics>=s->max_dec_pic_buffering
         || s->log2_max_poc_lsb<4 || s->log2_max_poc_lsb>16
         || p->poc_log2!=(unsigned)s->log2_max_poc_lsb
         || s->num_st_rps<0 || s->num_st_rps>64
         || s->num_long_term_ref_pics<0 || s->num_long_term_ref_pics>32
         || s->num_ref_idx_l0_default<1 || s->num_ref_idx_l0_default>15
         || s->num_ref_idx_l1_default<1 || s->num_ref_idx_l1_default>15
         || s->init_qp<-(s->bit_depth-8)*6 || s->init_qp>51
         || s->cb_qp_offset< -12 || s->cb_qp_offset>12
         || s->cr_qp_offset< -12 || s->cr_qp_offset>12
         || s->beta_offset_div2< -6 || s->beta_offset_div2>6
         || s->tc_offset_div2< -6 || s->tc_offset_div2>6
         || s->log2_parallel_merge_level<2 || s->log2_parallel_merge_level>s->log2_ctb
         || s->diff_cu_qp_delta_depth<0 || s->diff_cu_qp_delta_depth>s->log2_ctb-s->log2_min_cb
         || s->max_transform_hierarchy_depth_inter<0
         || s->max_transform_hierarchy_depth_inter>s->log2_ctb-s->log2_min_tb
         || s->max_transform_hierarchy_depth_intra<0
         || s->max_transform_hierarchy_depth_intra>s->log2_ctb-s->log2_min_tb
         || s->num_extra_slice_header_bits<0 || s->num_extra_slice_header_bits>7
         || surfaces[p->slot]==VA_INVALID_SURFACE) return false;
   h=&au->first;
   if (h->num_negative<0 || h->num_negative>16 || h->num_positive<0
         || h->num_positive>16 || h->long_term.count>16
         || h->nal_type<0 || h->nal_type>21 || (h->nal_type>9 && h->nal_type<16)
         || h->slice_type<0 || h->slice_type>2 || !au->slice_types
         || (au->slice_types&~7u) || !(au->slice_types&(1u<<h->slice_type))
         || !!au->intra!=(au->slice_types==4)) return false;
   for (i=0;i<p->num_refs;i++)
   {
      if (p->refs[i].slot>=slots || p->refs[i].slot==p->slot
            || p->refs[i].long_term>1 || surfaces[p->refs[i].slot]==VA_INVALID_SURFACE)
         return false;
      for (j=0;j<i;j++)
         if (p->refs[i].slot==p->refs[j].slot || p->refs[i].poc==p->refs[j].poc
               || surfaces[p->refs[i].slot]==surfaces[p->refs[j].slot]) return false;
      if (surfaces[p->refs[i].slot]==surfaces[p->slot]) return false;
      flags[i]=p->refs[i].long_term?VA_PICTURE_HEVC_LONG_TERM_REFERENCE:0;
   }
   for (list=0;list<3;list++)
   {
      unsigned count=list==2?h->long_term.count:(unsigned)(list?h->num_positive:h->num_negative);
      for (i=0;i<count;i++)
      {
         unsigned found=p->num_refs;
         uint32_t flag=list==2?VA_PICTURE_HEVC_RPS_LT_CURR:
            list?VA_PICTURE_HEVC_RPS_ST_CURR_AFTER:VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE;
         int64_t poc;
         unsigned active;
         if (list==2)
         {
            uint32_t mask=(1u<<s->log2_max_poc_lsb)-1;
            if (!s->long_term_ref_pics_present || h->long_term.used[i]>1
                  || h->long_term.msb_present[i]>1 || h->long_term.poc_lsb[i]>mask)
               return false;
            active=h->long_term.used[i];
            poc=(int64_t)p->poc-h->poc_lsb+h->long_term.poc_lsb[i]
               -(int64_t)h->long_term.msb_cycle[i]*(mask+1);
            for (j=0;j<p->num_refs;j++)
               if (p->refs[j].long_term && (h->long_term.msb_present[i]
                        ? p->refs[j].poc==poc
                        : ((uint32_t)p->refs[j].poc&mask)==h->long_term.poc_lsb[i]))
               { if (found!=p->num_refs) return false; found=j; }
         }
         else
         {
            const int32_t *delta=list?h->delta_poc_after:h->delta_poc_before;
            const uint8_t *use=list?h->used_after:h->used_before;
            if (use[i]>1 || (list?delta[i]<=0:delta[i]>=0)
                  || (i && (list?delta[i]<=delta[i-1]:delta[i]>=delta[i-1]))) return false;
            active=use[i]; poc=(int64_t)p->poc+delta[i];
            if (poc<INT32_MIN || poc>INT32_MAX) return false;
            for (j=0;j<p->num_refs;j++)
               if (!p->refs[j].long_term && p->refs[j].poc==poc) found=j;
         }
         if (!active) continue;
         if (found==p->num_refs || used++>=8
               || (flags[found]&(VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE
                  |VA_PICTURE_HEVC_RPS_ST_CURR_AFTER|VA_PICTURE_HEVC_RPS_LT_CURR))) return false;
         flags[found]|=flag;
      }
   }
   memset(&v,0,sizeof(v));
   v.CurrPic.picture_id=surfaces[p->slot]; v.CurrPic.pic_order_cnt=p->poc;
   for (i=0;i<15;i++)
   {
      v.ReferenceFrames[i].picture_id=VA_INVALID_SURFACE;
      v.ReferenceFrames[i].flags=VA_PICTURE_HEVC_INVALID;
      if (i<p->num_refs)
      {
         v.ReferenceFrames[i].picture_id=surfaces[p->refs[i].slot];
         v.ReferenceFrames[i].pic_order_cnt=p->refs[i].poc;
         v.ReferenceFrames[i].flags=flags[i];
      }
   }
   v.pic_width_in_luma_samples=(uint16_t)s->width;
   v.pic_height_in_luma_samples=(uint16_t)s->height;
#define VAAPI_HEVC_PIC(dst,src) v.pic_fields.bits.dst=!!s->src
   v.pic_fields.bits.chroma_format_idc=1;
   VAAPI_HEVC_PIC(scaling_list_enabled_flag,scaling_list_enabled);
   VAAPI_HEVC_PIC(transform_skip_enabled_flag,transform_skip_enabled);
   VAAPI_HEVC_PIC(amp_enabled_flag,amp_enabled);
   VAAPI_HEVC_PIC(strong_intra_smoothing_enabled_flag,strong_intra_smoothing);
   VAAPI_HEVC_PIC(sign_data_hiding_enabled_flag,sign_data_hiding);
   VAAPI_HEVC_PIC(constrained_intra_pred_flag,constrained_intra_pred);
   VAAPI_HEVC_PIC(cu_qp_delta_enabled_flag,cu_qp_delta_enabled);
   VAAPI_HEVC_PIC(weighted_pred_flag,weighted_pred);
   VAAPI_HEVC_PIC(weighted_bipred_flag,weighted_bipred);
   VAAPI_HEVC_PIC(transquant_bypass_enabled_flag,transquant_bypass_enabled);
   VAAPI_HEVC_PIC(entropy_coding_sync_enabled_flag,entropy_coding_sync_enabled);
   VAAPI_HEVC_PIC(pps_loop_filter_across_slices_enabled_flag,loop_filter_across_slices);
#undef VAAPI_HEVC_PIC
   v.pic_fields.bits.NoPicReorderingFlag=!s->max_num_reorder_pics;
   v.pic_fields.bits.NoBiPredFlag=!(au->slice_types&1u);
   v.sps_max_dec_pic_buffering_minus1=(uint8_t)(s->max_dec_pic_buffering-1);
   v.bit_depth_luma_minus8=v.bit_depth_chroma_minus8=(uint8_t)(s->bit_depth-8);
   v.log2_min_luma_coding_block_size_minus3=(uint8_t)(s->log2_min_cb-3);
   v.log2_diff_max_min_luma_coding_block_size=(uint8_t)(s->log2_ctb-s->log2_min_cb);
   v.log2_min_transform_block_size_minus2=(uint8_t)(s->log2_min_tb-2);
   v.log2_diff_max_min_transform_block_size=(uint8_t)(s->log2_max_tb-s->log2_min_tb);
   v.max_transform_hierarchy_depth_intra=(uint8_t)s->max_transform_hierarchy_depth_intra;
   v.max_transform_hierarchy_depth_inter=(uint8_t)s->max_transform_hierarchy_depth_inter;
   v.init_qp_minus26=(int8_t)(s->init_qp-26);
   v.diff_cu_qp_delta_depth=(uint8_t)s->diff_cu_qp_delta_depth;
   v.pps_cb_qp_offset=(int8_t)s->cb_qp_offset; v.pps_cr_qp_offset=(int8_t)s->cr_qp_offset;
   v.log2_parallel_merge_level_minus2=(uint8_t)(s->log2_parallel_merge_level-2);
#define VAAPI_HEVC_SLICE(dst,src) v.slice_parsing_fields.bits.dst=!!s->src
   VAAPI_HEVC_SLICE(lists_modification_present_flag,lists_modification_present);
   VAAPI_HEVC_SLICE(long_term_ref_pics_present_flag,long_term_ref_pics_present);
   VAAPI_HEVC_SLICE(sps_temporal_mvp_enabled_flag,temporal_mvp_enabled);
   VAAPI_HEVC_SLICE(cabac_init_present_flag,cabac_init_present);
   VAAPI_HEVC_SLICE(output_flag_present_flag,output_flag_present);
   VAAPI_HEVC_SLICE(pps_slice_chroma_qp_offsets_present_flag,slice_chroma_qp_offsets_present);
   VAAPI_HEVC_SLICE(sample_adaptive_offset_enabled_flag,sao_enabled);
   VAAPI_HEVC_SLICE(deblocking_filter_override_enabled_flag,deblocking_filter_override_enabled);
   VAAPI_HEVC_SLICE(pps_disable_deblocking_filter_flag,deblocking_filter_disabled);
   VAAPI_HEVC_SLICE(slice_segment_header_extension_present_flag,slice_segment_header_extension_present);
#undef VAAPI_HEVC_SLICE
   v.slice_parsing_fields.bits.RapPicFlag=h->nal_type>=16;
   v.slice_parsing_fields.bits.IdrPicFlag=h->nal_type==19 || h->nal_type==20;
   v.slice_parsing_fields.bits.IntraPicFlag=!!au->intra;
   v.log2_max_pic_order_cnt_lsb_minus4=(uint8_t)(s->log2_max_poc_lsb-4);
   v.num_short_term_ref_pic_sets=(uint8_t)s->num_st_rps;
   v.num_long_term_ref_pic_sps=(uint8_t)s->num_long_term_ref_pics;
   v.num_ref_idx_l0_default_active_minus1=(uint8_t)(s->num_ref_idx_l0_default-1);
   v.num_ref_idx_l1_default_active_minus1=(uint8_t)(s->num_ref_idx_l1_default-1);
   v.pps_beta_offset_div2=(int8_t)s->beta_offset_div2;
   v.pps_tc_offset_div2=(int8_t)s->tc_offset_div2;
   v.num_extra_slice_header_bits=(uint8_t)s->num_extra_slice_header_bits;
   v.st_rps_bits=h->short_term_rps_bits;
   *out=v; return true;
}
static INLINE bool vaapi_hevc_matrix(VAIQMatrixBufferHEVC *out,
      const rh265_video_params_t *s)
{
   if (!out || !s || !s->scaling_list_enabled) return false;
   memset(out,0,sizeof(*out));
   memcpy(out->ScalingList4x4,s->list4,sizeof(s->list4));
   memcpy(out->ScalingList8x8,s->list8,sizeof(s->list8));
   memcpy(out->ScalingList16x16,s->list16,sizeof(s->list16));
   memcpy(out->ScalingList32x32,s->list32,sizeof(s->list32));
   memcpy(out->ScalingListDC16x16,s->dc16,sizeof(s->dc16));
   memcpy(out->ScalingListDC32x32,s->dc32,sizeof(s->dc32));
   return true;
}
#endif
