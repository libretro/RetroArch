//
//  JITSupport.m
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 9/25/21.
//  Copyright © 2021 RetroArch. All rights reserved.
//
//  Copied from UTMApp, original author: osy
//  

#import <Foundation/Foundation.h>

#import "JITSupport.h"

#include <dlfcn.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/getsect.h>
#include <pthread.h>
#include <dirent.h>

#if defined(HAVE_ALTKIT)
@import AltKit;
#endif

#include <string/stdstring.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include "../../verbosity.h"

extern int csops(pid_t pid, unsigned int ops, void * useraddr, size_t usersize);
extern boolean_t exc_server(mach_msg_header_t *, mach_msg_header_t *);
extern int ptrace(int request, pid_t pid, caddr_t addr, int data);

#define    CS_OPS_STATUS        0    /* return status */
#define CS_KILL     0x00000200  /* kill process if it becomes invalid */
#define CS_DEBUGGED 0x10000000  /* process is currently or has previously been debugged and allowed to run with invalid pages */
#define PT_TRACE_ME     0       /* child declares it's being traced */
#define PT_SIGEXC       12      /* signals as exceptions for current_proc */

#if !TARGET_OS_TV
static void *exception_handler(void *argument) {
    mach_port_t port = *(mach_port_t *)argument;
    mach_msg_server(exc_server, 2048, port, 0);
    return NULL;
}
#endif

static bool jb_has_debugger_attached(void) {
    int flags;
    return !csops(getpid(), CS_OPS_STATUS, &flags, sizeof(flags)) && flags & CS_DEBUGGED;
}

bool jb_enable_ptrace_hack(void) {
#if !TARGET_OS_TV
    bool debugged = jb_has_debugger_attached();
    
    // Thanks to this comment: https://news.ycombinator.com/item?id=18431524
    // We use this hack to allow mmap with PROT_EXEC (which usually requires the
    // dynamic-codesigning entitlement) by tricking the process into thinking
    // that Xcode is debugging it. We abuse the fact that JIT is needed to
    // debug the process.
    if (ptrace(PT_TRACE_ME, 0, NULL, 0) < 0) {
        return false;
    }
    
    // ptracing ourselves confuses the kernel and will cause bad things to
    // happen to the system (hangs…) if an exception or signal occurs. Setup
    // some "safety nets" so we can cause the process to exit in a somewhat sane
    // state. We only need to do this if the debugger isn't attached. (It'll do
    // this itself, and if we do it we'll interfere with its normal operation
    // anyways.)
    if (!debugged) {
        // First, ensure that signals are delivered as Mach software exceptions…
        ptrace(PT_SIGEXC, 0, NULL, 0);
        
        // …then ensure that this exception goes through our exception handler.
        // I think it's OK to just watch for EXC_SOFTWARE because the other
        // exceptions (e.g. EXC_BAD_ACCESS, EXC_BAD_INSTRUCTION, and friends)
        // will end up being delivered as signals anyways, and we can get them
        // once they're resent as a software exception.
        mach_port_t port = MACH_PORT_NULL;
        mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
        mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
        task_set_exception_ports(mach_task_self(), EXC_MASK_SOFTWARE, port, EXCEPTION_DEFAULT, THREAD_STATE_NONE);
        pthread_t thread;
        pthread_create(&thread, NULL, exception_handler, (void *)&port);
    } else {
        // JIT code frequently causes an EXC_BAD_ACCESS exception that lldb
        // cannot be convinced to ignore. Instead we can set up a nul handler
        // that effectively causes it to be ignored. Note that this sometimes
        // also hides actual crashes from the debugger.
        task_set_exception_ports(mach_task_self(), EXC_MASK_BAD_ACCESS, MACH_PORT_NULL, EXCEPTION_DEFAULT, THREAD_STATE_NONE);
    }
#endif
    
    return true;
}

void jb_start_altkit(void) {
#if HAVE_ALTKIT
   // asking AltKit/AltServer to debug us when we're already debugged is bad, very bad
   if (jit_available())
      return;

   char fwpath[PATH_MAX_LENGTH] = {0};
   fill_pathname_expand_special(fwpath, ":/Frameworks/flycast_libretro.framework", sizeof(fwpath));
   if (!path_is_valid(fwpath))
      return;

   [[ALTServerManager sharedManager] autoconnectWithCompletionHandler:^(ALTServerConnection *connection, NSError *error) {
      if (error)
         return;

      [connection enableUnsignedCodeExecutionWithCompletionHandler:^(BOOL success, NSError *error) {
         if (success)
            [[ALTServerManager sharedManager] stopDiscovering];
         else
            RARCH_WARN("AltServer failed: %s\n", [error.description UTF8String]);

         [connection disconnect];
      }];
   }];

   [[ALTServerManager sharedManager] startDiscovering];
#endif
}

bool jit_available(void)
{
   static bool canOpenApps = false;
   static dispatch_once_t appsOnce = 0;
   dispatch_once(&appsOnce, ^{
      DIR *apps = opendir("/Applications");
      if (apps)
      {
         closedir(apps);
         canOpenApps = true;
      }
   });

   static bool dylded = false;
   static dispatch_once_t dyldOnce = 0;
   dispatch_once(&dyldOnce, ^{
      int imageCount = _dyld_image_count();
      for (int i = 0; i < imageCount; i++)
      {
         if (string_is_equal("/usr/lib/pspawn_payload-stg2.dylib", _dyld_get_image_name(i)))
            dylded = true;
      }
   });

   static bool doped = false;
   static dispatch_once_t dopeOnce = 0;
   dispatch_once(&dopeOnce, ^{
      int64_t (*jbdswDebugMe)(void) = dlsym(RTLD_DEFAULT, "jbdswDebugMe");
      if (jbdswDebugMe)
      {
         int64_t ret = jbdswDebugMe();
         doped = (ret == 0);
      }
   });

   /* the debugger could be attached at any time, its value can't be cached */
   return canOpenApps || dylded || doped || jb_has_debugger_attached();
}
