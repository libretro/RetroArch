/* Stand-in for CoreFoundation's run loop header: the types and calls
 * btstack_hid.c names, answered by btstack_hid_test.c. */
#ifndef BTSTACK_TEST_CFRUNLOOP_H
#define BTSTACK_TEST_CFRUNLOOP_H

typedef unsigned char Boolean;
typedef long CFIndex;
typedef unsigned long CFOptionFlags;
typedef double CFAbsoluteTime;
typedef double CFTimeInterval;
typedef const void *CFTypeRef;
typedef const struct __CFString *CFStringRef;
typedef const struct __CFAllocator *CFAllocatorRef;
typedef struct __CFRunLoop *CFRunLoopRef;
typedef struct __CFRunLoopSource *CFRunLoopSourceRef;
typedef struct __CFRunLoopTimer *CFRunLoopTimerRef;

typedef struct
{
   CFIndex version;
   void *info;
   const void *(*retain)(const void *info);
   void (*release)(const void *info);
   CFStringRef (*copyDescription)(const void *info);
   Boolean (*equal)(const void *a, const void *b);
   unsigned long (*hash)(const void *info);
   void (*schedule)(void *info, CFRunLoopRef rl, CFStringRef mode);
   void (*cancel)(void *info, CFRunLoopRef rl, CFStringRef mode);
   void (*perform)(void *info);
} CFRunLoopSourceContext;

typedef struct
{
   CFIndex version;
   void *info;
   const void *(*retain)(const void *info);
   void (*release)(const void *info);
   CFStringRef (*copyDescription)(const void *info);
} CFRunLoopTimerContext;

typedef void (*CFRunLoopTimerCallBack)(CFRunLoopTimerRef timer, void *info);

extern const CFStringRef kCFRunLoopCommonModes;

CFRunLoopRef CFRunLoopGetCurrent(void);
void CFRunLoopRun(void);
void CFRunLoopStop(CFRunLoopRef rl);
void CFRunLoopWakeUp(CFRunLoopRef rl);
CFRunLoopSourceRef CFRunLoopSourceCreate(CFAllocatorRef a, CFIndex order,
      CFRunLoopSourceContext *ctx);
void CFRunLoopAddSource(CFRunLoopRef rl, CFRunLoopSourceRef s,
      CFStringRef mode);
void CFRunLoopSourceSignal(CFRunLoopSourceRef s);
void CFRunLoopSourceInvalidate(CFRunLoopSourceRef s);
CFAbsoluteTime CFAbsoluteTimeGetCurrent(void);
CFRunLoopTimerRef CFRunLoopTimerCreate(CFAllocatorRef a, CFAbsoluteTime fire,
      CFTimeInterval interval, CFOptionFlags flags, CFIndex order,
      CFRunLoopTimerCallBack cb, CFRunLoopTimerContext *ctx);
void CFRunLoopAddTimer(CFRunLoopRef rl, CFRunLoopTimerRef t,
      CFStringRef mode);
void CFRunLoopTimerInvalidate(CFRunLoopTimerRef t);
void CFRelease(CFTypeRef p);

#endif
