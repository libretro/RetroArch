#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <queues/task_queue.h>

#include "../../../core_info.h"
#include "../../../tasks/tasks_internal.h"

static bool loop_active = true;

static void main_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush)
{
   fprintf(stderr, "MSGQ: %s\n", msg);
}

/*
 * return codes -
 * graceful exit: 1
 * normal   exit: 0
 * error    exit: -1
 */

static void main_db_cb(void *task_data, void *user_data, const char *err)
{
   fprintf(stderr, "DB CB: %s\n", err);
   loop_active = false;
}

int main(int argc, char *argv[])
{
   const char *db_dir        = NULL;
   const char *core_info_dir = NULL;
   const char *core_dir      = NULL;
   const char *input_dir     = NULL;
   const char *playlist_dir  = NULL;
#if defined(_WIN32)
   const char *exts          = "dll";
#elif defined(__MACH__)
   const char *exts          = "dylib";
#else
   const char *exts          = "so";
#endif

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

   fprintf(stderr, "RDB database dir: %s\n", db_dir);
   fprintf(stderr, "Core         dir: %s\n", core_dir);
   fprintf(stderr, "Core info    dir: %s\n", core_info_dir);
   fprintf(stderr, "Input        dir: %s\n", input_dir);
   fprintf(stderr, "Playlist     dir: %s\n", playlist_dir);
#ifdef HAVE_THREADS
   task_queue_init(true /* threaded enable */, main_msg_queue_push);
#else
   task_queue_init(false /* threaded enable */, main_msg_queue_push);
#endif
   core_info_init_list(core_info_dir, core_dir, exts, true);

   task_push_dbscan(playlist_dir, db_dir, input_dir, true,
         true, main_db_cb);

   while (loop_active)
      task_queue_check();

   fprintf(stderr, "Exit loop\n");

   core_info_deinit_list();
   task_queue_deinit();

   return 0;
}
