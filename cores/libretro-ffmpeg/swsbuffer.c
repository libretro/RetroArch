#include <libavformat/avformat.h>

#include <rthreads/rthreads.h>

#include "swsbuffer.h"

enum kbStatus
{
  KB_OPEN,
  KB_IN_PROGRESS,
  KB_FINISHED
};

struct swsbuffer
{
   sws_context_t *buffer;
   enum kbStatus *status;
   size_t size;
   slock_t *lock;
   int64_t head;
   int64_t tail;
};

swsbuffer_t *swsbuffer_create(size_t num, int frame_size, int width, int height)
{
   swsbuffer_t *b = malloc(sizeof (swsbuffer_t));
   if (b == NULL)
      return NULL;

   b->status = malloc(sizeof(enum kbStatus) * num);
   if (b->status == NULL)
      return NULL;
   for (int i = 0; i < num; i++)
      b->status[i] = KB_OPEN;

   b->lock = slock_new();
   if (b->lock == NULL)
      return NULL;

   b->buffer = malloc(sizeof(sws_context_t) * num);
   if (b->buffer == NULL)
      return NULL;
   for (int i = 0; i < num; i++)
   {
      b->buffer[i].index = i;
      b->buffer[i].sws = sws_alloc_context();
      b->buffer[i].source = av_frame_alloc();
#if LIBAVUTIL_VERSION_MAJOR > 55
      b->buffer[i].hw_source = av_frame_alloc();
#endif
      b->buffer[i].target = av_frame_alloc();
      b->buffer[i].frame_buf = av_malloc(frame_size);

      avpicture_fill((AVPicture*)b->buffer[i].target, (const uint8_t*)b->buffer[i].frame_buf,
            PIX_FMT_RGB32, width, height);
   }

   b->size = num;
   b->head = 0;
   b->tail = 0;
   return b;
}

void swsbuffer_destroy(swsbuffer_t *swsbuffer)
{
   if (swsbuffer != NULL) {
      slock_free(swsbuffer->lock);
      free(swsbuffer->status);
      for (int i = 0; i < swsbuffer->size; i++)
      {
#if LIBAVUTIL_VERSION_MAJOR > 55
         av_frame_free(&swsbuffer->buffer[i].hw_source);
#endif
         av_frame_free(&swsbuffer->buffer[i].source);
         av_frame_free(&swsbuffer->buffer[i].target);
         av_freep(&swsbuffer->buffer[i].frame_buf);
         sws_freeContext(swsbuffer->buffer[i].sws);
      }
      free(swsbuffer->buffer);
      free(swsbuffer);
   }
}

void swsbuffer_clear(swsbuffer_t *swsbuffer)
{
   slock_lock(swsbuffer->lock);

   swsbuffer->head = 0;
   swsbuffer->tail = 0;
   for (int i = 0; i < swsbuffer->size; i++)
      swsbuffer->status[i] = KB_OPEN;

   slock_unlock(swsbuffer->lock);
}

void swsbuffer_get_open_slot(swsbuffer_t *swsbuffer, sws_context_t **context)
{
   slock_lock(swsbuffer->lock);

   if (swsbuffer->status[swsbuffer->head] == KB_OPEN)
   {
      *context = &swsbuffer->buffer[swsbuffer->head];
      swsbuffer->status[swsbuffer->head] = KB_IN_PROGRESS;
      swsbuffer->head++;
      swsbuffer->head %= swsbuffer->size;
   }

   slock_unlock(swsbuffer->lock);
}

void swsbuffer_return_open_slot(swsbuffer_t *swsbuffer, sws_context_t *context)
{
   slock_lock(swsbuffer->lock);

   if (swsbuffer->status[context->index] == KB_IN_PROGRESS)
   {
      swsbuffer->status[context->index] = KB_OPEN;
      swsbuffer->head--;
      swsbuffer->head %= swsbuffer->size;
   }

   slock_unlock(swsbuffer->lock);
}

void swsbuffer_open_slot(swsbuffer_t *swsbuffer, sws_context_t *context)
{
   slock_lock(swsbuffer->lock);

   if (swsbuffer->status[context->index] == KB_FINISHED)
   {
      swsbuffer->status[context->index] = KB_OPEN;
      swsbuffer->tail++;
      swsbuffer->tail %= (swsbuffer->size);
   }

   slock_unlock(swsbuffer->lock);
}

void swsbuffer_get_finished_slot(swsbuffer_t *swsbuffer, sws_context_t **context)
{
   slock_lock(swsbuffer->lock);

   if (swsbuffer->status[swsbuffer->tail] == KB_FINISHED)
      *context = &swsbuffer->buffer[swsbuffer->tail];

   slock_unlock(swsbuffer->lock);
}

void swsbuffer_finish_slot(swsbuffer_t *swsbuffer, sws_context_t *context)
{
   slock_lock(swsbuffer->lock);

   if (swsbuffer->status[context->index] == KB_IN_PROGRESS)
      swsbuffer->status[context->index] = KB_FINISHED;

   slock_unlock(swsbuffer->lock);
}

bool swsbuffer_has_open_slot(swsbuffer_t *swsbuffer)
{
   bool ret = false;

   slock_lock(swsbuffer->lock);

   if (swsbuffer->status[swsbuffer->head] == KB_OPEN)
      ret = true;

   slock_unlock(swsbuffer->lock);

   return ret;
}

bool swsbuffer_has_finished_slot(swsbuffer_t *swsbuffer)
{
   bool ret = false;

   slock_lock(swsbuffer->lock);

   if (swsbuffer->status[swsbuffer->tail] == KB_FINISHED)
      ret = true;

   slock_unlock(swsbuffer->lock);

   return ret;
}
