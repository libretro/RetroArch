/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define VCOS_LOG_CATEGORY (&vcos_named_sem_log_cat)

#include "interface/vcos/vcos.h"
#include "interface/vcos/generic/vcos_generic_named_sem.h"
#include "interface/vcos/vcos_blockpool.h"

#if defined(VCOS_LOGGING_ENABLED)
static VCOS_LOG_CAT_T vcos_named_sem_log_cat =
VCOS_LOG_INIT("vcos_named_sem", VCOS_LOG_ERROR);
#endif

/**
  * \file
  *
  * Named semaphores, primarily for VCFW.
  *
  * Does not actually work across processes; merely emulate the API.
  *
  * The client initialises a VCOS_NAMED_SEMAPHORE_T, but this merely
  * points at the real underlying VCOS_NAMED_SEMAPHORE_IMPL_T.
  *
  *      semaphore_t  ---\
  *                       ----- semaphore_impl_t
  *      semaphore_t  ---/
  *                     /
  *      semaphore_t  -/
  *
  */

/* Maintain a block pool of semaphore implementations */
#define NUM_SEMS  16

/* Allow the pool to expand to MAX_SEMS in size */
#define MAX_SEMS  512

/** Each actual real semaphore is stored in one of these. Clients just
  * get a structure with a pointer to this in it.
  *
  * It also contains a doubly linked list tracking the semaphores in-use.
  */
typedef struct VCOS_NAMED_SEMAPHORE_IMPL_T
{
   VCOS_SEMAPHORE_T sem;                     /**< Actual underlying semaphore */
   char name[VCOS_NAMED_SEMAPHORE_NAMELEN];  /**< Name of semaphore, copied */
   unsigned refs;                            /**< Reference count */
   struct VCOS_NAMED_SEMAPHORE_IMPL_T *next; /**< Next in the in-use list   */
   struct VCOS_NAMED_SEMAPHORE_IMPL_T *prev; /**< Previous in the in-use list */
} VCOS_NAMED_SEMAPHORE_IMPL_T;

static VCOS_MUTEX_T lock;
static VCOS_NAMED_SEMAPHORE_IMPL_T* sems_in_use = NULL;
static int sems_in_use_count = 0;
static int sems_total_ref_count = 0;

static VCOS_BLOCKPOOL_T sems_pool;
static char pool_mem[VCOS_BLOCKPOOL_SIZE(
      NUM_SEMS, sizeof(VCOS_NAMED_SEMAPHORE_IMPL_T), VCOS_BLOCKPOOL_ALIGN_DEFAULT)];

VCOS_STATUS_T _vcos_named_semaphore_init()
{
   VCOS_STATUS_T status;

   status = vcos_blockpool_init(&sems_pool,
         NUM_SEMS, sizeof(VCOS_NAMED_SEMAPHORE_IMPL_T),
         pool_mem, sizeof(pool_mem),
         VCOS_BLOCKPOOL_ALIGN_DEFAULT, 0, "vcos named semaphores");

   if (status != VCOS_SUCCESS)
      goto fail_blockpool;

   status = vcos_blockpool_extend(&sems_pool, VCOS_BLOCKPOOL_MAX_SUBPOOLS - 1,
         (MAX_SEMS - NUM_SEMS) / (VCOS_BLOCKPOOL_MAX_SUBPOOLS - 1));
   if (status != VCOS_SUCCESS)
      goto fail_extend;

   status = vcos_mutex_create(&lock, "vcosnmsem");
   if (status != VCOS_SUCCESS)
      goto fail_mutex;

   return status;

fail_mutex:
fail_extend:
   vcos_blockpool_delete(&sems_pool);
fail_blockpool:
   return status;
}

void _vcos_named_semaphore_deinit(void)
{
   vcos_blockpool_delete(&sems_pool);
   vcos_mutex_delete(&lock);
   sems_in_use = NULL;
}

VCOS_STATUS_T
vcos_generic_named_semaphore_create(VCOS_NAMED_SEMAPHORE_T *sem,
      const char *name, VCOS_UNSIGNED count)
{
   VCOS_STATUS_T status = VCOS_ENOSPC;
   int name_len, cmp = -1;
   VCOS_NAMED_SEMAPHORE_IMPL_T *impl;
   VCOS_NAMED_SEMAPHORE_IMPL_T *new_impl;

   vcos_log_trace("%s: sem %p name %s count %d", __FUNCTION__,
         sem, (name ? name : "null"), count);

   vcos_assert(name);

   vcos_mutex_lock(&lock);
   name_len = vcos_strlen(name);
   if (name_len >= VCOS_NAMED_SEMAPHORE_NAMELEN)
   {
      vcos_assert(0);
      status = VCOS_EINVAL;
      goto end;
   }

   /* do we already have this semaphore? */
   impl = sems_in_use;
   while (impl && (cmp = vcos_strcmp(name, impl->name)) < 0)
      impl = impl->next;

   if (impl && cmp == 0)
   {
      /* Semaphore is already in use so just increase the ref count */
      impl->refs++;
      sems_total_ref_count++;
      sem->actual = impl;
      sem->sem = &impl->sem;
      status = VCOS_SUCCESS;
      vcos_log_trace(
            "%s: ref count %d name %s total refs %d num sems %d",
            __FUNCTION__, impl->refs, impl->name,
            sems_total_ref_count, sems_in_use_count);
      goto end;
   }

   /* search for unused semaphore */
   new_impl = vcos_blockpool_calloc(&sems_pool);
   if (new_impl)
   {
      status = vcos_semaphore_create(&new_impl->sem, name, count);
      if (status == VCOS_SUCCESS)
      {
         new_impl->refs = 1;
         sems_total_ref_count++;
         sems_in_use_count++;
         memcpy(new_impl->name, name, name_len + 1); /* already checked length! */
         sem->actual = new_impl;
         sem->sem = &new_impl->sem;

         /* Insert into the sorted list
          * impl is either NULL or the first element where
          * name > impl->name.
          */
         if (impl)
         {
            new_impl->prev = impl->prev;
            impl->prev = new_impl;
            new_impl->next = impl;

            if (new_impl->prev)
               new_impl->prev->next = new_impl;
         }
         else
         {
            /* Appending to the tail of the list / empty list */
            VCOS_NAMED_SEMAPHORE_IMPL_T *tail = sems_in_use;
            while(tail && tail->next)
               tail = tail->next;

            if (tail)
            {
               tail->next = new_impl;
               new_impl->prev = tail;
            }
         }

         if (sems_in_use == impl)
         {
            /* Inserted at head or list was empty */
            sems_in_use = new_impl;
         }

      vcos_log_trace(
            "%s: new ref actual %p prev %p next %p count %d name %s " \
            "total refs %d num sems %d",
            __FUNCTION__,
            new_impl, new_impl->prev, new_impl->next,
            new_impl->refs, new_impl->name,
            sems_total_ref_count, sems_in_use_count);
      }
   }

end:
   vcos_mutex_unlock(&lock);
   if (status != VCOS_SUCCESS)
   {
      vcos_log_error("%s: failed to create named semaphore name %s status %d " \
            "total refs %d num sems %d",
            __FUNCTION__, (name ? name : "NULL"), status,
            sems_total_ref_count, sems_in_use_count);
   }
   return status;
}

void vcos_named_semaphore_delete(VCOS_NAMED_SEMAPHORE_T *sem)
{
   VCOS_NAMED_SEMAPHORE_IMPL_T *actual = sem->actual;
   vcos_mutex_lock(&lock);

   /* if this fires, the semaphore has already been deleted */
   vcos_assert(actual->refs);

   vcos_log_trace(
         "%s: actual %p ref count %d name %s prev %p next %p total refs %d num sems %d",
         __FUNCTION__, actual, actual->refs, actual->name,
         actual->prev, actual->next,
         sems_total_ref_count, sems_in_use_count);

   sems_total_ref_count--;
   if (--actual->refs == 0)
   {
      sems_in_use_count--;
      if (actual->prev)
         actual->prev->next = actual->next;

      if (actual->next)
         actual->next->prev = actual->prev;

      if (sems_in_use == actual)
         sems_in_use = actual->next;

      vcos_semaphore_delete(&actual->sem);
      sem->actual = NULL;
      sem->sem = NULL;
      vcos_blockpool_free(actual);
   }
   vcos_mutex_unlock(&lock);
}
