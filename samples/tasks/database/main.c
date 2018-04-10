#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <queues/task_queue.h>

#include "../../../core_info.h"
#include "../../../tasks/tasks_internal.h"

/* 
 * return codes -
 * graceful exit: 1
 * normal   exit: 0
 * error    exit: -1
 */

int main(int argc, char *argv[])
{
   const char *db_dir        = NULL;
   const char *core_info_dir = NULL;
   const char *core_dir      = NULL;
   const char *input_dir     = NULL;
   const char *playlist_dir  = NULL;
   const char *exts          = "dll";

   if (argc < 6)
   {
      fprintf(stderr, "Usage: %s <database dir> <core dir> <core info dir> <input dir> <playlist dir>\n", argv[0]);
      return 1;
   }

   db_dir        = argv[1];
   core_dir      = argv[2];
   core_info_dir = argv[3];
   input_dir     = argv[4];
   playlist_dir  = argv[5];

   task_queue_init(false /* threaded enable */, NULL);

   core_info_init_list(core_info_dir, core_dir, exts, true);

   task_push_dbscan(playlist_dir, db_dir, input_dir, false,
         true, NULL /* bind callback here later */);

   task_queue_check();

   core_info_deinit_list();
   task_queue_deinit();

   return 0;
}
