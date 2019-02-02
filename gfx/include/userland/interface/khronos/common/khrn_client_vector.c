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

#include "interface/khronos/common/khrn_int_common.h"

#include "interface/khronos/common/khrn_client_platform.h"
#include "interface/khronos/common/khrn_client_vector.h"

#include <stdlib.h>
#include <string.h>

void khrn_vector_init(KHRN_VECTOR_T *vector, uint32_t capacity)
{
   vector->data = (capacity == 0) ? NULL : khrn_platform_malloc(capacity, "KHRN_VECTOR_T.data");
   vector->capacity = vector->data ? capacity : 0;
   vector->size = 0;
}

void khrn_vector_term(KHRN_VECTOR_T *vector)
{
   if (vector->data) {
      khrn_platform_free(vector->data);
   }
}

bool khrn_vector_extend(KHRN_VECTOR_T *vector, uint32_t size)
{
   uint32_t req_capacity = vector->size + size;
   if (req_capacity > vector->capacity) {
      uint32_t new_capacity = _max(vector->capacity + (vector->capacity >> 1), req_capacity);
      void *new_data = khrn_platform_malloc(new_capacity, "KHRN_VECTOR_T.data");
      if (!new_data) {
         new_capacity = req_capacity;
         new_data = khrn_platform_malloc(new_capacity, "KHRN_VECTOR_T.data");
         if (!new_data) {
            return false;
         }
      }
      if (vector->data) {
         memcpy(new_data, vector->data, vector->size);
         khrn_platform_free(vector->data);
      }
      vector->data = new_data;
      vector->capacity = new_capacity;
   }
   vector->size += size;
   return true;
}

void khrn_vector_clear(KHRN_VECTOR_T *vector)
{
   if (vector->data) {
      khrn_platform_free(vector->data);
   }
   vector->data = NULL;
   vector->capacity = 0;
   vector->size = 0;
}
