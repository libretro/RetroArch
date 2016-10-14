//in the future asm and new c++ features can be added to speed up copying
#include <stdint.h>
#include <string.h>
#include <retro_inline.h>

static INLINE void* memcpy16(void* dst,void* src,size_t size){
   return memcpy(dst,src,size * 2);
}

static INLINE void* memcpy32(void* dst,void* src,size_t size){
   return memcpy(dst,src,size * 4);
}

static INLINE void* memcpy64(void* dst,void* src,size_t size){
   return memcpy(dst,src,size * 8);
}

#ifdef USECPPSTDFILL
#include <algorithm>

static INLINE void* memset16(void* dst,uint16_t val,size_t size){
   uint16_t* typedptr = (uint16_t*)dst;
   std::fill(typedptr, typedptr + size, val);
   return dst;
}

static INLINE void* memset32(void* dst,uint32_t val,size_t size){
   uint32_t* typedptr = (uint32_t*)dst;
   std::fill(typedptr, typedptr + size, val);
   return dst;
}

static INLINE void* memset64(void* dst,uint64_t val,size_t size){
   uint64_t* typedptr = (uint64_t*)dst;
   std::fill(typedptr, typedptr + size, val);
   return dst;
}
#else

static INLINE void* memset16(void* dst,uint16_t val,size_t size){
   uint16_t* typedptr = (uint16_t*)dst;
   size_t i;
   for(i = 0;i < size;i++){
      typedptr[i] = val;
   }
   return dst;
}

static INLINE void* memset32(void* dst,uint32_t val,size_t size){
   uint32_t* typedptr = (uint32_t*)dst;
   size_t i;
   for(i = 0;i < size;i++){
      typedptr[i] = val;
   }
   return dst;
}

static INLINE void* memset64(void* dst,uint64_t val,size_t size){
   uint64_t* typedptr = (uint64_t*)dst;
   size_t i;
   for(i = 0;i < size;i++){
      typedptr[i] = val;
   }
   return dst;
}
#endif
