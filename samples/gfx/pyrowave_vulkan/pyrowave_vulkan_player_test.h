/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_pyrowave_player_vulkan.h"

static const char *test_pyrowave_player_directory;

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL test_pyrowave_player_instance(
      VkInstance instance, const char *name)
{
   if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)test_pyrowave_proc;
   return test_pyrowave_current->base->vkGetInstanceProcAddr(instance, name);
}

static record_pyrowave_vulkan_t *test_pyrowave_player_new(const record_pyrowave_vulkan_config_t *cfg)
{
   record_pyrowave_source_config_t setup;
   if (!cfg->decode) return record_pyrowave_vulkan_new(cfg);
   memset(&setup, 0, sizeof(setup));
   setup.device = cfg->device; setup.queue = cfg->queue;
   setup.instance = test_pyrowave_current->base->instance;
   setup.physical = test_pyrowave_current->base->physical;
   setup.instance_proc = (record_pyrowave_source_proc_t)test_pyrowave_player_instance;
   setup.family = cfg->queue_family; setup.directory = test_pyrowave_player_directory;
   setup.stream.sequence = cfg->sequence;
   setup.stream.maximum_frame_bytes = cfg->maximum_bytes;
   setup.stream.bit_depth = cfg->bit_depth;
   setup.slots = cfg->slots; setup.texture_mode = cfg->texture_mode;
   return record_pyrowave_player_vulkan_new(&setup);
}
#define record_pyrowave_vulkan_new test_pyrowave_player_new
