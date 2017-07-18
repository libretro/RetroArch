#pragma once

#include "Block.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

class Set
{
public:
  bool init(size_t start, size_t size);
  
  inline void destroy()
  {
    free(_bits);
  }
  
  inline void add(size_t addr)
  {
    addr -= _start;
    _bits[addr / 32] |= 1 << (addr & 31);
  }
  
  inline bool contains(size_t addr) const
  {
    addr -= _start;
    return (_bits[addr / 32] & (1 << (addr & 31))) != 0;
  }
  
  bool first(size_t* addr) const;
  bool next(size_t* addr) const;
  
  Set* union_(const Set* other) const;
  Set* intersection(const Set* other) const;
  Set* difference(const Set* other) const;
  Set* negate() const;
  
protected:
  size_t    _start;
  size_t    _size;
  size_t    _count;
  uint32_t* _bits;
};
