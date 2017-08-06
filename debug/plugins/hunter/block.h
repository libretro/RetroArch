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
    _bytes = NULL;
  }
  
  inline bool compatible(const Block* other) const
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

  bool bits(Set* set, uint8_t m, uint8_t v2) const;

  template<int N>
  bool bit(Set* set, uint8_t b) const
  {
    return bits(set, 1 << N, (b & 1) << N);
  }

  bool eqlow(Set* set, uint8_t v2) const;
  bool nelow(Set* set, uint8_t v2) const;
  bool ltlow(Set* set, uint8_t v2) const;
  bool lelow(Set* set, uint8_t v2) const;
  bool gtlow(Set* set, uint8_t v2) const;
  bool gelow(Set* set, uint8_t v2) const;

  bool eqhigh(Set* set, uint8_t v2) const;
  bool nehigh(Set* set, uint8_t v2) const;
  bool lthigh(Set* set, uint8_t v2) const;
  bool lehigh(Set* set, uint8_t v2) const;
  bool gthigh(Set* set, uint8_t v2) const;
  bool gehigh(Set* set, uint8_t v2) const;
  
  bool eq8(Set* set, uint8_t v2) const;
  bool ne8(Set* set, uint8_t v2) const;
  bool lt8(Set* set, uint8_t v2) const;
  bool le8(Set* set, uint8_t v2) const;
  bool gt8(Set* set, uint8_t v2) const;
  bool ge8(Set* set, uint8_t v2) const;

  bool eq16LE(Set* set, uint16_t v2) const;
  bool ne16LE(Set* set, uint16_t v2) const;
  bool lt16LE(Set* set, uint16_t v2) const;
  bool le16LE(Set* set, uint16_t v2) const;
  bool gt16LE(Set* set, uint16_t v2) const;
  bool ge16LE(Set* set, uint16_t v2) const;

  bool eq16BE(Set* set, uint16_t v2) const;
  bool ne16BE(Set* set, uint16_t v2) const;
  bool lt16BE(Set* set, uint16_t v2) const;
  bool le16BE(Set* set, uint16_t v2) const;
  bool gt16BE(Set* set, uint16_t v2) const;
  bool ge16BE(Set* set, uint16_t v2) const;

  bool eq32LE(Set* set, uint32_t v2) const;
  bool ne32LE(Set* set, uint32_t v2) const;
  bool lt32LE(Set* set, uint32_t v2) const;
  bool le32LE(Set* set, uint32_t v2) const;
  bool gt32LE(Set* set, uint32_t v2) const;
  bool ge32LE(Set* set, uint32_t v2) const;

  bool eq32BE(Set* set, uint32_t v2) const;
  bool ne32BE(Set* set, uint32_t v2) const;
  bool lt32BE(Set* set, uint32_t v2) const;
  bool le32BE(Set* set, uint32_t v2) const;
  bool gt32BE(Set* set, uint32_t v2) const;
  bool ge32BE(Set* set, uint32_t v2) const;

  bool eqlow(Set* set, const Block* other) const;
  bool nelow(Set* set, const Block* other) const;
  bool ltlow(Set* set, const Block* other) const;
  bool lelow(Set* set, const Block* other) const;
  bool gtlow(Set* set, const Block* other) const;
  bool gelow(Set* set, const Block* other) const;

  bool eqhigh(Set* set, const Block* other) const;
  bool nehigh(Set* set, const Block* other) const;
  bool lthigh(Set* set, const Block* other) const;
  bool lehigh(Set* set, const Block* other) const;
  bool gthigh(Set* set, const Block* other) const;
  bool gehigh(Set* set, const Block* other) const;

  bool eq8(Set* set, const Block* other) const;
  bool ne8(Set* set, const Block* other) const;
  bool lt8(Set* set, const Block* other) const;
  bool le8(Set* set, const Block* other) const;
  bool gt8(Set* set, const Block* other) const;
  bool ge8(Set* set, const Block* other) const;

  bool eq16LE(Set* set, const Block* other) const;
  bool ne16LE(Set* set, const Block* other) const;
  bool lt16LE(Set* set, const Block* other) const;
  bool le16LE(Set* set, const Block* other) const;
  bool gt16LE(Set* set, const Block* other) const;
  bool ge16LE(Set* set, const Block* other) const;

  bool eq16BE(Set* set, const Block* other) const;
  bool ne16BE(Set* set, const Block* other) const;
  bool lt16BE(Set* set, const Block* other) const;
  bool le16BE(Set* set, const Block* other) const;
  bool gt16BE(Set* set, const Block* other) const;
  bool ge16BE(Set* set, const Block* other) const;

  bool eq32LE(Set* set, const Block* other) const;
  bool ne32LE(Set* set, const Block* other) const;
  bool lt32LE(Set* set, const Block* other) const;
  bool le32LE(Set* set, const Block* other) const;
  bool gt32LE(Set* set, const Block* other) const;
  bool ge32LE(Set* set, const Block* other) const;

  bool eq32BE(Set* set, const Block* other) const;
  bool ne32BE(Set* set, const Block* other) const;
  bool lt32BE(Set* set, const Block* other) const;
  bool le32BE(Set* set, const Block* other) const;
  bool gt32BE(Set* set, const Block* other) const;
  bool ge32BE(Set* set, const Block* other) const;
  
protected:
  size_t   _start;
  size_t   _size;
  uint8_t* _bytes;
};
