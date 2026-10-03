/* Frontend symbols gfx/video_shader_parse.c references.
 *
 * The subject of this harness is which folder the shader hotkeys
 * cycle and in what order, so the directory walk, the preset parsing
 * and the #reference following all run for real against the fixture
 * on disk. Applying a preset is the one thing that cannot: the stub
 * records the path and adopts it as the runtime preset, which is what
 * video_shader_apply_shader() does on success.
 *
 * Signatures are copied from the tree's headers rather than guessed. */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <string/stdstring.h>

#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../../../command.h"
#include "../../../list_special.h"
#include "../../../file_path_special.h"
#include "../../../paths.h"
#include "../../../retroarch.h"
#include "../../../gfx/video_driver.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_shader.h"

#define STUB_MAX_APPLIED 16

static char applied[STUB_MAX_APPLIED][PATH_MAX_LENGTH];
static unsigned applied_count;
static char last_preset_dir[DIR_MAX_LENGTH];
static char last_preset_file[NAME_MAX_LENGTH];

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_LOG_OUTPUT(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

bool verbosity_is_enabled(void) { return false; }

settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

static runloop_state_t runloop_st;
runloop_state_t *runloop_state_get_ptr(void) { return &runloop_st; }

static video_driver_state_t video_st;
video_driver_state_t *video_state_get_ptr(void) { return &video_st; }

static struct menu_state menu_st;
struct menu_state *menu_state_get_ptr(void) { return &menu_st; }

/* --- harness control -------------------------------------------------- */

void stub_reset(void)
{
   applied_count        = 0;
   last_preset_dir[0]   = '\0';
   last_preset_file[0]  = '\0';
   memset(&runloop_st, 0, sizeof(runloop_st));
}

void stub_set_runtime_preset(const char *path)
{
   strlcpy(runloop_st.runtime_shader_preset_path, path,
         sizeof(runloop_st.runtime_shader_preset_path));
   runloop_st.system.info.library_name = "FCEUmm";
}

unsigned stub_applied_count(void) { return applied_count; }

const char *stub_applied(unsigned i)
{
   return (i < applied_count) ? applied[i] : "";
}

/* --- frontend --------------------------------------------------------- */

bool command_set_shader(command_t *cmd, const char *arg)
{
   (void)cmd;

   if (!arg || !*arg)
      return false;

   if (applied_count < STUB_MAX_APPLIED)
      strlcpy(applied[applied_count++], arg, PATH_MAX_LENGTH);

   strlcpy(runloop_st.runtime_shader_preset_path, arg,
         sizeof(runloop_st.runtime_shader_preset_path));
   return true;
}

bool command_event(enum event_command action, void *data)
{
   (void)action;
   (void)data;
   return true;
}

/* DIR_LIST_SHADERS as retroarch.c builds it, for a context that takes
 * slang and GLSL presets */
struct string_list *dir_list_new_special(const char *input_dir,
      enum dir_list_type type, const char *filter,
      bool show_hidden_files)
{
   (void)filter;

   if (type != DIR_LIST_SHADERS)
      return NULL;

   return dir_list_new(input_dir, "glslp|slangp", false,
         show_hidden_files, false, false);
}

bool video_context_driver_get_flags(gfx_ctx_flags_t *flags)
{
   flags->flags = 0;
   BIT32_SET(flags->flags, GFX_CTX_FLAGS_SHADERS_SLANG);
   BIT32_SET(flags->flags, GFX_CTX_FLAGS_SHADERS_GLSL);
   return true;
}

float video_driver_get_core_aspect(void) { return 4.0f / 3.0f; }

unsigned video_driver_get_output_dims(void)
{
   return VIDEO_SCALE_PACK(1920, 1080);
}

void video_driver_modify_disp_flags(uint32_t set_bits, uint32_t clear_bits)
{
   (void)set_bits;
   (void)clear_bits;
}

bool video_driver_test_all_flags(enum display_flags testflag)
{
   (void)testflag;
   return false;
}

unsigned int retroarch_get_rotation(void) { return 0; }
unsigned int retroarch_get_core_requested_rotation(void) { return 0; }

void runloop_msg_queue_push(
      const char *msg, size_t len,
      unsigned prio, unsigned duration,
      bool flush,
      char *title,
      enum message_queue_icon icon,
      enum message_queue_category category)
{
   (void)msg;     (void)len;   (void)prio;  (void)duration;
   (void)flush;   (void)title; (void)icon;  (void)category;
}

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   (void)msg;
   return "";
}

/* No content and no config file: the auto-preset search then looks
 * only for a core-specific preset, which is the reported case */
const char *path_get(enum rarch_path_type type)
{
   (void)type;
   return "";
}

bool path_is_empty(enum rarch_path_type type)
{
   (void)type;
   return true;
}

size_t fill_pathname_application_special(char *s, size_t len,
      enum application_special_type type)
{
   (void)len;
   (void)type;
   s[0] = '\0';
   return 0;
}

/* --- menu ------------------------------------------------------------- */

void menu_driver_set_last_shader_preset_path(const char *path)
{
   if (!path || !*path)
      return;
   fill_pathname_basedir(last_preset_dir, path, sizeof(last_preset_dir));
   strlcpy(last_preset_file, path_basename(path), sizeof(last_preset_file));
}

void menu_driver_get_last_shader_preset_path(
      const char **directory, const char **file_name)
{
   if (directory)
      *directory = last_preset_dir;
   if (file_name)
      *file_name = last_preset_file;
}

struct video_shader *menu_shader_get(void) { return NULL; }

bool menu_shader_manager_set_preset(
      struct video_shader *menu_shader,
      enum rarch_shader_type type,
      const char *preset_path,
      bool apply)
{
   (void)menu_shader;
   (void)type;
   (void)preset_path;
   (void)apply;
   return true;
}

bool video_shader_driver_get_current_shader(video_shader_ctx_t *shader)
{
   if (shader)
      shader->data = NULL;
   return false;
}

bool menu_shader_manager_set_preset_from_live(
      struct video_shader *menu_shader,
      const struct video_shader *live)
{
   (void)menu_shader;
   (void)live;
   return true;
}
