/* The link harness's Vulkan consoles: one instance and one device for
 * each, and each frame copied back from the image its core hands over.
 * Built only with HAVE_VULKAN; the Vulkan loader is opened at run time. */

#ifndef LINK_VK_H
#define LINK_VK_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>

struct retro_hw_render_interface_vulkan;

typedef struct link_vk link_vk_t;   /* one console's instance, device and readback */

/* SET_HW_RENDER: the Vulkan loader, opened once a process. false: the
 * reason on stderr after "link_harness: console <index>: " */
bool link_vk_loader(unsigned index);
/* After retro_load_game: an instance and a device on the first physical
 * device whose name has require in it, else on the first, which must then
 * name require (NULL: any). validate: the Khronos validation layer, each
 * error it reports on stderr and counted in *errors, as the core's misuse
 * of the interface is. NULL: no device, the reason on stderr after
 * "link_harness: console <index>: no Vulkan device: ", or a renderer that
 * is not require. */
link_vk_t *link_vk_new(unsigned index, const char *require, bool validate, unsigned *errors);
/* GET_HW_RENDER_INTERFACE's, version 5 */
const struct retro_hw_render_interface_vulkan *link_vk_interface(link_vk_t *v);
/* The image of the last set_image, width x height, into argb as 0x00RRGGBB,
 * top row first, once the core's work on it is finished. false: the reason
 * on stderr, and argb is not a picture. */
bool link_vk_read(link_vk_t *v, uint32_t *argb, unsigned width, unsigned height);
/* A NULL frame, or one not read: what the core submitted for it is
 * finished by the next wait_sync_index */
void link_vk_dupe(link_vk_t *v);
/* What wait_sync_index waits for, before context_destroy */
void link_vk_wait(link_vk_t *v);
/* "vulkan <deviceName> api <major>.<minor>.<patch>" and a newline */
void link_vk_describe(const link_vk_t *v, char *out, size_t len);
/* After context_destroy and retro_unload_game. NULL: nothing. */
void link_vk_free(link_vk_t *v);

#endif
