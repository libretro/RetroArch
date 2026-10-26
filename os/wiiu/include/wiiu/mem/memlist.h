#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMMemoryLink
{
   void *prev;
   void *next;
} MEMMemoryLink;

typedef struct MEMMemoryList
{
   void *head;
   void *tail;
   uint16_t count;
   uint16_t offsetToMemoryLink;
} MEMMemoryList;

void MEMInitList(MEMMemoryList *list, uint16_t offsetToMemoryLink);
void MEMAppendListObject(MEMMemoryList *list, void *object);
void MEMPrependListObject(MEMMemoryList *list, void *object);
void MEMInsertListObject(MEMMemoryList *list, void *before, void *object);
void MEMRemoveListObject(MEMMemoryList *list, void *object);
void *MEMGetNextListObject(MEMMemoryList *list, void *object);
void *MEMGetPrevListObject(MEMMemoryList *list, void *object);
void *MEMGetNthListObject(MEMMemoryList *list, uint16_t n);

#ifdef __cplusplus
}
#endif
