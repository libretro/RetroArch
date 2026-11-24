/* Built-in shader blobs borrow executable data for the process lifetime. */
#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif
#include "d3d_builtin.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef D3D_BUILTIN_TEST
#define D3D_BUILTIN_SM2
#define D3D_BUILTIN_SM3
#endif
#if defined(HAVE_D3D10) || defined(HAVE_D3D11)
#define D3D_BUILTIN_SM4
#endif
#if defined(HAVE_D3D11) || defined(HAVE_D3D12)
#define D3D_BUILTIN_SM5
#endif
#ifdef HAVE_D3D12
#define D3D_BUILTIN_SM6
#endif
typedef struct d3d_builtin_record
{
#ifdef D3D_BUILTIN_TEST
   uint64_t fingerprint;
   size_t source_bytes;
#endif
   const char *program, *entry, *profile;
   const void *data;
   size_t bytes;
} d3d_builtin_record_t;
#ifdef D3D_BUILTIN_TEST
#define D3D_BUILTIN_SOURCE(hash, bytes) hash, bytes,
#else
#define D3D_BUILTIN_SOURCE(hash, bytes)
#endif
#include "d3d_builtin_data.h"
typedef struct d3d_builtin_blob
{
   ID3DBlob iface;
   const d3d_builtin_record_t *record;
   volatile LONG references;
} d3d_builtin_blob_t;
static ULONG STDMETHODCALLTYPE d3d_builtin_add_ref(ID3DBlob *iface)
{ return (ULONG)InterlockedIncrement(&((d3d_builtin_blob_t*)iface)->references); }
static ULONG STDMETHODCALLTYPE d3d_builtin_release(ID3DBlob *iface)
{
   d3d_builtin_blob_t *blob = (d3d_builtin_blob_t*)iface;
   ULONG count = (ULONG)InterlockedDecrement(&blob->references);
   if (!count) free(blob);
   return count;
}
static HRESULT STDMETHODCALLTYPE d3d_builtin_query(ID3DBlob *iface, REFIID iid, void **out)
{
   if (!out) return E_POINTER;
   *out = NULL;
   if (memcmp(iid, &IID_IUnknown, sizeof(*iid)) && memcmp(iid, &IID_ID3DBlob, sizeof(*iid)))
      return E_NOINTERFACE;
   *out = iface; d3d_builtin_add_ref(iface);
   return S_OK;
}
static LPVOID STDMETHODCALLTYPE d3d_builtin_pointer(ID3DBlob *iface)
{ return (LPVOID)((d3d_builtin_blob_t*)iface)->record->data; }
static SIZE_T STDMETHODCALLTYPE d3d_builtin_size(ID3DBlob *iface)
{ return ((d3d_builtin_blob_t*)iface)->record->bytes; }
static bool d3d_builtin_wrap(const d3d_builtin_record_t *record, D3DBlob *out)
{
   static ID3D10BlobVtbl methods = { d3d_builtin_query, d3d_builtin_add_ref,
      d3d_builtin_release, d3d_builtin_pointer, d3d_builtin_size };
   d3d_builtin_blob_t *blob;
   if (!out) return false;
   blob = (d3d_builtin_blob_t*)malloc(sizeof(*blob));
   if (!blob) return false;
   blob->iface.lpVtbl = &methods; blob->record = record; blob->references = 1;
   *out = &blob->iface;
   return true;
}
#ifdef D3D_BUILTIN_TEST
bool d3d_builtin_shader(const char *source, size_t size,
      const char *entry, const char *profile, D3DBlob *out)
{
   uint64_t hash = UINT64_C(14695981039346656037);
   size_t i;
   if (!source || !entry || !profile || !out) return false;
   *out = NULL;
   if (!size) size = strlen(source);
   while (size && !source[size - 1]) size--;
   for (i = 0; i < size; i++) hash = (hash ^ (unsigned char)source[i]) * UINT64_C(1099511628211);
   for (i = 0; d3d_builtin_records[i].program; i++)
   {
      const d3d_builtin_record_t *record = &d3d_builtin_records[i];
      if (record->source_bytes == size && record->fingerprint == hash
            && !strcmp(record->entry, entry) && !strcmp(record->profile, profile))
         return d3d_builtin_wrap(record, out);
   }
   return false;
}
#endif
bool d3d_builtin_shader_named(const char *program, const char *entry,
      const char *profile, D3DBlob *out)
{
   size_t i;
   if (!program || !entry || !profile || !out) return false;
   *out = NULL;
   for (i = 0; d3d_builtin_records[i].program; i++)
   {
      const d3d_builtin_record_t *record = &d3d_builtin_records[i];
      if (!strcmp(record->program, program) && !strcmp(record->entry, entry) && !strcmp(record->profile, profile))
         return d3d_builtin_wrap(record, out);
   }
   return false;
}
