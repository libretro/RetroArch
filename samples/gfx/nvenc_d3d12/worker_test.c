/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include "../../../record/record_gpu_worker_d3d12.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #x); return 1; } } while (0)

struct nvenc_d3d12
{
   nvenc_d3d12_packet_t packets[16];
   void *fence;
   uint64_t value;
   unsigned read, count, submits, polls, releases, finishes;
   int submit_fault, poll_fault, release_fault, finish_fault;
   bool done, locked, eos;
};

int nvenc_d3d12_submit(nvenc_d3d12_t *e, unsigned slot, uint64_t timestamp,
      uint64_t duration, void *fence, uint64_t value, bool keyframe)
{
   nvenc_d3d12_packet_t *p;
   e->submits++;
   if (e->submit_fault)
   {
      int fault = e->submit_fault;
      e->submit_fault = 0;
      return fault;
   }
   if (e->eos || !duration || e->count == 16)
      return NVENC_D3D12_ERROR;
   p = &e->packets[(e->read + e->count++) % 16];
   p->data = "packet"; p->size = 6;
   p->slot = slot; p->timestamp = timestamp;
   p->duration = duration; p->keyframe = keyframe;
   e->fence = fence; e->value = value;
   return NVENC_D3D12_OK;
}
int nvenc_d3d12_poll(nvenc_d3d12_t *e, nvenc_d3d12_packet_t *packet)
{
   e->polls++;
   if (e->poll_fault)
   {
      int fault = e->poll_fault;
      e->poll_fault = 0;
      return fault;
   }
   if (e->locked)
      return NVENC_D3D12_ERROR;
   if (!e->count)
      return e->eos && e->done ? NVENC_D3D12_END : NVENC_D3D12_BUSY;
   if (!e->done)
      return NVENC_D3D12_BUSY;
   *packet = e->packets[e->read]; e->locked = true;
   return NVENC_D3D12_OK;
}
int nvenc_d3d12_release(nvenc_d3d12_t *e)
{
   e->releases++;
   if (e->release_fault)
   {
      e->release_fault = 0;
      return NVENC_D3D12_ERROR;
   }
   if (!e->locked)
      return NVENC_D3D12_ERROR;
   e->count--; e->read = (e->read + 1) % 16; e->locked = false;
   return NVENC_D3D12_OK;
}
void *nvenc_d3d12_wait_handle(const nvenc_d3d12_t *e)
{ return e && !e->locked && (e->count || e->eos) ? (void*)e : NULL; }
int nvenc_d3d12_finish(nvenc_d3d12_t *e)
{
   e->finishes++;
   if (e->finish_fault)
   {
      int fault = e->finish_fault;
      e->finish_fault = 0;
      return fault;
   }
   e->eos = true;
   return NVENC_D3D12_OK;
}

static bool publish(record_gpu_queue_t *q, uint64_t stamp, bool key)
{
   record_gpu_frame_t f;
   if (record_gpu_queue_acquire(q, &f.slot) != RECORD_GPU_QUEUE_OK)
      return false;
   f.timestamp = stamp; f.duration = 3; f.ready_value = stamp + 7;
   f.keyframe = key;
   return record_gpu_queue_submit(q, &f);
}

static int normal(void)
{
   nvenc_d3d12_t e;
   record_gpu_queue_t *q = record_gpu_queue_new(2);
   record_gpu_worker_d3d12_t *w;
   nvenc_d3d12_packet_t packet, held;
   unsigned slot, calls;
   memset(&e, 0, sizeof(e));
   CHECK(q != NULL);
   CHECK(record_gpu_worker_d3d12_new(NULL, &e, NULL) == NULL);
   CHECK(record_gpu_worker_d3d12_new(q, NULL, NULL) == NULL);
   CHECK(record_gpu_worker_d3d12_step(NULL, &packet) == RECORD_GPU_WORKER_ERROR);
   CHECK(!record_gpu_worker_d3d12_release(NULL));
   CHECK(!record_gpu_worker_d3d12_retry_needed(NULL));
   CHECK(record_gpu_worker_d3d12_free(NULL));
   CHECK(record_gpu_worker_d3d12_abandon(NULL));
   w = record_gpu_worker_d3d12_new(q, &e, &e);
   CHECK(w != NULL);
   CHECK(!record_gpu_worker_d3d12_wait_handle(w));
   CHECK(record_gpu_worker_d3d12_step(w, NULL) == RECORD_GPU_WORKER_ERROR);
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_BUSY);
   CHECK(e.polls == 1 && e.submits == 0 && e.finishes == 0);
   CHECK(!record_gpu_worker_d3d12_retry_needed(w));
   CHECK(publish(q, 10, true) && publish(q, 40, false));
   CHECK(record_gpu_queue_finish(q));
   e.submit_fault = NVENC_D3D12_BUSY;
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
   CHECK(e.submits == 1 && e.count == 0 && !e.eos);
   CHECK(record_gpu_worker_d3d12_retry_needed(w));
   CHECK(!record_gpu_worker_d3d12_wait_handle(w));
   CHECK(e.polls == 2 && e.submits == 1);
   CHECK(!record_gpu_worker_d3d12_free(w));
   CHECK(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_END);
   e.submit_fault = NVENC_D3D12_ERROR;
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_ERROR);
   CHECK(e.submits == 2 && e.count == 0 && !e.eos);
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
   CHECK(e.submits == 3 && e.count == 1 && e.fence == &e && e.value == 17);
   CHECK(record_gpu_worker_d3d12_retry_needed(w));
   CHECK(record_gpu_worker_d3d12_wait_handle(w) == &e);
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
   CHECK(e.count == 2 && e.value == 47 && e.finishes == 0);
   e.finish_fault = NVENC_D3D12_BUSY;
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
   CHECK(e.finishes == 1 && !e.eos);
   CHECK(record_gpu_worker_d3d12_retry_needed(w));
   e.finish_fault = NVENC_D3D12_ERROR;
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_ERROR);
   CHECK(e.finishes == 2 && !e.eos);
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
   CHECK(e.finishes == 3 && e.eos);
   e.poll_fault = NVENC_D3D12_ERROR;
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_ERROR);
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_BUSY);
   CHECK(e.finishes == 3 && !record_gpu_queue_free(q));
   CHECK(record_gpu_worker_d3d12_wait_handle(w) == &e);
   e.done = true;
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PACKET);
   CHECK(packet.timestamp == 10 && packet.duration == 3 && packet.keyframe && packet.slot == 0);
   calls = e.polls;
   CHECK(record_gpu_worker_d3d12_step(w, &held) == RECORD_GPU_WORKER_PACKET);
   CHECK(e.polls == calls && held.data == packet.data && held.timestamp == packet.timestamp);
   CHECK(!record_gpu_worker_d3d12_free(w) && !record_gpu_queue_free(q));
   e.release_fault = 1;
   CHECK(!record_gpu_worker_d3d12_release(w));
   CHECK(e.count == 2 && e.locked);
   CHECK(!record_gpu_worker_d3d12_wait_handle(w));
   CHECK(record_gpu_worker_d3d12_step(w, &held) == RECORD_GPU_WORKER_PACKET);
   CHECK(record_gpu_worker_d3d12_release(w));
   CHECK(!record_gpu_worker_d3d12_release(w));
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PACKET);
   CHECK(packet.timestamp == 40 && packet.duration == 3 && !packet.keyframe && packet.slot == 1);
   CHECK(record_gpu_worker_d3d12_release(w));
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_END);
   calls = e.polls;
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_END);
   CHECK(!record_gpu_worker_d3d12_wait_handle(w));
   CHECK(!record_gpu_worker_d3d12_retry_needed(w));
   CHECK(e.polls == calls && e.submits == 4 && e.releases == 3 && e.finishes == 3);
   CHECK(record_gpu_worker_d3d12_free(w));
   CHECK(record_gpu_queue_free(q));
   return 0;
}

static int backpressure(void)
{
   nvenc_d3d12_t e;
   record_gpu_queue_t *q = record_gpu_queue_new(2);
   record_gpu_worker_d3d12_t *w;
   nvenc_d3d12_packet_t packet;
   unsigned slot;
   memset(&e, 0, sizeof(e));
   CHECK(q != NULL);
   w = record_gpu_worker_d3d12_new(q, &e, NULL); CHECK(w != NULL);
   CHECK(publish(q, 0, true) && publish(q, 1, false));
   e.submit_fault = NVENC_D3D12_BUSY;
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
   CHECK(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_BUSY);
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
   CHECK(e.count == 2 && record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_BUSY);
   e.done = true;
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PACKET);
   CHECK(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_BUSY);
   CHECK(record_gpu_worker_d3d12_release(w));
   CHECK(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_OK && slot == 0);
   CHECK(record_gpu_queue_cancel(q));
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PACKET);
   CHECK(packet.slot == 1 && record_gpu_worker_d3d12_release(w));
   CHECK(record_gpu_queue_finish(q));
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
   CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_END);
   CHECK(record_gpu_worker_d3d12_free(w) && record_gpu_queue_free(q));
   return 0;
}

static int teardown(void)
{
   nvenc_d3d12_t e;
   record_gpu_queue_t *q;
   record_gpu_worker_d3d12_t *w;
   nvenc_d3d12_packet_t packet;
   unsigned mode, i;
   for (mode = 0; mode < 5; mode++)
   {
      memset(&e, 0, sizeof(e));
      q = record_gpu_queue_new(16); CHECK(q != NULL);
      w = record_gpu_worker_d3d12_new(q, &e, NULL); CHECK(w != NULL);
      if (mode == 0)
      {
         CHECK(record_gpu_worker_d3d12_free(w));
         CHECK(record_gpu_queue_free(q));
         continue;
      }
      if (mode == 1)
      {
         CHECK(record_gpu_queue_finish(q));
         e.finish_fault = NVENC_D3D12_BUSY;
         CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
         CHECK(!record_gpu_worker_d3d12_wait_handle(w));
         CHECK(record_gpu_worker_d3d12_retry_needed(w));
         e.finish_fault = NVENC_D3D12_BUSY;
         CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_BUSY);
         CHECK(record_gpu_worker_d3d12_retry_needed(w));
         CHECK(!record_gpu_worker_d3d12_wait_handle(w));
         CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
         CHECK(!record_gpu_worker_d3d12_free(w));
         e.done = true;
         CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_END);
         CHECK(e.submits == 0 && e.finishes == 3);
         CHECK(record_gpu_worker_d3d12_free(w));
         CHECK(record_gpu_queue_free(q));
         continue;
      }
      for (i = 0; i < 16; i++) CHECK(publish(q, i, false));
      if (mode == 2) e.submit_fault = NVENC_D3D12_BUSY;
      CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PROGRESS);
      if (mode == 4)
      {
         e.done = true;
         CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_PACKET);
      }
      CHECK(!record_gpu_worker_d3d12_free(w));
      /* Simulated successful encoder destruction precedes abandon. */
      memset(&e, 0, sizeof(e));
      CHECK(!record_gpu_worker_d3d12_abandon(w));
      CHECK(record_gpu_worker_d3d12_step(w, &packet) == RECORD_GPU_WORKER_ERROR);
      CHECK(record_gpu_queue_finish(q));
      CHECK(record_gpu_worker_d3d12_abandon(w));
      CHECK(record_gpu_queue_free(q));
   }
   return 0;
}
int main(void)
{
   if (normal() || backpressure() || teardown()) return 1;
   puts("PASS: bounded GPU encoder worker retries, packet ownership, EOS and teardown");
   return 0;
}
