#pragma once
#include <wut.h>
#include "time.h"

/**
 * \defgroup coreinit_taskq Task Queue
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MPTask MPTask;
typedef struct MPTaskInfo MPTaskInfo;
typedef struct MPTaskQueue MPTaskQueue;
typedef struct MPTaskQueueInfo MPTaskQueueInfo;

typedef uint32_t (*MPTaskFunc)(uint32_t, uint32_t);

typedef enum MPTaskState
{
   MP_TASK_STATE_INITIALISED           = 1 << 0,
   MP_TASK_STATE_READY                 = 1 << 1,
   MP_TASK_STATE_RUNNING               = 1 << 2,
   MP_TASK_STATE_FINISHED              = 1 << 3,
} MPTaskState;

typedef enum MPTaskQueueState
{
   MP_TASK_QUEUE_STATE_INITIALISED     = 1 << 0,
   MP_TASK_QUEUE_STATE_READY           = 1 << 1,
   MP_TASK_QUEUE_STATE_STOPPING        = 1 << 2,
   MP_TASK_QUEUE_STATE_STOPPED         = 1 << 3,
   MP_TASK_QUEUE_STATE_FINISHED        = 1 << 4,
} MPTaskQueueState;

#pragma pack(push, 1)
struct MPTaskInfo
{
   MPTaskState state;
   uint32_t result;
   uint32_t coreID;
   OSTime duration;
};
#pragma pack(pop)
CHECK_OFFSET(MPTaskInfo, 0x00, state);
CHECK_OFFSET(MPTaskInfo, 0x04, result);
CHECK_OFFSET(MPTaskInfo, 0x08, coreID);
CHECK_OFFSET(MPTaskInfo, 0x0C, duration);
CHECK_SIZE(MPTaskInfo, 0x14);

#pragma pack(push, 1)
struct MPTask
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
};
#pragma pack(pop)
CHECK_OFFSET(MPTask, 0x00, self);
CHECK_OFFSET(MPTask, 0x04, queue);
CHECK_OFFSET(MPTask, 0x08, state);
CHECK_OFFSET(MPTask, 0x0C, func);
CHECK_OFFSET(MPTask, 0x10, userArg1);
CHECK_OFFSET(MPTask, 0x14, userArg2);
CHECK_OFFSET(MPTask, 0x18, result);
CHECK_OFFSET(MPTask, 0x1C, coreID);
CHECK_OFFSET(MPTask, 0x20, duration);
CHECK_OFFSET(MPTask, 0x28, userData);
CHECK_SIZE(MPTask, 0x2C);

struct MPTaskQueueInfo
{
   MPTaskQueueState state;
   uint32_t tasks;
   uint32_t tasksReady;
   uint32_t tasksRunning;
   uint32_t tasksFinished;
};
CHECK_OFFSET(MPTaskQueueInfo, 0x00, state);
CHECK_OFFSET(MPTaskQueueInfo, 0x04, tasks);
CHECK_OFFSET(MPTaskQueueInfo, 0x08, tasksReady);
CHECK_OFFSET(MPTaskQueueInfo, 0x0C, tasksRunning);
CHECK_OFFSET(MPTaskQueueInfo, 0x10, tasksFinished);
CHECK_SIZE(MPTaskQueueInfo, 0x14);

struct MPTaskQueue
{
   MPTaskQueue *self;
   MPTaskQueueState state;
   uint32_t tasks;
   uint32_t tasksReady;
   uint32_t tasksRunning;
   UNKNOWN(4);
   uint32_t tasksFinished;
   UNKNOWN(8);
   uint32_t queueIndex;
   UNKNOWN(8);
   uint32_t queueSize;
   UNKNOWN(4);
   MPTask **queue;
   uint32_t queueMaxSize;
   OSSpinLock lock;
};
CHECK_OFFSET(MPTaskQueue, 0x00, self);
CHECK_OFFSET(MPTaskQueue, 0x04, state);
CHECK_OFFSET(MPTaskQueue, 0x08, tasks);
CHECK_OFFSET(MPTaskQueue, 0x0C, tasksReady);
CHECK_OFFSET(MPTaskQueue, 0x10, tasksRunning);
CHECK_OFFSET(MPTaskQueue, 0x18, tasksFinished);
CHECK_OFFSET(MPTaskQueue, 0x24, queueIndex);
CHECK_OFFSET(MPTaskQueue, 0x30, queueSize);
CHECK_OFFSET(MPTaskQueue, 0x38, queue);
CHECK_OFFSET(MPTaskQueue, 0x3C, queueMaxSize);
CHECK_OFFSET(MPTaskQueue, 0x40, lock);
CHECK_SIZE(MPTaskQueue, 0x50);

void
MPInitTaskQ(MPTaskQueue *queue,
            MPTask **queueBuffer,
            uint32_t queueBufferLen);

BOOL
MPTermTaskQ(MPTaskQueue *queue);

BOOL
MPGetTaskQInfo(MPTaskQueue *queue,
               MPTaskQueueInfo *info);

BOOL
MPStartTaskQ(MPTaskQueue *queue);

BOOL
MPStopTaskQ(MPTaskQueue *queue);

BOOL
MPResetTaskQ(MPTaskQueue *queue);

BOOL
MPEnqueTask(MPTaskQueue *queue,
            MPTask *task);

MPTask *
MPDequeTask(MPTaskQueue *queue);

uint32_t
MPDequeTasks(MPTaskQueue *queue,
             MPTask **queueBuffer,
             uint32_t queueBufferLen);

BOOL
MPWaitTaskQ(MPTaskQueue *queue,
            MPTaskQueueState mask);

BOOL
MPWaitTaskQWithTimeout(MPTaskQueue *queue,
                       MPTaskQueueState wmask,
                       OSTime timeout);

BOOL
MPPrintTaskQStats(MPTaskQueue *queue,
                  uint32_t unk);

void
MPInitTask(MPTask *task,
           MPTaskFunc func,
           uint32_t userArg1,
           uint32_t userArg2);

BOOL
MPTermTask(MPTask* task);

BOOL
MPGetTaskInfo(MPTask *task,
              MPTaskInfo *info);

void *
MPGetTaskUserData(MPTask *task);

void
MPSetTaskUserData(MPTask *task,
                  void *userData);

BOOL
MPRunTasksFromTaskQ(MPTaskQueue *queue,
                    uint32_t count);

BOOL
MPRunTask(MPTask *task);

#ifdef __cplusplus
}
#endif

/** @} */
