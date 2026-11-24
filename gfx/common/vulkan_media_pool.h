/* Public handles own bounded image versions; history owns immutable leases.
 * The recording thread alone changes this state. */
#ifndef VULKAN_MEDIA_POOL_H
#define VULKAN_MEDIA_POOL_H

static struct vk_texture *vulkan_media_sample_texture(struct vk_texture *texture)
{
   if (texture && texture->media_owner)
      texture = texture->media_owner;
   if (texture) vulkan_planar_materialize(texture);
   if (texture && texture->media_pool)
      texture = &texture->media_pool->current->texture;
   return texture;
}

static void vulkan_media_read(void *owner)
{
   struct vk_texture *texture = (struct vk_texture*)owner;
   struct vk_media_version *version = texture->media_version;
   if (version)
      version->read_serial = ((vk_t*)version->driver)->media_submit_serial + 1;
}

static bool vulkan_media_pool_held(const struct vk_texture *texture)
{
   const struct vk_media_version *version;
   if (!texture->media_pool)
      return texture->history_refs != 0;
   for (version = texture->media_pool->versions; version; version = version->next)
      if (version->texture.history_refs)
         return true;
   return false;
}

static void vulkan_media_pool_destroy(VkDevice device, struct vk_texture *texture)
{
   struct vk_media_pool *pool = texture->media_pool;
   struct vk_media_version *version;
   if (!pool)
   {
      vulkan_destroy_texture(device, texture);
      return;
   }
   version = pool->versions;
   while (version)
   {
      struct vk_media_version *next = version->next;
      vulkan_destroy_texture(device, &version->texture);
      free(version);
      version = next;
   }
   free(pool);
   texture->media_pool = NULL;
   texture->media_version = NULL;
   /* The public GPU fields alias the versions just destroyed. */
   texture->image = VK_NULL_HANDLE;
   texture->view = texture->history_view = VK_NULL_HANDLE;
   texture->memory = VK_NULL_HANDLE;
   texture->buffer = VK_NULL_HANDLE;
   texture->mapped = NULL;
   vulkan_destroy_texture(device, texture);
}

static void vulkan_media_pool_free(vk_t *vk, struct vk_texture *texture)
{
   vulkan_media_pool_destroy(vk->context->device, texture);
}

/* Native import caches belong to the stable handle, never an RGB version. */
static void vulkan_media_cache_owner(struct vk_texture *destination,
      const struct vk_texture *owner)
{
#if defined(ANDROID) && (defined(HAVE_RMP4) || defined(HAVE_RVP9) \
      || defined(HAVE_RAV1) || defined(HAVE_RECORD_MEDIACODEC))
   destination->android_cache = owner ? owner->android_cache : NULL;
   destination->android_next = owner ? owner->android_next : NULL;
#endif
#ifdef HAVE_VAAPI
   destination->vaapi_cache = owner ? owner->vaapi_cache : NULL;
   destination->vaapi_next = owner ? owner->vaapi_next : NULL;
#endif
   destination->planar_atlas = owner ? owner->planar_atlas : NULL;
   destination->planar_source = owner ? owner->planar_source : false;
   destination->planar_draw_serial = owner ? owner->planar_draw_serial : 0;
}

static VkImageView vulkan_media_level_view(vk_t *vk, const struct vk_texture *texture,
      unsigned levels)
{
   VkImageView view = VK_NULL_HANDLE;
   VkImageViewCreateInfo info;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   info.image = texture->image;
   info.viewType = VK_IMAGE_VIEW_TYPE_2D;
   info.format = texture->format;
   info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   info.subresourceRange.levelCount = levels;
   info.subresourceRange.layerCount = 1;
   if (vkCreateImageView(vk->context->device, &info, NULL, &view) != VK_SUCCESS)
      return VK_NULL_HANDLE;
   return view;
}

static void vulkan_media_pool_get(vk_t *vk, struct vk_texture *texture)
{
   struct vk_media_pool *pool;
   struct vk_media_version *version;
   struct vk_stream_state *st;
   bool storage;
   if (texture->media_pool || texture->immutable || !texture->image || !texture->view
         || texture->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
         || (texture->format != VK_FORMAT_R8G8B8A8_UNORM
            && texture->format != VK_FORMAT_B8G8R8A8_UNORM
            && texture->format != VK_FORMAT_R16G16B16A16_SFLOAT))
      return;
   for (st = vk->stream_states; st && st->texture != texture; st = st->next) {}
   storage = st && st->pool != VK_NULL_HANDLE;
#if defined(ANDROID) && (defined(HAVE_RMP4) || defined(HAVE_RVP9) \
      || defined(HAVE_RAV1) || defined(HAVE_RECORD_MEDIACODEC))
   storage = storage || texture->android_cache != NULL;
#endif
#ifdef HAVE_VAAPI
   storage = storage || texture->vaapi_cache != NULL;
#endif
   if (texture->type != VULKAN_TEXTURE_STATIC && !storage)
      return;
   pool = (struct vk_media_pool*)calloc(1, sizeof(*pool));
   version = (struct vk_media_version*)calloc(1, sizeof(*version));
   if (!pool || !version)
   {
      free(pool);
      free(version);
      return;
   }
   version->texture = *texture;
   vulkan_media_cache_owner(&version->texture, NULL);
   if (!version->texture.levels)
      version->texture.levels = 1;
   version->texture.history_view = version->texture.levels == 1
      ? texture->view : vulkan_media_level_view(vk, texture, 1);
   if (!version->texture.history_view)
   {
      free(pool);
      free(version);
      return;
   }
   version->driver = vk;
   version->read_serial = vk->media_submit_serial + 1;
   version->texture.media_version = version;
   pool->current = pool->versions = version;
   pool->count = 1;
   pool->storage = storage;
   texture->media_pool = pool;
   texture->media_version = version;
   texture->history_view = version->texture.history_view;
}

static struct vk_media_version *vulkan_media_version_new(vk_t *vk,
      struct vk_media_pool *pool)
{
   const struct vk_texture *source = &pool->current->texture;
   struct vk_media_version *version = (struct vk_media_version*)calloc(1, sizeof(*version));
   struct vk_texture *texture;
   VkImageCreateInfo info;
   VkMemoryRequirements requirements;
   VkMemoryAllocateInfo alloc;
   VkDevice device = vk->context->device;
   if (!version)
      return NULL;
   texture = &version->texture;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   info.imageType = VK_IMAGE_TYPE_2D;
   info.format = source->format;
   info.extent.width = VIDEO_SCALE_W(source->dims);
   info.extent.height = VIDEO_SCALE_H(source->dims);
   info.extent.depth = 1;
   info.mipLevels = source->levels;
   info.arrayLayers = 1;
   info.samples = VK_SAMPLE_COUNT_1_BIT;
   info.tiling = VK_IMAGE_TILING_OPTIMAL;
   info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
      | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   if (pool->storage)
      info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
   info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   if (vkCreateImage(device, &info, NULL, &texture->image) != VK_SUCCESS)
      goto fail;
#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
   vulkan_track_alloc(texture->image);
#endif
   vkGetImageMemoryRequirements(device, texture->image, &requirements);
   memset(&alloc, 0, sizeof(alloc));
   alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.allocationSize = requirements.size;
   alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(
         &vk->context->memory_properties, requirements.memoryTypeBits,
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
   if (vkAllocateMemory(device, &alloc, NULL, &texture->memory) != VK_SUCCESS
         || vkBindImageMemory(device, texture->image, texture->memory, 0) != VK_SUCCESS)
      goto fail;
   texture->memory_size = alloc.allocationSize;
   texture->memory_type = alloc.memoryTypeIndex;
   texture->format = source->format;
   texture->dims = source->dims;
   texture->levels = source->levels;
   texture->type = VULKAN_TEXTURE_DYNAMIC;
   texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;
   texture->view = vulkan_media_level_view(vk, texture, texture->levels);
   if (!texture->view)
      goto fail;
   texture->history_view = texture->levels == 1 ? texture->view
      : vulkan_media_level_view(vk, texture, 1);
   if (!texture->history_view)
      goto fail;
   version->driver = vk;
   texture->media_version = version;
   version->next = pool->versions;
   pool->versions = version;
   pool->count++;
   return version;
fail:
   vulkan_destroy_texture(device, texture);
   free(version);
   return NULL;
}

static struct vk_texture *vulkan_media_writable(vk_t *vk, struct vk_texture *texture)
{
   struct vk_media_pool *pool = texture->media_pool;
   struct vk_media_version *version;
   if (!pool)
      return texture;
   version = pool->current;
   if (!version->texture.history_refs && version->read_serial <= vk->media_submit_serial)
      return &version->texture;
   for (version = pool->versions; version; version = version->next)
      if (!version->texture.history_refs && version->read_serial <= vk->media_submit_serial)
         return &version->texture;
   /* Both SDR and output-space chains can retain a full history ring. */
   if (pool->count >= 2 * (GFX_MAX_FRAME_HISTORY + 1) + 2)
      return NULL;
   version = vulkan_media_version_new(vk, pool);
   return version ? &version->texture : NULL;
}

/* Each descriptor slot is changed only after its previous submission retires. */
static void vulkan_media_storage_target(vk_t *vk, struct vk_stream_state *st,
      unsigned slot, const struct vk_texture *destination)
{
   if (st->media_target[slot] != destination->view)
   {
      VkDescriptorImageInfo info;
      VkWriteDescriptorSet write;
      memset(&info, 0, sizeof(info));
      memset(&write, 0, sizeof(write));
      info.imageView = destination->view;
      info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = st->set[slot];
      write.dstBinding = 3;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      write.pImageInfo = &info;
      vkUpdateDescriptorSets(vk->context->device, 1, &write, 0, NULL);
      st->media_target[slot] = destination->view;
   }
}

static void vulkan_media_publish(struct vk_texture *texture, struct vk_texture *destination)
{
   if (texture->media_pool)
   {
      struct vk_media_pool *pool = texture->media_pool;
      struct vk_texture owner = *texture;
      destination->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      pool->current = destination->media_version;
      *texture = *destination;
      texture->media_pool = pool;
      texture->park = owner.park;
      texture->park_frames = owner.park_frames;
      texture->flags = owner.flags;
      vulkan_media_cache_owner(texture, &owner);
   }
   else
      texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   /* A successful RGB/native publication supersedes retained CPU planes.
    * The planar uploader marks its own RGB warmup again after publishing. */
   texture->planar_source = false;
   if (texture->planar_atlas)
      texture->planar_atlas->pending = false;
}

static void vulkan_media_mipmaps(VkCommandBuffer cmd, const struct vk_texture *texture)
{
   unsigned i;
   for (i = 1; i < texture->levels; i++)
   {
      VkImageBlit blit;
      memset(&blit, 0, sizeof(blit));
      blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.srcSubresource.mipLevel = i - 1;
      blit.srcSubresource.layerCount = 1;
      blit.dstSubresource = blit.srcSubresource;
      blit.dstSubresource.mipLevel = i;
      blit.srcOffsets[1].x = MAX(VIDEO_SCALE_W(texture->dims) >> (i - 1), 1);
      blit.srcOffsets[1].y = MAX(VIDEO_SCALE_H(texture->dims) >> (i - 1), 1);
      blit.srcOffsets[1].z = 1;
      blit.dstOffsets[1].x = MAX(VIDEO_SCALE_W(texture->dims) >> i, 1);
      blit.dstOffsets[1].y = MAX(VIDEO_SCALE_H(texture->dims) >> i, 1);
      blit.dstOffsets[1].z = 1;
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd, texture->image, texture->levels,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED);
      vkCmdBlitImage(cmd, texture->image, VK_IMAGE_LAYOUT_GENERAL,
            texture->image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_LINEAR);
   }
}
#endif
