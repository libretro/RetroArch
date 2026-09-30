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
   int rc = harness_main(argc, argv);
   (void)data;
   harness_cocoa_exit_status(rc);
   exit(rc);
}
