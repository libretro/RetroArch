/* Runs the production descriptor manager against a fake device that
 * records which pool generation each set was allocated in and which
 * sets a frame wrote. A set handed out after a frame-start reset must
 * be one allocated since its pool was reset, with nothing written. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>

#define VK_NULL_HANDLE 0
#define VK_SUCCESS 0
#define VK_ERROR_OUT_OF_POOL_MEMORY (-1000069000)
#define VK_ERROR_OUT_OF_DEVICE_MEMORY (-2)
#define VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO 33
#define VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO 34
#define VK_DESCRIPTOR_TYPE_SAMPLER 0
#define VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS 16
#define VULKAN_MAX_DESCRIPTOR_POOL_SIZES 16
#define RARCH_ERR rarch_err

typedef int VkResult;
typedef unsigned VkDevice;
typedef unsigned VkDescriptorPool;
typedef unsigned VkDescriptorSet;
typedef unsigned VkDescriptorSetLayout;
typedef struct { int type; unsigned descriptorCount; } VkDescriptorPoolSize;
typedef struct
{
   int sType;
   const void *pNext;
   unsigned flags;
   unsigned maxSets;
   unsigned poolSizeCount;
   const VkDescriptorPoolSize *pPoolSizes;
} VkDescriptorPoolCreateInfo;
typedef struct
{
   int sType;
   const void *pNext;
   VkDescriptorPool descriptorPool;
   unsigned descriptorSetCount;
   const VkDescriptorSetLayout *pSetLayouts;
} VkDescriptorSetAllocateInfo;

struct vk_descriptor_pool
{
   struct vk_descriptor_pool *next;
   VkDescriptorPool pool;
   VkDescriptorSet sets[VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS];
};

struct vk_descriptor_manager
{
   struct vk_descriptor_pool *head;
   struct vk_descriptor_pool *current;
   VkDescriptorSetLayout set_layout;
   VkDescriptorPoolSize sizes[VULKAN_MAX_DESCRIPTOR_POOL_SIZES];
   unsigned count;
   unsigned num_sizes;
};

#define MAX_POOLS 8
#define MAX_HANDLES 1024

struct fake_pool
{
   unsigned generation;
   unsigned used;      /* sets allocated since the last reset */
   unsigned resets;
   bool live;
};

struct fake_set
{
   VkDescriptorPool pool;
   unsigned generation;
   VkDescriptorSetLayout layout;
   bool written;
};

static struct fake_pool pools[MAX_POOLS + 1];
static unsigned num_pools;
static struct fake_set handles[MAX_HANDLES + 1];
static unsigned num_handles;
/* Allocations to fail from now on, and the layout the manager uses */
static unsigned fail_allocs;
static unsigned failures;

#define LAYOUT 77u

static VkResult vkCreateDescriptorPool(VkDevice device,
      const VkDescriptorPoolCreateInfo *info, const void *alloc,
      VkDescriptorPool *pool)
{
   (void)device; (void)alloc;
   if (num_pools >= MAX_POOLS || info->maxSets
         != VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *pool = ++num_pools;
   memset(&pools[*pool], 0, sizeof(pools[0]));
   pools[*pool].live = true;
   return VK_SUCCESS;
}

static void vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool pool,
      const void *alloc)
{
   (void)device; (void)alloc;
   pools[pool].live = false;
   pools[pool].generation++;
}

static VkResult vkResetDescriptorPool(VkDevice device, VkDescriptorPool pool,
      unsigned flags)
{
   (void)device; (void)flags;
   pools[pool].generation++;
   pools[pool].used = 0;
   pools[pool].resets++;
   return VK_SUCCESS;
}

static VkResult vkAllocateDescriptorSets(VkDevice device,
      const VkDescriptorSetAllocateInfo *info, VkDescriptorSet *sets)
{
   unsigned i;
   struct fake_pool *p = &pools[info->descriptorPool];
   (void)device;
   if (     fail_allocs
         || p->used + info->descriptorSetCount
            > VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS
         || num_handles + info->descriptorSetCount > MAX_HANDLES)
   {
      if (fail_allocs)
         fail_allocs--;
      /* As the spec has it: on failure every handle is VK_NULL_HANDLE */
      for (i = 0; i < info->descriptorSetCount; i++)
         sets[i] = VK_NULL_HANDLE;
      return VK_ERROR_OUT_OF_POOL_MEMORY;
   }
   for (i = 0; i < info->descriptorSetCount; i++)
   {
      sets[i]                      = ++num_handles;
      handles[sets[i]].pool        = info->descriptorPool;
      handles[sets[i]].generation  = p->generation;
      handles[sets[i]].layout      = info->pSetLayouts[i];
      handles[sets[i]].written     = false;
   }
   p->used += info->descriptorSetCount;
   return VK_SUCCESS;
}

static void rarch_err(const char *fmt, ...)
{
   (void)fmt;
}

#include "production_manager.h"

static void expect(const char *test, int ok, const char *what, double got)
{
   if (ok)
      return;
   printf("[FAIL] %s: %s (got %g)\n", test, what, got);
   failures++;
}

static void manager_init(struct vk_descriptor_manager *m)
{
   memset(m, 0, sizeof(*m));
   memset(pools, 0, sizeof(pools));
   memset(handles, 0, sizeof(handles));
   num_pools = num_handles = fail_allocs = 0;
   m->set_layout                = LAYOUT;
   m->num_sizes                 = 1;
   m->sizes[0].type             = VK_DESCRIPTOR_TYPE_SAMPLER;
   m->sizes[0].descriptorCount  = VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS;
   m->head                      = vulkan_alloc_descriptor_pool(1, m);
   m->current                   = m->head;
}

static void manager_free(struct vk_descriptor_manager *m)
{
   struct vk_descriptor_pool *node = m->head;
   while (node)
   {
      struct vk_descriptor_pool *next = node->next;
      free(node);
      node = next;
   }
}

/* A set the frame may write: allocated in its pool's current
 * generation, in the manager's layout, never written since */
static bool fresh(VkDescriptorSet set)
{
   return set != VK_NULL_HANDLE && set <= num_handles
      && pools[handles[set].pool].live
      && handles[set].generation == pools[handles[set].pool].generation
      && handles[set].layout == LAYOUT
      && !handles[set].written;
}

/* One frame on the slot: @n sets taken and written */
static bool frame(struct vk_descriptor_manager *m, unsigned n,
      const char *t)
{
   unsigned i;
   bool all = true;
   vulkan_descriptor_manager_reset(1, m);
   for (i = 0; i < n; i++)
   {
      VkDescriptorSet set = vulkan_descriptor_manager_alloc(1, m);
      if (!fresh(set))
         all = false;
      if (set != VK_NULL_HANDLE)
         handles[set].written = true;
   }
   expect(t, all, "every set taken is fresh", (double)n);
   return all;
}

/* The sets a frame wrote, a binding it alone used among them, are not
 * what the next frame on the slot gets */
static void test_next_frame_gets_unwritten_sets(void)
{
   static const char t[] = "test_next_frame_gets_unwritten_sets";
   struct vk_descriptor_manager m;
   unsigned before = failures;

   manager_init(&m);
   frame(&m, 5, t);
   frame(&m, 5, t);
   frame(&m, VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS, t);
   manager_free(&m);
   if (failures == before)
      printf("[pass] %s\n", t);
}

/* Blocks past the first, added by a busy frame, are reset too while a
 * frame reaches them, and left alone once frames stop reaching them */
static void test_reset_covers_the_blocks_reached(void)
{
   static const char t[] = "test_reset_covers_the_blocks_reached";
   struct vk_descriptor_manager m;
   unsigned before = failures;

   manager_init(&m);
   frame(&m, VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS + 3, t);
   expect(t, num_pools == 2, "a second block was added", num_pools);
   frame(&m, VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS + 3, t);
   expect(t, pools[2].resets == 1, "the second block was reset",
         pools[2].resets);
   frame(&m, 2, t);
   expect(t, pools[2].resets == 2,
         "reset once more after the frame that reached it",
         pools[2].resets);
   frame(&m, 2, t);
   expect(t, pools[2].resets == 2, "not reset after a frame within block 1",
         pools[2].resets);
   expect(t, pools[1].resets == 3, "the first block reset each used frame",
         pools[1].resets);
   manager_free(&m);
   if (failures == before)
      printf("[pass] %s\n", t);
}

/* A slot whose last frame took no set costs nothing at its reset */
static void test_untouched_slot_is_not_reset(void)
{
   static const char t[] = "test_untouched_slot_is_not_reset";
   struct vk_descriptor_manager m;
   unsigned before = failures;

   manager_init(&m);
   vulkan_descriptor_manager_reset(1, &m);
   vulkan_descriptor_manager_reset(1, &m);
   expect(t, pools[1].resets == 0, "no reset", pools[1].resets);
   expect(t, m.current == m.head && m.count == 0, "manager at the start",
         m.count);
   manager_free(&m);
   if (failures == before)
      printf("[pass] %s\n", t);
}

/* Sets that can't be had again hand out VK_NULL_HANDLE, never a set
 * of an earlier generation, and the next reset tries again */
static void test_failed_reallocation_hands_out_null(void)
{
   static const char t[] = "test_failed_reallocation_hands_out_null";
   struct vk_descriptor_manager m;
   VkDescriptorSet set;
   unsigned before = failures;

   manager_init(&m);
   frame(&m, 3, t);
   fail_allocs = 1;
   vulkan_descriptor_manager_reset(1, &m);
   set = vulkan_descriptor_manager_alloc(1, &m);
   expect(t, set == VK_NULL_HANDLE, "the frame gets no set", set);
   frame(&m, 3, t);
   manager_free(&m);
   if (failures == before)
      printf("[pass] %s\n", t);
}

int main(void)
{
   test_next_frame_gets_unwritten_sets();
   test_reset_covers_the_blocks_reached();
   test_untouched_slot_is_not_reset();
   test_failed_reallocation_hands_out_null();
   return failures ? 1 : 0;
}
