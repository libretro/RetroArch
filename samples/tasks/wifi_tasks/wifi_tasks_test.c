/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* tasks/task_wifi.c on the real threaded task queue.
 *
 * The Wi-Fi networks list pushes a scan every 30 seconds while it is
 * shown, and a scan can take seconds. Connect, Disconnect and the
 * Wi-Fi Enabled toggle pushed during it were turned away without a
 * word - the toggle leaving the setting and the driver apart. They
 * are held behind the scan now and run after it, in order; only a
 * scan behind a scan is refused. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <queues/task_queue.h>

#include "network/wifi_driver.h"
#include "msg_hash.h"
#include "tasks/tasks_internal.h"

/* The driver: what ran, in order, and a scan that takes a while. */
#define LOG_MAX 16
static char               ran[LOG_MAX];
static retro_atomic_int_t ran_n    = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t scanning = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t overlap  = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t inside   = RETRO_ATOMIC_INT_INITIALIZER(0);

static void ran_add(char c)
{
   int n;
   if (retro_atomic_fetch_add_int(&inside, 1) != 0)
      retro_atomic_store_release_int(&overlap, 1);
   n = retro_atomic_fetch_add_int(&ran_n, 1);
   if (n < LOG_MAX)
      ran[n] = c;
}

static void ran_end(void) { retro_atomic_fetch_sub_int(&inside, 1); }

void driver_wifi_scan(void)
{
   ran_add('S');
   retro_atomic_store_release_int(&scanning, 1);
   usleep(300000);
   retro_atomic_store_release_int(&scanning, 0);
   ran_end();
}

bool driver_wifi_enable(bool enable)
{
   ran_add(enable ? 'E' : 'D');
   ran_end();
   return true;
}

bool driver_wifi_connection_info(wifi_network_info_t *network)
{
   memset(network, 0, sizeof(*network));
   return true;
}

bool driver_wifi_connect_ssid(const wifi_network_info_t *netinfo)
{
   ran_add('C');
   ran_end();
   return true;
}

bool driver_wifi_disconnect_ssid(const wifi_network_info_t *netinfo)
{
   ran_add('X');
   ran_end();
   return true;
}

const char *msg_hash_to_str(enum msg_hash_enums msg) { return "msg %s"; }

static unsigned callbacks = 0;
static void cb(retro_task_t *task, void *td, void *ud, const char *err)
{
   callbacks++;
}

static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   printf("  [%s] %s\n", cond ? "pass" : "FAIL", what);
   if (!cond)
      failures++;
}

static void pump_until_idle(void)
{
   int i;
   for (i = 0; i < 5000; i++)
   {
      task_queue_check();
      if (retro_atomic_load_acquire_int(&ran_n) >= 5 && callbacks >= 5)
         break;
      usleep(1000);
   }
   for (i = 0; i < 20; i++)
   {
      task_queue_check();
      usleep(1000);
   }
}

int main(void)
{
   wifi_network_info_t net;
   int i;

   memset(&net, 0, sizeof(net));
   strcpy(net.ssid, "harness");
   task_queue_init(true, NULL);

   check(task_push_wifi_scan(cb), "the networks list's scan is pushed");
   for (i = 0; i < 1000 && !retro_atomic_load_acquire_int(&scanning); i++)
      usleep(1000);
   check(retro_atomic_load_acquire_int(&scanning), "the scan is running");

   check(!task_push_wifi_scan(cb), "a second scan behind it is refused");
   check(task_push_wifi_connect(cb, &net),
         "Connect during the scan is taken, not refused");
   check(task_push_wifi_disconnect(cb),
         "Disconnect during the scan is taken, not refused");
   check(task_push_wifi_disable(cb),
         "Wi-Fi Enabled off during the scan is taken, not refused");
   check(task_push_wifi_enable(cb),
         "Wi-Fi Enabled on during the scan is taken, not refused");

   pump_until_idle();

   ran[LOG_MAX - 1] = '\0';
   if (retro_atomic_load_acquire_int(&ran_n) < LOG_MAX)
      ran[retro_atomic_load_acquire_int(&ran_n)] = '\0';
   printf("  driver calls: %s\n", ran);
   check(!strcmp(ran, "SCXDE"),
         "they run after the scan, in the order they were asked for");
   check(!retro_atomic_load_acquire_int(&overlap),
         "one Wi-Fi task uses the driver at a time");
   check(callbacks == 5, "each one's callback runs");

   check(task_push_wifi_scan(cb), "a scan once all is through is pushed");
   for (i = 0; i < 2000 && callbacks < 6; i++)
   {
      task_queue_check();
      usleep(1000);
   }
   check(callbacks == 6, "and finishes");

   task_queue_deinit();
   printf(failures ? "\nFAILURES\n" : "\nall passed\n");
   return failures ? 1 : 0;
}
