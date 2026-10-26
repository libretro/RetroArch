#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ProcUISaveCallback)(void);
typedef uint32_t (*ProcUISaveCallbackEx)(void *);
typedef uint32_t (*ProcUICallback)(void *);

typedef enum ProcUIStatus
{
   PROCUI_STATUS_IN_FOREGROUND,
   PROCUI_STATUS_IN_BACKGROUND,
   PROCUI_STATUS_RELEASE_FOREGROUND,
   PROCUI_STATUS_EXITING,
} ProcUIStatus;

uint32_t ProcUICalcMemorySize(uint32_t unk);
void ProcUIClearCallbacks();
void ProcUIDrawDoneRelease();
BOOL ProcUIInForeground();
BOOL ProcUIInShutdown();
void ProcUIInit(ProcUISaveCallback saveCallback);
void ProcUIInitEx(ProcUISaveCallbackEx saveCallback, void *arg);
BOOL ProcUIIsRunning();
ProcUIStatus ProcUIProcessMessages(BOOL block);
void ProcUISetSaveCallback(ProcUISaveCallbackEx saveCallback, void *arg);
void ProcUIShutdown();
ProcUIStatus ProcUISubProcessMessages(BOOL block);

#ifdef __cplusplus
}
#endif
