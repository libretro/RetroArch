/*
Copyright (c) 2013, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_util.h"
#include "mmal_vc_api.h"
#include <stdio.h>
#include <stddef.h>
#include <sys/time.h>
#include <limits.h>
#ifdef __ANDROID__
#include <android/log.h>
#endif
#include "host_applications/linux/libs/debug_sym/debug_sym.h"
#include "mmal_vc_msgnames.h"
#include "mmal_vc_dbglog.h"
#include "vchiq.h"
#include "interface/vmcs_host/vc_imageconv_defs.h"

/** Command-line diagnostics at the VC MMAL API level.
  *
  * @fixme: how does this work with multiple videocores?
  */

struct cmd {
   const char *name;
   int (*pfn)(int argc, const char **argv);
   const char *descr;
   int flags;
};

#define CONNECT 1

static int do_commands(int argc, const char **argv);
static int do_version(int argc, const char **argv);
static int do_stats(int argc, const char **argv);
static int do_usage(int argc, const char **argv);
static int do_create(int argc, const char **argv);
static int do_eventlog(int argc, const char **argv);
static int do_components(int argc, const char **argv);
static int do_mmal_stats(int argc, const char **argv);
static int do_imageconv_stats(int argc, const char **argv);
static int do_compact(int argc, const char **argv);
static int do_autosusptest(int argc, const char **argv);
static int do_camerainfo(int argc, const char **argv);
static int do_host_log(int argc, const char **argv);
static int do_host_log_write(int argc, const char **argv);

static struct cmd cmds[] = {
   { "help",       do_usage, "give this help", 0 },
   { "version",    do_version, "report VC MMAL server version number", CONNECT },
   { "stats",      do_stats, "report VC MMAL statistics", CONNECT },
   { "reset",      do_stats, "reset VC MMAL statistics", CONNECT },
   { "commands",   do_commands, "list available commands", CONNECT },
   { "create",     do_create, "create a component", CONNECT },
   { "eventlog",   do_eventlog, "display event log", 0 },
   { "components", do_components, "[update] list components", 0 },
   { "mmal-stats", do_mmal_stats, "list mmal core stats", CONNECT },
   { "ic-stats",   do_imageconv_stats, "[reset] list imageconv stats", CONNECT },
   { "compact",    do_compact, "trigger memory compaction", CONNECT },
   { "autosusp",   do_autosusptest, "test out auto-suspend/resume", CONNECT },
   { "camerainfo", do_camerainfo, "get camera info", CONNECT },
   { "host_log",   do_host_log, "dumps the MMAL VC log", CONNECT },
   { "host_log_write",  do_host_log_write, "appends a message to the MMAL VC log of host messages", CONNECT },
   { NULL, NULL, NULL, 0},
};

static void do_connect(void)
{
   /* this command needs a vchiq connection */
   MMAL_STATUS_T st;
   if ((st = mmal_vc_init()) != MMAL_SUCCESS)
   {
      fprintf(stderr, "failed to initialize mmal vc library (%i:%s)\n",
            st, mmal_status_to_string(st));
      exit(1);
   }
}

int main(int argc, const char **argv)
{
   int i;

   if (argc < 2)
   {
      do_usage(argc, argv);
      exit(1);
   }

   for (i = 0; cmds[i].name; i++)
   {
      if (strcasecmp(cmds[i].name, argv[1]) == 0)
      {
         int rc;
         if (cmds[i].flags & CONNECT)
         {
            do_connect();
         }
         rc = cmds[i].pfn(argc, argv);

         if (cmds[i].flags & CONNECT)
            mmal_vc_deinit();
         return rc;
      }
   }
   fprintf(stderr,"unknown command %s\n", argv[1]);
   return -1;
}

static int do_commands(int argc, const char **argv)
{
   int i = 0;
   (void)argc; (void)argv;
   while (cmds[i].name)
   {
      printf("%-20s %s\n", cmds[i].name, cmds[i].descr);
      i++;
   }
   return 0;
}

static int do_create(int argc, const char **argv)
{
   MMAL_COMPONENT_T *comp;
   MMAL_STATUS_T st;
   if (argc != 3)
   {
      printf("usage: mmal-vc-diag create <name>\n");
      printf("   e.g. vc.camera\n");
      exit(1);
   }
   st = mmal_component_create(argv[2], &comp);
   if (comp)
      printf("Created component\n");
   else
      printf("Failed to create %s: %d\n", argv[2], st);

   return 0;
}

static int do_version(int argc, const char **argv)
{
   uint32_t maj = UINT_MAX, min = UINT_MAX, minimum;
   MMAL_STATUS_T st = mmal_vc_get_version(&maj, &min, &minimum);
   (void)argc; (void)argv;
   if (st == MMAL_SUCCESS)
   {
      printf("version %d.%02d (min %d)\n", maj, min, minimum);
      return 0;
   }
   else
   {
      fprintf(stderr, "error getting version (%i:%s)\n", st, mmal_status_to_string(st));
      return -1;
   }
}

#define STATS_FIELD(x) { #x, offsetof(MMAL_VC_STATS_T, x) }

static struct {
   const char *name;
   unsigned offset;
} stats_fields [] = {
   STATS_FIELD(buffers.rx),
   STATS_FIELD(buffers.rx_zero_copy),
   STATS_FIELD(buffers.rx_empty),
   STATS_FIELD(buffers.rx_fails),
   STATS_FIELD(buffers.tx),
   STATS_FIELD(buffers.tx_zero_copy),
   STATS_FIELD(buffers.tx_empty),
   STATS_FIELD(buffers.tx_fails),
   STATS_FIELD(buffers.tx_short_msg),
   STATS_FIELD(buffers.rx_short_msg),
   STATS_FIELD(service.created),
   STATS_FIELD(service.pending_destroy),
   STATS_FIELD(service.destroyed),
   STATS_FIELD(service.failures),
   STATS_FIELD(commands.bad_messages),
   STATS_FIELD(commands.executed),
   STATS_FIELD(commands.failed),
   STATS_FIELD(commands.replies),
   STATS_FIELD(commands.reply_fails),
   STATS_FIELD(events.tx),
   STATS_FIELD(events.tx_fails),
   STATS_FIELD(worker.enqueued_messages),
   STATS_FIELD(worker.dequeued_messages),
   STATS_FIELD(worker.max_parameter_set_delay),
   STATS_FIELD(worker.max_messages_waiting)
};

static int do_stats(int argc, const char **argv)
{
   MMAL_VC_STATS_T stats;
   int reset_stats = strcasecmp(argv[1], "reset") == 0;
   MMAL_STATUS_T st = mmal_vc_get_stats(&stats, reset_stats);
   int ret;
   (void)argc; (void)argv;
   if (st != MMAL_SUCCESS)
   {
      fprintf(stderr, "error getting status (%i,%s)\n", st, mmal_status_to_string(st));
      ret = -1;
   }
   else
   {
      unsigned i;
      uint32_t *ptr = (uint32_t*)&stats;
      for (i=0; i<vcos_countof(stats_fields); i++)
      {
         printf("%-32s: %u\n", stats_fields[i].name, ptr[stats_fields[i].offset/sizeof(uint32_t)]);
      }
      ret = 0;
   }
   return ret;
}

static int do_usage(int argc, const char **argv)
{
   const char *last_slash = strrchr(argv[0], '/');
   const char *progname = last_slash ? last_slash+1:argv[0];
   (void)argc;
   printf("usage: %s [command [args]]\n", progname);
   printf("   %s commands - list available commands\n", progname);
   return 0;
}

/*
 * Print out the event log
 */

struct event_handler
{
   MMAL_DBG_EVENT_TYPE_T type;
   void (*handler)(MMAL_DBG_ENTRY_T *entry, char *, size_t);
};

static void on_openclose(MMAL_DBG_ENTRY_T *entry,
                         char *buf,
                         size_t buflen)
{
   switch (entry->event_type) {
      case MMAL_DBG_OPENED: snprintf(buf,buflen,"opened"); break;
      case MMAL_DBG_CLOSED: snprintf(buf,buflen,"closed"); break;
      default: break;
   }
}

static void on_bulk_ack(MMAL_DBG_ENTRY_T *entry,
                        char *buf,
                        size_t buflen)
{
   switch (entry->u.uint)
   {
   case VCHIQ_BULK_RECEIVE_ABORTED: snprintf(buf,buflen,"vchiq bulk rx abort"); break;
   case VCHIQ_BULK_TRANSMIT_ABORTED: snprintf(buf,buflen,"vchiq bulk tx abort"); break;
   case VCHIQ_BULK_TRANSMIT_DONE: snprintf(buf,buflen,"vchiq bulk tx done"); break;
   case VCHIQ_BULK_RECEIVE_DONE: snprintf(buf,buflen,"vchiq bulk rx done"); break;
   default: snprintf(buf,buflen,"vchiq unknown reason %d", entry->u.uint); break;
   }
}

static void on_msg(MMAL_DBG_ENTRY_T *entry,
                   char *buf,
                   size_t buflen)
{
   uint32_t id = entry->u.msg.header.msgid;
   snprintf(buf,buflen,"msgid %d (%s)", id, mmal_msgname(id));
}

static void on_bulk(MMAL_DBG_ENTRY_T *entry,
                    char *buf,
                    size_t buflen)
{
   const char *name = entry->event_type == MMAL_DBG_BULK_TX ? "tx" : "rx";
   snprintf(buf,buflen,"bulk %s len %d", name, entry->u.bulk.len);
}

static struct event_handler handlers[] = {
   { MMAL_DBG_OPENED, on_openclose },
   { MMAL_DBG_CLOSED, on_openclose },
   { MMAL_DBG_BULK_ACK, on_bulk_ack },
   { MMAL_DBG_MSG, on_msg },
   { MMAL_DBG_BULK_TX, on_bulk },
   { MMAL_DBG_BULK_RX, on_bulk },
};
static int n_handlers = sizeof(handlers)/sizeof(handlers[0]);

static void print_mmal_event_log(VC_MEM_ACCESS_HANDLE_T vc, MMAL_DBG_LOG_T *log)
{
   uint32_t i;
   uint32_t n = vcos_min(log->num_entries, log->index);
   uint32_t mask = log->num_entries-1;
   uint32_t start = log->index < log->num_entries ?
      0 : log->index & mask;
   uint32_t last_t = 0;
   (void)vc;

   for (i=0; i<n; i++)
   {
      MMAL_DBG_ENTRY_T *e = &log->entries[(start+i) & mask];
      char buf[256];
      int j;
      uint32_t t = e->time;
      printf("[%08u]: ", t-last_t);
      last_t = t;
      for (j=0; j<n_handlers; j++)
      {
         if (handlers[j].type == e->event_type)
         {
            handlers[j].handler(e, buf, sizeof(buf));
            printf("%s\n", buf);
            break;
         }
      }
      if (j == n_handlers )
         printf("Unknown event type %d\n", e->event_type);
   }
}

static int do_eventlog(int argc, const char **argv)
{
   VC_MEM_ACCESS_HANDLE_T vc;
   VC_MEM_ADDR_T addr;     /** The address of the pointer to the log */
   size_t size;
   VC_MEM_ADDR_T logaddr;       /** The address of the log itself */
   MMAL_DBG_LOG_T log;

   (void)argc; (void)argv;
   int rc;
   if ((rc = OpenVideoCoreMemory(&vc)) < 0)
   {
      fprintf(stderr,"Unable to open videocore memory: %d\n", rc);
      return -1;
   }
   if (!LookupVideoCoreSymbol(vc, "mmal_dbg_log", &addr, &size))
   {
      fprintf(stderr,"Could not get MMAL log address\n");
      goto fail;
   }
   if (!ReadVideoCoreUInt32(vc, &logaddr, addr))
   {
      fprintf(stderr,"Could not read MMAL log pointer at address 0x%x\n",
              addr);
      goto fail;
   }
   if (!ReadVideoCoreMemory(vc, &log, logaddr, sizeof(log)))
   {
      fprintf(stderr,"Could not read MMAL log at address 0x%x\n",
              logaddr);
      goto fail;
   }
   if (log.magic != MMAL_MAGIC)
   {
      fprintf(stderr,"Bad magic 0x%08x in log at 0x%x\n", log.magic, logaddr);
      goto fail;
   }
   if (log.size != sizeof(log))
   {
      fprintf(stderr,"MMAL Log size mismatch (got %d, expected %d)\n",
              log.size, sizeof(log));
      goto fail;
   }
   if (log.elemsize != sizeof(MMAL_DBG_ENTRY_T))
   {
      fprintf(stderr,"MMAL log element size mismatch (got %d, expected %d)\n",
              log.elemsize, sizeof(MMAL_DBG_ENTRY_T));
      goto fail;
   }

   printf("reading MMAL log at 0x%x version %d magic %x\n",
          logaddr, log.version, log.magic);
   printf("%d events, %d entries each size %d\n", log.index, log.num_entries,
          log.elemsize);
   print_mmal_event_log(vc, &log);

   CloseVideoCoreMemory(vc);
   return 0;
fail:
   CloseVideoCoreMemory(vc);
   return -1;

}

static int print_component_stats(const MMAL_VC_STATS_T *stats)
{
   size_t i;
   if (stats->components.list_size > 64)
   {
      fprintf(stderr,"component array looks corrupt (list size %d\n",
            stats->components.list_size);
      goto fail;
   }
   printf("%d created, %d destroyed (%d destroying), %d create failures\n",
         stats->components.created,
         stats->components.destroyed,
         stats->components.destroying,
         stats->components.failed);

   for (i=0; i < stats->components.list_size; i++)
   {
      const struct MMAL_VC_COMP_STATS_T *cs = stats->components.component_list+i;
      const char *state;
      /* coverity[overrun-local] */
      if (cs->state != MMAL_STATS_COMP_IDLE)
      {
         switch (cs->state)
         {
            case MMAL_STATS_COMP_CREATED: state = "created"; break;
            case MMAL_STATS_COMP_DESTROYING: state = "destroying"; break;
            case MMAL_STATS_COMP_DESTROYED: state = "destroyed"; break;
            default: state = "corrupt"; break;
         }
         printf("%-32s: %s: pid %d address %p pool mem alloc size %d\n",
               cs->name, state, cs->pid, cs->comp, cs->pool_mem_alloc_size);
      }
   }
   return 0;
fail:
   return -1;
}

static int do_components(int argc, const char **argv)
{
   VC_MEM_ACCESS_HANDLE_T vc;
   VC_MEM_ADDR_T addr, statsaddr;
   size_t size;
   MMAL_VC_STATS_T stats;
   int rc;

   if (argc > 2 && (strcasecmp(argv[2], "update") == 0))
   {
      MMAL_STATUS_T status;
      do_connect();
      status = mmal_vc_get_stats(&stats, 0);
      if (status != MMAL_SUCCESS)
      {
         fprintf(stderr, "Failed to update MMAL stats. error %s",
              mmal_status_to_string(status));
         return -1;
      }
   }
   else
   {
      if ((rc = OpenVideoCoreMemory(&vc)) < 0)
      {
         fprintf(stderr,"Unable to open videocore memory: %d\n", rc);
         return -1;
      }
      if (!LookupVideoCoreSymbol(vc, "mmal_vc_stats", &addr, &size))
      {
         fprintf(stderr,"Could not get MMAL stats address\n");
         goto fail;
      }
      if (!ReadVideoCoreUInt32(vc, &statsaddr, addr))
      {
         fprintf(stderr,"Could not read MMAL stats pointer at address 0x%x\n",
               addr);
         goto fail;
      }
      if (!ReadVideoCoreMemory(vc, &stats, statsaddr, sizeof(stats)))
      {
         fprintf(stderr,"Could not read MMAL stats at address 0x%x\n", addr);
         goto fail;
      }
      CloseVideoCoreMemory(vc);
   }
   rc = print_component_stats(&stats);
   return rc;

fail:
   CloseVideoCoreMemory(vc);
   return -1;
}

static int do_mmal_stats(int argc, const char **argv)
{
   unsigned comp_index = 0;
   MMAL_BOOL_T more_ports = MMAL_TRUE, reset = MMAL_FALSE;
   unsigned port_index = 0;
   MMAL_PORT_TYPE_T type = MMAL_PORT_TYPE_INPUT;

   if (argc >= 3)
      reset = strcasecmp(argv[2], "reset") == 0;

   printf("component\t\tport\t\tbuffers\t\tfps\tdelay\n");
   while (more_ports)
   {
      int dir;
      const char *dirnames[] = {"rx","tx"};
      MMAL_STATS_RESULT_T result;

      for (dir = MMAL_CORE_STATS_RX; dir <= MMAL_CORE_STATS_TX; dir++)
      {
         double framerate;
         MMAL_CORE_STATISTICS_T stats;
         char name[32];
         MMAL_STATUS_T status = mmal_vc_get_core_stats(&stats,
                                                      &result,
                                                      name, sizeof(name),
                                                      type,
                                                      comp_index,
                                                      port_index,
                                                      dir,
                                                      reset);
         if (status != MMAL_SUCCESS)
         {
            fprintf(stderr, "could not get core stats: %s\n",
                  mmal_status_to_string(status));
            exit(1);
         }

         if (result == MMAL_STATS_FOUND)
         {
            if (stats.first_buffer_time == stats.last_buffer_time)
               framerate = 0;
            else
               framerate = 1.0e6*(1+stats.buffer_count)/(stats.last_buffer_time-stats.first_buffer_time);

            printf("%-20s\t%d [%s]%2s\t%-10d\t%4.1f\t%d\n",
                  name, port_index,
                  type == MMAL_PORT_TYPE_INPUT ? "in " : "out",
                  dirnames[dir],
                  stats.buffer_count, framerate, stats.max_delay);

         }
      }

      switch (result)
      {
      case MMAL_STATS_FOUND:
         port_index++;
         break;

      case MMAL_STATS_COMPONENT_NOT_FOUND:
         more_ports = MMAL_FALSE;
         break;

      case MMAL_STATS_PORT_NOT_FOUND:
         port_index = 0;
         if (type == MMAL_PORT_TYPE_INPUT)
         {
            type = MMAL_PORT_TYPE_OUTPUT;
         }
         else
         {
            type = MMAL_PORT_TYPE_INPUT;
            comp_index++;
         }
         break;
      default:
         fprintf(stderr, "bad result from query: %d\n", result);
         vcos_assert(0);
         exit(1);
      }
   }
   return 0;
}

static int do_imageconv_stats(int argc, const char **argv)
{
   VC_MEM_ACCESS_HANDLE_T vc;
   VC_MEM_ADDR_T addr, statsaddr;
   size_t size;
   IMAGECONV_STATS_T stats;
   long convert_time;
   double frame_rate;
   int rc;
   int reset_stats = 0;

   if (argc > 2)
      reset_stats = strcasecmp(argv[2], "reset") == 0;

   if ((rc = OpenVideoCoreMemory(&vc)) < 0)
   {
      fprintf(stderr,"Unable to open videocore memory: %d\n", rc);
      return -1;
   }
   if (!LookupVideoCoreSymbol(vc, "imageconv_stats", &addr, &size))
   {
      fprintf(stderr,"Could not get imageconv stats address\n");
      goto fail;
   }
   if (!ReadVideoCoreUInt32(vc, &statsaddr, addr))
   {
      fprintf(stderr, "Could not read imageconv stats address\n");
      goto fail;
   }

   if (reset_stats)
   {
      memset(&stats, 0, sizeof(stats));
      stats.magic = IMAGECONV_STATS_MAGIC;
      if (!WriteVideoCoreMemory(vc, &stats, statsaddr, sizeof(stats)))
      {
         fprintf(stderr, "Could not write stats at 0x%x\n", statsaddr);
         goto fail;
      }
   }

   if (!ReadVideoCoreMemory(vc, &stats, statsaddr, sizeof(stats)))
   {
      fprintf(stderr, "Could not read stats at 0x%x\n", statsaddr);
      goto fail;
   }

   if (stats.magic != IMAGECONV_STATS_MAGIC)
   {
      fprintf(stderr, "Bad magic 0x%x\n", stats.magic);
      goto fail;
   }

   if (stats.conversions)
      convert_time = stats.time_spent / stats.conversions;
   else
      convert_time = 0;

   if (stats.conversions)
      frame_rate = 1000000.0 * stats.conversions /
         (stats.last_image_ts - stats.first_image_ts);
   else
      frame_rate = 0;

   printf("%-25s:\t%d\n", "conversions", stats.conversions);
   printf("%-25s:\t%d\n", "size requests", stats.size_requests);
   printf("%-25s:\t%d\n", "max vrf delay", stats.max_vrf_delay);
   printf("%-25s:\t%d\n", "vrf wait time", stats.vrf_wait_time);
   printf("%-25s:\t%d\n", "duplicate conversions", stats.duplicate_conversions);
   printf("%-25s:\t%d\n", "failures", stats.failures);
   printf("%-25s:\t%ld\n", "convert time / image (us)", convert_time);
   printf("%-25s:\t%.1f\n", "client frame_rate", frame_rate);
   printf("%-25s:\t%d us\n", "max delay to consume", stats.max_delay);

   CloseVideoCoreMemory(vc);
   return 0;
fail:
   CloseVideoCoreMemory(vc);
   return -1;

}

static int do_compact(int argc, const char **argv)
{
   uint32_t duration;

   if (argc > 2)
   {
      if (strcmp(argv[2], "a") == 0)
      {
         mmal_vc_compact(MMAL_VC_COMPACT_AGGRESSIVE, &duration);
         printf("Triggered aggressive compaction on VC - duration %u us.\n", duration);
      }
      else if (strcmp(argv[2], "d") == 0)
      {
         mmal_vc_compact(MMAL_VC_COMPACT_DISCARD, &duration);
         printf("Triggered discard compaction on VC - duration %u us.\n", duration);
      }
      else if (strcmp(argv[2], "n") == 0)
      {
         mmal_vc_compact(MMAL_VC_COMPACT_NORMAL, &duration);
         printf("Triggered normal compaction on VC - duration %u us.\n", duration);
      }
      else
      {
         printf("Invalid memory compaction option %s\n.", argv[2]);
         exit(1);
      }
   }
   else
   {
      printf("Invalid memory compaction arguments.  Need to specify 'a', 'n' or 't'.\n");
      exit(1);
   }
   return 0;
}

/* Autosuspend test. Create a component, but kill process
 * shortly after startup.
 */

static int autosusp_signal;

static void autosusp_timeout_handler(int cause, siginfo_t *how, void *ucontext)
{
   (void)how; (void)ucontext; (void)cause;
   printf("Sending signal %d\n", autosusp_signal);
   kill(getpid(), autosusp_signal);
}

static int do_autosusptest(int argc, const char **argv)
{
   long timeout;
   struct timeval interval;
   MMAL_STATUS_T status;

   if (argc != 4)
   {
      printf("usage: %s autosusp <timeout-ms> <signal>\n",
             argv[0]);
      printf("   e.g. 650 9\n");
      exit(1);
   }
   timeout = 1000 * atoi(argv[2]);
   autosusp_signal = atoi(argv[3]);

   if ((status=mmal_vc_use()) != MMAL_SUCCESS)
   {
      fprintf(stderr,"mmal_vc_use failed: %d\n", status);
      exit(1);
   }

   /* install a signal handler for the alarm */
   struct sigaction sa;
   memset(&sa, 0, sizeof(struct sigaction));
   sa.sa_sigaction = autosusp_timeout_handler;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = SA_SIGINFO;
   if (sigaction(SIGALRM, &sa, 0))
   {
      perror("sigaction");
      exit(1);
   }

   /* when to expire */
   interval.tv_sec = timeout / 1000000;
   interval.tv_usec = timeout % 1000000;

   struct itimerval alarm_spec = {
     .it_interval = {0,0},
     .it_value = interval
   };

   int rc = setitimer(ITIMER_REAL, &alarm_spec, NULL);
   if (rc < 0)
   {
      perror("setitimer failed");
      exit(1);
   }

   usleep(timeout + 1000000);
   printf("%s: not killed by timer\n", argv[0]);
   mmal_vc_release();

   return 0;
}

static int do_camerainfo(int argc, const char **argv)
{
   MMAL_COMPONENT_T *comp;
   MMAL_STATUS_T st;
   MMAL_PARAMETER_CAMERA_INFO_T mmal_camera_config_info;
   unsigned i;
   (void)argc;
   (void)argv;

   st = mmal_component_create("vc.camera_info", &comp);
   if (st != MMAL_SUCCESS)
   {
      fprintf(stderr, "Failed to create camera_info: %d\n", st);
      exit(1);
   }

   mmal_camera_config_info.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
   mmal_camera_config_info.hdr.size = sizeof(mmal_camera_config_info);
   st = mmal_port_parameter_get(comp->control, &mmal_camera_config_info.hdr);
   if (st != MMAL_SUCCESS)
   {
      fprintf(stderr, "%s: get param failed:%d", __FUNCTION__, st);
      exit(1);
   }

   printf("cameras  : %d\n", mmal_camera_config_info.num_cameras);
   printf("flashes  : %d\n", mmal_camera_config_info.num_flashes);

   for (i=0; i<mmal_camera_config_info.num_cameras; i++)
   {
      printf("camera %u : port %u: %u x %u lens %s\n",
             i,
             mmal_camera_config_info.cameras[i].port_id,
             mmal_camera_config_info.cameras[i].max_width,
             mmal_camera_config_info.cameras[i].max_height,
             mmal_camera_config_info.cameras[i].lens_present ? "present" : "absent");
   }
   for (i=0; i<mmal_camera_config_info.num_flashes; i++)
   {
      const char *flash_type;
      switch (mmal_camera_config_info.flashes[i].flash_type)
      {
      case MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_XENON:
         flash_type = "Xenon";
         break;
      case MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_LED:
         flash_type = "LED";
         break;
      case MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_OTHER:
         flash_type = "Other";
         break;
      default:
         flash_type = "invalid";
      }
      printf("flash %u  : flash type %s\n", i, flash_type);
   }
   mmal_component_destroy(comp);
   return 0;
}

static int do_host_log(int argc, const char **argv)
{
   VC_MEM_ACCESS_HANDLE_T vc;
   VC_MEM_ADDR_T log_addr;
   MMAL_VC_HOST_LOG_T log;
   const char *msg = &log.buffer[0];
   const char *log_end = &log.buffer[sizeof(log.buffer)];
   int rc;

   (void) argc;
   (void) argv;

   if ((rc = OpenVideoCoreMemory(&vc)) < 0)
   {
      fprintf(stderr,"Unable to open videocore memory: %d\n", rc);
      return -1;
   }
   if (!ReadVideoCoreUInt32BySymbol(vc, "mmal_host_log", &log_addr))
   {
      fprintf(stderr, "Could not read mmal_host_log address\n");
      goto fail;
   }
   if (!ReadVideoCoreMemory(vc, &log, log_addr, sizeof(log)))
   {
      fprintf(stderr, "Could not read log at 0x%x\n", log_addr);
      goto fail;
   }

   while (msg < log_end)
   {
      if (*msg)
         msg += printf("%s", msg);

      /* Skip multiple null characters */
      while (msg < log_end && *msg == 0) ++msg;
   }

   CloseVideoCoreMemory(vc);
   return 0;

fail:
   CloseVideoCoreMemory(vc);
   return -1;
}

static int do_host_log_write(int argc, const char **argv)
{
   if (argc > 2)
      mmal_vc_host_log(argv[2]);
   return 0;
}
