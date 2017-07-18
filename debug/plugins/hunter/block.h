#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

class Set;

class Block
{
public:
  bool init(size_t start, size_t size, const uint8_t* bytes);
  
  inline void destroy()
  {
    free((void*)_bytes);
  }
  
  inline size_t start() const
  {
    return _start;
  }
  
  inline size_t size() const
  {
    return _size;
  }
  
  inline uint8_t at8(size_t addr)
  {
    return _bytes[addr - _start];
  }
  
  inline uint16_t at16LE(size_t addr)
  {
    addr -= _start;
    return _bytes[addr] | _bytes[addr + 1] << 8;
  }
  
  inline uint16_t at16BE(size_t addr)
  {
    addr -= _start;
    return _bytes[addr] << 8 | _bytes[addr + 1];
  }
  
  inline uint32_t at32LE(size_t addr)
  {
    addr -= _start;
    return _bytes[addr] | _bytes[addr + 1] << 8 | _bytes[addr + 2] << 16 | _bytes[addr + 3] << 24;
  }
  
  inline uint32_t at32BE(size_t addr)
  {
    addr -= _start;
    return _bytes[addr] << 24 | _bytes[addr + 1] << 16 | _bytes[addr + 2] << 8 | _bytes[addr + 3];
  }

  Set* bits(uint8_t m, uint8_t v2) const;

  template<int N>
  Set* bit(uint8_t b) const
  {
    return bits(1 << N, (b & 1) << N);
  }

  Set* eqlow(uint8_t v2) const;
  Set* nelow(uint8_t v2) const;
  Set* ltlow(uint8_t v2) const;
  Set* lelow(uint8_t v2) const;
  Set* gtlow(uint8_t v2) const;
  Set* gelow(uint8_t v2) const;

  Set* eqhigh(uint8_t v2) const;
  Set* nehigh(uint8_t v2) const;
  Set* lthigh(uint8_t v2) const;
  Set* lehigh(uint8_t v2) const;
  Set* gthigh(uint8_t v2) const;
  Set* gehigh(uint8_t v2) const;
  
  Set* eq8(uint8_t v2) const;
  Set* ne8(uint8_t v2) const;
  Set* lt8(uint8_t v2) const;
  Set* le8(uint8_t v2) const;
  Set* gt8(uint8_t v2) const;
  Set* ge8(uint8_t v2) const;

  Set* eq16LE(uint16_t v2) const;
  Set* ne16LE(uint16_t v2) const;
  Set* lt16LE(uint16_t v2) const;
  Set* le16LE(uint16_t v2) const;
  Set* gt16LE(uint16_t v2) const;
  Set* ge16LE(uint16_t v2) const;

  Set* eq16BE(uint16_t v2) const;
  Set* ne16BE(uint16_t v2) const;
  Set* lt16BE(uint16_t v2) const;
  Set* le16BE(uint16_t v2) const;
  Set* gt16BE(uint16_t v2) const;
  Set* ge16BE(uint16_t v2) const;

  Set* eq32LE(uint32_t v2) const;
  Set* ne32LE(uint32_t v2) const;
  Set* lt32LE(uint32_t v2) const;
  Set* le32LE(uint32_t v2) const;
  Set* gt32LE(uint32_t v2) const;
  Set* ge32LE(uint32_t v2) const;

  Set* eq32BE(uint32_t v2) const;
  Set* ne32BE(uint32_t v2) const;
  Set* lt32BE(uint32_t v2) const;
  Set* le32BE(uint32_t v2) const;
  Set* gt32BE(uint32_t v2) const;
  Set* ge32BE(uint32_t v2) const;
  
protected:
  size_t   _start;
  size_t   _size;
  uint8_t* _bytes;
};
