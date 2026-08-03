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

#include <sys/mman.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <sys/ucontext.h>

#include <libretro.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include "../../verbosity.h"

extern int csops(pid_t pid, unsigned int ops, void * useraddr, size_t usersize);
extern boolean_t exc_server(mach_msg_header_t *, mach_msg_header_t *);
extern int ptrace(int request, pid_t pid, caddr_t addr, int data);

#define    CS_OPS_STATUS        0    /* return status */
#define CS_DEBUGGED 0x10000000  /* process is currently or has previously been debugged and allowed to run with invalid pages */

static bool jb_has_debugger_attached(void) {
    int flags;
    return !csops(getpid(), CS_OPS_STATUS, &flags, sizeof(flags)) && flags & CS_DEBUGGED;
}

#if !TARGET_OS_TV
#define PT_TRACE_ME     0       /* child declares it's being traced */
#define PT_SIGEXC       12      /* signals as exceptions for current_proc */

static void *exception_handler(void *argument) {
    mach_port_t port = *(mach_port_t *)argument;
    mach_msg_server(exc_server, 2048, port, 0);
    return NULL;
}

bool jb_enable_ptrace_hack(void) {
    if (@available(iOS 26, *))
        return false;

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
    
    return true;
}
#endif /* !TARGET_OS_TV */

#if !TARGET_OS_SIMULATOR
static bool device_has_txm(void)
{
   static bool has_txm = false;
   static dispatch_once_t once = 0;
   dispatch_once(&once, ^{
      if (@available(iOS 26, tvOS 26, *))
      {
         /* Check for TXM firmware on disk. Non-TXM devices (e.g. A10X) running
          * iOS/tvOS 26 won't have this file. */
         __block bool checked = false;
         NSString *bootUUID = nil;
         NSString *preboot = @"/System/Volumes/Preboot";
         NSError *error = nil;
         NSArray<NSString *> *items = [[NSFileManager defaultManager]
            contentsOfDirectoryAtPath:preboot error:&error];
         for (NSString *entry in items)
         {
            if (entry.length == 36)
            {
               bootUUID = [preboot stringByAppendingPathComponent:entry];
               break;
            }
         }
         if (bootUUID)
         {
            NSString *bootDir = [bootUUID stringByAppendingPathComponent:@"boot"];
            items = [[NSFileManager defaultManager]
               contentsOfDirectoryAtPath:bootDir error:&error];
            for (NSString *entry in items)
            {
               if (entry.length == 96)
               {
                  NSString *img = [[bootDir stringByAppendingPathComponent:entry]
                     stringByAppendingPathComponent:
                     @"usr/standalone/firmware/FUD/Ap,TrustedExecutionMonitor.img4"];
                  checked = true;
                  if (access(img.fileSystemRepresentation, F_OK) == 0)
                     has_txm = true;
                  break;
               }
            }
         }

         if (!has_txm)
         {
            /* Fallback: /private/preboot/<96>/usr/.../Ap,TrustedExecutionMonitor.img4 */
            items = [[NSFileManager defaultManager]
               contentsOfDirectoryAtPath:@"/private/preboot" error:&error];
            for (NSString *entry in items)
            {
               if (entry.length == 96)
               {
                  NSString *img = [[@"/private/preboot" stringByAppendingPathComponent:entry]
                     stringByAppendingPathComponent:
                     @"usr/standalone/firmware/FUD/Ap,TrustedExecutionMonitor.img4"];
                  checked = true;
                  if (access(img.fileSystemRepresentation, F_OK) == 0)
                     has_txm = true;
                  break;
               }
            }
         }

         /* Never got far enough to test for the file, so we do not know.
          * Blessing a device that needed it leaves the core executing unblessed
          * pages, while blessing one that did not only costs time. */
         if (!has_txm && !checked)
         {
            RARCH_WARN("[JIT] Could not read preboot; assuming TXM\n");
            has_txm = true;
         }
      }
   });
   return has_txm;
}

static bool requires_dual_map(void)
{
   if (@available(iOS 26, tvOS 26, *))
      return true;
   return false;
}

#ifdef __arm64__
static volatile bool s_brk_trapped;
static void brk_trap_handler(int sig, siginfo_t *info, void *ctx)
{
   s_brk_trapped = true;
   ((ucontext_t *)ctx)->uc_mcontext->__ss.__pc += 4;
}
#endif

/* Ask the debugger to bless an R-X region so TXM allows execution.
 * Uses the universal.js protocol: brk #0xf00d with x16=1
 * (CMD_PREPARE_REGION). Installs a SIGTRAP handler so a missing
 * debugger doesn't crash. */
static bool bless_executable_region(void *ptr, size_t size)
{
#ifdef __arm64__
   struct sigaction prev, act = {};
   act.sa_sigaction = brk_trap_handler;
   act.sa_flags     = SA_SIGINFO;
   sigemptyset(&act.sa_mask);
   sigaction(SIGTRAP, &act, &prev);
   s_brk_trapped = false;
   __asm__ volatile(
      "mov x0, %0\n"
      "mov x1, %1\n"
      "mov x16, #1\n"
      "brk #0xf00d"
      :: "r"(ptr), "r"(size)
      : "x0", "x1", "x16", "memory"
   );
   sigaction(SIGTRAP, &prev, NULL);
   return !s_brk_trapped;
#else
   (void)ptr;
   (void)size;
   return false;
#endif
}

/* Tell the debugger to detach. Uses the universal.js protocol:
 * brk #0xf00d with x16=0 (CMD_DETACH). */
static void detach_debugger(void)
{
#ifdef __arm64__
   struct sigaction prev, act = {};
   act.sa_sigaction = brk_trap_handler;
   act.sa_flags     = SA_SIGINFO;
   sigemptyset(&act.sa_mask);
   sigaction(SIGTRAP, &act, &prev);
   s_brk_trapped = false;
   __asm__ volatile(
      "mov x16, #0\n"
      "brk #0xf00d"
      ::: "x16", "memory"
   );
   sigaction(SIGTRAP, &prev, NULL);
#endif
}

/* Create a R-W mirror of an existing R-X region via vm_remap.
 * Returns the R-W pointer on success, NULL on failure. */
static void *create_rw_mirror(void *rx, size_t size)
{
   vm_address_t rw = 0;
   vm_prot_t cur = 0, max = 0;
   kern_return_t kr = vm_remap(mach_task_self(), &rw, size, 0,
                               VM_FLAGS_ANYWHERE, mach_task_self(),
                               (vm_address_t)rx, FALSE,
                               &cur, &max, VM_INHERIT_DEFAULT);
   if (kr != KERN_SUCCESS)
      return NULL;
   if (mprotect((void *)rw, size, PROT_READ | PROT_WRITE) != 0)
   {
      vm_deallocate(mach_task_self(), rw, size);
      return NULL;
   }
   return (void *)rw;
}
#endif /* !TARGET_OS_SIMULATOR */

/* On iOS 26+ TXM devices the debugger must bless executable pages via
 * brk #0x69 before they can be executed. Rather than keeping the
 * debugger attached for the lifetime of the process, we allocate one
 * large pool at startup, bless it in a single brk call, and then the
 * debugger can detach. All subsequent exec_mem_alloc requests are
 * bump-allocated from this pre-blessed pool. */
#define EXEC_MEM_POOL_SIZE (544UL * 1024 * 1024)

static void    *s_pool_rx   = NULL;
static void    *s_pool_rw   = NULL;
static size_t   s_pool_size = 0;
static size_t   s_pool_used = 0;

#if !TARGET_OS_SIMULATOR
static void exec_mem_request_remote_bless(void);

/* Caller has established that a debugger is attached. */
static bool exec_mem_pool_create(void)
{
   size_t page = sysconf(_SC_PAGESIZE);
   size_t size = (EXEC_MEM_POOL_SIZE + page - 1) & ~(page - 1);

   void *rx = mmap(NULL, size, PROT_READ | PROT_EXEC,
                   MAP_ANON | MAP_PRIVATE, -1, 0);
   if (rx == MAP_FAILED)
      return false;

   /* TXM devices need the debugger to bless executable pages. Non-TXM devices
    * can mmap R-X freely with CS_DEBUGGED */
   if (device_has_txm() && !bless_executable_region(rx, size))
   {
      munmap(rx, size);
      return false;
   }

   void *rw = create_rw_mirror(rx, size);
   if (!rw)
   {
      munmap(rx, size);
      return false;
   }

   s_pool_rx   = rx;
   s_pool_rw   = rw;
   s_pool_size = size;
   s_pool_used = 0;
   RARCH_LOG("[JIT] Pool allocated: %zu MB (rx=%p rw=%p)\n",
             size / (1024 * 1024), rx, rw);

   /* if it weren't a mem pool we wouldn't be able to detach */
   detach_debugger();
   RARCH_LOG("[JIT] Debugger detached\n");

   return true;
}
#endif /* !TARGET_OS_SIMULATOR */

/* Called at boot, and again on each request. StikDebug and Xcode have
 * already attached by boot, so they get the pool immediately; otherwise
 * this kicks off the bless-service handshake and returns without waiting,
 * and the pool appears on that thread whenever the service answers. */
bool exec_mem_pool_init(void)
{
#if TARGET_OS_SIMULATOR
   return false;
#else
   if (s_pool_rx)
      return true;

   /* Pre-26 wants no pool: the attach alone sets CS_DEBUGGED, which is all a
    * recompiler needs there. */
   if (!requires_dual_map())
   {
      if (!jit_available())
         exec_mem_request_remote_bless();
      return false;
   }

   if (jb_has_debugger_attached())
      return exec_mem_pool_create();

   exec_mem_request_remote_bless();
   return false;
#endif
}

void exec_mem_pool_reset(void)
{
   s_pool_used = 0;
}

#if !TARGET_OS_SIMULATOR
/* tvOS can never run StikDebug (it needs a VPN app, which tvOS forbids), so
 * when nothing has attached by boot we go looking for the bless service on the
 * LAN and ask it to attach to us. Its reply is the only reliable sign that one
 * is listening: CS_DEBUGGED stays true after a detach, so it cannot say. */
#define RA_BLESS_SERVICE_TYPE @"_ra-bless._tcp."
#define RA_BLESS_RESOLVE_TIMEOUT 5.0
#define RA_BLESS_REPLY_TIMEOUT 120   /* blessing 544MB takes a while */
#define RA_BLESS_RETRY_SECONDS 15
#define RA_BLESS_PROTOCOL 1
#define RA_BLESS_CONNECT_TIMEOUT 3   /* per address, see connect_bounded */

static bool remote_bless_done(void)
{
   if (requires_dual_map())
      return s_pool_rx != NULL;
   return jit_available();
}

/* Called once the service says it has attached. */
static bool remote_bless_finish(void)
{
   if (requires_dual_map())
      return exec_mem_pool_init();
   detach_debugger();
   RARCH_LOG("[JIT] Debugger detached; JIT is available\n");
   return true;
}

@interface RABlessClient : NSObject <NSNetServiceBrowserDelegate,
                                     NSNetServiceDelegate>
@property (nonatomic, strong) NSNetServiceBrowser *browser;
@property (nonatomic, strong) NSMutableArray<NSNetService *> *pending;
@property (nonatomic, strong) dispatch_queue_t asks;
@end

@implementation RABlessClient

- (void)start
{
   self.pending = [NSMutableArray array];
   self.browser = [[NSNetServiceBrowser alloc] init];
   self.browser.delegate = self;
   [self.browser searchForServicesOfType:RA_BLESS_SERVICE_TYPE
                                inDomain:@"local."];
   [self scheduleRetry];
   RARCH_LOG("[JIT] Looking for a bless service on the network\n");
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
           didFindService:(NSNetService *)service
               moreComing:(BOOL)moreComing
{
   RARCH_LOG("[JIT] Found bless service \"%s\"; resolving\n",
             service.name.UTF8String);
   [self.pending addObject:service];
   service.delegate = self;
   [service resolveWithTimeout:RA_BLESS_RESOLVE_TIMEOUT];
}

- (void)netService:(NSNetService *)service
     didNotResolve:(NSDictionary<NSString *, NSNumber *> *)errorDict
{
   RARCH_WARN("[JIT] Could not resolve \"%s\"\n", service.name.UTF8String);
   [self.pending removeObject:service];
}

- (void)netServiceDidResolveAddress:(NSNetService *)service
{
   NSArray *addresses = [service.addresses copy];
   [self.pending removeObject:service];

   /* Keep browsing until it has actually worked. */
   if (!self.asks)
      self.asks = dispatch_queue_create("com.retroarch.bless",
                                        DISPATCH_QUEUE_SERIAL);
   dispatch_async(self.asks, ^{
      if (remote_bless_done())
         return;
      if ([self ask:addresses] && remote_bless_finish())
         dispatch_async(dispatch_get_main_queue(), ^{
            [self.browser stop];
            RARCH_LOG("[JIT] Stopped looking for a bless service\n");
         });
   });
}

/* A service already reported is not reported again, so re-enumerate: the helper
 * may have only just started, or local network permission may have only just
 * been granted. */
- (void)scheduleRetry
{
   __weak RABlessClient *weakSelf = self;
   dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                (int64_t)(RA_BLESS_RETRY_SECONDS * NSEC_PER_SEC)),
                  dispatch_get_main_queue(), ^{
      RABlessClient *self_ = weakSelf;
      if (!self_ || remote_bless_done())
         return;
      RARCH_LOG("[JIT] Still no bless service; looking again\n");
      [self_.browser stop];
      [self_.browser searchForServicesOfType:RA_BLESS_SERVICE_TYPE
                                    inDomain:@"local."];
      [self_ scheduleRetry];
   });
}

- (void)netServiceBrowser:(NSNetServiceBrowser *)browser
             didNotSearch:(NSDictionary<NSString *, NSNumber *> *)errorDict
{
   RARCH_WARN("[JIT] Cannot browse for a bless service: %s\n",
              errorDict.description.UTF8String);
}

/* connect() ignores SO_SNDTIMEO, so an unroutable address costs the kernel
 * default of over a minute. The helper advertises every address it has, and
 * only one of them is usually reachable, so each attempt has to be bounded. */
static int connect_bounded(int fd, const struct sockaddr *sa, socklen_t len,
                           int secs)
{
   int flags = fcntl(fd, F_GETFL, 0);
   int rc;

   fcntl(fd, F_SETFL, flags | O_NONBLOCK);
   rc = connect(fd, sa, len);
   if (rc != 0 && errno == EINPROGRESS)
   {
      struct pollfd pfd = { fd, POLLOUT, 0 };
      if (poll(&pfd, 1, secs * 1000) == 1)
      {
         int err = 0;
         socklen_t elen = sizeof(err);
         getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen);
         errno = err;
         rc = err ? -1 : 0;
      }
      else
      {
         errno = ETIMEDOUT;
         rc = -1;
      }
   }
   fcntl(fd, F_SETFL, flags);
   return rc;
}

/* Ask the service to attach to us, and wait for it to confirm. */
- (BOOL)ask:(NSArray<NSData *> *)addresses
{
   NSString *bundle = [[NSBundle mainBundle] bundleIdentifier] ?: @"";
   NSString *req    = [NSString stringWithFormat:
      @"{\"version\":%d,\"bundle_id\":\"%@\",\"pid\":%d}\n",
      RA_BLESS_PROTOCOL, bundle, getpid()];
   const char *out  = req.UTF8String;

   for (NSData *addr in addresses)
   {
      int fd = socket(((const struct sockaddr *)addr.bytes)->sa_family,
                      SOCK_STREAM, 0);
      if (fd < 0)
         continue;

      struct timeval tv = { RA_BLESS_REPLY_TIMEOUT, 0 };
      setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
      setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

      if (connect_bounded(fd, (const struct sockaddr *)addr.bytes,
                          (socklen_t)addr.length,
                          RA_BLESS_CONNECT_TIMEOUT) != 0
          || send(fd, out, strlen(out), 0) < 0)
      {
         RARCH_WARN("[JIT] Bless service unreachable at one address: %s\n",
                    strerror(errno));
         close(fd);
         continue;
      }

      char reply[256] = {0};
      ssize_t got = recv(fd, reply, sizeof(reply) - 1, 0);
      close(fd);
      if (got <= 0)
      {
         RARCH_WARN("[JIT] Bless service did not answer\n");
         continue;
      }

      /* The service attached before answering, so a brk is safe now. */
      if (strstr(reply, "\"ok\": true") || strstr(reply, "\"ok\":true"))
      {
         RARCH_LOG("[JIT] Bless service attached\n");
         return YES;
      }
      RARCH_WARN("[JIT] Bless service declined: %s\n", reply);
      return NO;
   }
   return NO;
}

@end

static void exec_mem_request_remote_bless(void)
{
   static RABlessClient  *client = nil;
   static dispatch_once_t once   = 0;

   /* Otherwise App Store builds ask for local network access to look for a
    * helper that could never attach to them anyway. */
   if (!jit_possible())
      return;

   dispatch_once(&once, ^{
      client = [[RABlessClient alloc] init];
      /* NSNetServiceBrowser is run-loop driven; the main run loop is the one
       * guaranteed to be turning, and browsing on it does not block. */
      dispatch_async(dispatch_get_main_queue(), ^{ [client start]; });
   });
}
#endif /* !TARGET_OS_SIMULATOR */

bool exec_mem_alloc(size_t *size, unsigned *mode, void **rx, void **rw)
{
#if TARGET_OS_SIMULATOR
   return false;
#else
   exec_mem_pool_init();

   if (s_pool_rx)
   {
      if (*size == 0)
      {
         *mode = RETRO_EXEC_MEM_MODE_DUAL_MAP;
         *size = s_pool_size - s_pool_used;
         *rx   = NULL;
         *rw   = NULL;
         return true;
      }

      size_t page = sysconf(_SC_PAGESIZE);
      *size = (*size + page - 1) & ~(page - 1);

      if (s_pool_used + *size > s_pool_size)
      {
         RARCH_ERR("[JIT] Pool exhausted: need %zu, have %zu\n",
                   *size, s_pool_size - s_pool_used);
         return false;
      }

      *mode = RETRO_EXEC_MEM_MODE_DUAL_MAP;
      *rx   = (uint8_t *)s_pool_rx + s_pool_used;
      *rw   = (uint8_t *)s_pool_rw + s_pool_used;
      s_pool_used += *size;
      return true;
   }

   if (!jb_has_debugger_attached())
      return false;

   bool dual = requires_dual_map();

   if (*size == 0)
   {
      *mode = dual ? RETRO_EXEC_MEM_MODE_DUAL_MAP
                   : RETRO_EXEC_MEM_MODE_WX_TOGGLE;
      *rx   = NULL;
      *rw   = NULL;
      return true;
   }

   size_t page = sysconf(_SC_PAGESIZE);
   *size = (*size + page - 1) & ~(page - 1);

   if (dual)
   {
      void *ptr_rx = mmap(NULL, *size, PROT_READ | PROT_EXEC,
                          MAP_ANON | MAP_PRIVATE, -1, 0);
      if (ptr_rx == MAP_FAILED)
         return false;

      if (device_has_txm() && !bless_executable_region(ptr_rx, *size))
      {
         munmap(ptr_rx, *size);
         return false;
      }

      void *ptr_rw = create_rw_mirror(ptr_rx, *size);
      if (!ptr_rw)
      {
         munmap(ptr_rx, *size);
         return false;
      }

      *mode = RETRO_EXEC_MEM_MODE_DUAL_MAP;
      *rx   = ptr_rx;
      *rw   = ptr_rw;
      return true;
   }

   /* Pre-iOS 26: single mapping, core toggles W^X via mprotect */
   void *ptr = mmap(NULL, *size, PROT_READ | PROT_WRITE,
                    MAP_ANON | MAP_PRIVATE, -1, 0);
   if (ptr == MAP_FAILED)
      return false;
   *mode = RETRO_EXEC_MEM_MODE_WX_TOGGLE;
   *rx   = ptr;
   *rw   = ptr;
   return true;
#endif
}

void exec_mem_free(void *rx, void *rw, size_t size, bool dual)
{
   /* Pool allocations are freed in bulk via exec_mem_pool_reset */
   if (s_pool_rx)
      return;

   if (dual && rw && rw != rx)
      vm_deallocate(mach_task_self(), (vm_address_t)rw, size);
   if (rx)
      munmap(rx, size);
}

/* Unlike jit_available(), true before any debugger has turned up. */
bool jit_possible(void)
{
   static bool possible = false;
   static dispatch_once_t once = 0;
   dispatch_once(&once, ^{
      NSString *path = [[NSBundle mainBundle] pathForResource:@"embedded"
                                                       ofType:@"mobileprovision"];
      if (!path)
         return; /* App Store and TestFlight builds carry no profile */

      NSData *raw = [NSData dataWithContentsOfFile:path];
      NSString *text = [[NSString alloc] initWithData:raw
                                             encoding:NSISOLatin1StringEncoding];
      NSRange open  = [text rangeOfString:@"<plist"];
      NSRange close = [text rangeOfString:@"</plist>"];
      if (open.location == NSNotFound || close.location == NSNotFound)
         return;

      NSString *xml = [text substringWithRange:
         NSMakeRange(open.location, NSMaxRange(close) - open.location)];
      NSDictionary *profile = [NSPropertyListSerialization
         propertyListWithData:[xml dataUsingEncoding:NSISOLatin1StringEncoding]
                      options:0 format:NULL error:NULL];
      possible = [profile[@"Entitlements"][@"get-task-allow"] boolValue];
   });
   return possible;
}

bool jit_available(void)
{
   if (s_pool_rx)
      return true;

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

   if (canOpenApps || dylded || doped)
      return true;

#if TARGET_OS_SIMULATOR
   return false;
#else
   if (!jb_has_debugger_attached())
      return false;

   if (requires_dual_map() && device_has_txm())
   {
      /* TXM device on iOS/tvOS 26: probe whether the debugger script
       * is handling brk #0x69.  Uses the safe SIGTRAP handler so a
       * missing script doesn't crash — it just means JIT won't work. */
      size_t page = sysconf(_SC_PAGESIZE);
      void *probe = mmap(NULL, page, PROT_READ | PROT_EXEC,
                         MAP_ANON | MAP_PRIVATE, -1, 0);
      if (probe == MAP_FAILED)
         return false;
      bool blessed = bless_executable_region(probe, page);
      munmap(probe, page);
      if (!blessed)
         return false;
   }
   return true;
#endif
}
