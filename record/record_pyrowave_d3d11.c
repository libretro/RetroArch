/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include "record_pyrowave_d3d11.h"
#include "record_pyrowave_session_internal.h"
#include <retro_atomic.h>
#include <stdlib.h>
#include <string.h>

enum rpy_d11_state { RPY_D11_IDLE, RPY_D11_ENCODING, RPY_D11_MAPPED,
   RPY_D11_DECODING, RPY_D11_READY, RPY_D11_LENT, RPY_D11_DISPLAYING };
typedef struct record_pyrowave_d11_buffer
{
   ID3D11Buffer *buffer;
   ID3D11ShaderResourceView *srv;
   ID3D11UnorderedAccessView *uav;
} record_pyrowave_d11_buffer_t;
typedef struct record_pyrowave_d11_slot
{
   uint8_t *host;
   record_gpu_frame_t frame;
   rpyrowave_packet_parameters_t parameters;
   ID3D11Buffer *readback;
   ID3D11Query *query;
   ID3D11Texture2D *texture;
   ID3D11ShaderResourceView *view;
   ID3D11UnorderedAccessView *uav;
   retro_atomic_int_t ready;
   unsigned state;
} record_pyrowave_d11_slot_t;
struct record_pyrowave_d3d11
{
   record_pyrowave_d11_slot_t slots[RECORD_GPU_QUEUE_SLOTS];
   record_pyrowave_d11_buffer_t atlas[2], blocks, workspace;
   ID3D11Device *device;
   ID3D11DeviceContext *context;
   ID3D11ComputeShader *shader[3];
   ID3D11Buffer *color_constants, *decode_constants;
   ID3D11Buffer *dwt_constants[50], *packet_constants[48];
   ID3D11Query *retirement;
   record_gpu_queue_t *queue;
   rpyrowave_gpu_layout_t layout;
   rpyrowave_sequence_t sequence;
   rpyrowave_color_parameters_t color;
   uint64_t next_ticket, retired;
   unsigned count, reservation, owned, usability;
   bool decode, failed, quiescing, quiesced;
};
static uint32_t record_pyrowave_d11_load32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void record_pyrowave_d11_buffer_free(record_pyrowave_d11_buffer_t *b)
{
   if (b->srv) ID3D11ShaderResourceView_Release(b->srv);
   if (b->uav) ID3D11UnorderedAccessView_Release(b->uav);
   if (b->buffer) ID3D11Buffer_Release(b->buffer);
}
static bool record_pyrowave_d11_buffer_new(record_pyrowave_d3d11_t *c,
      record_pyrowave_d11_buffer_t *b, size_t bytes, unsigned stride, const void *initial)
{
   D3D11_BUFFER_DESC desc;
   D3D11_SUBRESOURCE_DATA data;
   D3D11_SHADER_RESOURCE_VIEW_DESC srv;
   D3D11_UNORDERED_ACCESS_VIEW_DESC uav;
   if (!bytes || bytes > 0x7fffffffu) return false;
   memset(&desc, 0, sizeof(desc)); memset(&data, 0, sizeof(data));
   desc.ByteWidth = (UINT)bytes;
   desc.Usage = initial ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
   desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (initial ? 0 : D3D11_BIND_UNORDERED_ACCESS);
   desc.MiscFlags = stride ? D3D11_RESOURCE_MISC_BUFFER_STRUCTURED : D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
   desc.StructureByteStride = stride;
   data.pSysMem = initial;
   if (FAILED(ID3D11Device_CreateBuffer(c->device, &desc, initial ? &data : NULL, &b->buffer))) return false;
   memset(&srv, 0, sizeof(srv));
   srv.Format = stride ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_TYPELESS;
   srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
   srv.BufferEx.NumElements = desc.ByteWidth / (stride ? stride : 4);
   srv.BufferEx.Flags = stride ? 0 : D3D11_BUFFEREX_SRV_FLAG_RAW;
   if (FAILED(ID3D11Device_CreateShaderResourceView(c->device, (ID3D11Resource*)b->buffer, &srv, &b->srv))) return false;
   if (initial) return true;
   memset(&uav, 0, sizeof(uav)); uav.Format = srv.Format;
   uav.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
   uav.Buffer.NumElements = srv.BufferEx.NumElements;
   uav.Buffer.Flags = stride ? 0 : D3D11_BUFFER_UAV_FLAG_RAW;
   return SUCCEEDED(ID3D11Device_CreateUnorderedAccessView(c->device, (ID3D11Resource*)b->buffer, &uav, &b->uav));
}
static void record_pyrowave_d11_destroy(record_pyrowave_d3d11_t *c)
{
   unsigned i;
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_d11_slot_t *s = &c->slots[i];
      if (s->state == RPY_D11_MAPPED) ID3D11DeviceContext_Unmap(c->context, (ID3D11Resource*)s->readback, 0);
      if (c->decode) free(s->host);
      if (s->query) ID3D11Query_Release(s->query);
      if (s->readback) ID3D11Buffer_Release(s->readback);
      if (s->view) ID3D11ShaderResourceView_Release(s->view);
      if (s->uav) ID3D11UnorderedAccessView_Release(s->uav);
      if (s->texture) ID3D11Texture2D_Release(s->texture);
   }
   for (i = 0; i < 3; i++)
   {
      if (c->shader[i]) ID3D11ComputeShader_Release(c->shader[i]);

   }
   if (c->color_constants) ID3D11Buffer_Release(c->color_constants);
   if (c->decode_constants) ID3D11Buffer_Release(c->decode_constants);
   for (i = 0; i < 50; i++) if (c->dwt_constants[i]) ID3D11Buffer_Release(c->dwt_constants[i]);
   for (i = 0; i < 48; i++) if (c->packet_constants[i]) ID3D11Buffer_Release(c->packet_constants[i]);
   record_pyrowave_d11_buffer_free(&c->atlas[0]); record_pyrowave_d11_buffer_free(&c->atlas[1]);
   record_pyrowave_d11_buffer_free(&c->blocks); record_pyrowave_d11_buffer_free(&c->workspace);
   if (c->retirement) ID3D11Query_Release(c->retirement);
   if (c->context) ID3D11DeviceContext_Release(c->context);
   if (c->device) ID3D11Device_Release(c->device);
   free(c);
}
static bool record_pyrowave_d11_constant(record_pyrowave_d3d11_t *c, ID3D11Buffer **target, const void *value, unsigned bytes)
{
   D3D11_BUFFER_DESC desc;
   D3D11_SUBRESOURCE_DATA initial;
   memset(&desc, 0, sizeof(desc)); memset(&initial, 0, sizeof(initial));
   desc.ByteWidth = bytes; desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
   desc.Usage = value ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
   initial.pSysMem = value;
   return SUCCEEDED(ID3D11Device_CreateBuffer(c->device, &desc, value ? &initial : NULL, target));
}
static int record_pyrowave_d11_dwt_constant(void *user, const rpyrowave_dwt_parameters_t *p,
      unsigned source, unsigned x, unsigned y, unsigned z)
{
   record_pyrowave_d3d11_t *c = (record_pyrowave_d3d11_t*)user;
   unsigned index = (p->level * 2 + p->axis) * 5 + p->phase;
   (void)source; (void)x; (void)y; (void)z;
   return record_pyrowave_d11_constant(c, &c->dwt_constants[index], p, sizeof(*p));
}
static int record_pyrowave_d11_usability(void *user, const rpyrowave_packet_parameters_t *p, unsigned x, unsigned y)
{
   record_pyrowave_d3d11_t *c = (record_pyrowave_d3d11_t*)user;
   unsigned index = p->sequence * 6 + p->operation;
   (void)x; (void)y; c->usability = p->usability;
   if (c->decode || c->packet_constants[index]) return 1;
   return record_pyrowave_d11_constant(c, &c->packet_constants[index], p, sizeof(*p));
}
record_pyrowave_d3d11_t *record_pyrowave_d3d11_new(const record_pyrowave_d3d11_config_t *cfg)
{
   record_pyrowave_d3d11_t *c;
   rpyrowave_gpu_block_t *blocks = NULL;
   D3D11_BUFFER_DESC buffer;
   D3D11_TEXTURE2D_DESC texture;
   D3D11_QUERY_DESC query;
   ID3D11Device *owner = NULL;
   size_t atlas_bytes;
   unsigned i;
   if (!cfg || !cfg->device || !cfg->context || !cfg->slots || cfg->slots > RECORD_GPU_QUEUE_SLOTS
         || ID3D11Device_GetFeatureLevel(cfg->device) < D3D_FEATURE_LEVEL_11_0
         || ID3D11DeviceContext_GetType(cfg->context) != D3D11_DEVICE_CONTEXT_IMMEDIATE) return NULL;
   ID3D11DeviceContext_GetDevice(cfg->context, &owner);
   if (owner) ID3D11Device_Release(owner);
   if (owner != cfg->device) return NULL;
   c = (record_pyrowave_d3d11_t*)calloc(1, sizeof(*c));
   if (!c) return NULL;
   c->device = cfg->device; ID3D11Device_AddRef(c->device);
   c->context = cfg->context; ID3D11DeviceContext_AddRef(c->context);
   c->sequence = cfg->sequence; c->decode = cfg->decode; c->count = cfg->slots;
   c->reservation = RECORD_GPU_QUEUE_SLOTS; c->next_ticket = 1;
   if (cfg->texture_mode > (unsigned)(cfg->decode ? RPYROWAVE_COLOR_SDR709 : RPYROWAVE_COLOR_SCRGB)
         || rpyrowave_gpu_layout(&cfg->sequence, cfg->maximum_bytes, &c->layout)
         || rpyrowave_gpu_color_parameters_ex(&cfg->sequence, cfg->bit_depth, cfg->decode,
            cfg->decode ? 0 : cfg->flip_y,
            cfg->decode ? cfg->sequence.width : cfg->texture_width,
            cfg->decode ? cfg->sequence.height : cfg->texture_height,
            cfg->decode ? 0 : cfg->x, cfg->decode ? 0 : cfg->y, cfg->scale_source, &c->color)) goto error;
   c->color.texture_mode = cfg->texture_mode;
   c->queue = record_gpu_queue_new(c->count);
   if (!c->queue) goto error;
   memset(&query, 0, sizeof(query)); query.Query = D3D11_QUERY_EVENT;
   if (FAILED(ID3D11Device_CreateQuery(c->device, &query, &c->retirement))) goto error;
   atlas_bytes = (size_t)c->layout.image_width * c->layout.image_height * 12;
   blocks = (rpyrowave_gpu_block_t*)calloc(c->layout.block_count, sizeof(*blocks));
   if (!blocks || rpyrowave_gpu_blocks(&c->layout, blocks, c->layout.block_count)
         || !record_pyrowave_d11_buffer_new(c, &c->atlas[0], atlas_bytes, 4, NULL)
         || !record_pyrowave_d11_buffer_new(c, &c->atlas[1], atlas_bytes, 4, NULL)
         || !record_pyrowave_d11_buffer_new(c, &c->workspace, c->layout.workspace_bytes, 0, NULL)
         || !record_pyrowave_d11_buffer_new(c, &c->blocks, c->layout.block_count * sizeof(*blocks), sizeof(*blocks), blocks)) goto error;
   free(blocks); blocks = NULL;
   if (!record_pyrowave_d11_constant(c, &c->color_constants, &c->color, sizeof(c->color))
         || rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height, c->layout.chroma_444,
            c->decode, record_pyrowave_d11_dwt_constant, c)) goto error;
   if (c->decode && !record_pyrowave_d11_constant(c, &c->decode_constants, NULL, sizeof(rpyrowave_packet_parameters_t))) goto error;
   for (i = 0; i < 8; i++)
   {
      rpyrowave_sequence_t sequence = c->sequence;
      sequence.sequence = i;
      if (rpyrowave_gpu_encode_record(&c->layout, &sequence, cfg->maximum_bytes, -1, record_pyrowave_d11_usability, c)) goto error;
   }
   for (i = 0; i < 3; i++)
      if (!cfg->shaders[i] || !cfg->shader_bytes[i]
            || FAILED(ID3D11Device_CreateComputeShader(c->device, cfg->shaders[i], cfg->shader_bytes[i], NULL, &c->shader[i]))) goto error;
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_d11_slot_t *s = &c->slots[i];
      retro_atomic_int_init(&s->ready, 0);
      memset(&query, 0, sizeof(query)); query.Query = D3D11_QUERY_EVENT;
      memset(&buffer, 0, sizeof(buffer)); buffer.Usage = D3D11_USAGE_STAGING;
      buffer.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      buffer.ByteWidth = cfg->decode ? 16 : 16 + ((cfg->maximum_bytes + 3) & ~3u);
      if (FAILED(ID3D11Device_CreateQuery(c->device, &query, &s->query))
            || FAILED(ID3D11Device_CreateBuffer(c->device, &buffer, NULL, &s->readback))) goto error;
      if (!c->decode) continue;
      s->host = (uint8_t*)calloc(1, c->layout.workspace_bytes);
      memset(&texture, 0, sizeof(texture)); texture.Width = c->layout.width; texture.Height = c->layout.height;
      texture.MipLevels = texture.ArraySize = texture.SampleDesc.Count = 1;
      texture.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; texture.Usage = D3D11_USAGE_DEFAULT;
      texture.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
      if (!s->host || FAILED(ID3D11Device_CreateTexture2D(c->device, &texture, NULL, &s->texture))
            || FAILED(ID3D11Device_CreateShaderResourceView(c->device, (ID3D11Resource*)s->texture, NULL, &s->view))
            || FAILED(ID3D11Device_CreateUnorderedAccessView(c->device, (ID3D11Resource*)s->texture, NULL, &s->uav))) goto error;
   }
   return c;
error:
   free(blocks); record_gpu_queue_free(c->queue); record_pyrowave_d11_destroy(c); return NULL;
}
static void record_pyrowave_d11_unbind(record_pyrowave_d3d11_t *c)
{
   ID3D11ShaderResourceView *srv[2] = {NULL, NULL};
   ID3D11UnorderedAccessView *uav[2] = {NULL, NULL};
   ID3D11DeviceContext_CSSetShaderResources(c->context, 0, 2, srv);
   ID3D11DeviceContext_CSSetUnorderedAccessViews(c->context, 0, 2, uav, NULL);
}
static void record_pyrowave_d11_end(record_pyrowave_d3d11_t *c)
{
   ID3D11Buffer *none = NULL;
   record_pyrowave_d11_unbind(c);
   ID3D11DeviceContext_CSSetShader(c->context, NULL, NULL, 0);
   ID3D11DeviceContext_CSSetConstantBuffers(c->context, 0, 1, &none);
}
static void record_pyrowave_d11_bind(record_pyrowave_d3d11_t *c, unsigned kind, const void *parameters,
      ID3D11ShaderResourceView *srv0, ID3D11ShaderResourceView *srv1,
      ID3D11UnorderedAccessView *uav0, ID3D11UnorderedAccessView *uav1)
{
   ID3D11ShaderResourceView *srv[2]; ID3D11UnorderedAccessView *uav[2];
   ID3D11Buffer *constant = c->color_constants;
   if (kind == 1)
   {
      const rpyrowave_dwt_parameters_t *p = (const rpyrowave_dwt_parameters_t*)parameters;
      constant = c->dwt_constants[(p->level * 2 + p->axis) * 5 + p->phase];
   }
   else if (kind == 2)
   {
      const rpyrowave_packet_parameters_t *p = (const rpyrowave_packet_parameters_t*)parameters;
      constant = c->decode ? c->decode_constants : c->packet_constants[p->sequence * 6 + p->operation];
   }
   srv[0] = srv0; srv[1] = srv1; uav[0] = uav0; uav[1] = uav1;
   record_pyrowave_d11_unbind(c);
   ID3D11DeviceContext_CSSetShader(c->context, c->shader[kind], NULL, 0);
   ID3D11DeviceContext_CSSetConstantBuffers(c->context, 0, 1, &constant);
   ID3D11DeviceContext_CSSetShaderResources(c->context, 0, 2, srv);
   ID3D11DeviceContext_CSSetUnorderedAccessViews(c->context, 0, 2, uav, NULL);
}
static int record_pyrowave_d11_dwt(void *user, const rpyrowave_dwt_parameters_t *p,
      unsigned source, unsigned x, unsigned y, unsigned z)
{
   record_pyrowave_d3d11_t *c = (record_pyrowave_d3d11_t*)user;
   record_pyrowave_d11_bind(c, 1, p, c->atlas[source].srv, NULL, c->atlas[source ^ 1].uav, NULL);
   ID3D11DeviceContext_Dispatch(c->context, x, y, z); return 1;
}
static int record_pyrowave_d11_packet_dispatch(void *user, const rpyrowave_packet_parameters_t *p, unsigned x, unsigned y)
{
   record_pyrowave_d3d11_t *c = (record_pyrowave_d3d11_t*)user;
   record_pyrowave_d11_bind(c, 2, p, c->decode ? NULL : c->atlas[0].srv, c->blocks.srv,
         c->workspace.uav, c->decode ? c->atlas[0].uav : NULL);
   ID3D11DeviceContext_Dispatch(c->context, x, y, 1); return 1;
}
static void record_pyrowave_d11_copy(record_pyrowave_d3d11_t *c, ID3D11Buffer *target, unsigned at, unsigned from, unsigned bytes)
{
   D3D11_BOX box;
   memset(&box, 0, sizeof(box)); box.left = from; box.right = from + bytes; box.bottom = box.back = 1;
   ID3D11DeviceContext_CopySubresourceRegion(c->context, (ID3D11Resource*)target, 0, at, 0, 0,
         (ID3D11Resource*)c->workspace.buffer, 0, &box);
}
int record_pyrowave_d3d11_push(record_pyrowave_d3d11_t *c, ID3D11ShaderResourceView *source,
      uint64_t pts, uint64_t duration, uint64_t *ticket)
{
   record_pyrowave_d11_slot_t *s;
   rpyrowave_sequence_t sequence;
   unsigned slot;
   int result;
   if (!c || c->decode || c->failed || c->quiescing || !source || !ticket || !c->next_ticket
         || !duration || pts > (uint64_t)-1 - duration) return RECORD_GPU_QUEUE_ERROR;
   if (!record_pyrowave_d3d11_poll(c)) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_acquire(c->queue, &slot);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   s = &c->slots[slot];
   if (s->state != RPY_D11_IDLE) { record_gpu_queue_cancel(c->queue); return RECORD_GPU_QUEUE_BUSY; }
   c->reservation = slot;
   s->frame.timestamp = pts; s->frame.duration = duration; s->frame.ready_value = c->next_ticket++;
   s->frame.slot = slot; s->frame.keyframe = true; *ticket = s->frame.ready_value;
   retro_atomic_store_release_int(&s->ready, 0);
   record_pyrowave_d11_bind(c, 0, &c->color, source, NULL, c->atlas[0].uav, NULL);
   ID3D11DeviceContext_Dispatch(c->context, (c->layout.image_width+7)/8, (c->layout.image_height+7)/8, 3);
   rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height, c->layout.chroma_444, 0, record_pyrowave_d11_dwt, c);
   sequence = c->sequence; sequence.sequence = (unsigned)(*ticket & 7);
   rpyrowave_gpu_encode_record(&c->layout, &sequence, c->layout.maximum_bytes, -1, record_pyrowave_d11_packet_dispatch, c);
   record_pyrowave_d11_end(c);
   record_pyrowave_d11_copy(c, s->readback, 0, 0, 16);
   record_pyrowave_d11_copy(c, s->readback, 16, c->layout.wire_offset, (c->layout.maximum_bytes + 3) & ~3u);
   ID3D11DeviceContext_End(c->context, (ID3D11Asynchronous*)s->query);
   s->state = RPY_D11_ENCODING;
   if (!record_gpu_queue_submit(c->queue, &s->frame)) { c->failed = true; return RECORD_GPU_QUEUE_ERROR; }
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return RECORD_GPU_QUEUE_OK;
}
int record_pyrowave_d3d11_decode(record_pyrowave_d3d11_t *c)
{
   record_gpu_frame_t frame;
   record_pyrowave_d11_slot_t *s;
   int result;
   if (!c || !c->decode || c->failed || c->quiescing) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, &frame);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   s = &c->slots[frame.slot];
   record_pyrowave_d11_unbind(c);
   ID3D11DeviceContext_UpdateSubresource(c->context, (ID3D11Resource*)c->workspace.buffer, 0, NULL, s->host, 0, 0);
   ID3D11DeviceContext_UpdateSubresource(c->context, (ID3D11Resource*)c->decode_constants, 0, NULL, &s->parameters, 0, 0);
   rpyrowave_gpu_decode_record(&c->layout, &s->parameters, record_pyrowave_d11_packet_dispatch, c);
   rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height, c->layout.chroma_444, 1, record_pyrowave_d11_dwt, c);
   record_pyrowave_d11_bind(c, 0, &c->color, NULL, c->atlas[0].srv, NULL, s->uav);
   ID3D11DeviceContext_Dispatch(c->context, (c->layout.width+7)/8, (c->layout.height+7)/8, 1);
   record_pyrowave_d11_end(c);
   record_pyrowave_d11_copy(c, s->readback, 0, 0, 16);
   ID3D11DeviceContext_End(c->context, (ID3D11Asynchronous*)s->query);
   s->state = RPY_D11_DECODING;
   return RECORD_GPU_QUEUE_OK;
}

uint64_t record_pyrowave_d3d11_source_retired(const record_pyrowave_d3d11_t *c)
{
   return c ? c->retired : 0;
}

int record_pyrowave_d3d11_take(record_pyrowave_d3d11_t *c, record_gpu_frame_t *frame)
{
   int result;
   if (!c || c->decode || !frame) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, frame);
   if (result == RECORD_GPU_QUEUE_OK) c->owned |= (unsigned)1 << frame->slot;
   return result;
}

int record_pyrowave_d3d11_packet(record_pyrowave_d3d11_t *c, unsigned slot, record_pyrowave_packet_t *packet)
{
   record_pyrowave_d11_slot_t *s;
   int ready;
   uint32_t bytes;
   if (!c || c->decode || !packet || slot >= c->count || !(c->owned & ((unsigned)1 << slot))) return RECORD_GPU_QUEUE_ERROR;
   s = &c->slots[slot]; ready = retro_atomic_load_acquire_int(&s->ready);
   if (!ready) return RECORD_GPU_QUEUE_BUSY;
   if (ready != 1) return RECORD_GPU_QUEUE_ERROR;
   bytes = record_pyrowave_d11_load32(s->host + 4);
   if (bytes < 8 || bytes > c->layout.maximum_bytes || record_pyrowave_d11_load32(s->host + 12)) return RECORD_GPU_QUEUE_ERROR;
   packet->data = s->host + 16; packet->bytes = bytes;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_d3d11_release(record_pyrowave_d3d11_t *c, unsigned slot)
{
   if (!c || c->decode || slot >= c->count || !(c->owned & ((unsigned)1 << slot))
         || !retro_atomic_load_acquire_int(&c->slots[slot].ready)) return false;
   retro_atomic_store_release_int(&c->slots[slot].ready, 3);
   c->owned &= ~((unsigned)1 << slot);
   return record_gpu_queue_release(c->queue, slot);
}

int record_pyrowave_d3d11_receive_acquire(record_pyrowave_d3d11_t *c, void **data, size_t *capacity)
{
   int result;
   if (data) *data = NULL;
   if (capacity) *capacity = 0;
   if (!c || !c->decode || !data || !capacity) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_acquire(c->queue, &c->reservation);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   *data = c->slots[c->reservation].host + c->layout.wire_offset;
   *capacity = c->layout.maximum_bytes;
   return RECORD_GPU_QUEUE_OK;
}

int record_pyrowave_d3d11_receive_submit(record_pyrowave_d3d11_t *c,
      size_t bytes, uint64_t timestamp, uint64_t duration)
{
   record_pyrowave_d11_slot_t *s;
   if (!c || !c->decode || c->reservation >= c->count || !c->next_ticket
         || !duration || timestamp > (uint64_t)-1 - duration) return RECORD_GPU_QUEUE_ERROR;
   s = &c->slots[c->reservation];
   if (rpyrowave_gpu_index_frame(&c->layout, s->host, c->layout.workspace_bytes, bytes, 0, &s->parameters)
         || s->parameters.usability != c->usability) return RECORD_GPU_QUEUE_ERROR;
   s->frame.timestamp = timestamp; s->frame.duration = duration; s->frame.ready_value = c->next_ticket;
   s->frame.slot = c->reservation; s->frame.keyframe = true;
   if (!record_gpu_queue_submit(c->queue, &s->frame)) return RECORD_GPU_QUEUE_ERROR;
   c->next_ticket++; c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_d3d11_receive_cancel(record_pyrowave_d3d11_t *c)
{
   if (!c || !c->decode || !record_gpu_queue_cancel(c->queue)) return false;
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return true;
}

int record_pyrowave_d3d11_receive_peek(record_pyrowave_d3d11_t *c, record_gpu_frame_t *frame)
{
   return c && c->decode ? record_gpu_queue_peek(c->queue, frame) : RECORD_GPU_QUEUE_ERROR;
}

int record_pyrowave_d3d11_receive_drop(record_pyrowave_d3d11_t *c, record_gpu_frame_t *frame)
{
   int result;
   if (!c || !c->decode || !frame) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, frame);
   if (result == RECORD_GPU_QUEUE_OK && !record_gpu_queue_release(c->queue, frame->slot)) return RECORD_GPU_QUEUE_ERROR;
   return result;
}

int record_pyrowave_d3d11_image(record_pyrowave_d3d11_t *c, record_pyrowave_d3d11_image_t *image)
{
   unsigned i, selected = RECORD_GPU_QUEUE_SLOTS;
   if (!c || !c->decode || !image) return RECORD_GPU_QUEUE_ERROR;
   for (i = 0; i < c->count; i++)
      if (c->slots[i].state == RPY_D11_READY && (selected == RECORD_GPU_QUEUE_SLOTS
               || c->slots[i].frame.ready_value < c->slots[selected].frame.ready_value)) selected = i;
   if (selected == RECORD_GPU_QUEUE_SLOTS) return RECORD_GPU_QUEUE_BUSY;
   image->frame = c->slots[selected].frame; image->view = c->slots[selected].view;
   image->texture_mode = c->color.texture_mode; c->slots[selected].state = RPY_D11_LENT;
   return RECORD_GPU_QUEUE_OK;
}
bool record_pyrowave_d3d11_image_drop(record_pyrowave_d3d11_t *c, unsigned slot)
{
   if (!c || !c->decode || slot >= c->count || c->slots[slot].state != RPY_D11_LENT) return false;
   c->slots[slot].state = RPY_D11_IDLE; return record_gpu_queue_release(c->queue, slot);
}
bool record_pyrowave_d3d11_present(record_pyrowave_d3d11_t *c, unsigned slot)
{
   if (!c || !c->decode || c->failed || c->quiescing || slot >= c->count || c->slots[slot].state != RPY_D11_LENT) return false;
   ID3D11DeviceContext_End(c->context, (ID3D11Asynchronous*)c->slots[slot].query);
   c->slots[slot].state = RPY_D11_DISPLAYING; return true;
}
bool record_pyrowave_d3d11_poll(record_pyrowave_d3d11_t *c)
{
   unsigned i;
   if (!c || c->failed) return false;
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_d11_slot_t *s = &c->slots[i];
      D3D11_MAPPED_SUBRESOURCE mapped;
      HRESULT hr;
      if (s->state == RPY_D11_MAPPED && retro_atomic_load_acquire_int(&s->ready) == 3)
      {
         ID3D11DeviceContext_Unmap(c->context, (ID3D11Resource*)s->readback, 0);
         s->host = NULL; s->state = RPY_D11_IDLE;
      }
      if (s->state != RPY_D11_ENCODING && s->state != RPY_D11_DECODING && s->state != RPY_D11_DISPLAYING) continue;
      hr = ID3D11DeviceContext_GetData(c->context, (ID3D11Asynchronous*)s->query, NULL, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
      if (hr == S_FALSE) continue;
      if (FAILED(hr)) { c->failed = true; return false; }
      if (s->state == RPY_D11_DISPLAYING)
      {
         s->state = RPY_D11_IDLE;
         if (!record_gpu_queue_release(c->queue, i)) { c->failed = true; return false; }
         continue;
      }
      hr = ID3D11DeviceContext_Map(c->context, (ID3D11Resource*)s->readback, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
      if (hr == DXGI_ERROR_WAS_STILL_DRAWING) continue;
      if (FAILED(hr)) { c->failed = true; return false; }
      if (c->decode)
      {
         bool valid = !record_pyrowave_d11_load32((const uint8_t*)mapped.pData + 12);
         ID3D11DeviceContext_Unmap(c->context, (ID3D11Resource*)s->readback, 0);
         if (!valid) { c->failed = true; return false; }
         s->state = RPY_D11_READY;
      }
      else
      {
         s->host = (uint8_t*)mapped.pData; s->state = RPY_D11_MAPPED;
         retro_atomic_store_release_int(&s->ready, 1);
      }
   }
   /* The immediate context executes queries in ticket order. */
   for (i = 0; !c->decode && i < c->count; i++)
   {
      unsigned j;
      bool advanced = false;
      for (j = 0; j < c->count; j++)
         if (c->slots[j].frame.ready_value == c->retired + 1
               && c->slots[j].state != RPY_D11_ENCODING)
         { c->retired++; advanced = true; break; }
      if (!advanced) break;
   }
   return true;
}
bool record_pyrowave_d3d11_idle(const record_pyrowave_d3d11_t *c)
{
   unsigned i;
   if (!c) return false;
   for (i = 0; i < c->count; i++)
      if (c->slots[i].state == RPY_D11_ENCODING || c->slots[i].state == RPY_D11_DECODING
            || c->slots[i].state == RPY_D11_DISPLAYING) return false;
   return true;
}
bool record_pyrowave_d3d11_finish(record_pyrowave_d3d11_t *c)
{ return c && record_gpu_queue_finish(c->queue); }
bool record_pyrowave_d3d11_free(record_pyrowave_d3d11_t *c)
{
   if (!c) return true;
   if (!record_pyrowave_d3d11_idle(c) || !record_gpu_queue_free(c->queue)) return false;
   record_pyrowave_d11_destroy(c); return true;
}
bool record_pyrowave_d3d11_quiesce(record_pyrowave_d3d11_t *c)
{
   HRESULT hr;
   if (!c) return false;
   if (FAILED(ID3D11Device_GetDeviceRemovedReason(c->device))) return true;
   if (!c->quiescing)
   {
      ID3D11DeviceContext_End(c->context, (ID3D11Asynchronous*)c->retirement);
      ID3D11DeviceContext_Flush(c->context); c->quiescing = true;
   }
   hr = ID3D11DeviceContext_GetData(c->context, (ID3D11Asynchronous*)c->retirement, NULL, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
   if (hr == S_OK) c->quiesced = true;
   return c->quiesced;
}
bool record_pyrowave_d3d11_abandon(record_pyrowave_d3d11_t *c)
{
   record_gpu_frame_t frame;
   unsigned i;
   if (!c) return true;
   if (!c->quiesced && SUCCEEDED(ID3D11Device_GetDeviceRemovedReason(c->device)) && !record_pyrowave_d3d11_idle(c)) return false;
   if (c->reservation != RECORD_GPU_QUEUE_SLOTS) record_gpu_queue_cancel(c->queue);
   for (i = 0; i < c->count; i++) if (record_gpu_queue_take(c->queue, &frame) != RECORD_GPU_QUEUE_OK) break;
   for (i = 0; i < c->count; i++) record_gpu_queue_release(c->queue, i);
   if (!record_gpu_queue_free(c->queue)) return false;
   record_pyrowave_d11_destroy(c); return true;
}
RECORD_PYROWAVE_SESSION_BINDING(record_pyrowave_d3d11, record_pyrowave_d3d11_t, record_pyrowave_d3d11_receive_acquire)
