/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
/* Included after the shared Vulkan decoder helpers. */

#ifdef HAVE_RAV1
static int vvd_av1_decode(void *decoder, const uint8_t *packet, size_t length,
                          const rav1_sequence_t *sequence,
                          const rav1_hw_picture_t *p)
{
   vvd_dec_t *d = (vvd_dec_t *)decoder;
   VkDeviceSize size;
   uint32_t used = 0;
   unsigned i, n = 0, k = d->dring % VVD_RING;
   int ready;
   if (!sequence || !p ||
       memcmp(sequence, &d->av1_sequence, sizeof(*sequence)) ||
       p->grain.params.apply || p->setup.size.width != d->coded.width ||
       p->setup.size.upscaled_width != d->coded.width ||
       p->setup.size.height != d->coded.height)
      return -2;
   if ((unsigned)p->slot >= d->slots ||
       !vulkan_av1_picture(d->av1, packet, length, sequence, p))
      return -1;
   for (i = 0; i < STD_VIDEO_AV1_REFS_PER_FRAME; i++)
   {
      int slot = d->av1->info.referenceNameSlotIndices[i];
      if (slot < 0)
         continue;
      if ((unsigned)slot >= d->slots)
         return -1;
      if (used & (UINT32_C(1) << slot))
         continue;
      used |= UINT32_C(1) << slot;
      d->res[n] = vvd_resource(d, (unsigned)slot);
      d->av1_ref[n] = d->av1_saved[slot];
      d->av1_slot[n].sType =
          VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_DPB_SLOT_INFO_KHR;
      d->av1_slot[n].pNext = NULL;
      d->av1_slot[n].pStdReferenceInfo = &d->av1_ref[n];
      d->slots_info[n].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
      d->slots_info[n].pNext = &d->av1_slot[n];
      d->slots_info[n].slotIndex = slot;
      d->slots_info[n].pPictureResource = &d->res[n];
      d->begin_slots[n] = d->slots_info[n];
      d->begin_slots[n].pNext = NULL;
      n++;
   }
   d->res[n] = vvd_resource(d, (unsigned)p->slot);
   d->av1_ref[n] = d->av1->reference;
   d->av1_slot[n].sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_DPB_SLOT_INFO_KHR;
   d->av1_slot[n].pNext = NULL;
   d->av1_slot[n].pStdReferenceInfo = &d->av1_ref[n];
   d->slots_info[n].sType = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
   d->slots_info[n].pNext = &d->av1_slot[n];
   d->slots_info[n].slotIndex = p->slot;
   d->slots_info[n].pPictureResource = &d->res[n];
   d->begin_slots[n] = d->slots_info[n];
   d->begin_slots[n].pNext = NULL;
   d->begin_slots[n].slotIndex = -1;
   ready = vvd_try_wait(d->tl, d->dcb_value[k]);
   if (ready)
      return ready;
   size = ((VkDeviceSize)length + d->bs_size_align - 1) / d->bs_size_align *
          d->bs_size_align;
   if (size < length || size > SIZE_MAX || !vvd_bitstream(d, k, size))
      return -1;
   memcpy(d->bs_map[k], packet, length);
   memset((uint8_t *)d->bs_map[k] + length, 0, (size_t)size - length);
   if (!d->bs_coherent[k])
   {
      VkMappedMemoryRange range;
      range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
      range.pNext = NULL;
      range.memory = d->bs_mem[k];
      range.offset = 0;
      range.size = VK_WHOLE_SIZE;
      if (vkFlushMappedMemoryRanges(vvd.device, 1, &range) != VK_SUCCESS)
         return -1;
   }
   ready = vvd_record(d, k, size, n, (unsigned)p->slot, &d->av1->info);
   if (!ready)
   {
      d->av1_saved[p->slot] = d->av1->reference;
      d->yuv[p->slot] =
          (sequence->color_range ? IMAGE_YUV_FLAG_FULL_RANGE : 0) |
          (sequence->matrix_coefficients == 1 ? IMAGE_YUV_FLAG_BT709 : 0);
   }
   return ready;
}

static int vvd_av1_output(void *decoder, unsigned slot,
                          const rav1_grain_t *grain, video_hw_frame_t *frame)
{
   if (!grain || grain->apply)
      return -2;
   return vvd_output(decoder, slot, frame);
}

static bool vvd_av1_open(void *user, const rav1_sequence_t *sequence,
                         unsigned slots, rav1_hw_ops_t *ops)
{
   settings_t *settings = config_get_ptr();
   vulkan_av1_sequence_t mapped;
   vvd_dec_t *d;
   VkVideoCapabilitiesKHR caps;
   VkVideoDecodeCapabilitiesKHR decode;
   VkVideoDecodeAV1CapabilitiesKHR av1;
   VkVideoDecodeAV1SessionParametersCreateInfoKHR ap;
   VkVideoSessionParametersCreateInfoKHR params;
   unsigned lane, gx, gy;
   (void)user;
   if (!ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   if (!vvd.up || slots < 9 || slots > VVD_MAX_SLOTS ||
       !(vvd.codecs & VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR) ||
       (settings && settings->uints.video_hw_decode == VIDEO_HW_DECODE_OFF) ||
       !vulkan_av1_sequence(&mapped, sequence) ||
       sequence->film_grain_params_present ||
       ((sequence->max_width | sequence->max_height) & 1) ||
       (sequence->matrix_coefficients != 1 &&
        sequence->matrix_coefficients != 2 &&
        sequence->matrix_coefficients != 5 &&
        sequence->matrix_coefficients != 6) ||
       sequence->transfer_characteristics == 16 ||
       sequence->transfer_characteristics == 18)
      return false;
   vvd_grave_reap(false);
   for (lane = 0; lane < vvd.lanes; lane++)
      if (retro_atomic_cas_int(&vvd.lane_busy[lane], 0, 1))
         break;
   if (lane == vvd.lanes)
      return false;
   d = (vvd_dec_t *)calloc(1, sizeof(*d));
   if (!d)
   {
      vvd_lane_release(lane);
      return false;
   }
   d->lane = lane;
   d->slots = slots;
   d->av1_sequence = *sequence;
   d->av1 = (vulkan_av1_picture_t *)calloc(1, sizeof(*d->av1));
   if (!d->av1 || !vulkan_av1_sequence(&d->av1_params, sequence))
      goto fail;
   d->format = sequence->bit_depth == 10
                   ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16
                   : VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
   d->pixfmt =
       sequence->bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   d->coded.width = d->visible_w = sequence->max_width;
   d->coded.height = d->visible_h = sequence->max_height;
   vkGetDeviceQueue(vvd.device, vvd.dec_family, lane, &d->dq);
   vkGetDeviceQueue(vvd.device, vvd.xfer_family, vvd.xfer_first + lane, &d->xq);
   d->av1_profile.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR;
   d->av1_profile.pNext = NULL;
   d->av1_profile.stdProfile = STD_VIDEO_AV1_PROFILE_MAIN;
   d->av1_profile.filmGrainSupport = VK_FALSE;
   d->profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
   d->profile.pNext = &d->av1_profile;
   d->profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
   d->profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
   d->profile.lumaBitDepth = d->profile.chromaBitDepth =
       sequence->bit_depth == 10 ? VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR
                                 : VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
   caps.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
   caps.pNext = &decode;
   decode.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
   decode.pNext = &av1;
   av1.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR;
   av1.pNext = NULL;
   if (vvd.get_caps(vvd.gpu, &d->profile, &caps) != VK_SUCCESS ||
       d->coded.width < caps.minCodedExtent.width ||
       d->coded.width > caps.maxCodedExtent.width ||
       d->coded.height < caps.minCodedExtent.height ||
       d->coded.height > caps.maxCodedExtent.height ||
       caps.maxDpbSlots < slots ||
       caps.maxActiveReferencePictures < STD_VIDEO_AV1_REFS_PER_FRAME ||
       (unsigned)av1.maxLevel < sequence->operating_points[0].level ||
       !(decode.flags &
         VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR))
      goto fail;
   caps.pNext = NULL;
   d->profiles.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR;
   d->profiles.pNext = NULL;
   d->profiles.profileCount = 1;
   d->profiles.pProfiles = &d->profile;
   gx = caps.pictureAccessGranularity.width
            ? caps.pictureAccessGranularity.width
            : 1;
   gy = caps.pictureAccessGranularity.height
            ? caps.pictureAccessGranularity.height
            : 1;
   d->image.width = (d->coded.width + gx - 1) / gx * gx;
   d->image.height = (d->coded.height + gy - 1) / gy * gy;
   d->bs_offset_align = caps.minBitstreamBufferOffsetAlignment
                            ? caps.minBitstreamBufferOffsetAlignment
                            : 1;
   d->bs_size_align = caps.minBitstreamBufferSizeAlignment
                          ? caps.minBitstreamBufferSizeAlignment
                          : 1;
   if (!vvd_format_ok(d, VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR |
                             VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR |
                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ||
       !vvd_make_session(d, &caps) || !vvd_make_dpb(d, &caps) ||
       !vvd_make_outputs(d) || !vvd_make_commands(d) ||
       !(d->tl = vvd_timeline()))
      goto fail;
   ap.sType =
       VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_SESSION_PARAMETERS_CREATE_INFO_KHR;
   ap.pNext = NULL;
   ap.pStdSequenceHeader = &d->av1_params.header;
   params.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR;
   params.pNext = &ap;
   params.flags = 0;
   params.videoSessionParametersTemplate = VK_NULL_HANDLE;
   params.videoSession = d->session;
   if (vvd.create_params(vvd.device, &params, NULL, &d->params) != VK_SUCCESS)
   {
      d->params = VK_NULL_HANDLE;
      goto fail;
   }
   ops->decoder = d;
   ops->decode = vvd_av1_decode;
   ops->output = vvd_av1_output;
   retro_atomic_fetch_add_int(&vvd.live, 1);
   return true;
fail:
   vvd_destroy(d);
   vvd_lane_release(lane);
   return false;
}

static void vvd_av1_close(rav1_hw_ops_t *ops)
{
   if (!ops)
      return;
   vvd_close(ops->decoder);
   memset(ops, 0, sizeof(*ops));
}
#endif
