/* Autoconfig connects and disconnects reported from another thread.
 *
 * Drives the shipping tasks/task_autodetect.c on the shipping task
 * queue, with the rest of RetroArch stubbed below.  A driver's own
 * thread (libusb's hotplug thread, the Wii's HID poll thread, the
 * video thread starting the input driver) reports pads.  Each report
 * is handed to the main thread and made there, in the order it was
 * reported, so the queue is searched where its answer is exact: the
 * same pad reported twice queues one connect.  task_queue_find() and
 * task_queue_push() are wrapped to see which thread calls them.  Run
 * on the unthreaded queue, then on the threaded one.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <compat/strl.h>
#include <features/features_cpu.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <queues/task_queue.h>
#include <retro_dirent.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>

#include "command.h"
#include "configuration.h"
#include "input/input_driver.h"
#include "input/input_remapping.h"
#include "msg_hash.h"
#include "runloop.h"
#include "tasks/tasks_internal.h"
#include "verbosity.h"

#define LIMIT_US 2000000

static unsigned failures;

static void check(bool cond, const char *what)
{
   if (cond)
      printf("  [pass] %s\n", what);
   else
   {
      printf("  [FAIL] %s\n", what);
      failures++;
   }
   fflush(stdout);
}

/* --- the rest of RetroArch, stubbed ----------------------------------- */

/* Written by the autoconfig callbacks, on the main thread. */
static char port_names[MAX_INPUT_DEVICES][128];

const char* const input_builtin_autoconfs[] = { NULL };

uint32_t djb2_calculate(const char *str)
{
   uint32_t hash = 5381;

   for (; *str; str++)
      hash = ((hash << 5) + hash) + (unsigned char)*str;
   return hash;
}

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }

bool command_event(enum event_command action, void *data) { return false; }
/* All zero: autoconfig off, so a connect scans no directory, and no
 * notification or pause. */
settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}
runloop_state_t *runloop_state_get_ptr(void)
{
   static runloop_state_t state;
   return &state;
}
const char *msg_hash_to_str(enum msg_hash_enums msg) { return ""; }

void config_file_free(config_file_t *conf) { }
config_file_t *config_file_new(const char *path) { return NULL; }
config_file_t *config_file_new_alloc(void) { return NULL; }
config_file_t *config_file_new_take_string(char *from_string, size_t s_len,
      const char *path)
{
   free(from_string);
   return NULL;
}
bool config_file_write(config_file_t *conf, const char *path, bool val)
{
   return false;
}
struct config_entry_list *config_get_entry(const config_file_t *conf,
      const char *key)
{
   return NULL;
}
bool config_get_float(config_file_t *conf, const char *entry, float *in)
{
   return false;
}
bool config_get_int(config_file_t *conf, const char *entry, int *in)
{
   return false;
}
void config_set_string(config_file_t *conf, const char *entry,
      const char *val) { }
int64_t filestream_read_file(const char *path, void **buf, int64_t *len)
{
   return 0;
}

size_t fill_pathname_join(char *s, const char *dir, const char *path,
      size_t len)
{
   if (len)
      *s = '\0';
   return 0;
}
const char *path_basename_nocompression(const char *path) { return path; }
const char *path_get_extension(const char *path) { return ""; }
bool path_is_directory(const char *path) { return false; }
struct RDIR *retro_opendir(const char *name) { return NULL; }
int retro_readdir(struct RDIR *rdir) { return 0; }
const char *retro_dirent_get_name(struct RDIR *rdir) { return NULL; }
int retro_dirent_stat(struct RDIR *rdir, int64_t *size, int64_t *mtime)
{
   return -1;
}
void retro_closedir(struct RDIR *rdir) { }

void input_config_set_device_name(unsigned port, const char *name)
{
   strlcpy(port_names[port], name, sizeof(port_names[port]));
}
void input_config_clear_device_name(unsigned port)
{
   port_names[port][0] = '\0';
}
const char *input_config_get_device_name(unsigned port)
{
   return port_names[port];
}
void input_config_clear_device_config_name(unsigned port) { }
void input_config_clear_device_display_name(unsigned port) { }
void input_config_clear_device_joypad_driver(unsigned port) { }
void input_config_follow_shared_ports(const uint8_t *leader) { }
bool input_config_get_device_autoconfigured(unsigned port) { return false; }
const char *input_config_get_device_display_name(unsigned port) { return ""; }
const char *input_config_get_device_joypad_driver(unsigned port) { return ""; }
const char *input_config_get_device_phys(unsigned port) { return ""; }
uint16_t input_config_get_device_pid(unsigned port) { return 0; }
uint16_t input_config_get_device_vid(unsigned port) { return 0; }
void input_config_reset_autoconfig_binds(unsigned port) { }
bool input_config_sanitize_joypad_indices(void) { return false; }
void input_config_set_autoconfig_binds(unsigned port, void *data) { }
void input_config_set_device_autoconfigured(unsigned port,
      bool autoconfigured) { }
void input_config_set_device_config_name(unsigned port, const char *name) { }
void input_config_set_device_deadzones(unsigned port, float stick,
      float trigger) { }
void input_config_set_device_display_name(unsigned port, const char *name) { }
void input_config_set_device_joypad_driver(unsigned port,
      const char *driver) { }
void input_config_set_device_phys(unsigned port, const char *phys) { }
void input_config_set_device_pid(unsigned port, uint16_t pid) { }
void input_config_set_device_vid(unsigned port, uint16_t vid) { }
unsigned input_config_shared_ports(uint8_t *leader) { return 0; }
void input_driver_deadzones_refresh(void) { }
void input_driver_registry_connect(unsigned slot, const char *provider,
      const char *name, const char *phys, uint16_t vid, uint16_t pid) { }
void input_driver_registry_disconnect(unsigned slot) { }

/* --- which thread finds and pushes ------------------------------------ */

static retro_atomic_int_t finds_off_main;
static retro_atomic_int_t pushes_off_main;
static unsigned           pushes_on_main;

bool __real_task_queue_find(task_finder_data_t *find_data);
bool __real_task_queue_push(retro_task_t *task);

bool __wrap_task_queue_find(task_finder_data_t *find_data)
{
   if (!task_is_on_main_thread())
      retro_atomic_fetch_add_int(&finds_off_main, 1);
   return __real_task_queue_find(find_data);
}

bool __wrap_task_queue_push(retro_task_t *task)
{
   if (task_is_on_main_thread())
      pushes_on_main++;
   else
      retro_atomic_fetch_add_int(&pushes_off_main, 1);
   return __real_task_queue_push(task);
}

/* --- reports from a driver's thread ----------------------------------- */

typedef struct
{
   const char *name;
   unsigned    port;
   unsigned    vid;
   bool        connect;
} report_t;

static retro_atomic_int_t reports_taken;

/* Makes each report in the list, up to one without a name. */
static void report_all(void *data)
{
   const report_t *report = (const report_t*)data;
   int             taken  = 1;

   for (; report->name; report++)
   {
      bool ok = report->connect
         ? input_autoconfigure_connect(report->name, NULL, NULL, "hid",
               report->port, report->vid, 1)
         : input_autoconfigure_disconnect(report->port, report->name);
      if (!ok)
         taken = 0;
   }
   retro_atomic_store_release_int(&reports_taken, taken);
}

/* Whether every report returned true. */
static bool report_from_thread(const report_t *reports)
{
   sthread_t *th;

   retro_atomic_store_release_int(&reports_taken, -1);
   if (!(th = sthread_create(report_all, (void*)reports)))
      return false;
   sthread_join(th);
   return retro_atomic_load_acquire_int(&reports_taken) == 1;
}

/* Checks the queue until no connect or disconnect is pending. */
static bool settle(void)
{
   retro_time_t until = cpu_features_get_time_usec() + LIMIT_US;

   while (input_autoconfigure_pending())
   {
      if (cpu_features_get_time_usec() > until)
         return false;
      task_queue_check();
      retro_sleep(1);
   }
   task_queue_check();
   return true;
}

static void pushes_reset(void)
{
   pushes_on_main = 0;
   retro_atomic_store_release_int(&pushes_off_main, 0);
}

static void lanes(bool threaded)
{
   static const report_t pad_a[]   = {
      { "pad A", 0, 1, true }, { NULL, 0, 0, false } };
   static const report_t twice[]   = {
      { "pad A", 1, 1, true }, { "pad A", 1, 1, true },
      { NULL, 0, 0, false } };
   static const report_t swapped[] = {
      { "pad A", 2, 1, true }, { "pad A", 2, 1, false },
      { "pad B", 2, 2, true }, { NULL, 0, 0, false } };
   static const report_t gone[]    = {
      { "pad D", 4, 4, true }, { "pad D", 4, 4, false },
      { NULL, 0, 0, false } };
   static const report_t dropped[] = {
      { "pad E", 5, 5, true }, { NULL, 0, 0, false } };
   bool reported;

   printf("reports from another thread, %s queue\n",
         threaded ? "threaded" : "unthreaded");
   memset(port_names, 0, sizeof(port_names));
   retro_atomic_store_release_int(&finds_off_main, 0);
   task_queue_init(threaded, NULL);

   check(report_from_thread(pad_a),
         "a connect from another thread returns true at once");
   check(input_autoconfigure_pending(),
         "it is pending until the main thread has made it");
   check(settle() && !strcmp(port_names[0], "pad A"),
         "the main thread makes it");

   pushes_reset();
   check(report_from_thread(twice) && settle()
         && !strcmp(port_names[1], "pad A"),
         "the same pad reported twice is connected");
   check(     retro_atomic_load_acquire_int(&pushes_off_main) == 2
         && pushes_on_main == 1,
         "and queues one connect, on the main thread");

   check(report_from_thread(swapped) && settle()
         && !strcmp(port_names[2], "pad B"),
         "a connect, a disconnect and a connect on one port end with the last");
   check(report_from_thread(gone) && settle() && !*port_names[4],
         "a connect then a disconnect on one port leave it empty");

   reported = report_from_thread(dropped);
   task_queue_reset();
   check(reported && settle() && !*port_names[5],
         "a request cancelled before the main thread makes it is dropped");

   pushes_reset();
   check(input_autoconfigure_connect("pad C", NULL, NULL, "hid", 3, 3, 1)
         && pushes_on_main == 1
         && !retro_atomic_load_acquire_int(&pushes_off_main),
         "a connect on the main thread is queued at once, as before");
   check(settle() && !strcmp(port_names[3], "pad C"), "and is made");

   check(!retro_atomic_load_acquire_int(&finds_off_main),
         "no find runs off the main thread");
   task_queue_deinit();
}

int main(void)
{
   lanes(false);
   lanes(true);

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("autoconfig reports from other threads are made on the main thread\n");
   return 0;
}
