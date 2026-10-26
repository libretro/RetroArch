#pragma once
#include <wiiu/types.h>
#include <wiiu/os/time.h>
#include <wiiu/os/spinlock.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
   MP_TASK_STATE_INITIALISED           = 1 << 0,
   MP_TASK_STATE_READY                 = 1 << 1,
   MP_TASK_STATE_RUNNING               = 1 << 2,
   MP_TASK_STATE_FINISHED              = 1 << 3,
} MPTaskState;

typedef enum
{
   MP_TASK_QUEUE_STATE_INITIALISED     = 1 << 0,
   MP_TASK_QUEUE_STATE_READY           = 1 << 1,
   MP_TASK_QUEUE_STATE_STOPPING        = 1 << 2,
   MP_TASK_QUEUE_STATE_STOPPED         = 1 << 3,
   MP_TASK_QUEUE_STATE_FINISHED        = 1 << 4,
} MPTaskQueueState;

typedef uint32_t (*MPTaskFunc)(uint32_t, uint32_t);

#pragma pack(push, 1)
typedef struct
{
   MPTaskState state;
   uint32_t result;
   uint32_t coreID;
   OSTime duration;
} MPTaskInfo;
#pragma pack(pop)

typedef struct MPTask MPTask;
typedef struct MPTaskQueue MPTaskQueue;

#pragma pack(push, 1)
typedef struct MPTask
{
   MPTask *self;
   MPTaskQueue *queue;
   MPTaskState state;
   MPTaskFunc func;
   uint32_t userArg1;
   uint32_t userArg2;
   uint32_t result;
   uint32_t coreID;
   OSTime duration;
   void *userData;
}MPTask;
#pragma pack(pop)

typedef struct
{
   MPTaskQueueState state;
   uint32_t tasks;
   uint32_t tasksReady;
   uint32_t tasksRunning;
   uint32_t tasksFinished;
}MPTaskQueueInfo;

typedef struct MPTaskQueue
{
   MPTaskQueue *self;
   MPTaskQueueState state;
   uint32_t tasks;
   uint32_t tasksReady;
   uint32_t tasksRunning;
   uint32_t __unknown0;
   uint32_t tasksFinished;
   uint32_t __unknown1;
   uint32_t __unknown2;
   uint32_t queueIndex;
   uint32_t __unknown3;
   uint32_t __unknown4;
   uint32_t queueSize;
   uint32_t __unknown5;
   MPTask **queue;
   uint32_t queueMaxSize;
   OSSpinLock lock;
}MPTaskQueue;

void MPInitTaskQ(MPTaskQueue *queue, MPTask **queueBuffer, uint32_t queueBufferLen);
BOOL MPTermTaskQ(MPTaskQueue *queue);
BOOL MPGetTaskQInfo(MPTaskQueue *queue, MPTaskQueueInfo *info);
BOOL MPStartTaskQ(MPTaskQueue *queue);
BOOL MPStopTaskQ(MPTaskQueue *queue);
BOOL MPResetTaskQ(MPTaskQueue *queue);
BOOL MPEnqueTask(MPTaskQueue *queue, MPTask *task);
MPTask *MPDequeTask(MPTaskQueue *queue);
uint32_t MPDequeTasks(MPTaskQueue *queue, MPTask **queueBuffer, uint32_t queueBufferLen);
BOOL MPWaitTaskQ(MPTaskQueue *queue, MPTaskQueueState mask);
BOOL MPWaitTaskQWithTimeout(MPTaskQueue *queue, MPTaskQueueState wmask, OSTime timeout);
BOOL MPPrintTaskQStats(MPTaskQueue *queue, uint32_t unk);
void MPInitTask(MPTask *task, MPTaskFunc func, uint32_t userArg1, uint32_t userArg2);
BOOL MPTermTask(MPTask *task);
BOOL MPGetTaskInfo(MPTask *task, MPTaskInfo *info);
void *MPGetTaskUserData(MPTask *task);
void MPSetTaskUserData(MPTask *task, void *userData);
BOOL MPRunTasksFromTaskQ(MPTaskQueue *queue, uint32_t count);
BOOL MPRunTask(MPTask *task);

#ifdef __cplusplus
}
#endif
