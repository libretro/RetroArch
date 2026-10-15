/* SPDX-License-Identifier: MIT */
#include <formats/rav1_hw.h>
#include <retro_atomic.h>

#ifdef RETRO_ATOMIC_HAS_PTR
static retro_atomic_ptr_t rav1_backend;
#define RAV1_BACKEND_GET()                                                     \
   ((const rav1_hw_backend_t *)retro_atomic_load_acquire_ptr(&rav1_backend))
#else
static const rav1_hw_backend_t *rav1_backend;
#define RAV1_BACKEND_GET() (rav1_backend)
#endif

void rav1_set_hw_backend(const rav1_hw_backend_t *backend)
{
#ifdef RETRO_ATOMIC_HAS_PTR
   retro_atomic_store_release_ptr(&rav1_backend, (void *)backend);
#else
   rav1_backend = backend;
#endif
}

bool rav1_hw_backend_open(const rav1_sequence_t *sequence, unsigned slots,
                          rav1_hw_ops_t *ops, rav1_hw_backend_t *backend)
{
   const rav1_hw_backend_t *factory = RAV1_BACKEND_GET();
   if (!ops || !backend)
      return false;
   memset(ops, 0, sizeof(*ops));
   memset(backend, 0, sizeof(*backend));
   if (!sequence || slots < 9 || slots > 32 || !factory || !factory->open ||
       !factory->close)
      return false;
   if (!factory->open(factory->user, sequence, slots, ops) || !ops->decoder ||
       !ops->decode || !ops->output)
   {
      if (ops->decoder)
         factory->close(ops);
      memset(ops, 0, sizeof(*ops));
      return false;
   }
   *backend = *factory;
   return true;
}
