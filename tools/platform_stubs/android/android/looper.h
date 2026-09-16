/* Hermetic compile-only stand-in for the NDK header: only the names RetroArch uses. */
#ifndef STUB_android_looper_h
#define STUB_android_looper_h
typedef struct ALooper ALooper;
enum { ALOOPER_PREPARE_ALLOW_NON_CALLBACKS = 1 };
enum { ALOOPER_POLL_WAKE = -1, ALOOPER_POLL_CALLBACK = -2, ALOOPER_POLL_TIMEOUT = -3, ALOOPER_POLL_ERROR = -4 };
enum { ALOOPER_EVENT_INPUT = 1 };
typedef int (*ALooper_callbackFunc)(int fd, int events, void *data);
ALooper *ALooper_forThread(void);
ALooper *ALooper_prepare(int opts);
int ALooper_pollOnce(int timeoutMillis, int *outFd, int *outEvents, void **outData);
int ALooper_pollAll(int timeoutMillis, int *outFd, int *outEvents, void **outData);
int ALooper_addFd(ALooper *l, int fd, int ident, int events, ALooper_callbackFunc cb, void *data);
void ALooper_wake(ALooper *l);
#endif
