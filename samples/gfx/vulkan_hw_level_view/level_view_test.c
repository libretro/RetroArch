/* A core's mipmapped image is read in place through a view of its top
 * level alone, made once a frame; an image of one level, or a view the
 * core made with chained structures, is left to the copy. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

static unsigned            creates;
static VkImageViewCreateInfo made;
static VkResult            result;

static VkResult fake_create_image_view(VkDevice device,
      const VkImageViewCreateInfo *info,
      const VkAllocationCallbacks *alloc, VkImageView *view)
{
   (void)device; (void)alloc;
   creates++;
   made  = *info;
   *view = result == VK_SUCCESS ? (VkImageView)(uintptr_t)0x1000 : VK_NULL_HANDLE;
   return result;
}
#define vkCreateImageView fake_create_image_view

struct retro_vulkan_image { VkImageView image_view; VkImageLayout image_layout;
   VkImageViewCreateInfo create_info; };
struct vk_context { VkDevice device; };
typedef struct { struct vk_context *context;
   struct { struct retro_vulkan_image *image; } hw; } vk_t;
struct vk_per_frame { VkImageView hw_level_view; };

#include "production_level_view.h"

static int fails;
#define CHECK(c, what) do { if (c) printf("  [ok]   %s\n", what); \
   else { printf("  [FAIL] %s\n", what); fails++; } } while (0)

int main(void)
{
   struct vk_context ctx;
   struct retro_vulkan_image img;
   struct vk_per_frame slot;
   vk_t vk;
   int chained = 1;

   memset(&ctx, 0, sizeof(ctx));
   memset(&img, 0, sizeof(img));
   memset(&slot, 0, sizeof(slot));
   vk.context  = &ctx;
   vk.hw.image = &img;
   img.create_info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   img.create_info.image    = (VkImage)(uintptr_t)0x2000;
   img.create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
   img.create_info.format   = VK_FORMAT_R8G8B8A8_UNORM;
   img.create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   img.create_info.subresourceRange.baseMipLevel   = 2;
   img.create_info.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
   img.create_info.subresourceRange.baseArrayLayer = 1;
   img.create_info.subresourceRange.layerCount     = 4;
   result = VK_SUCCESS;

   printf("[vulkan_hw_level_view]\n");
   CHECK(vulkan_hw_level_view(&vk, &slot) && slot.hw_level_view,
         "a mipmapped image gets a view");
   CHECK(made.subresourceRange.levelCount == 1
         && made.subresourceRange.baseMipLevel == 2,
         "of the level the core's view starts at, alone");
   CHECK(made.subresourceRange.layerCount == 1
         && made.subresourceRange.baseArrayLayer == 1
         && made.viewType == VK_IMAGE_VIEW_TYPE_2D,
         "and its first layer, as a 2D image");
   CHECK(made.image == img.create_info.image && made.format == img.create_info.format,
         "of the core's image, in its format");
   CHECK(vulkan_hw_level_view(&vk, &slot) && creates == 1,
         "made once a frame");

   memset(&slot, 0, sizeof(slot));
   img.create_info.subresourceRange.levelCount = 1;
   CHECK(!vulkan_hw_level_view(&vk, &slot) && creates == 1,
         "an image of one level is not given one");

   img.create_info.subresourceRange.levelCount = 3;
   img.create_info.pNext = &chained;
   CHECK(!vulkan_hw_level_view(&vk, &slot) && creates == 1,
         "nor one the core viewed with chained structures");

   img.create_info.pNext = NULL;
   result = VK_ERROR_OUT_OF_HOST_MEMORY;
   CHECK(!vulkan_hw_level_view(&vk, &slot) && !slot.hw_level_view,
         "a view that can't be made leaves the copy");

   if (fails)
      return 1;
   printf("[vulkan_hw_level_view] all passed\n");
   return 0;
}
