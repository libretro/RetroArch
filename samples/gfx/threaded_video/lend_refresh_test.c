#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <rthreads/rthreads.h>
#include "gfx/video_driver.h"
#include "gfx/video_thread_wrapper.h"

static thread_video_t thr;
static video_driver_state_t state;
static video_poke_interface_t poke;
static video_thread_async_load_t node;
static struct texture_image image;
static uint32_t pixels[2][2][24];
static unsigned generation[2],ready_mask,valid_mask,fail_mask;
static bool packed,alias;
static uintptr_t owner;
static retro_atomic_int_t command,ack,refresh,started;
#define ROUNDS 2000

video_driver_state_t *video_state_get_ptr(void) { return &state; }
static bool ready(void *data,uintptr_t id,unsigned slot)
{
   (void)data;
   assert(id==1 && slot<2 && owner==sthread_get_current_thread_id());
   return (ready_mask & (1u<<slot))!=0;
}
static void *lend(void *data,uintptr_t id,unsigned slot,size_t row,size_t *pitch)
{
   size_t actual;
   (void)data;
   assert(id==1 && slot<2 && row==16 && owner==sthread_get_current_thread_id());
   if (!(valid_mask & (1u<<slot)))
   {
      generation[slot]++;
      valid_mask|=1u<<slot;
   }
   actual=packed && generation[slot]==1 ? 16 : (generation[slot]&1) ? 32 : 48;
   if ((fail_mask & (1u<<slot)) || (!pitch && row!=actual)) return NULL;
   if (pitch) *pitch=actual;
   return alias && slot==1 ? pixels[0][generation[0]&1] : pixels[slot][generation[slot]&1];
}
#include "lend.inc"

#include "states.inc"
typedef struct { retro_atomic_int_t status; bool gpu; } vp_job_t;
static unsigned publish_fences,publish_stores;
static bool publish_needed;
static void counted_fence(void) { publish_fences++; }
static void counted_store(retro_atomic_int_t *status,int value)
{
   assert(publish_fences==(publish_needed ? 1u : 0u));
   publish_stores++;
   retro_atomic_store_release_int(status,value);
}
#include "publish.inc"
static void publication_fence(void)
{
   vp_job_t job;
   unsigned gpu,alive;
   retro_atomic_int_init(&job.status,VP_JOB_RUNNING);
   for (gpu=0;gpu<2;gpu++)
      for (alive=0;alive<3;alive++)
      {
         job.gpu=gpu!=0;publish_needed=!job.gpu;
         publish_fences=publish_stores=0;
         vp_job_publish(&job,(int)alive);
         assert(publish_stores==1 && publish_fences==1-gpu);
         assert(retro_atomic_load_acquire_int(&job.status)==
               (alive==2 ? VP_JOB_WAITING : alive ? VP_JOB_READY : VP_JOB_FINISHED));
      }
}

static void init(bool pad)
{
   memset(&thr,0,sizeof(thr));memset(&node,0,sizeof(node));
   memset(generation,0,sizeof(generation));memset(&image,0,sizeof(image));
   memset(&poke,0,sizeof(poke));memset(&state,0,sizeof(state));
   poke.texture_lend=lend;poke.texture_lend_ready=ready;poke.texture_lend_per_write=true;
   thr.poke=&poke;thr.driver_data=&thr;
   state.thread_wrapper_active=true;state.data=&thr;
   node.handle=1;node.lend=3;node.lend_padded=pad;
   image.width=4;image.height=2;
   ready_mask=3;valid_mask=fail_mask=0;packed=!pad;alias=false;
   owner=sthread_get_current_thread_id();
   video_thread_lend_slots(&thr,&node,&image);
   assert(node.lend_refresh==3 && node.lent_idx[0]>=0 && node.lent_idx[1]>=0);
}
static void submitted(unsigned slot)
{
   int i=node.lent_idx[slot];
   const void *p=thr.lend[i].mem;
   assert(p);
   valid_mask&=~(1u<<slot);ready_mask&=~(1u<<slot);
   video_thread_lend_mark(&thr,1,p);
   assert(retro_atomic_load_acquire_int(&thr.lend[i].busy) && !thr.lend[i].mem);
}
static void sequential(void)
{
   void *mem,*old;
   size_t pitch;
   int i;
   init(true);i=node.lent_idx[0];old=thr.lend[i].mem;
   submitted(0);video_thread_lend_poll(&thr);
   assert(!video_thread_lend_get(i,&mem,&pitch));
   ready_mask|=1;video_thread_lend_poll(&thr);
   assert(video_thread_lend_get(i,&mem,&pitch) && mem!=old && pitch==48 && !thr.lend_busy);
   submitted(0);submitted(1);alias=true;ready_mask=2;
   video_thread_lend_poll(&thr);
   assert(video_thread_lend_get(node.lent_idx[1],&mem,&pitch) && mem==pixels[0][generation[0]&1]);
   video_thread_lend_mark(&thr,1,mem);
   assert(retro_atomic_load_acquire_int(&thr.lend[node.lent_idx[1]].busy));
   video_thread_lend_forget(&thr,1);assert(!thr.lend_busy && !thr.lend[i].used);

   init(false);i=node.lent_idx[0];submitted(0);ready_mask|=1;
   video_thread_lend_poll(&thr);
   assert(video_thread_lend_get(i,&mem,&pitch) && !mem && !thr.lend_busy);
   init(true);i=node.lent_idx[0];submitted(0);fail_mask=1;ready_mask|=1;
   video_thread_lend_poll(&thr);
   assert(video_thread_lend_get(i,&mem,&pitch) && !mem && !thr.lend_busy);
}
static void worker(void *unused)
{
   unsigned round;
   (void)unused;
   init(true);
   retro_atomic_store_release_int(&started,1);
   for (round=1;round<=ROUNDS;round++)
   {
      unsigned slot=round&1;
      int i=node.lent_idx[slot];
      uint32_t *mem;
      size_t pitch;
      while (retro_atomic_load_acquire_int(&command)!=(int)round) sthread_yield();
      mem=(uint32_t*)thr.lend[i].mem;pitch=thr.lend[i].pitch;
      assert(mem[0]==round && mem[pitch/4]==(round^0x12345678u));
      submitted(slot);
      retro_atomic_store_release_int(&ack,(int)round);
      while (retro_atomic_load_acquire_int(&refresh)!=(int)round) sthread_yield();
      ready_mask|=1u<<slot;
      video_thread_lend_poll(&thr);
   }
}
int main(void)
{
   sthread_t *thread;
   unsigned round;
   void *mem;
   size_t pitch;
   publication_fence();
   sequential();
   retro_atomic_int_init(&command,0);retro_atomic_int_init(&ack,0);
   retro_atomic_int_init(&refresh,0);retro_atomic_int_init(&started,0);
   thread=sthread_create(worker,NULL);assert(thread);
   while (!retro_atomic_load_acquire_int(&started)) sthread_yield();
   for (round=1;round<=ROUNDS;round++)
   {
      unsigned slot=round&1;
      int i=node.lent_idx[slot];
      assert(video_thread_lend_get(i,&mem,&pitch) && mem && (pitch==32 || pitch==48));
      ((uint32_t*)mem)[0]=round;((uint32_t*)mem)[pitch/4]=round^0x12345678u;
      retro_atomic_store_release_int(&command,(int)round);
      while (retro_atomic_load_acquire_int(&ack)!=(int)round) sthread_yield();
      assert(!video_thread_lend_get(i,&mem,&pitch));
      retro_atomic_store_release_int(&refresh,(int)round);
      while (!video_thread_lend_get(i,&mem,&pitch)) sthread_yield();
      assert(mem && (pitch==32 || pitch==48));
   }
   sthread_join(thread);
   puts("PASS: shipping lend descriptor publication, revocation, address reuse and 2000 worker handoffs");
   return 0;
}
