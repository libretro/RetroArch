/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Included after the shared Vulkan decoder helpers. */

#ifdef HAVE_RVP9
static int vvd_vp9_decode(void *dec, const uint8_t *packet, size_t length,
      const rvp9_hw_picture_t *pic)
{
   vvd_dec_t *d = (vvd_dec_t*)dec;
   vulkan_vp9_picture_t picture;
   VkDeviceSize size;
   uint32_t used = 0;
   unsigned i, n = 0, k = d->dring % VVD_RING;
   int ready;
   if (!packet || !vulkan_vp9_picture(&picture, pic, length)
         || (unsigned)pic->slot >= d->slots
         || (unsigned)pic->header.w != d->coded.width
         || (unsigned)pic->header.h != d->coded.height
         || pic->header.profile != (int)d->vp9_profile.stdProfile
         || pic->header.color_space < 0 || pic->header.color_space > 3
         || (pic->header.bit_depth == 10) != (d->pixfmt == IMAGE_PIXFMT_P010)) return -1;
   ready = vvd_try_wait(d->tl, d->dcb_value[k]);
   if (ready) return ready;
   size = ((VkDeviceSize)length + d->bs_size_align - 1) / d->bs_size_align * d->bs_size_align;
   if (size < length || size > SIZE_MAX || !vvd_bitstream(d, k, size)) return -1;
   memcpy(d->bs_map[k], packet, length);
   memset((uint8_t*)d->bs_map[k] + length, 0, (size_t)size - length);
   if (!d->bs_coherent[k])
   {
      VkMappedMemoryRange range;
      range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE; range.pNext = NULL;
      range.memory = d->bs_mem[k]; range.offset = 0; range.size = VK_WHOLE_SIZE;
      if (vkFlushMappedMemoryRanges(vvd.device, 1, &range) != VK_SUCCESS) return -1;
   }
   /* Reference names may alias the same physical slot. */
   for (i = 0; i < RVP9_REFS_PER_FRAME; i++)
   {
      int slot = pic->named_slots[i];
      if (slot < 0) continue;
      if ((unsigned)slot >= d->slots) return -1;
      if (used & (UINT32_C(1) << slot)) continue;
      used |= UINT32_C(1) << slot;
      d->res[n] = vvd_resource(d, (unsigned)slot);
      d->slots_info[n].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
      d->slots_info[n].pNext = NULL; d->slots_info[n].slotIndex = slot;
      d->slots_info[n].pPictureResource = &d->res[n];
      d->begin_slots[n] = d->slots_info[n];
      n++;
   }
   d->res[n] = vvd_resource(d, (unsigned)pic->slot);
   d->slots_info[n].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
   d->slots_info[n].pNext = NULL; d->slots_info[n].slotIndex = pic->slot;
   d->slots_info[n].pPictureResource = &d->res[n];
   d->begin_slots[n] = d->slots_info[n]; d->begin_slots[n].slotIndex = -1;
   ready = vvd_record(d, k, size, n, (unsigned)pic->slot, &picture.info);
   if (!ready)
      d->yuv[pic->slot] = (pic->header.color_range ? IMAGE_YUV_FLAG_FULL_RANGE : 0)
         | (pic->header.color_space == 2 ? IMAGE_YUV_FLAG_BT709 : 0);
   return ready;
}

static bool vvd_vp9_open(void *user, const rvp9_hdr *header,
      unsigned slots, rvp9_hw_ops_t *ops)
{
   settings_t *settings = config_get_ptr();
   vvd_dec_t *d;
   VkVideoCapabilitiesKHR caps;
   VkVideoDecodeCapabilitiesKHR decode;
   VkVideoDecodeVP9CapabilitiesKHR vp9;
   unsigned lane, gx, gy;
   (void)user;
   if (!ops) return false;
   memset(ops, 0, sizeof(*ops));
   if (!vvd.up || !header || slots < 10 || slots > VVD_MAX_SLOTS
         || !(vvd.codecs & VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)
         || (settings && settings->uints.video_hw_decode == VIDEO_HW_DECODE_OFF)
         || !((header->profile == 0 && header->bit_depth == 8)
            || (header->profile == 2 && header->bit_depth == 10))
         || header->subsampling_x != 1 || header->subsampling_y != 1
         || header->w <= 0 || header->h <= 0 || ((header->w | header->h) & 1)
         || header->frame_type || header->show_existing_frame
         || header->color_space < 0 || header->color_space > 3) return false;
   vvd_grave_reap(false);
   for (lane = 0; lane < vvd.lanes; lane++)
      if (retro_atomic_cas_int(&vvd.lane_busy[lane], 0, 1)) break;
   if (lane == vvd.lanes) return false;
   d = (vvd_dec_t*)calloc(1, sizeof(*d));
   if (!d) { vvd_lane_release(lane); return false; }
   d->lane = lane; d->slots = slots;
   d->format = header->bit_depth == 10 ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16
      : VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
   d->pixfmt = header->bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   d->coded.width = d->visible_w = (unsigned)header->w;
   d->coded.height = d->visible_h = (unsigned)header->h;
   vkGetDeviceQueue(vvd.device, vvd.dec_family, lane, &d->dq);
   vkGetDeviceQueue(vvd.device, vvd.xfer_family, vvd.xfer_first + lane, &d->xq);
   d->vp9_profile.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_PROFILE_INFO_KHR;
   d->vp9_profile.stdProfile = (StdVideoVP9Profile)header->profile;
   d->profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
   d->profile.pNext = &d->vp9_profile;
   d->profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
   d->profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
   d->profile.lumaBitDepth = d->profile.chromaBitDepth = header->bit_depth == 10
      ? VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR : VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
   memset(&caps, 0, sizeof(caps)); memset(&decode, 0, sizeof(decode)); memset(&vp9, 0, sizeof(vp9));
   caps.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR; caps.pNext = &decode;
   decode.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR; decode.pNext = &vp9;
   vp9.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_CAPABILITIES_KHR;
   if (vvd.get_caps(vvd.gpu, &d->profile, &caps) != VK_SUCCESS
         || d->coded.width < caps.minCodedExtent.width || d->coded.width > caps.maxCodedExtent.width
         || d->coded.height < caps.minCodedExtent.height || d->coded.height > caps.maxCodedExtent.height
         || caps.maxDpbSlots < slots || caps.maxActiveReferencePictures < RVP9_REFS_PER_FRAME
         || !(decode.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR)) goto fail;
   caps.pNext = NULL;
   d->profiles.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR;
   d->profiles.profileCount = 1; d->profiles.pProfiles = &d->profile;
   gx = caps.pictureAccessGranularity.width ? caps.pictureAccessGranularity.width : 1;
   gy = caps.pictureAccessGranularity.height ? caps.pictureAccessGranularity.height : 1;
   d->image.width = (d->coded.width + gx - 1) / gx * gx;
   d->image.height = (d->coded.height + gy - 1) / gy * gy;
   d->bs_offset_align = caps.minBitstreamBufferOffsetAlignment ? caps.minBitstreamBufferOffsetAlignment : 1;
   d->bs_size_align = caps.minBitstreamBufferSizeAlignment ? caps.minBitstreamBufferSizeAlignment : 1;
   if (!vvd_format_ok(d, VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR
            | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
         || !vvd_make_session(d, &caps) || !vvd_make_dpb(d, &caps)
         || !vvd_make_outputs(d) || !vvd_make_commands(d) || !(d->tl = vvd_timeline())) goto fail;
   ops->decoder = d; ops->decode = vvd_vp9_decode; ops->output = vvd_output;
   retro_atomic_fetch_add_int(&vvd.live, 1);
   return true;
fail:
   vvd_destroy(d); vvd_lane_release(lane);
   return false;
}

static void vvd_vp9_close(rvp9_hw_ops_t *ops)
{
   if (!ops) return;
   vvd_close(ops->decoder); memset(ops, 0, sizeof(*ops));
}
#endif
