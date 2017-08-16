#include "block.h"
#include "set.h"

#include <string.h>

void Block::init(size_t start, size_t size, const uint8_t* bytes)
{
  _start = start;
  _size = size;
  _bytes = new uint8_t[size];
  memcpy(_bytes, bytes, size);
}

#define FILTER_8(cmp) \
  set->init(_start, _size); \
  const uint8_t* byte = _bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size) \
    do { \
      uint8_t v1 = *byte++; \
      if (cmp) \
        set->add(addr); \
      addr++; \
    } \
    while (--size); \
  return true;

#define FILTER_16_LE(cmp) \
  set->init(_start, _size); \
  const uint8_t* byte = _bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size > 1) { \
    uint16_t v1 = *byte++ << 8; \
    size--; \
    do { \
      v1 = v1 >> 8 | *byte++ << 8; \
      if (cmp) \
        set->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return true;

#define FILTER_16_BE(cmp) \
  set->init(_start, _size); \
  const uint8_t* byte = _bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size > 1) { \
    uint16_t v1 = *byte++; \
    size--; \
    do { \
      v1 = v1 << 8 | *byte++; \
      if (cmp) \
        set->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return true;

#define FILTER_32_LE(cmp) \
  set->init(_start, _size); \
  const uint8_t* byte = _bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size > 3) { \
    uint32_t v1 = byte[0] << 8 | byte[1] << 16 | byte[2] << 24; \
    byte += 3; \
    size -= 3; \
    do { \
      v1 = v1 >> 8 | *byte++ << 24; \
      if (cmp) \
        set->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return true;

#define FILTER_32_BE(cmp) \
  set->init(_start, _size); \
  const uint8_t* byte = _bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size > 3) { \
    uint32_t v1 = byte[0] << 16 | byte[1] << 8 | byte[2]; \
    byte += 3; \
    size -= 3; \
    do { \
      v1 = v1 << 8 | *byte++; \
      if (cmp) \
        set->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return true;

bool Block::bits(Set* set, uint8_t m, uint8_t v2) const { FILTER_8((v1 & m) == v2) }

bool Block::eqlow(Set* set, uint8_t v2) const { FILTER_8((v1 & 15) == v2) }
bool Block::nelow(Set* set, uint8_t v2) const { FILTER_8((v1 & 15) != v2) }
bool Block::ltlow(Set* set, uint8_t v2) const { FILTER_8((v1 & 15) <  v2) }
bool Block::lelow(Set* set, uint8_t v2) const { FILTER_8((v1 & 15) <= v2) }
bool Block::gtlow(Set* set, uint8_t v2) const { FILTER_8((v1 & 15) >  v2) }
bool Block::gelow(Set* set, uint8_t v2) const { FILTER_8((v1 & 15) >= v2) }

bool Block::eqhigh(Set* set, uint8_t v2) const { FILTER_8((v1 >> 4) == v2) }
bool Block::nehigh(Set* set, uint8_t v2) const { FILTER_8((v1 >> 4) != v2) }
bool Block::lthigh(Set* set, uint8_t v2) const { FILTER_8((v1 >> 4) <  v2) }
bool Block::lehigh(Set* set, uint8_t v2) const { FILTER_8((v1 >> 4) <= v2) }
bool Block::gthigh(Set* set, uint8_t v2) const { FILTER_8((v1 >> 4) >  v2) }
bool Block::gehigh(Set* set, uint8_t v2) const { FILTER_8((v1 >> 4) >= v2) }

bool Block::eq8(Set* set, uint8_t v2) const { FILTER_8(v1 == v2) }
bool Block::ne8(Set* set, uint8_t v2) const { FILTER_8(v1 != v2) }
bool Block::lt8(Set* set, uint8_t v2) const { FILTER_8(v1 <  v2) }
bool Block::le8(Set* set, uint8_t v2) const { FILTER_8(v1 <= v2) }
bool Block::gt8(Set* set, uint8_t v2) const { FILTER_8(v1 >  v2) }
bool Block::ge8(Set* set, uint8_t v2) const { FILTER_8(v1 >= v2) }

bool Block::eq16LE(Set* set, uint16_t v2) const { FILTER_16_LE(v1 == v2) }
bool Block::ne16LE(Set* set, uint16_t v2) const { FILTER_16_LE(v1 != v2) }
bool Block::lt16LE(Set* set, uint16_t v2) const { FILTER_16_LE(v1 <  v2) }
bool Block::le16LE(Set* set, uint16_t v2) const { FILTER_16_LE(v1 <= v2) }
bool Block::gt16LE(Set* set, uint16_t v2) const { FILTER_16_LE(v1 >  v2) }
bool Block::ge16LE(Set* set, uint16_t v2) const { FILTER_16_LE(v1 >= v2) }

bool Block::eq16BE(Set* set, uint16_t v2) const { FILTER_16_BE(v1 == v2) }
bool Block::ne16BE(Set* set, uint16_t v2) const { FILTER_16_BE(v1 != v2) }
bool Block::lt16BE(Set* set, uint16_t v2) const { FILTER_16_BE(v1 <  v2) }
bool Block::le16BE(Set* set, uint16_t v2) const { FILTER_16_BE(v1 <= v2) }
bool Block::gt16BE(Set* set, uint16_t v2) const { FILTER_16_BE(v1 >  v2) }
bool Block::ge16BE(Set* set, uint16_t v2) const { FILTER_16_BE(v1 >= v2) }

bool Block::eq32LE(Set* set, uint32_t v2) const { FILTER_32_LE(v1 == v2) }
bool Block::ne32LE(Set* set, uint32_t v2) const { FILTER_32_LE(v1 != v2) }
bool Block::lt32LE(Set* set, uint32_t v2) const { FILTER_32_LE(v1 <  v2) }
bool Block::le32LE(Set* set, uint32_t v2) const { FILTER_32_LE(v1 <= v2) }
bool Block::gt32LE(Set* set, uint32_t v2) const { FILTER_32_LE(v1 >  v2) }
bool Block::ge32LE(Set* set, uint32_t v2) const { FILTER_32_LE(v1 >= v2) }

bool Block::eq32BE(Set* set, uint32_t v2) const { FILTER_32_BE(v1 == v2) }
bool Block::ne32BE(Set* set, uint32_t v2) const { FILTER_32_BE(v1 != v2) }
bool Block::lt32BE(Set* set, uint32_t v2) const { FILTER_32_BE(v1 <  v2) }
bool Block::le32BE(Set* set, uint32_t v2) const { FILTER_32_BE(v1 <= v2) }
bool Block::gt32BE(Set* set, uint32_t v2) const { FILTER_32_BE(v1 >  v2) }
bool Block::ge32BE(Set* set, uint32_t v2) const { FILTER_32_BE(v1 >= v2) }

#define FILTER_BLOCK_8(cmp) \
  if (!compatible(other)) \
    return false; \
  set->init(_start, _size); \
  const uint8_t* byte1 = _bytes; \
  const uint8_t* byte2 = other->_bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size) \
    do { \
      uint8_t v1 = *byte1++; \
      uint8_t v2 = *byte2++; \
      if (cmp) \
        set->add(addr); \
      addr++; \
    } \
    while (--size); \
  return true;

#define FILTER_BLOCK_16_LE(cmp) \
  if (!compatible(other)) \
    return false; \
  set->init(_start, _size); \
  const uint8_t* byte1 = _bytes; \
  const uint8_t* byte2 = other->_bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size > 1) { \
    uint16_t v1 = *byte1++ << 8; \
    uint16_t v2 = *byte2++ << 8; \
    size--; \
    do { \
      v1 = v1 >> 8 | *byte1++ << 8; \
      v2 = v2 >> 8 | *byte2++ << 8; \
      if (cmp) \
        set->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return true;

#define FILTER_BLOCK_16_BE(cmp) \
  if (!compatible(other)) \
    return false; \
  set->init(_start, _size); \
  const uint8_t* byte1 = _bytes; \
  const uint8_t* byte2 = other->_bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size > 1) { \
    uint16_t v1 = *byte1++; \
    uint16_t v2 = *byte2++; \
    size--; \
    do { \
      v1 = v1 << 8 | *byte1++; \
      v2 = v2 << 8 | *byte2++; \
      if (cmp) \
        set->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return true;

#define FILTER_BLOCK_32_LE(cmp) \
  if (!compatible(other)) \
    return false; \
  set->init(_start, _size); \
  const uint8_t* byte1 = _bytes; \
  const uint8_t* byte2 = other->_bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size > 3) { \
    uint32_t v1 = byte1[0] << 8 | byte1[1] << 16 | byte1[2] << 24; \
    uint32_t v2 = byte2[0] << 8 | byte2[1] << 16 | byte2[2] << 24; \
    byte1 += 3; \
    byte2 += 3; \
    size -= 3; \
    do { \
      v1 = v1 >> 8 | *byte1++ << 24; \
      v2 = v2 >> 8 | *byte2++ << 24; \
      if (cmp) \
        set->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return true;

#define FILTER_BLOCK_32_BE(cmp) \
  if (!compatible(other)) \
    return false; \
  set->init(_start, _size); \
  const uint8_t* byte1 = _bytes; \
  const uint8_t* byte2 = other->_bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size > 3) { \
    uint32_t v1 = byte1[0] << 16 | byte1[1] << 8 | byte1[2]; \
    uint32_t v2 = byte2[0] << 16 | byte2[1] << 8 | byte2[2]; \
    byte1 += 3; \
    byte2 += 3; \
    size -= 3; \
    do { \
      v1 = v1 << 8 | *byte1++; \
      v2 = v2 << 8 | *byte2++; \
      if (cmp) \
        set->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return true;

bool Block::eqlow(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 & 15) == (v2 & 15)) }
bool Block::nelow(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 & 15) != (v2 & 15)) }
bool Block::ltlow(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 & 15) <  (v2 & 15)) }
bool Block::lelow(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 & 15) <= (v2 & 15)) }
bool Block::gtlow(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 & 15) >  (v2 & 15)) }
bool Block::gelow(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 & 15) >= (v2 & 15)) }

bool Block::eqhigh(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 >> 4) == (v2 >> 4)) }
bool Block::nehigh(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 >> 4) != (v2 >> 4)) }
bool Block::lthigh(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 >> 4) <  (v2 >> 4)) }
bool Block::lehigh(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 >> 4) <= (v2 >> 4)) }
bool Block::gthigh(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 >> 4) >  (v2 >> 4)) }
bool Block::gehigh(Set* set, const Block* other) const { FILTER_BLOCK_8((v1 >> 4) >= (v2 >> 4)) }

bool Block::eq8(Set* set, const Block* other) const { FILTER_BLOCK_8(v1 == v2) }
bool Block::ne8(Set* set, const Block* other) const { FILTER_BLOCK_8(v1 != v2) }
bool Block::lt8(Set* set, const Block* other) const { FILTER_BLOCK_8(v1 <  v2) }
bool Block::le8(Set* set, const Block* other) const { FILTER_BLOCK_8(v1 <= v2) }
bool Block::gt8(Set* set, const Block* other) const { FILTER_BLOCK_8(v1 >  v2) }
bool Block::ge8(Set* set, const Block* other) const { FILTER_BLOCK_8(v1 >= v2) }

bool Block::eq16LE(Set* set, const Block* other) const { FILTER_BLOCK_16_LE(v1 == v2) }
bool Block::ne16LE(Set* set, const Block* other) const { FILTER_BLOCK_16_LE(v1 != v2) }
bool Block::lt16LE(Set* set, const Block* other) const { FILTER_BLOCK_16_LE(v1 <  v2) }
bool Block::le16LE(Set* set, const Block* other) const { FILTER_BLOCK_16_LE(v1 <= v2) }
bool Block::gt16LE(Set* set, const Block* other) const { FILTER_BLOCK_16_LE(v1 >  v2) }
bool Block::ge16LE(Set* set, const Block* other) const { FILTER_BLOCK_16_LE(v1 >= v2) }

bool Block::eq16BE(Set* set, const Block* other) const { FILTER_BLOCK_16_BE(v1 == v2) }
bool Block::ne16BE(Set* set, const Block* other) const { FILTER_BLOCK_16_BE(v1 != v2) }
bool Block::lt16BE(Set* set, const Block* other) const { FILTER_BLOCK_16_BE(v1 <  v2) }
bool Block::le16BE(Set* set, const Block* other) const { FILTER_BLOCK_16_BE(v1 <= v2) }
bool Block::gt16BE(Set* set, const Block* other) const { FILTER_BLOCK_16_BE(v1 >  v2) }
bool Block::ge16BE(Set* set, const Block* other) const { FILTER_BLOCK_16_BE(v1 >= v2) }

bool Block::eq32LE(Set* set, const Block* other) const { FILTER_BLOCK_32_LE(v1 == v2) }
bool Block::ne32LE(Set* set, const Block* other) const { FILTER_BLOCK_32_LE(v1 != v2) }
bool Block::lt32LE(Set* set, const Block* other) const { FILTER_BLOCK_32_LE(v1 <  v2) }
bool Block::le32LE(Set* set, const Block* other) const { FILTER_BLOCK_32_LE(v1 <= v2) }
bool Block::gt32LE(Set* set, const Block* other) const { FILTER_BLOCK_32_LE(v1 >  v2) }
bool Block::ge32LE(Set* set, const Block* other) const { FILTER_BLOCK_32_LE(v1 >= v2) }

bool Block::eq32BE(Set* set, const Block* other) const { FILTER_BLOCK_32_BE(v1 == v2) }
bool Block::ne32BE(Set* set, const Block* other) const { FILTER_BLOCK_32_BE(v1 != v2) }
bool Block::lt32BE(Set* set, const Block* other) const { FILTER_BLOCK_32_BE(v1 <  v2) }
bool Block::le32BE(Set* set, const Block* other) const { FILTER_BLOCK_32_BE(v1 <= v2) }
bool Block::gt32BE(Set* set, const Block* other) const { FILTER_BLOCK_32_BE(v1 >  v2) }
bool Block::ge32BE(Set* set, const Block* other) const { FILTER_BLOCK_32_BE(v1 >= v2) }
