#include "Set.h"

static inline unsigned zeroes(uint32_t v)
{
  unsigned int c = 32;
  v &= -signed(v);
  
  if (v) c--;
  if (v & 0x0000FFFF) c -= 16;
  if (v & 0x00FF00FF) c -=  8;
  if (v & 0x0F0F0F0F) c -=  4;
  if (v & 0x33333333) c -=  2;
  if (v & 0x55555555) c -=  1;
  
  return c;
}

bool Set::init(size_t start, size_t size)
{
  _start = start;
  _size = size;
  _count = (size + 31) / 32;
  _bits = (uint32_t*)calloc(sizeof(uint32_t), _count);

  return _bits != NULL;
}

bool Set::first(size_t* addr) const
{
  const uint32_t* bits = _bits;
  size_t count = _count;
  
  for (; count; --count)
  {
    if (*bits != 0)
    {
      size_t a = (bits - _bits) * 32 + zeroes(*bits);
      
      if (a < _size)
      {
        *addr = _start + a;
        return true;
      }
      
      return false;
    }
  }
  
  return false;
}

bool Set::next(size_t* addr) const
{
  size_t a = *addr - _start;
  const uint32_t* bits = _bits + a / 32;
  uint32_t bit = 2 << (a & 31);
  
  for (a++; bit != 0; bit <<= 1, a++)
  {
    if ((*bits & bit) != 0)
    {
      if (a < _size)
      {
        *addr = _start + a;
        return true;
      }
      
      return false;
    }
  }
  
  const uint32_t* end = _bits + _count;
  
  for (bits++; bits < end; bits++)
  {
    if (*bits != 0)
    {
      a = (bits - _bits) * 32 + zeroes(*bits);
      
      if (a < _size)
      {
        *addr = _start + a;
        return true;
      }
      
      return false;
    }
  }
  
  return false;
}

Set* Set::union_(const Set* other) const
{
  if (_start != other->_start || _size != other->_size)
  {
    return NULL;
  }
  
  Set* addrs = new Set;
  
  if (!addrs->init(_start, _size))
  {
    delete addrs;
    return NULL;
  }
  
  const uint32_t* bits1 = _bits;
  const uint32_t* bits2 = other->_bits;
  uint32_t* bits3 = addrs->_bits;

  size_t count = _count;
  
  if (count)
  {
    do
    {
      *bits3++ = *bits1++ | *bits2++;
    }
    while (--count);
  }
  
  return addrs;
}

Set* Set::intersection(const Set* other) const
{
  if (_start != other->_start || _size != other->_size)
  {
    return NULL;
  }
  
  Set* addrs = new Set;
  
  if (!addrs->init(_start, _size))
  {
    delete addrs;
    return NULL;
  }
  
  const uint32_t* bits1 = _bits;
  const uint32_t* bits2 = other->_bits;
  uint32_t* bits3 = addrs->_bits;

  size_t count = _count;
  
  if (count)
  {
    do
    {
      *bits3++ = *bits1++ & *bits2++;
    }
    while (--count);
  }
  
  return addrs;
}

Set* Set::difference(const Set* other) const
{
  if (_start != other->_start || _size != other->_size)
  {
    return NULL;
  }
  
  Set* addrs = new Set;
  
  if (!addrs->init(_start, _size))
  {
    delete addrs;
    return NULL;
  }
  
  const uint32_t* bits1 = _bits;
  const uint32_t* bits2 = other->_bits;
  uint32_t* bits3 = addrs->_bits;

  size_t count = _count;
  
  if (count)
  {
    do
    {
      *bits3++ = *bits1++ & ~*bits2++;
    }
    while (--count);
  }
  
  return addrs;
}

Set* Set::negate() const
{
  Set* addrs = new Set;
  
  if (!addrs->init(_start, _size))
  {
    delete addrs;
    return NULL;
  }
  
  const uint32_t* bits1 = _bits;
  uint32_t* bits2 = addrs->_bits;

  size_t count = _count;
  
  if (count)
  {
    do
    {
      *bits2++ = ~*bits1++;
    }
    while (--count);
  }
  
  return addrs;
}
