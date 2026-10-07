/* harness_cocoa.m -- the harness on macOS.
 *
 * On macOS the frontend's main() is Cocoa's (HAVE_MAIN, ui_cocoa.m):
 * it stands up NSApplication, the delegate and the window, and calls
 * rarch_main() from applicationDidFinishLaunching:. The Metal and
 * Cocoa GL contexts need all of that, so the harness keeps that main
 * and takes rarch_main() over instead: the lanes run in its place, on
 * the main thread with the application up, and build.sh compiles
 * retroarch.c with its own rarch_main renamed out of the way.
 *
 * Two things the run loop otherwise does are done here by hand. Each
 * frame turns the main run loop once, so the window's layer tree is
 * committed and the drawables the driver presents come back round.
 * And the process leaves with the harness's status: the Cocoa
 * companion's deinit, reached from main_exit(), terminates the
 * application, which is exit(0) - an atexit handler installed once
 * the status is known ends the process with that status instead. */

#import <Foundation/Foundation.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../../paths.h"

int harness_main(int argc, char *argv[]);

static int harness_status = -1;

static void harness_cocoa_leave(void)
{
   if (harness_status >= 0)
   {
      fflush(stdout);
      fflush(stderr);
      _exit(harness_status);
   }
}

void harness_cocoa_exit_status(int status)
{
   if (harness_status < 0)
      atexit(harness_cocoa_leave);
   harness_status = status;
}

void harness_cocoa_pump(void)
{
   while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.002, FALSE)
         == kCFRunLoopRunHandledSource);
}

int rarch_main(int argc, char *argv[], void *data)
{
   int rc;
   (void)data;
   /* AppKit hands every command-line argument that is not a flag to
    * the delegate as a file to open (application:openFiles:) while
    * the application finishes launching, before this is reached; with
    * no core up yet the delegate stores it as the content path. The
    * harness's argument is its cycle count, not content: forget it,
    * or retroarch_main_init() tries to load a file named "1". */
   path_clear(RARCH_PATH_CONTENT);
   rc = harness_main(argc, argv);
   harness_cocoa_exit_status(rc);
   exit(rc);
}
