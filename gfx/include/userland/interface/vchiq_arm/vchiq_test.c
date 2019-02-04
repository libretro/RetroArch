/*
Copyright (c) 2012, Broadcom Europe Ltd
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vchiq_test.h"
#ifndef USE_VCHIQ_ARM
#define USE_VCHIQ_ARM
#endif
#include "interface/vchi/vchi.h"

#define NUM_BULK_BUFS 2
#define BULK_SIZE (1024*256)
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define INIT_PARAMS(sp_, fourcc_, cb_, userdata_, ver_) \
   do {                                \
      memset((sp_), 0, sizeof(*(sp_)));   \
      (sp_)->fourcc = fourcc_;            \
      (sp_)->callback = cb_;              \
      (sp_)->userdata = userdata_;        \
      (sp_)->version = ver_;              \
      (sp_)->version_min = ver_;          \
   } while (0)


static struct test_params g_params = { MSG_CONFIG, 64, 100, 1, 1, 1, 0, 0, 0, 0 };
static const char *g_servname = "echo";

static VCOS_EVENT_T g_server_reply;
static VCOS_EVENT_T g_shutdown;
static VCOS_MUTEX_T g_mutex;

static const char *g_server_error = NULL;

static volatile int g_sync_mode = 0;

static VCOS_EVENT_T func_test_sync;
static int want_echo = 1;
static int func_error = 0;
static int fun2_error = 0;
static int func_data_test_start = -1;
static int func_data_test_end = 0x7fffffff;
static int func_data_test_iter;

char *bulk_bufs[NUM_BULK_BUFS * 2];
char *bulk_tx_data[NUM_BULK_BUFS];
char *bulk_rx_data[NUM_BULK_BUFS];

static int ctrl_received = 0;
static int bulk_tx_sent = 0;
static int bulk_rx_sent = 0;
static int bulk_tx_received = 0;
static int bulk_rx_received = 0;

static char clnt_service1_data[SERVICE1_DATA_SIZE];
static char clnt_service2_data[SERVICE2_DATA_SIZE];

static VCOS_LOG_CAT_T vchiq_test_log_category;

static int vchiq_test(int argc, char **argv);
static VCHIQ_STATUS_T vchiq_bulk_test(void);
static VCHIQ_STATUS_T vchiq_ctrl_test(void);
static VCHIQ_STATUS_T vchiq_functional_test(void);
static VCHIQ_STATUS_T vchiq_ping_test(void);
static VCHIQ_STATUS_T vchiq_signal_test(void);

static VCHIQ_STATUS_T do_functional_test(void);
static void do_ping_test(VCHIQ_SERVICE_HANDLE_T service, int size, int async, int oneway, int iters);
static void do_vchi_ping_test(VCHI_SERVICE_HANDLE_T service, int size, int async, int oneway, int iters);

static VCHIQ_STATUS_T func_data_test(VCHIQ_SERVICE_HANDLE_T service, int size, int align, int server_align);

#ifdef VCHIQ_LOCAL
static void *vchiq_test_server(void *);
#endif

static VCHIQ_STATUS_T
clnt_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header,
   VCHIQ_SERVICE_HANDLE_T service, void *bulk_userdata);
static void
vchi_clnt_callback(void *callback_param, VCHI_CALLBACK_REASON_T reason,
   void *handle);
static VCHIQ_STATUS_T func_clnt_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header,
                                         VCHIQ_SERVICE_HANDLE_T service, void *bulk_userdata);
static VCHIQ_STATUS_T fun2_clnt_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header,
                                         VCHIQ_SERVICE_HANDLE_T service, void *bulk_userdata);
static int mem_check(const void *expected, const void *actual, int size);
static void usage(void);
static void check_timer(void);
static char *buf_align(char *buf, int align_size, int align);

#ifdef ANDROID

static int g_timeout_ms = 0;
static pid_t main_process_pid;
static void kill_timeout_handler(int cause, siginfo_t *how, void *ucontext);
static int setup_auto_kill(int timeout_ms);

#endif

#ifdef __linux__

#include <fcntl.h>
#include <sys/ioctl.h>
#include "interface/vmcs_host/vc_cma.h"

static void reserve_test(int reserve, int delay)
{
   int fd = open("/dev/vc-cma", O_RDWR);
   int rc = -1;
   if (fd >= 0)
   {
      rc = ioctl(fd, VC_CMA_IOC_RESERVE, reserve);
      if (rc == 0)
      {
         printf("Sleeping for %d seconds...\n", delay);
         sleep(delay);
      }
      else
         printf("* failed to ioctl /dev/vc-cma - rc %d\n", rc);
      close(fd);
   }
   else
      printf("* failed to open /dev/vc-cma - rc %d\n", fd);
}

#endif

static int vchiq_test(int argc, char **argv)
{
   VCHIQ_STATUS_T status;
   int run_bulk_test = 0;
   int run_ctrl_test = 0;
   int run_functional_test = 0;
   int run_ping_test = 0;
   int run_signal_test = 0;
   int verbose = 0;
   int argn;
 
   argn = 1;
   while ((argn < argc) && (argv[argn][0] == '-'))
   {
      const char *arg = argv[argn++];
      if (strcmp(arg, "-s") == 0)
      {
         g_servname = argv[argn++];
         if (!g_servname || (strlen(g_servname) != 4))
         {
            usage();
         }
      }
      else if (strcasecmp(arg, "-a") == 0)
      {
         g_params.align_size = (strcmp(arg, "-A") == 0) ? 4096 : 32;
         g_params.client_align = atoi(argv[argn++]);
         g_params.server_align = atoi(argv[argn++]);
      }
      else if (strcmp(arg, "-b") == 0)
      {
         run_bulk_test = 1;
         g_params.blocksize = atoi(argv[argn++]);
      }
      else if (strcmp(arg, "-c") == 0)
      {
         run_ctrl_test = 1;
         g_params.blocksize = atoi(argv[argn++]);
      }
      else if (strcmp(arg, "-e") == 0)
      {
         want_echo = 0;
      }
      else if (strcmp(arg, "-f") == 0)
      {
         run_functional_test = 1;
      }
      else if (strcmp(arg, "-h") == 0)
      {
         usage();
      }
      else if (strcmp(arg, "-i") == 0)
      {
         run_signal_test = 1;
      }
      else if (strcmp(arg, "-m") == 0)
      {
         g_params.client_message_quota = atoi(argv[argn++]);
      }
      else if (strcmp(arg, "-M") == 0)
      {
         g_params.server_message_quota = atoi(argv[argn++]);
      }
      else if (strcmp(arg, "-p") == 0)
      {
         run_ping_test = 1;
         g_params.iters = 1000;
      }
      else if (strcmp(arg, "-q") == 0)
      {
         /* coverity[missing_lock : FALSE] - g_server_reply is not used for mutual exclusion */
         g_params.verify = 0;
      }
#ifdef __linux__
      else if (strcmp(arg, "-r") == 0)
      {
         int reserve, delay;
         if (argn+1 < argc)
         {
            reserve = atoi(argv[argn++]);
            delay = atoi(argv[argn++]);
            reserve_test(reserve, delay);
            exit(0);
         }
         else
         {
            printf("not enough arguments (-r reserve delay)\n");
            exit(-1);
         }
      }
#endif
#ifdef ANDROID
      else if (strcmp(arg, "-K") == 0)
      {
         if (argn < argc)
            g_timeout_ms = atoi(argv[argn++]);
         else
         {
            printf("not enough arguments (-K timeout)\n");
            exit(-1);
         }
      }
#endif
      else if (strcmp(arg, "-t") == 0)
      {
         check_timer();
         exit(0);
      }
      else if (strcmp(arg, "-v") == 0)
      {
         verbose = 1;
      }
      else if (strcmp(arg, "-S") == 0)
      {
         func_data_test_start = atoi(argv[argn++]);
      }
      else if (strcmp(arg, "-E") == 0)
      {
         func_data_test_end = atoi(argv[argn++]);
      }
      else
      {
         printf("* unknown option '%s'\n", arg);
         usage();
      }
   }

   if ((run_ctrl_test + run_bulk_test + run_functional_test + run_ping_test + run_signal_test) != 1)
      usage();

   if (argn < argc)
   {
      g_params.iters = atoi(argv[argn++]);
      if (argn != argc)
      {
         usage();
      }
   }

   vcos_log_set_level(VCOS_LOG_CATEGORY, verbose ? VCOS_LOG_TRACE : VCOS_LOG_INFO);
   vcos_log_register("vchiq_test", VCOS_LOG_CATEGORY);

#ifdef VCHIQ_LOCAL
   {
      static VCOS_THREAD_T server_task;
      void          *pointer = NULL;
      int stack_size = 4096;

#if VCOS_CAN_SET_STACK_ADDR
      pointer = malloc(stack_size);
      vcos_demand(pointer);
#endif
      vcos_thread_create_classic(&server_task, "vchiq_test server", vchiq_test_server, (void *)0, pointer, stack_size,
                                 10 | VCOS_AFFINITY_CPU1, 20, VCOS_START);
   }
#endif

   vcos_event_create(&g_server_reply, "g_server_reply");
   vcos_event_create(&g_shutdown, "g_shutdown");
   vcos_mutex_create(&g_mutex, "g_mutex");


   status = VCHIQ_ERROR;

   if (run_bulk_test)
      status = vchiq_bulk_test();
   else if (run_ctrl_test)
      status = vchiq_ctrl_test();
   else if (run_functional_test)
      status = vchiq_functional_test();
   else if (run_ping_test)
      status = vchiq_ping_test();
   else if (run_signal_test)
      status = vchiq_signal_test();

   return (status == VCHIQ_SUCCESS) ? 0 : -1;
}

static VCHIQ_STATUS_T
vchiq_bulk_test(void)
{
   VCHIQ_INSTANCE_T vchiq_instance;
   VCHIQ_SERVICE_HANDLE_T vchiq_service;
   VCHIQ_SERVICE_PARAMS_T service_params;
   VCHIQ_ELEMENT_T elements[4];
   VCHIQ_ELEMENT_T *element;
   int num_bulk_bufs = NUM_BULK_BUFS;
   uint32_t start, end;
   int i;

   g_params.blocksize *= 1024;

   for (i = 0; i < (NUM_BULK_BUFS * 2); i++)
   {
      bulk_bufs[i] = malloc(g_params.blocksize + BULK_ALIGN_SIZE - 1);
      if (!bulk_bufs[i])
      {
         printf("* out of memory\n");
         while (i > 0)
         {
            free(bulk_bufs[--i]);
         }
         return VCHIQ_ERROR;
      }
   }

   for (i = 0; i < NUM_BULK_BUFS; i++)
   {
      int j;
      bulk_tx_data[i] = buf_align(bulk_bufs[i*2 + 0], g_params.align_size, g_params.client_align);
      bulk_rx_data[i] = buf_align(bulk_bufs[i*2 + 1], g_params.align_size, g_params.client_align);
      for (j = 0; j < g_params.blocksize; j+=4)
      {
         *(unsigned int *)(bulk_tx_data[i] + j) = ((0x80 | i) << 24) + j;
      }
      memset(bulk_rx_data[i], 0xff, g_params.blocksize);
   }

#ifdef ANDROID
   if (g_timeout_ms)
   {
      setup_auto_kill(g_timeout_ms);
   }
#endif

   if (vchiq_initialise(&vchiq_instance) != VCHIQ_SUCCESS)
   {
      printf("* failed to open vchiq instance\n");
      return VCHIQ_ERROR;
   }

   vchiq_connect(vchiq_instance);

   memset(&service_params, 0, sizeof(service_params));

   service_params.version = service_params.version_min = VCHIQ_TEST_VER;
   service_params.fourcc = VCHIQ_MAKE_FOURCC(g_servname[0], g_servname[1], g_servname[2], g_servname[3]);
   service_params.callback = clnt_callback;
   service_params.userdata = "clnt userdata";

   if (vchiq_open_service(vchiq_instance, &service_params, &vchiq_service) != VCHIQ_SUCCESS)
   {
      printf("* failed to open service - already in use?\n");
      return VCHIQ_ERROR;
   }

   printf("Bulk test - service:%s, block size:%d, iters:%d\n", g_servname, g_params.blocksize, g_params.iters);

   /* coverity[missing_lock : FALSE] - g_server_reply is not used for mutual exclusion */
   g_params.echo = want_echo;
   element = elements;
   element->data = &g_params;
   element->size = sizeof(g_params);
   element++;
   
   vchiq_queue_message(vchiq_service, elements, element - elements);

   vcos_event_wait(&g_server_reply);

   if (g_server_error)
   {
      printf("* server error: %s\n", g_server_error);
      return VCHIQ_ERROR;
   }

   if ( num_bulk_bufs > g_params.iters )
      num_bulk_bufs = g_params.iters;

   start = vcos_getmicrosecs();

   vcos_mutex_lock(&g_mutex);

   for (i = 0; i < num_bulk_bufs; i++)
   {
      vchiq_queue_bulk_transmit(vchiq_service, bulk_tx_data[i], g_params.blocksize, (void *)i);

      vcos_log_trace("vchiq_test: queued bulk tx %d", i);
      bulk_tx_sent++;

      if (g_params.echo)
      {
         vchiq_queue_bulk_receive(vchiq_service, bulk_rx_data[i], g_params.blocksize, (void *)i);

         vcos_log_trace("vchiq_test: queued bulk rx %d", i);
         bulk_rx_sent++;
      }
   }

   vcos_mutex_unlock(&g_mutex);

   vcos_log_trace("Sent all messages");

   vcos_log_trace("vchiq_test: waiting for shutdown");

   vcos_event_wait(&g_shutdown);

   end = vcos_getmicrosecs();

   for (i = 0; i < (NUM_BULK_BUFS * 2); i++)
   {
      free(bulk_bufs[i]);
   }

   vchiq_remove_service(vchiq_service);

   vcos_log_trace("vchiq_test: shutting down");

   vchiq_shutdown(vchiq_instance);

   printf("Elapsed time: %dus per iteration\n", (end - start) / g_params.iters);

   return VCHIQ_SUCCESS;
}

static VCHIQ_STATUS_T
vchiq_ctrl_test(void)
{
   VCHIQ_INSTANCE_T vchiq_instance;
   VCHIQ_SERVICE_HANDLE_T vchiq_service;
   VCHIQ_SERVICE_PARAMS_T service_params;
   uint32_t start, end;
   int i;

   ctrl_received = 0;
   if (g_params.blocksize < 4)
      g_params.blocksize = 4;

   for (i = 0; i < NUM_BULK_BUFS; i++)
   {
      int j;
      bulk_tx_data[i] = malloc(g_params.blocksize);
      if (!bulk_tx_data[i])
      {
         printf("* out of memory\n");
         return VCHIQ_ERROR;
      }
      *(int *)bulk_tx_data[i] = MSG_ECHO;
      for (j = 4; j < g_params.blocksize; j+=4)
      {
         *(unsigned int *)(bulk_tx_data[i] + j) = ((0x80 | i) << 24) + j;
      }
   }

#ifdef ANDROID
   if (g_timeout_ms)
   {
      setup_auto_kill(g_timeout_ms);
   }
#endif

   if (vchiq_initialise(&vchiq_instance) != VCHIQ_SUCCESS)
   {
      printf("* failed to open vchiq instance\n");
      return VCHIQ_ERROR;
   }

   vchiq_connect(vchiq_instance);

   memset(&service_params, 0, sizeof(service_params));

   service_params.fourcc = VCHIQ_MAKE_FOURCC(g_servname[0], g_servname[1], g_servname[2], g_servname[3]);
   service_params.callback = clnt_callback;
   service_params.userdata = "clnt userdata";
   service_params.version = VCHIQ_TEST_VER;
   service_params.version_min = VCHIQ_TEST_VER;

   if (vchiq_open_service(vchiq_instance, &service_params, &vchiq_service) != VCHIQ_SUCCESS)
   {
      printf("* failed to open service - already in use?\n");
      return VCHIQ_ERROR;
   }

   printf("Ctrl test - service:%s, block size:%d, iters:%d\n", g_servname, g_params.blocksize, g_params.iters);

   start = vcos_getmicrosecs();

   for (i = 0; i < g_params.iters; i++)
   {
      VCHIQ_ELEMENT_T element;
      element.data = bulk_tx_data[i % NUM_BULK_BUFS];
      element.size = g_params.blocksize;

      if (vchiq_queue_message(vchiq_service, &element, 1) != VCHIQ_SUCCESS)
      {
         printf("* failed to send a message\n");
         goto error_exit;
      }
      if (g_server_error)
      {
         printf("* error - %s\n", g_server_error);
         goto error_exit;
      }
   }

   vcos_log_trace("Sent all messages");

   if (g_params.echo)
   {
      vcos_log_trace("vchiq_test: waiting for shutdown");

      vcos_event_wait(&g_shutdown);
   }

   if (g_server_error)
   {
      printf("* error - %s\n", g_server_error);
      goto error_exit;
   }

   end = vcos_getmicrosecs();

   vchiq_remove_service(vchiq_service);

   vcos_log_trace("vchiq_test: shutting down");

   vchiq_shutdown(vchiq_instance);

   printf("Elapsed time: %dus per iteration\n", (end - start) / g_params.iters);

   return VCHIQ_SUCCESS;

error_exit:
   vchiq_remove_service(vchiq_service);
   vchiq_shutdown(vchiq_instance);
   return VCHIQ_ERROR;
}

static VCHIQ_STATUS_T
vchiq_functional_test(void)
{
   int i;
   printf("Functional test - iters:%d\n", g_params.iters);
   for (i = 0; i < g_params.iters; i++)
   {
      printf("======== iteration %d ========\n", i+1);

      if (do_functional_test() != VCHIQ_SUCCESS)
         return VCHIQ_ERROR;
   }
   return VCHIQ_SUCCESS;
}

static VCHIQ_STATUS_T
vchiq_ping_test(void)
{
   /* Measure message round trip time for various sizes*/
   VCHIQ_INSTANCE_T vchiq_instance;
   VCHIQ_SERVICE_HANDLE_T vchiq_service;
   VCHI_SERVICE_HANDLE_T vchi_service;
   SERVICE_CREATION_T service_params;
   VCHIQ_SERVICE_PARAMS_T vchiq_service_params;
   int fourcc;

   static int sizes[] = { 0, 1024, 2048, VCHIQ_MAX_MSG_SIZE };
   unsigned int i;

   fourcc = VCHIQ_MAKE_FOURCC(g_servname[0], g_servname[1], g_servname[2], g_servname[3]);

   printf("Ping test - service:%s, iters:%d, version %d\n", g_servname, g_params.iters, VCHIQ_TEST_VER);

#ifdef ANDROID
   if (g_timeout_ms)
   {
      setup_auto_kill(g_timeout_ms);
   }
#endif

   if (vchiq_initialise(&vchiq_instance) != VCHIQ_SUCCESS)
   {
      printf("* failed to open vchiq instance\n");
      return VCHIQ_ERROR;
   }

   vchiq_connect(vchiq_instance);

   memset(&service_params, 0, sizeof(service_params));
   service_params.version.version = service_params.version.version_min = VCHIQ_TEST_VER;
   service_params.service_id = fourcc;
   service_params.callback = vchi_clnt_callback;
   service_params.callback_param = &vchi_service;

   if (vchi_service_open((VCHI_INSTANCE_T)vchiq_instance, &service_params, &vchi_service) != VCHIQ_SUCCESS)
   {
      printf("* failed to open service - already in use?\n");
      return VCHIQ_ERROR;
   }

   for (i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++)
   {
      const int iter_count = g_params.iters;
      do_vchi_ping_test(vchi_service, sizes[i], 0, 0, iter_count);
      do_vchi_ping_test(vchi_service, sizes[i], 0, 0, iter_count);
      do_vchi_ping_test(vchi_service, sizes[i], 1, 0, iter_count);
      do_vchi_ping_test(vchi_service, sizes[i], 2, 0, iter_count);
      do_vchi_ping_test(vchi_service, sizes[i], 10, 0, iter_count);
      do_vchi_ping_test(vchi_service, sizes[i], 0, 1, iter_count);
      do_vchi_ping_test(vchi_service, sizes[i], 0, 2, iter_count);
      do_vchi_ping_test(vchi_service, sizes[i], 0, 10, iter_count);
      do_vchi_ping_test(vchi_service, sizes[i], 10, 10, iter_count);
      do_vchi_ping_test(vchi_service, sizes[i], 100, 0, iter_count/10);
      do_vchi_ping_test(vchi_service, sizes[i], 0, 100, iter_count/10);
      do_vchi_ping_test(vchi_service, sizes[i], 100, 100, iter_count/10);
      do_vchi_ping_test(vchi_service, sizes[i], 200, 0, iter_count/10);
      do_vchi_ping_test(vchi_service, sizes[i], 0, 200, iter_count/10);
      do_vchi_ping_test(vchi_service, sizes[i], 200, 200, iter_count/10);
      do_vchi_ping_test(vchi_service, sizes[i], 400, 0, iter_count/20);
      do_vchi_ping_test(vchi_service, sizes[i], 0, 400, iter_count/20);
      do_vchi_ping_test(vchi_service, sizes[i], 400, 400, iter_count/20);
      do_vchi_ping_test(vchi_service, sizes[i], 1000, 0, iter_count/50);
      do_vchi_ping_test(vchi_service, sizes[i], 0, 1000, iter_count/50);
      do_vchi_ping_test(vchi_service, sizes[i], 1000, 1000, iter_count/50);
   }

   vchi_service_close(vchi_service);

   INIT_PARAMS(&vchiq_service_params, fourcc, clnt_callback, "clnt userdata", VCHIQ_TEST_VER);
   if (vchiq_open_service(vchiq_instance, &vchiq_service_params, &vchiq_service) != VCHIQ_SUCCESS)
   {
      printf("* failed to open service - already in use?\n");
      return VCHIQ_ERROR;
   }

   for (i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++)
   {
      const int iter_count = g_params.iters;
      do_ping_test(vchiq_service, sizes[i], 0, 0, iter_count);
      do_ping_test(vchiq_service, sizes[i], 0, 0, iter_count);
      do_ping_test(vchiq_service, sizes[i], 1, 0, iter_count);
      do_ping_test(vchiq_service, sizes[i], 2, 0, iter_count);
      do_ping_test(vchiq_service, sizes[i], 10, 0, iter_count);
      do_ping_test(vchiq_service, sizes[i], 0, 1, iter_count);
      do_ping_test(vchiq_service, sizes[i], 0, 2, iter_count);
      do_ping_test(vchiq_service, sizes[i], 0, 10, iter_count);
      do_ping_test(vchiq_service, sizes[i], 10, 10, iter_count);
      do_ping_test(vchiq_service, sizes[i], 100, 0, iter_count/10);
      do_ping_test(vchiq_service, sizes[i], 0, 100, iter_count/10);
      do_ping_test(vchiq_service, sizes[i], 100, 100, iter_count/10);
      do_ping_test(vchiq_service, sizes[i], 200, 0, iter_count/10);
      do_ping_test(vchiq_service, sizes[i], 0, 200, iter_count/10);
      do_ping_test(vchiq_service, sizes[i], 200, 200, iter_count/10);
      do_ping_test(vchiq_service, sizes[i], 400, 0, iter_count/20);
      do_ping_test(vchiq_service, sizes[i], 0, 400, iter_count/20);
      do_ping_test(vchiq_service, sizes[i], 400, 400, iter_count/20);
      do_ping_test(vchiq_service, sizes[i], 1000, 0, iter_count/50);
      do_ping_test(vchiq_service, sizes[i], 0, 1000, iter_count/50);
      do_ping_test(vchiq_service, sizes[i], 1000, 1000, iter_count/50);
   }

   vchiq_close_service(vchiq_service);

   return VCHIQ_SUCCESS;
}

static VCHIQ_STATUS_T
vchiq_signal_test(void)
{
   /* Measure message round trip time for various sizes*/
   VCHIQ_INSTANCE_T vchiq_instance;
   VCHIQ_SERVICE_HANDLE_T vchiq_service;
   VCHIQ_SERVICE_PARAMS_T vchiq_service_params;
   int fourcc;

   static int sizes[] = { 0, 1024, 2048, VCHIQ_MAX_MSG_SIZE };

   fourcc = VCHIQ_MAKE_FOURCC(g_servname[0], g_servname[1], g_servname[2], g_servname[3]);

   printf("signal test - service:%s, iters:%d, version %d\n", g_servname, g_params.iters, VCHIQ_TEST_VER);

#ifdef ANDROID
   if (g_timeout_ms)
   {
      setup_auto_kill(g_timeout_ms);
   }
#endif

   if (vchiq_initialise(&vchiq_instance) != VCHIQ_SUCCESS)
   {
      printf("* failed to open vchiq instance\n");
      return VCHIQ_ERROR;
   }

   vchiq_connect(vchiq_instance);

   INIT_PARAMS(&vchiq_service_params, fourcc, clnt_callback, "clnt userdata", VCHIQ_TEST_VER);
   if (vchiq_open_service(vchiq_instance, &vchiq_service_params, &vchiq_service) != VCHIQ_SUCCESS)
   {
      printf("* failed to open service - already in use?\n");
      return VCHIQ_ERROR;
   }

   vchiq_bulk_transmit(vchiq_service, &sizes, 16, 0, VCHIQ_BULK_MODE_BLOCKING);

   vchiq_close_service(vchiq_service);

   return VCHIQ_SUCCESS;
}

static VCHIQ_STATUS_T
do_functional_test(void)
{
   VCHIQ_ELEMENT_T elements[4];
   VCHIQ_INSTANCE_T instance;
   VCHIQ_SERVICE_HANDLE_T service, service2, service3;
   VCHIQ_SERVICE_PARAMS_T service_params;
   VCHIQ_CONFIG_T config;
   unsigned int size, i;

   vcos_event_create(&func_test_sync, "test_sync");

#ifdef ANDROID
   if (g_timeout_ms)
   {
      setup_auto_kill(g_timeout_ms);
   }
#endif

   if (func_data_test_start != -1)
      goto bulk_tests_only;

   EXPECT(vchiq_initialise(&instance), VCHIQ_SUCCESS);
   EXPECT(vchiq_get_config(instance, sizeof(config) - 1, &config), VCHIQ_SUCCESS); // too small, but allowed for backwards compatibility
   EXPECT(vchiq_get_config(instance, sizeof(config) + 1, &config), VCHIQ_ERROR);   // too large
   EXPECT(vchiq_get_config(instance, sizeof(config), &config), VCHIQ_SUCCESS);    // just right
   EXPECT(config.max_msg_size, VCHIQ_MAX_MSG_SIZE);

   INIT_PARAMS(&service_params, FUNC_FOURCC, func_clnt_callback, (void *)1, VCHIQ_TEST_VER);
   EXPECT(vchiq_add_service(instance, &service_params, &service), VCHIQ_SUCCESS);

   INIT_PARAMS(&service_params, FUNC_FOURCC, func_clnt_callback, (void *)2, VCHIQ_TEST_VER);
   EXPECT(vchiq_add_service(instance, &service_params, &service2), VCHIQ_SUCCESS);

   INIT_PARAMS(&service_params, FUNC_FOURCC, clnt_callback, (void *)3, VCHIQ_TEST_VER);
   EXPECT(vchiq_add_service(instance, &service_params, &service3), VCHIQ_ERROR); // callback doesn't match

   EXPECT(vchiq_set_service_option(service, VCHIQ_SERVICE_OPTION_AUTOCLOSE, 0), VCHIQ_SUCCESS);
   EXPECT(vchiq_set_service_option(service, VCHIQ_SERVICE_OPTION_AUTOCLOSE, 1), VCHIQ_SUCCESS);
   EXPECT(vchiq_set_service_option(service, 42, 1), VCHIQ_ERROR); // invalid option
   EXPECT(vchiq_remove_service(service), VCHIQ_SUCCESS);
   EXPECT(vchiq_remove_service(service), VCHIQ_ERROR); // service already removed
   EXPECT(vchiq_remove_service(service2), VCHIQ_SUCCESS);
   EXPECT(vchiq_queue_message(service, NULL, 0), VCHIQ_ERROR); // service not valid
   EXPECT(vchiq_set_service_option(service, VCHIQ_SERVICE_OPTION_AUTOCLOSE, 0), VCHIQ_ERROR); // service not valid

   INIT_PARAMS(&service_params, FUNC_FOURCC, clnt_callback, (void *)3, VCHIQ_TEST_VER);
   EXPECT(vchiq_add_service(instance, &service_params, &service3), VCHIQ_SUCCESS);

   EXPECT(vchiq_queue_message(service, NULL, 0), VCHIQ_ERROR); // service not open
   EXPECT(vchiq_queue_bulk_transmit(service, clnt_service1_data, sizeof(clnt_service1_data), (void *)1), VCHIQ_ERROR); // service not open
   EXPECT(vchiq_queue_bulk_receive(service2, clnt_service2_data, sizeof(clnt_service2_data), (void *)2), VCHIQ_ERROR); // service not open
   EXPECT(vchiq_queue_bulk_receive(service, 0, sizeof(clnt_service1_data), (void *)1), VCHIQ_ERROR); // invalid buffer
   EXPECT(vchiq_shutdown(instance), VCHIQ_SUCCESS);
   EXPECT(vchiq_initialise(&instance), VCHIQ_SUCCESS);
   INIT_PARAMS(&service_params, FUNC_FOURCC, func_clnt_callback, (void*)1, 0);
   EXPECT(vchiq_open_service(instance, &service_params, &service), VCHIQ_ERROR); // not connected
   EXPECT(vchiq_connect(instance), VCHIQ_SUCCESS);
   EXPECT(vchiq_open_service(instance, &service_params, &service), VCHIQ_ERROR); // wrong version number
   memset(&service_params, 0, sizeof(service_params));
   service_params.fourcc = FUNC_FOURCC;
   service_params.callback = func_clnt_callback;
   service_params.userdata = (void*)1;
   service_params.version = 1;
   service_params.version_min = 1;
   EXPECT(vchiq_open_service(instance, &service_params, &service), VCHIQ_ERROR); // Still the wrong version number
   service_params.version = VCHIQ_TEST_VER + 1;
   service_params.version_min = VCHIQ_TEST_VER + 1;
   EXPECT(vchiq_open_service(instance, &service_params, &service), VCHIQ_ERROR); // Still the wrong version number
   service_params.version = VCHIQ_TEST_VER;
   service_params.version_min = VCHIQ_TEST_VER;
   EXPECT(vchiq_open_service(instance, &service_params, &service), VCHIQ_SUCCESS); // That's better

   INIT_PARAMS(&service_params, VCHIQ_MAKE_FOURCC('n','o','n','e'), func_clnt_callback, (void*)2, VCHIQ_TEST_VER);
   EXPECT(vchiq_open_service(instance, &service_params, &service2), VCHIQ_ERROR); // no listener
   INIT_PARAMS(&service_params, FUNC_FOURCC, func_clnt_callback, (void*)2, VCHIQ_TEST_VER);
   EXPECT(vchiq_open_service(instance, &service_params, &service2), VCHIQ_SUCCESS);
   INIT_PARAMS(&service_params, FUNC_FOURCC, func_clnt_callback, (void*)3, VCHIQ_TEST_VER);
   EXPECT(vchiq_open_service(instance, &service_params, &service3), VCHIQ_ERROR); // no more listeners
   EXPECT(vchiq_remove_service(service2), VCHIQ_SUCCESS);
   INIT_PARAMS(&service_params, FUNC_FOURCC, func_clnt_callback, (void*)2, VCHIQ_TEST_VER);
   EXPECT(vchiq_open_service(instance, &service_params, &service2), VCHIQ_SUCCESS);

   elements[0].data = "a";
   elements[0].size = 1;
   elements[1].data = "bcdef";
   elements[1].size = 5;
   elements[2].data = "ghijklmnopq";
   elements[2].size = 11;
   elements[3].data = "rstuvwxyz";
   elements[3].size = 9;
   EXPECT(vchiq_queue_message(service, elements, 4), VCHIQ_SUCCESS);

   EXPECT(vchiq_queue_bulk_transmit(service2, clnt_service2_data, sizeof(clnt_service2_data), (void *)0x2001), VCHIQ_SUCCESS);
   for (i = 0; i < sizeof(clnt_service1_data); i++)
   {
      clnt_service1_data[i] = (char)i;
   }
   EXPECT(vchiq_queue_bulk_transmit(service, clnt_service1_data, sizeof(clnt_service1_data), (void*)0x1001), VCHIQ_SUCCESS);

   vcos_event_wait(&func_test_sync);
   EXPECT(func_error, 0);
   EXPECT(vchiq_remove_service(service), VCHIQ_SUCCESS);
   vcos_event_wait(&func_test_sync);

   EXPECT(vchiq_shutdown(instance), VCHIQ_SUCCESS);

   vcos_event_wait(&func_test_sync);
   EXPECT(func_error, 0);

   INIT_PARAMS(&service_params, FUNC_FOURCC, func_clnt_callback, NULL, VCHIQ_TEST_VER);
   EXPECT(vchiq_open_service(instance, &service_params, &service), VCHIQ_ERROR); /* Instance not initialised */
   EXPECT(vchiq_add_service(instance, &service_params, &service), VCHIQ_ERROR); /* Instance not initialised */
   EXPECT(vchiq_connect(instance), VCHIQ_ERROR); /* Instance not initialised */

bulk_tests_only:
   /* Now test the bulk data transfers */
   EXPECT(vchiq_initialise(&instance), VCHIQ_SUCCESS);
   EXPECT(vchiq_connect(instance), VCHIQ_SUCCESS);

   func_data_test_iter = 0;

   INIT_PARAMS(&service_params, FUN2_FOURCC, fun2_clnt_callback, NULL, VCHIQ_TEST_VER);
   EXPECT(vchiq_open_service(instance, &service_params, &service), VCHIQ_SUCCESS);

   if (func_data_test_end < func_data_test_start)
      goto skip_bulk_tests;

   printf("Testing bulk transfer for alignment.\n");
   for (size = 1; size < 64; size++)
   {
      int align, srvr_align;
      for (srvr_align = 32; srvr_align; srvr_align >>= 1)
      {
          for (align = 32; align; align >>= 1)
          {
             EXPECT(func_data_test(service, size, align & 31, srvr_align & 31), VCHIQ_SUCCESS);
          }
      }
   }

   printf("Testing bulk transfer at PAGE_SIZE.\n");
   for (size = 1; size < 64; size++)
   {
      int align, srvr_align;
      for (srvr_align = 32; srvr_align; srvr_align >>= 1)
      {
          for (align = 32; align; align >>= 1)
          {
             EXPECT(func_data_test(service, size, PAGE_SIZE - align, srvr_align & 31), VCHIQ_SUCCESS);
          }
      }
   }

   for (size = 64; size < FUN2_MAX_DATA_SIZE; size<<=1)
   {
      static const int aligns[] = { 0, 1, 31 };

      for (i = 0; i < vcos_countof(aligns); i++)
      {
         int srvr_align = aligns[i];
         unsigned int j;
         for (j = 0; j < vcos_countof(aligns); j++)
         {
            int k;
            int align = aligns[j];
            for (k = 0; k <= 8; k++)
            {
               EXPECT(func_data_test(service, size, align, srvr_align + k), VCHIQ_SUCCESS);
            }
         }
      }
   }

skip_bulk_tests:

   EXPECT(vchiq_shutdown(instance), VCHIQ_SUCCESS);

   vcos_event_delete(&func_test_sync);

   return VCHIQ_SUCCESS;

error_exit:
   return VCHIQ_ERROR;
}

static void
do_ping_test(VCHIQ_SERVICE_HANDLE_T service, int size, int async, int oneway, int iters)
{
   uint32_t start, end;
   char *ping_buf = malloc(size + sizeof(struct test_params));
   struct test_params *params = (struct test_params *)ping_buf;
   VCHIQ_ELEMENT_T element;
   int i;

   element.data = ping_buf;

   /* Set up the quotas for messages */
   *params = g_params;
   params->magic = MSG_CONFIG;
   params->blocksize = 0;
   element.size = sizeof(*params);
   vchiq_queue_message(service, &element, 1);
   vcos_event_wait(&g_server_reply);
   vchiq_set_service_option(service, VCHIQ_SERVICE_OPTION_MESSAGE_QUOTA, params->client_message_quota);

   /* Allow enough room for the type header */
   element.size = (size < 4) ? 4 : size;

   bulk_tx_received = -1;

   start = vcos_getmicrosecs();
   for (i = 0; i < iters; i++)
   {
      int j;
      for (j = 0; j < vcos_max(async, oneway); j++)
      {
         if (j < async)
         {
            params->magic = MSG_ASYNC;
            vchiq_queue_message(service, &element, 1);
         }
         if (j < oneway)
         {
            params->magic = MSG_ONEWAY;
            vchiq_queue_message(service, &element, 1);
         }
      }
      params->magic = MSG_SYNC;
      vchiq_queue_message(service, &element, 1);
      vcos_event_wait(&g_server_reply);
   }
   end = vcos_getmicrosecs();

   printf("ping (size %d, %d async, %d oneway) -> %fus\n", size, async, oneway, ((float)(end - start))/iters);

   vcos_sleep(20);

   if ((async == 0) && (oneway == 0))
   {
      *params = g_params;
      params->magic = MSG_CONFIG;
      params->blocksize = size ? size : 8;
      params->iters = iters;
      params->verify = 0;
      params->echo = 0;

      element.size = sizeof(*params);
      vchiq_queue_message(service, &element, 1);
      vcos_event_wait(&g_server_reply);

      vcos_sleep(30);

      start = vcos_getmicrosecs();
      for (i = 0; i < iters; i++)
      {
         vchiq_queue_bulk_transmit(service, ping_buf, params->blocksize, 0);
         vcos_event_wait(&g_server_reply);
      }
      end = vcos_getmicrosecs();

      printf("bulk (size %d, async) -> %fus\n", size, ((float)(end - start))/iters);

      vcos_sleep(40);
   }

   if (oneway == 0)
   {
      *params = g_params;
      params->magic = MSG_CONFIG;
      params->blocksize = size ? size : 8;
      params->iters = iters * (async + 1);
      params->verify = 0;
      params->echo = 0;

      element.size = sizeof(*params);
      vchiq_queue_message(service, &element, 1);
      vcos_event_wait(&g_server_reply);

      vcos_sleep(50);

      start = vcos_getmicrosecs();
      for (i = 0; i < iters; i++)
      {
         int j;
         for (j = 0; j < async; j++)
            vchiq_bulk_transmit(service, ping_buf, params->blocksize, 0, VCHIQ_BULK_MODE_NOCALLBACK);
         vchiq_bulk_transmit(service, ping_buf, params->blocksize, 0, VCHIQ_BULK_MODE_BLOCKING);
      }
      end = vcos_getmicrosecs();

      printf("bulk (size %d, %d async) -> %fus\n", size, async, ((float)(end - start))/iters);

      vcos_sleep(60);
   }

   free(ping_buf);

   bulk_tx_received = 0;
}

static void
do_vchi_ping_test(VCHI_SERVICE_HANDLE_T service, int size, int async, int oneway, int iters)
{
   uint32_t start, end;
   uint32_t actual;
   char *ping_buf = malloc(size + sizeof(struct test_params));
   char pong_buf[100];
   struct test_params *params = (struct test_params *)ping_buf;
   int msg_size;
   int i;

   /* Set up the quotas for messages */
   *params = g_params;
   params->magic = MSG_CONFIG;
   params->blocksize = 0;
   vchi_msg_queue(service, params, sizeof(*params), VCHI_FLAGS_BLOCK_UNTIL_QUEUED, 0);
   vcos_event_wait(&g_server_reply);
   vchiq_set_service_option((VCHIQ_SERVICE_HANDLE_T)service, VCHIQ_SERVICE_OPTION_MESSAGE_QUOTA, params->client_message_quota);

   /* Allow enough room for the type header */
   msg_size = (size < 4) ? 4 : size;

   bulk_tx_received = -1;

   if ((oneway == 0) && (async == 0))
   {
      params->magic = MSG_SYNC;

      g_sync_mode = 1;

      start = vcos_getmicrosecs();
      for (i = 0; i < iters; i++)
      {
         vchi_msg_queue(service, ping_buf, msg_size, VCHI_FLAGS_BLOCK_UNTIL_QUEUED, 0);
         vchi_msg_dequeue(service, pong_buf, sizeof(pong_buf), &actual, VCHI_FLAGS_BLOCK_UNTIL_OP_COMPLETE);
      }
      end = vcos_getmicrosecs();

      printf("vchi ping (size %d) -> %fus\n", size, ((float)(end - start))/iters);

      vcos_sleep(10);

      g_sync_mode = 0;
   }

   while (vchi_msg_dequeue(service, pong_buf, sizeof(pong_buf), &actual, VCHI_FLAGS_NONE) != -1)
   {
      printf("* Unexpected message found in queue - size %d\n", actual);
   }

   start = vcos_getmicrosecs();
   for (i = 0; i < iters; i++)
   {
      int j;
      for (j = 0; j < vcos_max(async, oneway); j++)
      {
         if (j < async)
         {
            params->magic = MSG_ASYNC;
            vchi_msg_queue(service, ping_buf, msg_size, VCHI_FLAGS_BLOCK_UNTIL_QUEUED, 0);
         }
         if (j < oneway)
         {
            params->magic = MSG_ONEWAY;
            vchi_msg_queue(service, ping_buf, msg_size, VCHI_FLAGS_BLOCK_UNTIL_QUEUED, 0);
         }
      }
      params->magic = MSG_SYNC;
      vchi_msg_queue(service, ping_buf, msg_size, VCHI_FLAGS_BLOCK_UNTIL_QUEUED, 0);
      vcos_event_wait(&g_server_reply);
   }
   end = vcos_getmicrosecs();

   printf("vchi ping (size %d, %d async, %d oneway) -> %fus\n", size, async, oneway, ((float)(end - start))/iters);

   vcos_sleep(20);

   if ((async == 0) && (oneway == 0))
   {
      *params = g_params;
      params->magic = MSG_CONFIG;
      params->blocksize = size ? size : 8;
      params->iters = iters;
      params->verify = 0;
      params->echo = 0;

      vchi_msg_queue(service, params, sizeof(*params), VCHI_FLAGS_BLOCK_UNTIL_QUEUED, 0);
      vcos_event_wait(&g_server_reply);

      vcos_sleep(30);

      start = vcos_getmicrosecs();
      for (i = 0; i < iters; i++)
      {
         vchi_bulk_queue_transmit(service, ping_buf, params->blocksize,
            VCHI_FLAGS_CALLBACK_WHEN_OP_COMPLETE | VCHI_FLAGS_BLOCK_UNTIL_QUEUED, 0);
         vcos_event_wait(&g_server_reply);
      }
      end = vcos_getmicrosecs();

      printf("vchi bulk (size %d, %d async, %d oneway) -> %fus\n", size, async, oneway, ((float)(end - start))/iters);

      vcos_sleep(40);
   }

   if (oneway == 0)
   {
      *params = g_params;
      params->magic = MSG_CONFIG;
      params->blocksize = size ? size : 8;
      params->iters = iters * (async + 1);
      params->verify = 0;
      params->echo = 0;

      vchi_msg_queue(service, params, sizeof(*params), VCHI_FLAGS_BLOCK_UNTIL_QUEUED, 0);
      vcos_event_wait(&g_server_reply);

      vcos_sleep(50);

      start = vcos_getmicrosecs();
      for (i = 0; i < iters; i++)
      {
         int j;
         for (j = 0; j < async; j++)
            vchi_bulk_queue_transmit(service, ping_buf, params->blocksize, VCHI_FLAGS_NONE, 0);
         vchi_bulk_queue_transmit(service, ping_buf, params->blocksize, VCHI_FLAGS_BLOCK_UNTIL_DATA_READ, 0);
      }
      end = vcos_getmicrosecs();

      printf("vchi bulk (size %d, %d oneway) -> %fus\n", size, oneway, ((float)(end - start))/iters);

      vcos_sleep(60);
   }

   free(ping_buf);

   bulk_tx_received = 0;
}

static VCHIQ_STATUS_T
func_data_test(VCHIQ_SERVICE_HANDLE_T service, int datalen, int align, int server_align)
{
   enum { PROLOGUE_SIZE = 32, EPILOGUE_SIZE = 32 };
   static uint8_t databuf[PAGE_SIZE + PROLOGUE_SIZE + FUN2_MAX_DATA_SIZE + EPILOGUE_SIZE];
   static uint8_t databuf2[PAGE_SIZE + PROLOGUE_SIZE + FUN2_MAX_DATA_SIZE + EPILOGUE_SIZE];
   uint8_t *data, *data2, *prologue, *epilogue;
   VCHIQ_ELEMENT_T element;
   int params[4] = { datalen, server_align, align, func_data_test_iter };
   int success = 1, i;

   if (!vcos_verify(datalen < FUN2_MAX_DATA_SIZE))
      return VCHIQ_ERROR;

   if ((func_data_test_iter < func_data_test_start) || (func_data_test_iter > func_data_test_end))
      goto skip_iter;

   element.size = sizeof(params);
   element.data = &params;
   EXPECT(vchiq_queue_message(service, &element, 1), VCHIQ_SUCCESS);

   memset(databuf, 0xff, sizeof(databuf));
   data = (uint8_t *)((uintptr_t)databuf & ~(PAGE_SIZE - 1)) + align;
   data = (uint8_t *)((((intptr_t)databuf + PROLOGUE_SIZE) & ~(FUN2_MAX_ALIGN - 1)) + align - PROLOGUE_SIZE);
   if (data < databuf)
      data += PAGE_SIZE;
   data += PROLOGUE_SIZE;

   EXPECT(vchiq_queue_bulk_receive(service, data, datalen, NULL), VCHIQ_SUCCESS);

   data2 = (uint8_t *)(((uintptr_t)databuf2 + PROLOGUE_SIZE) & ~(PAGE_SIZE - 1)) + align - PROLOGUE_SIZE;
   if (data2 < databuf2)
      data2 += PAGE_SIZE;
   prologue = data2;
   data2 += PROLOGUE_SIZE;
   epilogue = data2 + datalen;

   memset(prologue, 0xff, PROLOGUE_SIZE);
   memset(epilogue, 0xff, EPILOGUE_SIZE);

   for (i = 0; i < (datalen - 1); i++)
   {
      data2[i] = (uint8_t)(((i & 0x1f) == 0) ? (i >> 5) : i);
   }
   data2[i] = '\0';

   /* Attempt to pull the boundaries into the cache */
   prologue = data - PROLOGUE_SIZE;
   epilogue = data + datalen;
   prologue[PROLOGUE_SIZE - 1] = 0xfe;
   epilogue[0] = 0xfe;

   EXPECT(vchiq_queue_bulk_transmit(service, data2, datalen, NULL), VCHIQ_SUCCESS);
   vcos_event_wait(&func_test_sync); /* Wait for the receive to complete */

   for (i = 0; i < PROLOGUE_SIZE; i++)
   {
      if (prologue[i] != (uint8_t)((i == PROLOGUE_SIZE - 1) ? '\xfe' : '\xff'))
      {
         vcos_log_error("%d: Prologue corrupted at %x (datalen %x, align %x, server_align %x) -> %02x", func_data_test_iter, i, datalen, align, server_align, prologue[i]);
         VCOS_BKPT;
         success = 0;
         break;
      }
   }
   for (i = 0; i < EPILOGUE_SIZE; i++)
   {
      if (epilogue[i] != (uint8_t)((i == 0) ? '\xfe' : '\xff'))
      {
         vcos_log_error("%d: Epilogue corrupted at %x (datalen %x, align %x, server_align %x) -> %02x", func_data_test_iter, i, datalen, align, server_align, epilogue[i]);
         VCOS_BKPT;
         success = 0;
         break;
      }
   }

   if (success)
   {
      int diffs = 0;
      for (i = 0; i < datalen; i++)
      {
         int diff = (i == datalen - 1) ?
            (data[i] != 0) :
            ((i & 0x1f) == 0) ?
            (data[i] != (uint8_t)(i >> 5)) :
            (data[i] != (uint8_t)i);

         if (diff)
            diffs++;
         else if (diffs)
         {
            vcos_log_error("%d: Data corrupted at %x-%x (datalen %x, align %x, server_align %x) -> %02x", func_data_test_iter, i - diffs, i - 1, datalen, align, server_align, data[i-1]);
            VCOS_BKPT;
            success = 0;
            diffs = 0;
         }
      }
      if (diffs)
      {
         vcos_log_error("%d: Data corrupted at %x-%x (datalen %x, align %x, server_align %x) -> %02x", func_data_test_iter, i - diffs, i - 1, datalen, align, server_align, data[i-1]);
         VCOS_BKPT;
         success = 0;
      }
   }

skip_iter:
   if (success)
   {
      func_data_test_iter++;
      return VCHIQ_SUCCESS;
   }

error_exit:
   return VCHIQ_ERROR;
}


#ifdef VCHIQ_LOCAL

static void *vchiq_test_server(void *param)
{
   VCHIQ_INSTANCE_T instance;

   vcos_demand(vchiq_initialise(&instance) == VCHIQ_SUCCESS);
   vchiq_test_start_services(instance);
   vchiq_connect(instance);
   printf("test server started\n");
   return 0;
}

#endif

static VCHIQ_STATUS_T
clnt_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header,
   VCHIQ_SERVICE_HANDLE_T service, void *bulk_userdata)
{
   int data;
   vcos_mutex_lock(&g_mutex);
   if (reason == VCHIQ_MESSAGE_AVAILABLE)
   {
      /* 
       * Store the header size as it is going to be released
       * and the size may be overwritten by the release.
       */
      size_t header_size = header->size;

      if (header_size <= 1)
         vchiq_release_message(service, header);
      else
      /* Responses of length 0 are not sync points */
      if ((header_size >= 4) && (memcpy(&data, header->data, sizeof(data)), data == MSG_ECHO))
      {
         /* This is a complete echoed packet */
         if (g_params.verify && (mem_check(header->data, bulk_tx_data[ctrl_received % NUM_BULK_BUFS], g_params.blocksize) != 0))
            g_server_error = "corrupt data";
         else
            ctrl_received++;
         if (g_server_error || (ctrl_received == g_params.iters))
            vcos_event_signal(&g_shutdown);
         vchiq_release_message(service, header);
      }
      else if (header_size != 0)
         g_server_error = header->data;

      if ((header_size != 0) || g_server_error)
         vcos_event_signal(&g_server_reply);
   }
   else if (reason == VCHIQ_BULK_TRANSMIT_DONE)
   {
      int i = (int)bulk_userdata;
      vcos_log_trace("  BULK_TRANSMIT_DONE(%d)", i);
      if (bulk_tx_received < 0)
         vcos_event_signal(&g_server_reply);
      else
      {
         vcos_assert(i == bulk_tx_received);
         bulk_tx_received++;
         if (bulk_tx_sent < g_params.iters)
         {
            vchiq_queue_bulk_transmit(service, bulk_tx_data[i % NUM_BULK_BUFS], g_params.blocksize, (void *)bulk_tx_sent);
            bulk_tx_sent++;
         }
      }
   }
   else if (reason == VCHIQ_BULK_RECEIVE_DONE)
   {
      int i = (int)bulk_userdata;
      vcos_log_trace("  BULK_RECEIVE_DONE(%d): data '%s'", i, bulk_rx_data[i % NUM_BULK_BUFS]);
      vcos_assert(i == bulk_rx_received);
      if (g_params.verify && (mem_check(bulk_tx_data[i % NUM_BULK_BUFS], bulk_rx_data[i % NUM_BULK_BUFS], g_params.blocksize) != 0))
      {
         vcos_log_error("* Data corruption - %d: %x, %x, %x", i, (unsigned int)bulk_tx_data[i % NUM_BULK_BUFS], (unsigned int)bulk_rx_data[i % NUM_BULK_BUFS], g_params.blocksize);
         VCOS_BKPT;
      }
      bulk_rx_received++;
      if (bulk_rx_sent < g_params.iters)
      {
         if (g_params.verify)
            memset(bulk_rx_data[i % NUM_BULK_BUFS], 0xff, g_params.blocksize);
         vchiq_queue_bulk_receive(service, bulk_rx_data[i % NUM_BULK_BUFS], g_params.blocksize, (void *)bulk_rx_sent);
         bulk_rx_sent++;
      }
   }
   else if (reason == VCHIQ_BULK_TRANSMIT_ABORTED)
   {
      int i = (int)bulk_userdata;
      vcos_log_info("  BULK_TRANSMIT_ABORTED(%d)", i);
   }
   else if (reason == VCHIQ_BULK_RECEIVE_ABORTED)
   {
      int i = (int)bulk_userdata;
      vcos_log_info("  BULK_RECEIVE_ABORTED(%d)", i);
   }
   if ((bulk_tx_received == g_params.iters) &&
      ((g_params.echo == 0) || (bulk_rx_received == g_params.iters)))
      vcos_event_signal(&g_shutdown);
   vcos_mutex_unlock(&g_mutex);
   return VCHIQ_SUCCESS;
}

static void
vchi_clnt_callback(void *callback_param,
   VCHI_CALLBACK_REASON_T reason,
   void *handle)
{
   VCHI_SERVICE_HANDLE_T service = *(VCHI_SERVICE_HANDLE_T *)callback_param;
   vcos_mutex_lock(&g_mutex);
   if (reason == VCHI_CALLBACK_MSG_AVAILABLE)
   {
      if (!g_sync_mode)
      {
         static char pong_buf[100];
         uint32_t actual;
         while (vchi_msg_dequeue(service, pong_buf, sizeof(pong_buf), &actual, VCHI_FLAGS_NONE) == 0)
         {
            if (actual > 1)
               g_server_error = pong_buf;
            if (actual != 0)
            {
               /* Responses of length 0 are not sync points */
               vcos_event_signal(&g_server_reply);
               break;
            }
         }
      }
   }
   else if (reason == VCHI_CALLBACK_BULK_SENT)
   {
      int i = (int)handle;
      vcos_log_trace("  BULK_TRANSMIT_DONE(%d)", i);
      if (bulk_tx_received < 0)
         vcos_event_signal(&g_server_reply);
      else
      {
         vcos_assert(i == bulk_tx_received);
         bulk_tx_received++;
         if (bulk_tx_sent < g_params.iters)
         {
            vchi_bulk_queue_transmit(service, bulk_tx_data[i % NUM_BULK_BUFS],
               g_params.blocksize,
               VCHI_FLAGS_CALLBACK_WHEN_OP_COMPLETE | VCHI_FLAGS_BLOCK_UNTIL_QUEUED,
               (void *)bulk_tx_sent);
            bulk_tx_sent++;
         }
      }
   }
   else if (reason == VCHI_CALLBACK_BULK_RECEIVED)
   {
      int i = (int)handle;
      vcos_log_trace("  BULK_RECEIVE_DONE(%d): data '%s'", i, bulk_rx_data[i % NUM_BULK_BUFS]);
      vcos_assert(i == bulk_rx_received);
      if (g_params.verify && (mem_check(bulk_tx_data[i % NUM_BULK_BUFS], bulk_rx_data[i % NUM_BULK_BUFS], g_params.blocksize) != 0))
      {
         vcos_log_error("* Data corruption - %x, %x, %x", (unsigned int)bulk_tx_data[i % NUM_BULK_BUFS], (unsigned int)bulk_rx_data[i % NUM_BULK_BUFS], g_params.blocksize);
         VCOS_BKPT;
      }
      bulk_rx_received++;
      if (bulk_rx_sent < g_params.iters)
      {
         if (g_params.verify)
            memset(bulk_rx_data[i % NUM_BULK_BUFS], 0xff, g_params.blocksize);
         vchi_bulk_queue_receive(service, bulk_rx_data[i % NUM_BULK_BUFS], g_params.blocksize,
            VCHI_FLAGS_CALLBACK_WHEN_OP_COMPLETE | VCHI_FLAGS_BLOCK_UNTIL_QUEUED,
            (void *)bulk_rx_sent);
         bulk_rx_sent++;
      }
   }
   else if (reason == VCHI_CALLBACK_BULK_TRANSMIT_ABORTED)
   {
      int i = (int)handle;
      vcos_log_info("  BULK_TRANSMIT_ABORTED(%d)", i);
   }
   else if (reason == VCHI_CALLBACK_BULK_RECEIVE_ABORTED)
   {
      int i = (int)handle;
      vcos_log_info("  BULK_RECEIVE_ABORTED(%d)", i);
   }
   if ((bulk_tx_received == g_params.iters) && (bulk_rx_received == g_params.iters))
      vcos_event_signal(&g_shutdown);
   vcos_mutex_unlock(&g_mutex);
}

static VCHIQ_STATUS_T
func_clnt_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header,
   VCHIQ_SERVICE_HANDLE_T service, void *bulk_userdata)
{
   static int callback_count = 0, bulk_count = 0;
   int callback_index = 0, bulk_index = 0;

   if (reason < VCHIQ_BULK_TRANSMIT_DONE)
   {
      callback_count++;

      START_CALLBACK(VCHIQ_SERVICE_CLOSED, 2)
      END_CALLBACK(VCHIQ_SUCCESS)

      START_CALLBACK(VCHIQ_MESSAGE_AVAILABLE, 1)
      EXPECT(bulk_userdata, NULL);
      EXPECT(header->size, 26);
      EXPECT(mem_check(header->data, "abcdefghijklmnopqrstuvwxyz", 26), 0);
      vchiq_release_message(service, header);
      END_CALLBACK(VCHIQ_SUCCESS)

      START_CALLBACK(VCHIQ_MESSAGE_AVAILABLE, 1)
      EXPECT(bulk_userdata, NULL);
      EXPECT(header->size, 0);
      vchiq_release_message(service, header);
      EXPECT(vchiq_queue_bulk_receive(service, clnt_service2_data, sizeof(clnt_service2_data), (void*)0x1004), VCHIQ_SUCCESS);
      vcos_event_signal(&func_test_sync);
      END_CALLBACK(VCHIQ_SUCCESS)

      START_CALLBACK(VCHIQ_SERVICE_CLOSED, 1)
      vcos_event_signal(&func_test_sync);
      END_CALLBACK(VCHIQ_SUCCESS)

      START_CALLBACK(VCHIQ_SERVICE_CLOSED, 2)
      vcos_event_signal(&func_test_sync);
      callback_count = 0;
      bulk_count = 0;
      END_CALLBACK(VCHIQ_SUCCESS)
   }
   else
   {
      bulk_count++;

      START_BULK_CALLBACK(VCHIQ_BULK_TRANSMIT_DONE, 1, 0x1001)
      memset(clnt_service2_data, 0xff, sizeof(clnt_service2_data));
      EXPECT(vchiq_queue_bulk_receive(service, clnt_service2_data, sizeof(clnt_service2_data), (void*)0x1002), VCHIQ_SUCCESS);
      END_CALLBACK(VCHIQ_SUCCESS)

      START_BULK_CALLBACK(VCHIQ_BULK_RECEIVE_ABORTED, 1, 0x1002)
      EXPECT(vchiq_queue_bulk_receive(service, clnt_service2_data, sizeof(clnt_service2_data), (void*)0x1003), VCHIQ_SUCCESS);
      END_CALLBACK(VCHIQ_SUCCESS)

      START_BULK_CALLBACK(VCHIQ_BULK_RECEIVE_DONE, 1, 0x1003)
      (void)(mem_check(clnt_service1_data, clnt_service2_data, sizeof(clnt_service1_data)), 0);
      (void)(mem_check(clnt_service1_data, clnt_service2_data + sizeof(clnt_service1_data), sizeof(clnt_service1_data)), 0);
      END_CALLBACK(VCHIQ_SUCCESS)

      START_BULK_CALLBACK(VCHIQ_BULK_RECEIVE_ABORTED, 1, 0x1004)
      END_CALLBACK(VCHIQ_SUCCESS)

      START_BULK_CALLBACK(VCHIQ_BULK_TRANSMIT_ABORTED, 2, 0x2001)
      END_CALLBACK(VCHIQ_SUCCESS)
   }

error_exit:
   callback_count = 0;
   bulk_count = 0;

   func_error = 1;
   vcos_event_signal(&func_test_sync);

   return VCHIQ_ERROR;
}

static VCHIQ_STATUS_T
fun2_clnt_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header,
   VCHIQ_SERVICE_HANDLE_T service, void *bulk_userdata)
{
   vcos_unused(header);
   vcos_unused(service);
   vcos_unused(bulk_userdata);

   switch (reason)
   {
   case VCHIQ_SERVICE_OPENED:
   case VCHIQ_SERVICE_CLOSED:
   case VCHIQ_BULK_TRANSMIT_DONE:
      break;
   case VCHIQ_BULK_RECEIVE_DONE:
      vcos_event_signal(&func_test_sync);
      break;
   default:
      fun2_error = 1;
      vcos_event_signal(&func_test_sync);
      break;
   }

   return VCHIQ_SUCCESS;
}

static int mem_check(const void *expected, const void *actual, int size)
{
   if (memcmp(expected, actual, size) != 0)
   {
      int i;
      for (i = 0; i < size; i++)
      {
         int ce = ((const char *)expected)[i];
         int ca = ((const char *)actual)[i];
         if (ca != ce)
            printf("%08x,%x: %02x <-> %02x\n", i + (unsigned int)actual, i, ce, ca);
      }
      printf("mem_check failed - buffer %x, size %d\n", (unsigned int)actual, size);
      return 1;
   }
   return 0;
}

static void usage(void)
{
   printf("Usage: vchiq_test [<options>] <mode> <iters>\n");
   printf("  where <options> is any of:\n");
   printf("    -a <c> <s>  set the client and server bulk alignment (modulo 32)\n");
   printf("    -A <c> <s>  set the client and server bulk alignment (modulo 4096)\n");
   printf("    -e          disable echoing in the main bulk transfer mode\n");
   printf("    -k <n>      skip the first <n> func data tests\n");
   printf("    -m <n>      set the client message quota to <n>\n");
   printf("    -M <n>      set the server message quota to <n>\n");
   printf("    -q          disable data verification\n");
   printf("    -s ????     service (any 4 characters)\n");
   printf("    -v          enable more verbose output\n");
   printf("    -r <b> <s>  reserve <b> bytes for <s> seconds\n");
   printf("    -K <t>      send a SIGKILL after <t> ms\n");
   printf("  and <mode> is one of:\n");
   printf("    -c <size>   control test (size in bytes)\n");
   printf("    -b <size>   bulk test (size in kilobytes)\n");
   printf("    -f          functional test\n");
   printf("    -p          ping test\n");
   printf("    -t          check the timer\n");
   printf("  and <iters> is the number of test iterations\n");
   exit(1);
}

static void check_timer(void)
{
   uint32_t start = vcos_getmicrosecs();
   uint32_t sleep_time = 1000;

   printf("0\n");

   while (1)
   {
      uint32_t now;
      vcos_sleep(sleep_time);
      now = vcos_getmicrosecs();
      printf("%d - sleep %d\n", now - start, sleep_time);
   }
}

static char *buf_align(char *buf, int align_size, int align)
{
   char *aligned = buf - ((intptr_t)buf & (align_size - 1)) + align;
   if (aligned < buf)
      aligned += align_size;
   return aligned;
}

#ifdef ANDROID

static void kill_timeout_handler(int cause, siginfo_t *how, void *ucontext)
{
   printf("Sending signal SIGKILL\n");
   kill(main_process_pid, SIGKILL);
}

static int setup_auto_kill(int timeout_ms)
{
   long timeout;
   struct timeval interval;

   if (timeout_ms <= 0)
   {
      return -1;
   }
   timeout = 1000 * timeout_ms;

   /* install a signal handler for the alarm */
   struct sigaction sa;
   memset(&sa, 0, sizeof(struct sigaction));
   sa.sa_sigaction = kill_timeout_handler;
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

   return 0;
}



#endif

#ifdef VCOS_APPLICATION_INITIALIZE

static VCOS_THREAD_T   Task_0;

void *main_task(void *param)
{
   vchiq_test(rtos_argc, rtos_argv);

   VCOS_BKPT;

   return NULL;
}

#include "vcfw/logging/logging.h"

void VCOS_APPLICATION_INITIALIZE(void *first_available_memory)
{
   const int      stack_size = 64*1024;
   void          *pointer = NULL;
   VCOS_STATUS_T  status;

   logging_init();
   logging_level(LOGGING_VCOS);
   vcos_init();

   /* Create task 0.  */
#if VCOS_CAN_SET_STACK_ADDR
   pointer = malloc(stack_size);
   vcos_demand(pointer);
#endif
   status = vcos_thread_create_classic( &Task_0, "TASK 0", main_task, (void *)0, pointer, stack_size,
                                        10 | VCOS_AFFINITY_DEFAULT, 20, VCOS_START );
   vcos_demand(status == VCOS_SUCCESS);
}

#else

int main(int argc, char **argv)
{
#ifdef ANDROID
   main_process_pid = getpid();
#endif

   vcos_init();
   vcos_use_android_log = 0;
   return vchiq_test(argc, argv);
}

#endif
