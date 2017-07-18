#include "block.h"
#include "set.h"

#include <string.h>

bool Block::init(size_t start, size_t size, const uint8_t* bytes)
{
  _bytes = (uint8_t*)malloc(size);

  if (!_bytes)
  {
    return false;
  }

  _start = start;
  _size = size;
  memcpy(_bytes, bytes, size);
  return true;
}

#define FILTER_8(cmp) \
  Set* addrs = new Set; \
  if (!addrs->init(_start, _size)) { \
    delete addrs; \
    return 0; \
  } \
  const uint8_t* byte = _bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size) \
    do { \
      uint8_t v1 = *byte++; \
      if (cmp) \
        addrs->add(addr); \
      addr++; \
    } \
    while (--size); \
  return addrs;

#define FILTER_16_LE(cmp) \
  Set* addrs = new Set; \
  if (!addrs->init(_start, _size)) { \
    delete addrs; \
    return 0; \
  } \
  const uint8_t* byte = _bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size > 1) { \
    uint16_t v1 = *byte++ << 8; \
    size--; \
    do { \
      v1 = v1 >> 8 | *byte++ << 8; \
      if (cmp) \
        addrs->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return addrs;

#define FILTER_16_BE(cmp) \
  Set* addrs = new Set; \
  if (!addrs->init(_start, _size)) { \
    delete addrs; \
    return 0; \
  } \
  const uint8_t* byte = _bytes; \
  size_t size = _size; \
  size_t addr = _start; \
  if (size > 1) { \
    uint16_t v1 = *byte++; \
    size--; \
    do { \
      v1 = v1 << 8 | *byte++; \
      if (cmp) \
        addrs->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return addrs;

#define FILTER_32_LE(cmp) \
  Set* addrs = new Set; \
  if (!addrs->init(_start, _size)) { \
    delete addrs; \
    return 0; \
  } \
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
        addrs->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return addrs;

#define FILTER_32_BE(cmp) \
  Set* addrs = new Set; \
  if (!addrs->init(_start, _size)) { \
    delete addrs; \
    return 0; \
  } \
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
        addrs->add(addr); \
      addr++; \
    } \
    while (--size); \
  } \
  return addrs;

Set* Block::bits(uint8_t m, uint8_t v2) const { FILTER_8((v1 & m) == v2) }

Set* Block::eqlow(uint8_t v2) const { FILTER_8((v1 & 15) == v2) }
Set* Block::nelow(uint8_t v2) const { FILTER_8((v1 & 15) != v2) }
Set* Block::ltlow(uint8_t v2) const { FILTER_8((v1 & 15) <  v2) }
Set* Block::lelow(uint8_t v2) const { FILTER_8((v1 & 15) <= v2) }
Set* Block::gtlow(uint8_t v2) const { FILTER_8((v1 & 15) >  v2) }
Set* Block::gelow(uint8_t v2) const { FILTER_8((v1 & 15) >= v2) }

Set* Block::eqhigh(uint8_t v2) const { FILTER_8((v1 >> 4) == v2) }
Set* Block::nehigh(uint8_t v2) const { FILTER_8((v1 >> 4) != v2) }
Set* Block::lthigh(uint8_t v2) const { FILTER_8((v1 >> 4) <  v2) }
Set* Block::lehigh(uint8_t v2) const { FILTER_8((v1 >> 4) <= v2) }
Set* Block::gthigh(uint8_t v2) const { FILTER_8((v1 >> 4) >  v2) }
Set* Block::gehigh(uint8_t v2) const { FILTER_8((v1 >> 4) >= v2) }

Set* Block::eq8(uint8_t v2) const { FILTER_8(v1 == v2) }
Set* Block::ne8(uint8_t v2) const { FILTER_8(v1 != v2) }
Set* Block::lt8(uint8_t v2) const { FILTER_8(v1 <  v2) }
Set* Block::le8(uint8_t v2) const { FILTER_8(v1 <= v2) }
Set* Block::gt8(uint8_t v2) const { FILTER_8(v1 >  v2) }
Set* Block::ge8(uint8_t v2) const { FILTER_8(v1 >= v2) }

Set* Block::eq16LE(uint16_t v2) const { FILTER_16_LE(v1 == v2) }
Set* Block::ne16LE(uint16_t v2) const { FILTER_16_LE(v1 != v2) }
Set* Block::lt16LE(uint16_t v2) const { FILTER_16_LE(v1 <  v2) }
Set* Block::le16LE(uint16_t v2) const { FILTER_16_LE(v1 <= v2) }
Set* Block::gt16LE(uint16_t v2) const { FILTER_16_LE(v1 >  v2) }
Set* Block::ge16LE(uint16_t v2) const { FILTER_16_LE(v1 >= v2) }

Set* Block::eq16BE(uint16_t v2) const { FILTER_16_BE(v1 == v2) }
Set* Block::ne16BE(uint16_t v2) const { FILTER_16_BE(v1 != v2) }
Set* Block::lt16BE(uint16_t v2) const { FILTER_16_BE(v1 <  v2) }
Set* Block::le16BE(uint16_t v2) const { FILTER_16_BE(v1 <= v2) }
Set* Block::gt16BE(uint16_t v2) const { FILTER_16_BE(v1 >  v2) }
Set* Block::ge16BE(uint16_t v2) const { FILTER_16_BE(v1 >= v2) }

Set* Block::eq32LE(uint32_t v2) const { FILTER_32_LE(v1 == v2) }
Set* Block::ne32LE(uint32_t v2) const { FILTER_32_LE(v1 != v2) }
Set* Block::lt32LE(uint32_t v2) const { FILTER_32_LE(v1 <  v2) }
Set* Block::le32LE(uint32_t v2) const { FILTER_32_LE(v1 <= v2) }
Set* Block::gt32LE(uint32_t v2) const { FILTER_32_LE(v1 >  v2) }
Set* Block::ge32LE(uint32_t v2) const { FILTER_32_LE(v1 >= v2) }

Set* Block::eq32BE(uint32_t v2) const { FILTER_32_BE(v1 == v2) }
Set* Block::ne32BE(uint32_t v2) const { FILTER_32_BE(v1 != v2) }
Set* Block::lt32BE(uint32_t v2) const { FILTER_32_BE(v1 <  v2) }
Set* Block::le32BE(uint32_t v2) const { FILTER_32_BE(v1 <= v2) }
Set* Block::gt32BE(uint32_t v2) const { FILTER_32_BE(v1 >  v2) }
Set* Block::ge32BE(uint32_t v2) const { FILTER_32_BE(v1 >= v2) }
