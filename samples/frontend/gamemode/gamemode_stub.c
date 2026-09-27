/* Stand-in for libgamemode.so.0.
 *
 * RetroArch's bundled gamemode_client.h dlopen()s libgamemode.so.0 and
 * binds the real_gamemode_* symbols below.  Putting this library first
 * on LD_LIBRARY_PATH lets the harness see every call the frontend makes
 * without a gamemoded daemon or a D-Bus session.  Each call appends its
 * name to the file named by GAMEMODE_STUB_LOG.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

static int stub_active = 0;

static void stub_note(const char *what)
{
   FILE *f;
   const char *path = getenv("GAMEMODE_STUB_LOG");
   if (!path)
      return;
   if (!(f = fopen(path, "a")))
      return;
   fprintf(f, "%s\n", what);
   fclose(f);
}

int real_gamemode_request_start(void)
{
   stub_note("request_start");
   stub_active = 1;
   return 0;
}

int real_gamemode_request_end(void)
{
   stub_note("request_end");
   stub_active = 0;
   return 0;
}

/* 0 = inactive, 2 = active for this client. */
int real_gamemode_query_status(void)
{
   stub_note("query_status");
   return stub_active ? 2 : 0;
}

const char *real_gamemode_error_string(void)
{
   return "";
}

int real_gamemode_request_start_for(pid_t pid)
{
   (void)pid;
   stub_note("request_start_for");
   return 0;
}

int real_gamemode_request_end_for(pid_t pid)
{
   (void)pid;
   stub_note("request_end_for");
   return 0;
}

int real_gamemode_query_status_for(pid_t pid)
{
   (void)pid;
   stub_note("query_status_for");
   return 0;
}
