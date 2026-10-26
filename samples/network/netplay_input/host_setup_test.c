#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>

#define NET_DRIVER_ST_FLAG_NETPLAY_ENABLED 1
#define NET_DRIVER_ST_FLAG_NETPLAY_IS_CLIENT 2
#define RETRO_TASK_FLG_CANCELLED 1
#define RETRO_TASK_FLG_FINISHED 2
#define RETRO_TASK_FLG_MAIN_THREAD 4
#define RETRO_TASK_FLG_MUTE 8
#define CMD_EVENT_NETPLAY_INIT 1
#define RARCH_WARN(text) ((void)(text))
#define RARCH_LOG(text) ((void)(text))

typedef int64_t retro_time_t;
typedef struct retro_task retro_task_t;
struct retro_task
{
   void (*handler)(retro_task_t *);
   void (*cleanup)(retro_task_t *);
   void *state;
   unsigned flags;
};
typedef struct
{
   bool (*func)(retro_task_t *, void *);
   void *userdata;
} task_finder_data_t;
typedef struct
{
   retro_time_t deadline;
} netplay_host_setup_t;
typedef struct
{
   struct
   {
      bool netplay_use_mitm_server;
   } bools;
   struct
   {
      char netplay_mitm_server[16];
   } arrays;
} settings_t;
struct netplay_room
{
   char mitm_address[16];
   int mitm_port;
};
typedef struct
{
   void *data;
   struct netplay_room host_room;
   unsigned flags;
} net_driver_state_t;

static net_driver_state_t networking_driver_st;
static settings_t settings;
static retro_task_t *queued;
static retro_time_t now;
static bool netplay_mitm_query_pending;
static bool netplay_mitm_query_gave_up;
static unsigned netplay_mitm_query_generation;
static char netplay_mitm_query_handle[16];
static unsigned queries, setups, direct, relay;

static settings_t *config_get_ptr(void) { return &settings; }
static retro_time_t cpu_features_get_time_usec(void) { return now; }
static bool string_is_equal(const char *a, const char *b) { return !strcmp(a, b); }
static unsigned task_get_flags(retro_task_t *task) { return task->flags; }
static void task_set_flags(retro_task_t *task, unsigned flags, bool value)
{
   if (value) task->flags |= flags;
   else task->flags &= ~flags;
}
static retro_task_t *task_init(void)
{
   return (retro_task_t*)calloc(1, sizeof(retro_task_t));
}
static bool task_queue_push(retro_task_t *task)
{
   assert(!queued);
   queued = task;
   return true;
}
static bool task_queue_find(task_finder_data_t *find)
{
   return queued && find->func(queued, find->userdata);
}
static bool netplay_mitm_query_begin(const char *handle)
{
   assert(strlen(handle) < sizeof(netplay_mitm_query_handle));
   strcpy(netplay_mitm_query_handle, handle);
   netplay_mitm_query_pending = true;
   queries++;
   return true;
}
static void command_event(unsigned command, void *data);

#include "host_setup.inc"

static void command_event(unsigned command, void *data)
{
   (void)data;
   assert(command == CMD_EVENT_NETPLAY_INIT);
   if (netplay_host_setup_defer()) return;
   setups++;
   if (netplay_mitm_query(settings.arrays.netplay_mitm_server)) relay++;
   else direct++;
}

static void tick(void)
{
   if (!queued) return;
   queued->handler(queued);
   if (queued->flags & RETRO_TASK_FLG_FINISHED)
   {
      queued->cleanup(queued);
      free(queued);
      queued = NULL;
   }
}

static void reset(void)
{
   assert(!queued);
   memset(&networking_driver_st, 0, sizeof(networking_driver_st));
   memset(&settings, 0, sizeof(settings));
   memset(netplay_mitm_query_handle, 0, sizeof(netplay_mitm_query_handle));
   networking_driver_st.flags = NET_DRIVER_ST_FLAG_NETPLAY_ENABLED;
   settings.bools.netplay_use_mitm_server = true;
   strcpy(settings.arrays.netplay_mitm_server, "nyc");
   netplay_mitm_query_pending = false;
   netplay_mitm_query_gave_up = false;
   netplay_mitm_query_generation = 0;
   queries = setups = direct = relay = 0;
   now = 100;
}

int main(void)
{
   unsigned i;
   reset();
   command_event(CMD_EVENT_NETPLAY_INIT, NULL);
   assert(queued && queries == 1 && !setups);
   for (i = 0; i < 100; i++) tick();
   assert(queued && queries == 1 && !setups);
   netplay_mitm_query_pending = false;
   tick();
   assert(!queued && queries == 1 && setups == 1 && direct == 1 && !relay);
   for (i = 0; i < 100; i++) tick();
   assert(queries == 1 && setups == 1 && !netplay_mitm_query_gave_up);
   command_event(CMD_EVENT_NETPLAY_INIT, NULL);
   assert(queued && queries == 2 && setups == 1);
   netplay_mitm_query_pending = false;
   tick();
   assert(!queued && queries == 2 && setups == 2 && direct == 2);

   reset();
   command_event(CMD_EVENT_NETPLAY_INIT, NULL);
   strcpy(networking_driver_st.host_room.mitm_address, "127.0.0.1");
   networking_driver_st.host_room.mitm_port = 55435;
   netplay_mitm_query_pending = false;
   tick();
   assert(!queued && queries == 1 && setups == 1 && relay == 1 && !direct);

   reset();
   command_event(CMD_EVENT_NETPLAY_INIT, NULL);
   now += NETPLAY_MITM_QUERY_TIMEOUT - 1;
   tick();
   assert(queued && !setups && queries == 1);
   now++;
   tick();
   assert(!queued && queries == 1 && setups == 1 && direct == 1);
   assert(netplay_mitm_query_generation == 1 && !netplay_mitm_query_pending);

   reset();
   command_event(CMD_EVENT_NETPLAY_INIT, NULL);
   queued->flags |= RETRO_TASK_FLG_CANCELLED;
   tick();
   assert(!queued && queries == 1 && !setups);

   reset();
   command_event(CMD_EVENT_NETPLAY_INIT, NULL);
   networking_driver_st.flags = 0;
   tick();
   assert(!queued && queries == 1 && !setups);

   reset();
   command_event(CMD_EVENT_NETPLAY_INIT, NULL);
   networking_driver_st.flags |= NET_DRIVER_ST_FLAG_NETPLAY_IS_CLIENT;
   tick();
   assert(!queued && queries == 1 && !setups);
   puts("PASS relay host setup: failure, retry, success, deadline and cancellation");
   return 0;
}
