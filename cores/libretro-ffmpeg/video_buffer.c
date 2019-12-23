#include <libavformat/avformat.h>

#include <rthreads/rthreads.h>

#include "video_buffer.h"

enum kbStatus
{
  KB_OPEN,
  KB_IN_PROGRESS,
  KB_FINISHED
};

struct video_buffer
{
   video_decoder_context_t *buffer;
   enum kbStatus *status;
   size_t size;
   slock_t *lock;
   scond_t *open_cond;
   scond_t *finished_cond;
   int64_t head;
   int64_t tail;
};

video_buffer_t *video_buffer_create(size_t num, int frame_size, int width, int height)
{
   video_buffer_t *b = malloc(sizeof (video_buffer_t));
   if (!b)
      return NULL;

   b->lock = NULL;
   b->open_cond = NULL;
   b->finished_cond = NULL;
   b->buffer = NULL;
   b->size = num;
   b->head = 0;
   b->tail = 0;

   b->status = malloc(sizeof(enum kbStatus) * num);
   if (!b->status)
      goto fail;
   for (int i = 0; i < num; i++)
      b->status[i] = KB_OPEN;

   b->lock = slock_new();
   b->open_cond = scond_new();
   b->finished_cond = scond_new();
   if (!b->lock || !b->open_cond || !b->finished_cond)
      goto fail;

   b->buffer = malloc(sizeof(video_decoder_context_t) * num);
   if (!b->buffer)
      goto fail;

   for (int i = 0; i < num; i++)
   {
      b->buffer[i].index = i;
      b->buffer[i].pts = 0;
      b->buffer[i].sws = sws_alloc_context();
      b->buffer[i].source = av_frame_alloc();
#if LIBAVUTIL_VERSION_MAJOR > 55
      b->buffer[i].hw_source = av_frame_alloc();
#endif
      b->buffer[i].target = av_frame_alloc();
      b->buffer[i].frame_buf = av_malloc(frame_size);

      avpicture_fill((AVPicture*)b->buffer[i].target, (const uint8_t*)b->buffer[i].frame_buf,
            PIX_FMT_RGB32, width, height);

      if (!b->buffer[i].sws ||
          !b->buffer[i].source ||
          !b->buffer[i].hw_source ||
          !b->buffer[i].target ||
          !b->buffer[i].frame_buf)
         goto fail;
   }
   return b;

fail:
   video_buffer_destroy(b);
   return NULL;
}

void video_buffer_destroy(video_buffer_t *video_buffer)
{
   if (!video_buffer)
      return;

   slock_free(video_buffer->lock);
   scond_free(video_buffer->open_cond);
   scond_free(video_buffer->finished_cond);
   free(video_buffer->status);
   if (video_buffer->buffer)
      for (int i = 0; i < video_buffer->size; i++)
      {
   #if LIBAVUTIL_VERSION_MAJOR > 55
         av_frame_free(&video_buffer->buffer[i].hw_source);
   #endif
         av_frame_free(&video_buffer->buffer[i].source);
         av_frame_free(&video_buffer->buffer[i].target);
         av_freep(&video_buffer->buffer[i].frame_buf);
         sws_freeContext(video_buffer->buffer[i].sws);
      }
   free(video_buffer->buffer);
   free(video_buffer);
}

void video_buffer_clear(video_buffer_t *video_buffer)
{
   if (!video_buffer)
      return;

   slock_lock(video_buffer->lock);

   scond_signal(video_buffer->open_cond);
   scond_signal(video_buffer->finished_cond);

   video_buffer->head = 0;
   video_buffer->tail = 0;
   for (int i = 0; i < video_buffer->size; i++)
      video_buffer->status[i] = KB_OPEN;

   slock_unlock(video_buffer->lock);
}

void video_buffer_get_open_slot(video_buffer_t *video_buffer, video_decoder_context_t **context)
{
   slock_lock(video_buffer->lock);

   if (video_buffer->status[video_buffer->head] == KB_OPEN)
   {
      *context = &video_buffer->buffer[video_buffer->head];
      video_buffer->status[video_buffer->head] = KB_IN_PROGRESS;
      video_buffer->head++;
      video_buffer->head %= video_buffer->size;
   }

   slock_unlock(video_buffer->lock);
}

void video_buffer_return_open_slot(video_buffer_t *video_buffer, video_decoder_context_t *context)
{
   slock_lock(video_buffer->lock);

   if (video_buffer->status[context->index] == KB_IN_PROGRESS)
   {
      video_buffer->status[context->index] = KB_OPEN;
      video_buffer->head--;
      video_buffer->head %= video_buffer->size;
   }

   slock_unlock(video_buffer->lock);
}

void video_buffer_open_slot(video_buffer_t *video_buffer, video_decoder_context_t *context)
{
   slock_lock(video_buffer->lock);

   if (video_buffer->status[context->index] == KB_FINISHED)
   {
      video_buffer->status[context->index] = KB_OPEN;
      video_buffer->tail++;
      video_buffer->tail %= (video_buffer->size);
      scond_signal(video_buffer->open_cond);
   }

   slock_unlock(video_buffer->lock);
}

void video_buffer_get_finished_slot(video_buffer_t *video_buffer, video_decoder_context_t **context)
{
   slock_lock(video_buffer->lock);

   if (video_buffer->status[video_buffer->tail] == KB_FINISHED)
      *context = &video_buffer->buffer[video_buffer->tail];

   slock_unlock(video_buffer->lock);
}

void video_buffer_finish_slot(video_buffer_t *video_buffer, video_decoder_context_t *context)
{
   slock_lock(video_buffer->lock);

   if (video_buffer->status[context->index] == KB_IN_PROGRESS)
   {
      video_buffer->status[context->index] = KB_FINISHED;
      scond_signal(video_buffer->finished_cond);
   }

   slock_unlock(video_buffer->lock);
}

bool video_buffer_wait_for_open_slot(video_buffer_t *video_buffer)
{
   slock_lock(video_buffer->lock);

   while (video_buffer->status[video_buffer->head] != KB_OPEN)
      scond_wait(video_buffer->open_cond, video_buffer->lock);

   slock_unlock(video_buffer->lock);

   return true;
}

bool video_buffer_wait_for_finished_slot(video_buffer_t *video_buffer)
{
   slock_lock(video_buffer->lock);

   while (video_buffer->status[video_buffer->tail] != KB_FINISHED)
      scond_wait(video_buffer->finished_cond, video_buffer->lock);

   slock_unlock(video_buffer->lock);

   return true;
}

bool video_buffer_has_open_slot(video_buffer_t *video_buffer)
{
   bool ret = false;

   slock_lock(video_buffer->lock);

   if (video_buffer->status[video_buffer->head] == KB_OPEN)
      ret = true;

   slock_unlock(video_buffer->lock);

   return ret;
}

bool video_buffer_has_finished_slot(video_buffer_t *video_buffer)
{
   bool ret = false;

   slock_lock(video_buffer->lock);

   if (video_buffer->status[video_buffer->tail] == KB_FINISHED)
      ret = true;

   slock_unlock(video_buffer->lock);

   return ret;
}
