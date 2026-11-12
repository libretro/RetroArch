/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define RECORD_AVF_SESSION_TEST
#define main existing_avf_session_main
#include "av_worker_test.c"
#undef main

struct output_fixture
{
   struct fixture native;
   record_avfoundation_worker_t *pump;
   unsigned steps, cancellations, output_frees, open_failure;
   bool invalid_result, invalid_network;
};
static void *output_open(void *context,void *sample,
      record_gpu_worker_vt_t *video,record_audio_queue_t *audio)
{
   struct output_fixture *f=(struct output_fixture*)context;
   record_avfoundation_worker_ops_t ops;
   assert(sample==&f->native.sample && f->native.packet && !f->native.opens++);
   if (f->open_failure) return NULL;
   ops.video=mux_video; ops.audio=mux_audio; ops.finish=mux_finish;
   f->pump=record_avfoundation_worker_new(video,audio,&f->native,&ops);
   assert(f->pump); return f;
}
static int output_step(void *context)
{
   struct output_fixture *f=(struct output_fixture*)context;
   f->steps++;
   return f->invalid_result?99:record_avfoundation_worker_step(f->pump);
}
static bool output_cancel(void *context)
{
   struct output_fixture *f=(struct output_fixture*)context;
   assert(!f->native.encoder_destroyed);
   record_avfoundation_worker_abort(f->pump);
   return ++f->cancellations>=3;
}
static bool output_free(void *context)
{
   struct output_fixture *f=(struct output_fixture*)context;
   assert(!f->native.encoder_destroyed);
   if (!record_avfoundation_worker_free(f->pump)) return false;
   f->pump=NULL;
   assert(!f->output_frees++); return true;
}
static bool output_retry(const void *context)
{
   const struct output_fixture *f=(const struct output_fixture*)context;
   assert(f && f->pump); return false;
}
static bool output_network(const void *context,int *fd,bool *read,bool *write)
{
   const struct output_fixture *f=(const struct output_fixture*)context;
   assert(f && f->pump);
   *fd=f->invalid_network?-2:17; *read=true; *write=false; return true;
}
static void output_encoder_destroy(void *context)
{
   struct output_fixture *f=(struct output_fixture*)context;
   assert(!f->pump && (f->open_failure || f->output_frees==1 || !f->native.opens));
   session_encoder_destroy(&f->native);
}
static bool output_context_destroy(void *context)
{
   struct output_fixture *f=(struct output_fixture*)context;
   assert(!f->pump); return session_context_destroy(&f->native);
}
static void output_test(unsigned failure)
{
   struct output_fixture f;
   record_gpu_worker_vt_ops_t video_ops;
   record_avfoundation_session_output_ops_t ops;
   record_gpu_queue_t *queue=record_gpu_queue_new(2);
   record_audio_queue_t *audio=record_audio_queue_new(2,4,64);
   record_gpu_worker_vt_t *video;
   record_avfoundation_session_t *session;
   unsigned i,j;
   int result=0,fd;
   int16_t pcm[8];
   bool read,write;
   memset(&f,0,sizeof(f)); memset(&ops,0,sizeof(ops));
   f.native.with_audio=true; f.native.context_busy=2;
   f.native.video_busy=2; f.native.audio_busy=3; f.native.finish_busy=2;
   f.open_failure=failure==1;
   f.invalid_result=failure==3;
   video_ops.ready=ready; video_ops.buffer=buffer; video_ops.release=release;
   video_ops.submit=submit; video_ops.peek=peek; video_ops.ack=ack; video_ops.finish=finish_video;
   video=record_gpu_worker_vt_new(queue,&f.native,&f.native,&video_ops);
   assert(queue && video && audio);
   assert(!record_avfoundation_session_new_output(queue,video,audio,&f,&ops));
   ops.open=output_open; ops.step=output_step; ops.cancel=output_cancel;
   ops.free_output=output_free; ops.retry_needed=output_retry; ops.network=output_network;
   ops.destroy_encoder=output_encoder_destroy; ops.destroy_context=output_context_destroy;
   session=record_avfoundation_session_new_output(queue,video,audio,&f,&ops); assert(session);
   memset(&ops,0,sizeof(ops));
   assert(!record_avfoundation_session_network(session,&fd,&read,&write) && fd==-1 && !read && !write);
   assert(record_avfoundation_session_step(session)==RECORD_AVF_WORKER_BUSY);
   assert(!record_avfoundation_session_retry_needed(session));
   for (i=0;i<2;i++)
   {
      record_gpu_frame_t frame;
      memset(&frame,0,sizeof(frame)); assert(record_gpu_queue_acquire(queue,&frame.slot)==RECORD_GPU_QUEUE_OK);
      frame.timestamp=(uint64_t)i*1001; frame.duration=1001; assert(record_gpu_queue_submit(queue,&frame));
      for (j=0;j<8;j++) pcm[j]=(int16_t)(i+1);
      assert(record_avfoundation_session_audio(session,pcm,4,i?200:100)==RECORD_AUDIO_OK);
   }
   for (i=0;i<100 && !f.native.opens;i++) assert(record_avfoundation_session_step(session)>=0);
   assert(f.native.opens==1 && f.native.packet && !record_avfoundation_session_free(session));
   if (!f.open_failure)
   {
      assert(record_avfoundation_session_network(session,&fd,&read,&write) && fd==17 && read && !write);
      assert(!record_avfoundation_session_retry_needed(session));
      f.invalid_network=true;
      assert(!record_avfoundation_session_network(session,&fd,&read,&write) && fd==-1 && !read && !write);
      f.invalid_network=false;
   }
   if (failure==2) record_avfoundation_session_cancel(session);
   if (failure)
   {
      for (i=0;i<8;i++) record_avfoundation_session_step(session);
      assert(record_avfoundation_session_error(session)!=RECORD_AVF_SESSION_NONE);
      assert(!f.cancellations && !f.output_frees && !f.native.encoder_destroyed);
      assert(!record_avfoundation_session_free(session));
   }
   assert(record_avfoundation_session_video_finish(session));
   assert(record_avfoundation_session_audio_finish(session));
   for (i=0;i<1000;i++)
   {
      result=record_avfoundation_session_step(session);
      record_avfoundation_session_retry_needed(session);
      if (result==RECORD_AVF_WORKER_END || result==RECORD_AVF_WORKER_ERROR) break;
   }
   assert(i<1000 && f.native.encoder_destroyed==1 && f.native.context_destroyed==1);
   assert(failure?result==RECORD_AVF_WORKER_ERROR:result==RECORD_AVF_WORKER_END);
   assert(!record_avfoundation_session_network(session,&fd,&read,&write) && fd==-1 && !read && !write);
   assert(!record_avfoundation_session_retry_needed(session));
   assert(f.output_frees==(f.open_failure?0u:1u));
   assert(f.cancellations==((failure && !f.open_failure)?3u:0u));
   assert(record_avfoundation_session_free(session));
}
int main(void)
{
   unsigned failure;
   for (failure=0;failure<4;failure++) output_test(failure);
   puts("PASS: owned output bind, network interests, producer barriers and native/GPU retirement");
   return 0;
}
