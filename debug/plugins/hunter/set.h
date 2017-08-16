#pragma once

#include "Block.h"

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

class Set
{
public:
  inline Set(): _start(0), _size(0), _count(0), _bits(NULL) {}

  inline Set(size_t start, size_t size)
  {
    init(start, size);
  }

  inline Set(const Set& other)
  {
    init(other._start, other._size);
    memcpy(_bits, other._bits, _count * 4);
  }

  inline Set& operator=(const Set& other)
  {
    if (&other != this)
    {
      free((void*)_bits);
      init(other._start, other._size);
      memcpy(_bits, other._bits, _count * 4);
    }

    return *this;
  }

  inline ~Set()
  {
    free((void*)_bits);
    _bits = NULL;
  }
  
  void init(size_t start, size_t size);
  
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
  
  void union_(Set* res, const Set* other) const;
  void intersection(Set* res, const Set* other) const;
  void difference(Set* res, const Set* other) const;
  void negate(Set* res) const;
  
protected:
  size_t    _start;
  size_t    _size;
  size_t    _count;
  uint32_t* _bits;
};
