/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_pool.h"

static bool vaapi_hevc_pool_init_work(vaapi_hevc_pool_t *p,
      const vaapi_hevc_session_t *session, const vaapi_hevc_submit_ops_t *ops,
      vaapi_hevc_short_work_t *work, rh265_video *source,
      vaapi_hevc_long_work_t *long_work)
{
   vaapi_hevc_submit_ops_t native;
   uint64_t generation;
   if (!p || p->active || !session || !session->ready || session->closing
         || !session->display || session->context == VA_INVALID_ID
         || !session->slots || session->slots > 32 || !ops || !ops->create
         || !ops->destroy || !ops->begin || !ops->render || !ops->end
         || !ops->sync)
      return false;
   if (session->slice_mode == VA_DEC_SLICE_MODE_BASE)
   {
      if (!work || !work->data || !work->capacity || long_work || source)
         return false;
   }
   else if (session->slice_mode == VA_DEC_SLICE_MODE_NORMAL)
   {
      if (work || !source || !long_work || !long_work->data
            || !long_work->capacity)
         return false;
   }
   else
      return false;
   native     = *ops;
   generation = p->next_generation;
   memset(p, 0, sizeof(*p));
   p->next_generation = generation;
   p->ops             = native;
   p->session         = session;
   p->display         = session->display;
   p->work            = work;
   p->long_work       = long_work;
   p->source          = source;
   p->active          = true;
   return true;
}

bool vaapi_hevc_pool_init(vaapi_hevc_pool_t *p,
      const vaapi_hevc_session_t *session, const vaapi_hevc_submit_ops_t *ops,
      vaapi_hevc_short_work_t *work)
{
   return vaapi_hevc_pool_init_work(p, session, ops, work, NULL, NULL);
}

bool vaapi_hevc_pool_init_long(vaapi_hevc_pool_t *p,
      const vaapi_hevc_session_t *session, const vaapi_hevc_submit_ops_t *ops,
      rh265_video *source, vaapi_hevc_long_work_t *work)
{
   return vaapi_hevc_pool_init_work(p, session, ops, NULL, source, work);
}

uint32_t vaapi_hevc_pool_held(const vaapi_hevc_pool_t *p)
{
   unsigned i;
   uint32_t held;
   if (!p || !p->active)
      return 0;
   held = p->exports;
   for (i = 0; i < p->session->slots; i++)
   {
      if (p->transactions[i].active)
         held |= UINT32_C(1) << i;
      held |= p->reads[i];
   }
   return held;
}
int vaapi_hevc_pool_decode(vaapi_hevc_pool_t *p,const rh265_video_params_t *params,
 const rh265_access_unit_t *au,const rh265_hw_picture_t *picture,const rh265_nal_t *slices)
{
 unsigned i;
 uint32_t bit,reads=0;
 vaapi_hevc_submit_t *t;
 bool accepted;
 if (!p || !p->active || p->broken || !picture
       || picture->slot >= p->session->slots || picture->num_refs > 15
       || !picture->serial || p->last_serial == (uint64_t)-1
       || picture->serial != p->last_serial + 1
       || p->next_generation == (uint64_t)-1)
    return -1;
 bit = UINT32_C(1) << picture->slot;
 if (vaapi_hevc_pool_held(p) & bit)
    return 1;
 for (i = 0; i < picture->num_refs; i++)
 {
    unsigned slot = picture->refs[i].slot;
    uint32_t ref;
    if (slot >= p->session->slots || slot == picture->slot)
       return -1;
    ref = UINT32_C(1) << slot;
    if (!(p->valid & ref) || (reads & ref)
          || p->poc[slot] != picture->refs[i].poc)
       return -1;
    reads |= ref;
 }
 t = &p->transactions[picture->slot];
 if (p->session->slice_mode == VA_DEC_SLICE_MODE_NORMAL)
    accepted = vaapi_hevc_long_submit(t, p->session, &p->ops, p->source, params,
          au, picture, slices, p->long_work);
 else
    accepted = vaapi_hevc_short_submit(
          t, p->session, &p->ops, params, au, picture, slices, p->work);
 if (t->active)
 {
    p->reads[picture->slot] = reads;
    p->valid &= ~bit;
    p->ready &= ~bit;
 }
 if (!accepted)
 {
    if (t->active)
       p->broken = true;
    return -1;
 }
 p->valid |= bit;
 p->generation[picture->slot] = ++p->next_generation;
 p->poc[picture->slot]        = picture->poc;
 p->last_serial               = picture->serial;
 return 0;
}

int vaapi_hevc_pool_poll(vaapi_hevc_pool_t *p, unsigned slot)
{
   vaapi_hevc_submit_t *t;
   uint32_t bit;
   int status;
   if (!p || !p->active || slot >= p->session->slots)
      return -1;
   bit = UINT32_C(1) << slot;
   t   = &p->transactions[slot];
   if (!t->active)
      return (p->ready & bit) ? 0 : -1;
   if (!t->submitted)
   {
      if (vaapi_hevc_submit_retire(t))
         p->reads[slot] = 0;
      return -1;
   }
   status = vaapi_hevc_submit_poll(t);
   if (status == 1)
      return 1;
   if (status < 0)
   {
      p->broken = true;
      p->valid &= ~bit;
      p->ready &= ~bit;
   }
   else p->ready|=bit;
 if(t->complete)
 {
  p->reads[slot]=0;
  vaapi_hevc_submit_retire(t);
 }
 return status;
}
bool vaapi_hevc_pool_acquire(vaapi_hevc_pool_t *p,unsigned slot,uint64_t *ticket)
{
 uint32_t bit;
 if(!p || !p->active || p->broken || !ticket || slot>=p->session->slots)return false;
 bit=UINT32_C(1)<<slot;
 if(!(p->ready&bit) || (p->exports&bit) || !p->generation[slot])return false;
 p->exports|=bit;*ticket=p->generation[slot];return true;
}
bool vaapi_hevc_pool_release(vaapi_hevc_pool_t *p,unsigned slot,uint64_t ticket)
{
 uint32_t bit;
 if(!p || !p->active || slot>=p->session->slots || !ticket)return false;
 bit=UINT32_C(1)<<slot;
 if(!(p->exports&bit) || p->generation[slot]!=ticket)return false;
 p->exports&=~bit;return true;
}
bool vaapi_hevc_pool_close(vaapi_hevc_pool_t *p)
{
 uint64_t generation;
 if(!p || !p->active)return true;
 if(vaapi_hevc_pool_held(p) || p->broken)return false;
 generation=p->next_generation;memset(p,0,sizeof(*p));p->next_generation=generation;return true;
}
bool vaapi_hevc_pool_terminated(vaapi_hevc_pool_t *p,VADisplay display)
{
 uint64_t generation;
 if(!p || !p->active || !display || p->display!=display || p->exports)return false;
 generation=p->next_generation;memset(p,0,sizeof(*p));p->next_generation=generation;return true;
}
