/* A stand-in for the Windows headers audio/drivers/wasapi.c includes,
 * for the pacing harness: the types, constants and COM vtable shapes
 * the driver uses, over a fake device that runs on a real-time clock
 * and counts every period nobody answered. The driver's text is
 * compiled unmodified except for its three include lines. */

#ifndef FAKE_WASAPI_H
#define FAKE_WASAPI_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <boolean.h>

/* --- Windows scalar types ------------------------------------------ */
typedef int32_t  HRESULT;
typedef int32_t  LONG;
typedef uint32_t DWORD;
typedef uint32_t UINT32;
typedef uint16_t WORD;
typedef uint8_t  BYTE;
typedef int      BOOL;
typedef int64_t  REFERENCE_TIME;
typedef void    *HANDLE;
typedef uint16_t WCHAR;
typedef WCHAR   *LPWSTR;
#define TRUE 1
#define FALSE 0
#define S_OK       ((HRESULT)0)
#define S_FALSE    ((HRESULT)1)
#define E_FAIL     ((HRESULT)0x80004005)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define FAILED(hr)    ((hr) < 0)
#define SUCCEEDED(hr) ((hr) >= 0)
#define INFINITE      0xFFFFFFFFu
#define WAIT_OBJECT_0 0u
#define WAIT_TIMEOUT  258u
#define WAIT_FAILED   0xFFFFFFFFu
#define CLSCTX_ALL    23

typedef struct { uint32_t Data1; uint16_t Data2, Data3; uint8_t Data4[8]; } GUID;
typedef const GUID *REFIID;
extern const GUID IID_IAudioClient, IID_IAudioRenderClient, IID_IAudioCaptureClient,
       mmdevice_IID_IAudioClient3, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, KSDATAFORMAT_SUBTYPE_PCM;

/* --- Wave formats ---------------------------------------------------- */
#define WAVE_FORMAT_PCM        1
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE
#define KSAUDIO_SPEAKER_MONO   0x4
#define KSAUDIO_SPEAKER_STEREO 0x3
typedef struct {
   WORD wFormatTag, nChannels; DWORD nSamplesPerSec, nAvgBytesPerSec;
   WORD nBlockAlign, wBitsPerSample, cbSize;
} WAVEFORMATEX;
typedef struct {
   WAVEFORMATEX Format;
   union { WORD wValidBitsPerSample, wSamplesPerBlock, wReserved; } Samples;
   DWORD dwChannelMask; GUID SubFormat;
} WAVEFORMATEXTENSIBLE;

/* --- AUDCLNT --------------------------------------------------------- */
typedef enum { AUDCLNT_SHAREMODE_SHARED = 0, AUDCLNT_SHAREMODE_EXCLUSIVE = 1 } AUDCLNT_SHAREMODE;
#define AUDCLNT_STREAMFLAGS_EVENTCALLBACK 0x00040000
#define AUDCLNT_STREAMFLAGS_NOPERSIST     0x00080000
#define AUDCLNT_BUFFERFLAGS_SILENT        0x2
#define AUDCLNT_ERR(n) ((HRESULT)(0x88890000 | (n)))
#define AUDCLNT_E_NOT_INITIALIZED          AUDCLNT_ERR(0x01)
#define AUDCLNT_E_ALREADY_INITIALIZED      AUDCLNT_ERR(0x02)
#define AUDCLNT_E_DEVICE_IN_USE            AUDCLNT_ERR(0x0a)
#define AUDCLNT_E_UNSUPPORTED_FORMAT       AUDCLNT_ERR(0x08)
#define AUDCLNT_E_NOT_STOPPED              AUDCLNT_ERR(0x05)
#define AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED AUDCLNT_ERR(0x0e)
#define AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED  AUDCLNT_ERR(0x19)
#define AUDCLNT_E_BUFFER_SIZE_ERROR        AUDCLNT_ERR(0x16)
#define AUDCLNT_E_INVALID_DEVICE_PERIOD    AUDCLNT_ERR(0x20)
#define AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL AUDCLNT_ERR(0x13)
#define AUDCLNT_E_ENGINE_PERIODICITY_LOCKED AUDCLNT_ERR(0x028)
#define AUDCLNT_E_ENGINE_FORMAT_LOCKED AUDCLNT_ERR(0x029)

/* --- COM objects: the vtable shapes the driver's macros dereference --- */
typedef struct IAudioClient IAudioClient;
typedef struct IAudioClient3 IAudioClient3;
typedef struct IAudioRenderClient IAudioRenderClient;
typedef struct IMMDevice IMMDevice;

typedef struct IAudioClientVtbl {
   HRESULT (*QueryInterface)(IAudioClient *, REFIID, void **);
   DWORD   (*AddRef)(IAudioClient *);
   DWORD   (*Release)(IAudioClient *);
   HRESULT (*Initialize)(IAudioClient *, AUDCLNT_SHAREMODE, DWORD, REFERENCE_TIME, REFERENCE_TIME, const WAVEFORMATEX *, const GUID *);
   HRESULT (*GetBufferSize)(IAudioClient *, UINT32 *);
   HRESULT (*GetStreamLatency)(IAudioClient *, REFERENCE_TIME *);
   HRESULT (*GetCurrentPadding)(IAudioClient *, UINT32 *);
   HRESULT (*IsFormatSupported)(IAudioClient *, AUDCLNT_SHAREMODE, const WAVEFORMATEX *, WAVEFORMATEX **);
   HRESULT (*GetMixFormat)(IAudioClient *, WAVEFORMATEX **);
   HRESULT (*GetDevicePeriod)(IAudioClient *, REFERENCE_TIME *, REFERENCE_TIME *);
   HRESULT (*Start)(IAudioClient *);
   HRESULT (*Stop)(IAudioClient *);
   HRESULT (*Reset)(IAudioClient *);
   HRESULT (*SetEventHandle)(IAudioClient *, HANDLE);
   HRESULT (*GetService)(IAudioClient *, REFIID, void **);
} IAudioClientVtbl;
struct IAudioClient { const IAudioClientVtbl *lpVtbl; void *fake; };

typedef struct IAudioRenderClientVtbl {
   HRESULT (*QueryInterface)(IAudioRenderClient *, REFIID, void **);
   DWORD   (*AddRef)(IAudioRenderClient *);
   DWORD   (*Release)(IAudioRenderClient *);
   HRESULT (*GetBuffer)(IAudioRenderClient *, UINT32, BYTE **);
   HRESULT (*ReleaseBuffer)(IAudioRenderClient *, UINT32, DWORD);
} IAudioRenderClientVtbl;
struct IAudioRenderClient { const IAudioRenderClientVtbl *lpVtbl; void *fake; };

typedef struct IMMDeviceVtbl {
   HRESULT (*QueryInterface)(IMMDevice *, REFIID, void **);
   DWORD   (*AddRef)(IMMDevice *);
   DWORD   (*Release)(IMMDevice *);
   HRESULT (*Activate)(IMMDevice *, REFIID, DWORD, void *, void **);
} IMMDeviceVtbl;
struct IMMDevice { const IMMDeviceVtbl *lpVtbl; void *fake; };

/* The driver's wrapper macros, the C branch of mmdevice_common_inline.h. */
#define RELEASE(x) do { if (x) { (x)->lpVtbl->Release(x); (x) = NULL; } } while (0)
#define _IAudioClient_Start(This)                       ((This)->lpVtbl->Start(This))
#define _IAudioClient_Stop(This)                        ((This)->lpVtbl->Stop(This))
#define _IAudioClient_GetCurrentPadding(This,p)         ((This)->lpVtbl->GetCurrentPadding(This,p))
#define _IAudioRenderClient_GetBuffer(This,n,pp)        ((This)->lpVtbl->GetBuffer(This,n,pp))
#define _IAudioRenderClient_ReleaseBuffer(This,n,f)     ((This)->lpVtbl->ReleaseBuffer(This,n,f))
#define _IAudioClient_GetService(This,riid,ppv)         ((This)->lpVtbl->GetService(This,&(riid),ppv))
#define _IAudioClient_SetEventHandle(This,h)            ((This)->lpVtbl->SetEventHandle(This,h))
#define _IAudioClient_GetBufferSize(This,p)             ((This)->lpVtbl->GetBufferSize(This,p))
#define _IAudioClient_GetStreamLatency(This,p)          ((This)->lpVtbl->GetStreamLatency(This,p))
#define _IAudioClient_GetDevicePeriod(This,a,b)         ((This)->lpVtbl->GetDevicePeriod(This,a,b))
#define _IAudioClient_Initialize(This,m,f,d,p,fmt,g)    ((This)->lpVtbl->Initialize(This,m,f,d,p,fmt,g))
#define _IAudioClient_QueryInterface(This,riid,ppv)     ((This)->lpVtbl->QueryInterface(This,riid,ppv))
#define _IAudioClient_IsFormatSupported(This,m,fmt,pp)  ((This)->lpVtbl->IsFormatSupported(This,m,fmt,pp))
#define _IMMDevice_Activate(This,iid,c,pa,ppv)          ((This)->lpVtbl->Activate(This,&(iid),c,pa,ppv))
/* IAudioClient3: the Windows 10 interface with selectable shared-mode
 * engine periods. Offered when configured to be; the fake's vtable
 * holds only what the driver's macros dereference. */
#define __IAudioClient3_INTERFACE_DEFINED__ 1
typedef struct IAudioClient3Vtbl {
   HRESULT (*QueryInterface)(IAudioClient3 *, REFIID, void **);
   DWORD   (*AddRef)(IAudioClient3 *);
   DWORD   (*Release)(IAudioClient3 *);
   HRESULT (*GetSharedModeEnginePeriod)(IAudioClient3 *, const WAVEFORMATEX *, UINT32 *, UINT32 *, UINT32 *, UINT32 *);
   HRESULT (*GetCurrentSharedModeEnginePeriod)(IAudioClient3 *, WAVEFORMATEX **, UINT32 *);
   HRESULT (*InitializeSharedAudioStream)(IAudioClient3 *, DWORD, UINT32, const WAVEFORMATEX *, const GUID *);
} IAudioClient3Vtbl;
struct IAudioClient3 { const IAudioClient3Vtbl *lpVtbl; void *fake; };
#define _IAudioClient3_GetSharedModeEnginePeriod(This,f,d,fu,mn,mx) ((This)->lpVtbl->GetSharedModeEnginePeriod(This,f,d,fu,mn,mx))
#define _IAudioClient3_GetCurrentSharedModeEnginePeriod(This,pf,p)   ((This)->lpVtbl->GetCurrentSharedModeEnginePeriod(This,pf,p))
#define _IAudioClient3_InitializeSharedAudioStream(This,fl,p,f,g)     ((This)->lpVtbl->InitializeSharedAudioStream(This,fl,p,f,g))
#define _IAudioClient3_Release(This)                                  ((This)->lpVtbl->Release(This))

/* --- Win32 calls ----------------------------------------------------- */
HANDLE CreateEventA(void *sa, BOOL manual, BOOL initial, const char *name);
BOOL   CloseHandle(HANDLE h);
BOOL   SetEvent(HANDLE h);
DWORD  WaitForSingleObject(HANDLE h, DWORD ms);
void   Sleep(DWORD ms);
void   CoTaskMemFree(void *p);

/* --- mmdevice_common.h ----------------------------------------------- */
bool        mmdevice_com_init(void);
void        mmdevice_com_uninit(bool init);
void       *mmdevice_init_device(const char *id, unsigned data_flow);
const char *mmdevice_hresult_name(int hr);
void       *mmdevice_list_new(const void *u, unsigned data_flow);
char       *mmdevice_name(void *data);
void        mmdevice_thread(void *data);
#define WASAPI_SH_BUFFER_AUDIO_LATENCY 0
#define WASAPI_SH_BUFFER_DEVICE_PERIOD 32
#define WASAPI_SH_BUFFER_CLIENT_BUFFER 64

/* --- The fake device's instrumentation, read by the harness --------- */
typedef struct
{
   unsigned periods;            /* device periods elapsed since Start */
   unsigned periods_unanswered; /* of those, with no buffer released for them */
   unsigned buffers_released;   /* ReleaseBuffer calls */
   unsigned frames_consumed;    /* frames the device took from released buffers */
   unsigned period_frames;      /* the exclusive period, frames */
   unsigned buffer_frames;      /* the endpoint buffer, frames */
   unsigned share_mode;         /* 0 shared, 1 exclusive */
   REFERENCE_TIME period_hns;   /* as initialised */
} fake_device_stats_t;
void fake_device_configure(unsigned rate, REFERENCE_TIME min_period_hns,
      REFERENCE_TIME default_period_hns, bool accept_float);
/* IAudioClient3: offered when engine_min_frames > 0, with that minimum
 * shared-mode period; when locked_period_frames > 0 another stream
 * holds the engine there and any other period is refused with
 * AUDCLNT_E_ENGINE_PERIODICITY_LOCKED. */
void fake_device_configure_engine(unsigned engine_min_frames, unsigned locked_period_frames);
void fake_device_stats(fake_device_stats_t *out);
/* Outstanding mmdevice_com_init() references; zero once every driver
 * instance opened in the test has been freed. */
int  fake_com_refs(void);

/* The device-notification thread the driver starts: the fake gives it
 * nothing to do, and the driver posts it WM_QUIT on stop. */
extern DWORD IMMNotificationThreadId;
#define WM_QUIT 0x12
BOOL PostThreadMessage(DWORD id, unsigned msg, unsigned long wp, long lp);

/* Error-string plumbing the driver's wasapi_error() uses. */
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x200
#define FORMAT_MESSAGE_FROM_SYSTEM    0x1000
#define LANG_ENGLISH 9
#define SUBLANG_DEFAULT 1
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))
typedef char *LPSTR;
DWORD FormatMessageA(DWORD flags, const void *src, DWORD id, DWORD lang, LPSTR buf, DWORD size, void *args);
DWORD GetLastError(void);
#define THREAD_PRIORITY_TIME_CRITICAL 15
HANDLE GetCurrentThread(void);
BOOL SetThreadPriority(HANDLE h, int prio);

#endif
