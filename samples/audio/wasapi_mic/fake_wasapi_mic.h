/* The capture side of the fake WASAPI: the shared fake's types and
 * macros, and an IAudioCaptureClient whose packets the test scripts. */
#ifndef FAKE_WASAPI_MIC_H
#define FAKE_WASAPI_MIC_H

#include "../wasapi_exclusive_pacing/fake_wasapi.h"

typedef struct IAudioCaptureClient IAudioCaptureClient;
typedef struct IAudioCaptureClientVtbl {
   HRESULT (*QueryInterface)(IAudioCaptureClient *, REFIID, void **);
   DWORD   (*AddRef)(IAudioCaptureClient *);
   DWORD   (*Release)(IAudioCaptureClient *);
   HRESULT (*GetBuffer)(IAudioCaptureClient *, BYTE **, UINT32 *, DWORD *, UINT64 *, UINT64 *);
   HRESULT (*ReleaseBuffer)(IAudioCaptureClient *, UINT32);
   HRESULT (*GetNextPacketSize)(IAudioCaptureClient *, UINT32 *);
} IAudioCaptureClientVtbl;
struct IAudioCaptureClient { const IAudioCaptureClientVtbl *lpVtbl; void *fake; };

#define _IAudioCaptureClient_GetBuffer(This,pp,n,f,p,q)   ((This)->lpVtbl->GetBuffer(This,pp,n,f,p,q))
#define _IAudioCaptureClient_ReleaseBuffer(This,n)        ((This)->lpVtbl->ReleaseBuffer(This,n))
#define _IAudioCaptureClient_GetNextPacketSize(This,n)    ((This)->lpVtbl->GetNextPacketSize(This,n))

#endif
