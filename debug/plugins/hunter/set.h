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

  inline bool compatible(const Set* other) const
  {
    return _start == other->_start && _size == other->_size;
  }

  inline size_t start() const
  {
    return _start;
  }
  
  inline size_t size() const
  {
    return _size;
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

  size_t count() const;
  
  bool union_(Set* res, const Set* other) const;
  bool intersection(Set* res, const Set* other) const;
  bool difference(Set* res, const Set* other) const;
  bool negate(Set* res) const;
  
protected:
  size_t    _start;
  size_t    _size;
  size_t    _count;
  uint32_t* _bits;
};
