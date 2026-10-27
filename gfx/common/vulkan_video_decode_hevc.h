/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Included after the shared Vulkan decoder helpers. */

static bool vvd_hevc_params(vvd_dec_t *d)
{
   VkVideoDecodeH265SessionParametersAddInfoKHR add;
   VkVideoDecodeH265SessionParametersCreateInfoKHR hevc;
   VkVideoSessionParametersCreateInfoKHR info;
   memset(&add, 0, sizeof(add)); memset(&hevc, 0, sizeof(hevc)); memset(&info, 0, sizeof(info));
   add.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR;
   add.stdVPSCount = add.stdSPSCount = add.stdPPSCount = 1;
   add.pStdVPSs = &d->hevc->vps; add.pStdSPSs = &d->hevc->sps; add.pStdPPSs = &d->hevc->pps;
   hevc.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR;
   hevc.maxStdVPSCount = hevc.maxStdSPSCount = hevc.maxStdPPSCount = 1;
   hevc.pParametersAddInfo = &add;
   info.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR;
   info.pNext = &hevc; info.videoSession = d->session;
   if (vvd.create_params(vvd.device, &info, NULL, &d->params) != VK_SUCCESS)
   { d->params = VK_NULL_HANDLE; return false; }
   return true;
}

static bool vvd_hevc_profile(vvd_dec_t *d, VkVideoCapabilitiesKHR *caps)
{
   VkVideoDecodeH265CapabilitiesKHR hevc;
   VkVideoDecodeCapabilitiesKHR decode;
   memset(&d->h265_profile, 0, sizeof(d->h265_profile));
   memset(&d->profile, 0, sizeof(d->profile));
   memset(&hevc, 0, sizeof(hevc)); memset(&decode, 0, sizeof(decode)); memset(caps, 0, sizeof(*caps));
   d->h265_profile.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR;
   d->h265_profile.stdProfileIdc = d->hevc->sps_ptl.general_profile_idc;
   d->profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
   d->profile.pNext = &d->h265_profile;
   d->profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
   d->profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
   d->profile.lumaBitDepth = d->profile.chromaBitDepth = d->hevc->params.bit_depth == 10
      ? VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR : VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
   hevc.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR;
   decode.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR; decode.pNext = &hevc;
   caps->sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR; caps->pNext = &decode;
   if (vvd.get_caps(vvd.gpu, &d->profile, caps) != VK_SUCCESS
         || d->coded.width < caps->minCodedExtent.width || d->coded.width > caps->maxCodedExtent.width
         || d->coded.height < caps->minCodedExtent.height || d->coded.height > caps->maxCodedExtent.height
         || caps->maxDpbSlots < d->slots
         || caps->maxActiveReferencePictures < (unsigned)(d->hevc->params.max_dec_pic_buffering - 1)
         || hevc.maxLevelIdc < d->hevc->sps_ptl.general_level_idc
         || !(decode.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR)) return false;
   d->profiles.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR;
   d->profiles.profileCount = 1; d->profiles.pProfiles = &d->profile;
   d->hevc_max_level = hevc.maxLevelIdc;
   caps->pNext = NULL;
   return true;
}

static bool vvd_hevc_supported(const vulkan_hevc_params_t *p)
{
   return ((p->params.bit_depth == 8 && p->sps_ptl.general_profile_idc == STD_VIDEO_H265_PROFILE_IDC_MAIN)
         || (p->params.bit_depth == 10 && p->sps_ptl.general_profile_idc == STD_VIDEO_H265_PROFILE_IDC_MAIN_10))
      && !p->sequence.field_seq;
}

static int vvd_hevc_decode(void *dec, const rh265_video_params_t *params,
      const rh265_access_unit_t *au, const rh265_nal_t *slices,
      const rh265_hw_picture_t *pic)
{
   vvd_dec_t *d = (vvd_dec_t*)dec;
   StdVideoDecodeH265PictureInfo std_pic;
   VkVideoDecodeH265PictureInfoKHR info;
   unsigned i, n = pic->num_refs, k = d->dring % VVD_RING;
   size_t total = 0, pos = 0;
   VkDeviceSize size;
   int ready;
   if (pic->slot >= d->slots || n > 15 || !au->num_slices || au->num_slices > 64) return -1;
   if (memcmp(params, &d->hevc->params, sizeof(*params)))
   {
      vulkan_hevc_params_t *candidate, *old;
      VkVideoSessionParametersKHR old_params;
      ready = vvd_try_wait(d->tl, d->tl_value);
      if (ready) return ready;
      candidate = (vulkan_hevc_params_t*)calloc(1, sizeof(*candidate));
      if (!candidate) return -1;
      if (!vulkan_hevc_params_build(candidate, d->hevc_source, (unsigned)params->pps_id)
            || !vvd_hevc_supported(candidate)
            || candidate->params.bit_depth != d->hevc->params.bit_depth
            || memcmp(params, &candidate->params, sizeof(*params))
            || candidate->sps_ptl.general_level_idc > d->hevc_max_level
            || params->width != (int)d->coded.width || params->height != (int)d->coded.height
            || params->max_dec_pic_buffering >= (int)d->slots
            || params->crop_left != (int)d->crop_x || params->crop_top != (int)d->crop_y
            || params->width - params->crop_left - params->crop_right != (int)d->visible_w
            || params->height - params->crop_top - params->crop_bottom != (int)d->visible_h)
      { free(candidate); return -1; }
      old = d->hevc; old_params = d->params;
      d->hevc = candidate; d->params = VK_NULL_HANDLE;
      if (!vvd_hevc_params(d))
      { d->hevc = old; d->params = old_params; free(candidate); return -1; }
      vvd.destroy_params(vvd.device, old_params, NULL); free(old);
   }
   if (!vulkan_hevc_picture(&std_pic, d->hevc->sequence.vps_id, params, au, pic)) return -1;
   for (i = 0; i < au->num_slices; i++)
   {
      if (!slices[i].data || slices[i].size < 2 || slices[i].size > UINT32_MAX - 3
            || total > UINT32_MAX - 3 - slices[i].size) return -1;
      total += 3 + slices[i].size;
   }
   ready = vvd_try_wait(d->tl, d->dcb_value[k]);
   if (ready) return ready;
   size = ((VkDeviceSize)total + d->bs_size_align - 1) / d->bs_size_align * d->bs_size_align;
   if (size < total || size > SIZE_MAX || !vvd_bitstream(d, k, size)) return -1;
   for (i = 0; i < au->num_slices; i++)
   {
      uint8_t *dst = (uint8_t*)d->bs_map[k] + pos;
      d->hevc_offsets[i] = (uint32_t)pos;
      dst[0] = dst[1] = 0; dst[2] = 1;
      memcpy(dst + 3, slices[i].data, slices[i].size);
      pos += 3 + slices[i].size;
   }
   memset((uint8_t*)d->bs_map[k] + total, 0, (size_t)size - total);
   if (!d->bs_coherent[k])
   {
      VkMappedMemoryRange range;
      range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE; range.pNext = NULL;
      range.memory = d->bs_mem[k]; range.offset = 0; range.size = VK_WHOLE_SIZE;
      if (vkFlushMappedMemoryRanges(vvd.device, 1, &range) != VK_SUCCESS) return -1;
   }
   for (i = 0; i <= n; i++)
   {
      unsigned slot = i == n ? pic->slot : pic->refs[i].slot;
      if (slot >= d->slots || (i < n && slot == pic->slot)) return -1;
      memset(&d->hevc_ref[i], 0, sizeof(d->hevc_ref[i]));
      d->hevc_ref[i].PicOrderCntVal = i == n ? pic->poc : pic->refs[i].poc;
      d->hevc_ref[i].flags.used_for_long_term_reference = i < n && pic->refs[i].long_term;
      d->hevc_ref[i].flags.unused_for_reference = i == n && !pic->reference;
      d->hevc_slot[i].sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_DPB_SLOT_INFO_KHR;
      d->hevc_slot[i].pNext = NULL; d->hevc_slot[i].pStdReferenceInfo = &d->hevc_ref[i];
      d->res[i] = vvd_resource(d, slot);
      d->slots_info[i].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
      d->slots_info[i].pNext = &d->hevc_slot[i]; d->slots_info[i].slotIndex = (int32_t)slot;
      d->slots_info[i].pPictureResource = &d->res[i];
      d->begin_slots[i] = d->slots_info[i]; d->begin_slots[i].pNext = NULL;
      if (i == n) d->begin_slots[i].slotIndex = -1;
   }
   info.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PICTURE_INFO_KHR;
   info.pNext = NULL; info.pStdPictureInfo = &std_pic;
   info.sliceSegmentCount = au->num_slices; info.pSliceSegmentOffsets = d->hevc_offsets;
   return vvd_record(d, k, size, n, pic->slot, &info);
}

static bool vvd_hevc_open(void *user, const rh265_video *source,
      const rh265_video_params_t *params, unsigned slots, rh265_hw_ops_t *ops)
{
   vvd_dec_t *d;
   VkVideoCapabilitiesKHR caps;
   unsigned lane, gx, gy;
   (void)user;
   if (!vvd.up || !source || !params || !ops || slots > 16
         || !(vvd.codecs & VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)) return false;
   vvd_grave_reap(false);
   for (lane = 0; lane < vvd.lanes; lane++)
      if (retro_atomic_cas_int(&vvd.lane_busy[lane], 0, 1)) break;
   if (lane == vvd.lanes) return false;
   d = (vvd_dec_t*)calloc(1, sizeof(*d));
   if (!d) { vvd_lane_release(lane); return false; }
   d->lane = lane; d->slots = slots; d->hevc_source = source;
   d->hevc = (vulkan_hevc_params_t*)calloc(1, sizeof(*d->hevc));
   if (!d->hevc || !vulkan_hevc_params_build(d->hevc, source, (unsigned)params->pps_id)
         || !vvd_hevc_supported(d->hevc)) goto fail;
   d->format = params->bit_depth == 10 ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16
      : VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
   d->pixfmt = params->bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   d->coded.width = (unsigned)params->width; d->coded.height = (unsigned)params->height;
   d->crop_x = (unsigned)params->crop_left; d->crop_y = (unsigned)params->crop_top;
   d->visible_w = (unsigned)(params->width - params->crop_left - params->crop_right);
   d->visible_h = (unsigned)(params->height - params->crop_top - params->crop_bottom);
   vkGetDeviceQueue(vvd.device, vvd.dec_family, lane, &d->dq);
   vkGetDeviceQueue(vvd.device, vvd.xfer_family, vvd.xfer_first + lane, &d->xq);
   if (!vvd_hevc_profile(d, &caps)) goto fail;
   gx = caps.pictureAccessGranularity.width ? caps.pictureAccessGranularity.width : 1;
   gy = caps.pictureAccessGranularity.height ? caps.pictureAccessGranularity.height : 1;
   d->image.width = (d->coded.width + gx - 1) / gx * gx;
   d->image.height = (d->coded.height + gy - 1) / gy * gy;
   d->bs_offset_align = caps.minBitstreamBufferOffsetAlignment ? caps.minBitstreamBufferOffsetAlignment : 1;
   d->bs_size_align = caps.minBitstreamBufferSizeAlignment ? caps.minBitstreamBufferSizeAlignment : 1;
   if (!vvd_format_ok(d, VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR
            | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
         || !vvd_make_session(d, &caps) || !vvd_make_dpb(d, &caps)
         || !vvd_make_outputs(d) || !vvd_make_commands(d) || !(d->tl = vvd_timeline())
         || !vvd_hevc_params(d)) goto fail;
   ops->decoder = d; ops->decode = vvd_hevc_decode; ops->output = vvd_output;
   ops->drain = NULL;
   retro_atomic_fetch_add_int(&vvd.live, 1);
   return true;
fail:
   vvd_destroy(d); vvd_lane_release(lane);
   return false;
}

static void vvd_hevc_close(rh265_hw_ops_t *ops)
{
   if (!ops) return;
   vvd_close(ops->decoder); memset(ops, 0, sizeof(*ops));
}
