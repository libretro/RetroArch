// Adapted from OpenAI's retro source code:
// https://github.com/openai/retro

#include "memory.h"

#include <cstdlib>
#include <unordered_map>
#include <stdexcept>
#include <string.h>

using namespace Retro;
using namespace std;

Endian Retro::reduce(Endian e) {
	switch (e) {
	case Endian::BIG:
	case Endian::LITTLE:
	case Endian::UNDEF:
	case Endian::MIXED_BL:
	case Endian::MIXED_LB:
		return e;
	case Endian::NATIVE:
		return Endian::REAL_NATIVE;
	case Endian::MIXED_BN:
		return Endian::REAL_MIXED_BN;
	case Endian::MIXED_LN:
		return Endian::REAL_MIXED_LN;
	}
	return e;
}

bool Retro::reduceCompare(Endian a, Endian b) {
	return reduce(a) == reduce(b);
}

DataType::DataType(const char* type)
	: width(type[strlen(type) - 1] - '0')
	, endian(
		  type[0] == '=' ? Endian::NATIVE : type[0] == '>' ? (type[1] == '<' ? Endian::MIXED_BL : type[1] == '=' ? Endian::MIXED_BN : Endian::BIG) : type[0] == '<' ? (type[1] == '>' ? Endian::MIXED_LB : type[1] == '=' ? Endian::MIXED_LN : Endian::LITTLE) : Endian::UNDEF)
	, repr(static_cast<Repr>(type[strlen(type) - 2]))
	, type{ type[0], type[1], type[2], type[3] }
	, maskLo(repr == Repr::LN_BCD || repr == Repr::BCD ? 0xF : 0xFF)
	, maskHi(repr == Repr::BCD ? 0xF0 : 0x0)
	, cvt(repr == Repr::BCD || repr == Repr::LN_BCD ? 10 : 256) {
	uint64_t shiftInc =
		repr == Repr::BCD ? 100 : repr == Repr::LN_BCD ? 10 : 256;

	int baseLoc;
	int baseEnd;
	int halfLoc = -1;
	int diff;

	if (width > 8) {
		throw std::out_of_range("Invalid DataType width");
	}

	switch (reduce(endian)) {
	case Endian::LITTLE:
	default:
		baseLoc = 0;
		baseEnd = width;
		diff = 1;
		break;
	case Endian::BIG:
		baseLoc = width - 1;
		baseEnd = -1;
		diff = -1;
		break;
	case Endian::MIXED_LB:
		baseLoc = width / 2 - 1;
		baseEnd = -1;
		halfLoc = width - 1;
		diff = -1;
		break;
	case Endian::MIXED_BL:
		baseLoc = width / 2;
		baseEnd = width;
		halfLoc = 0;
		diff = 1;
		break;
	}

	uint64_t baseShift = 1;
	for (int i = baseLoc; i != baseEnd; i += diff, baseShift *= shiftInc) {
		shift[i] = baseShift;
	}
	if (halfLoc >= 0) {
		for (int i = halfLoc; i != baseLoc; i += diff, baseShift *= shiftInc) {
			shift[i] = baseShift;
		}
	}
}

DataType::DataType(const string& type)
	: DataType(type.c_str()) {
}

Datum DataType::operator()(void* base) const {
	return Datum(base, *this);
}

Datum DataType::operator()(void* base, size_t offset, const MemoryOverlay& overlay) const {
	return Datum(base, offset, *this, overlay);
}

bool DataType::operator==(const DataType& other) const {
	return width == other.width && endian == other.endian && repr == other.repr;
}

bool DataType::operator!=(const DataType& other) const {
	return !(*this == other);
}

void DataType::encode(void* buffer, int64_t value) const {
	for (size_t i = 0; i < width; ++i) {
		uint64_t b = (uint64_t) value / shift[i];
		b = b % cvt + b / cvt % cvt * (~maskHi + 1);
		static_cast<uint8_t*>(buffer)[i] = b;
	}
}

int64_t DataType::decode(const void* buffer) const {
	int64_t datum = 0;
	for (size_t i = 0; i < width; ++i) {
		uint8_t b = static_cast<const uint8_t*>(buffer)[i];
		datum += ((b & maskLo) % cvt + ((b & maskHi) >> 4) % cvt * 10) * shift[i];
	}
	if (repr == Repr::SIGNED) {
		datum <<= 8 * (8 - width);
		datum >>= 8 * (8 - width);
	}
	return datum;
}

size_t hash<DataType>::operator()(const DataType& type) const {
	return hash<uint32_t>()(*reinterpret_cast<const uint32_t*>(type.type));
}

static constexpr char endianTag(Endian e) {
	switch (e) {
	case Endian::BIG:
		return '>';
	case Endian::LITTLE:
		return '<';
	default:
	case Endian::UNDEF:
		return '|';
	case Endian::NATIVE:
		return '=';
	}
}

MemoryOverlay::MemoryOverlay(Endian backing, Endian real, size_t width)
	: width(width)
	, m_backing({ endianTag(backing), 'u', static_cast<char>('0' + width) })
	, m_real({ endianTag(real), 'u', static_cast<char>('0' + width) }) {
}

MemoryOverlay::MemoryOverlay(char backing, char real, size_t width)
	: width(width)
	, m_backing({ backing, 'u', static_cast<char>('0' + width) })
	, m_real({ real, 'u', static_cast<char>('0' + width) }) {
}

void* MemoryOverlay::parse(const void* in, size_t offset, void* out, size_t size) const {
	size_t offsetEdge = offset & (width - 1);
	uintptr_t base = reinterpret_cast<uintptr_t>(in);
	base += offset & ~(width - 1);
	size += offsetEdge;
	uintptr_t outBase = reinterpret_cast<uintptr_t>(out);
	for (size_t i = 0; i < size; i += width) {
		int64_t val = m_backing.decode(reinterpret_cast<const void*>(base + i));
		m_real.encode(reinterpret_cast<void*>(outBase + i), val);
	}
	return reinterpret_cast<void*>(outBase + offsetEdge);
}

void MemoryOverlay::unparse(void* out, size_t offset, const void* in, size_t size) const {
	size_t offsetEdge = offset & (width - 1);
	uintptr_t base = reinterpret_cast<uintptr_t>(out);
	base += offset & ~(width - 1);
	size += offsetEdge;
	uintptr_t inBase = reinterpret_cast<uintptr_t>(in);
	for (size_t i = 0; i < size; i += width) {
		int64_t val = m_real.decode(reinterpret_cast<void*>(inBase + i));
		m_backing.encode(reinterpret_cast<void*>(base + i), val);
	}
}

Variant::Variant(int64_t i)
	: m_type(Type::INT)
	, m_vi(i) {
}

Variant::Variant(double d)
	: m_type(Type::FLOAT)
	, m_vf(d) {
}

Variant::Variant(bool b)
	: m_type(Type::BOOL)
	, m_vb(b) {
}

Variant::operator int64_t() const {
	return cast<int64_t>();
}

Variant::operator int() const {
	return cast<int>();
}

Variant::operator float() const {
	return cast<float>();
}

Variant::operator double() const {
	return cast<double>();
}

Variant::operator bool() const {
	return cast<bool>();
}

void Variant::clear() {
	m_type = Type::VOID;
}

Variant& Variant::operator=(int64_t v) {
	m_type = Type::INT;
	m_vi = v;
	return *this;
}

Variant& Variant::operator=(double v) {
	m_type = Type::FLOAT;
	m_vf = v;
	return *this;
}

Variant& Variant::operator=(bool v) {
	m_type = Type::BOOL;
	m_vb = v;
	return *this;
}

Datum::Datum(void* base, const DataType& type)
	: m_base(base)
	, m_type(type) {
}

Datum::Datum(void* base, size_t offset, const DataType& type, const MemoryOverlay& overlay)
	: m_base(base)
	, m_offset(offset)
	, m_type(type)
	, m_overlay(overlay) {
}

Datum::Datum(void* base, const Variable& var, const MemoryOverlay& overlay)
	: m_base(base)
	, m_offset(var.address)
	, m_type(var.type)
	, m_mask(var.mask)
	, m_overlay(overlay) {
}

Datum::Datum(Variant* variant)
	: m_type("=i8")
	, m_variant(variant) {
}

Datum& Datum::operator=(int64_t value) {
	if (m_base) {
		if (m_overlay.width > 1 || m_offset) {
			uint8_t fakeBase[16]{};
			m_type.encode(m_overlay.parse(m_base, m_offset, reinterpret_cast<void*>(fakeBase), m_type.width), value);
			m_overlay.unparse(m_base, m_offset, reinterpret_cast<void*>(fakeBase), m_type.width);
		} else {
			m_type.encode(m_base, value);
		}
	} else if (m_variant) {
		*m_variant = value;
	}
	return *this;
}

Datum::operator int64_t() const {
	if (!m_base) {
		if (m_variant) {
			return *m_variant;
		}
		return 0;
	}
	int64_t value;
	if (m_overlay.width > 1 || m_offset) {
		uint8_t fakeBase[16]{};
		value = m_type.decode(m_overlay.parse(m_base, m_offset, reinterpret_cast<void*>(fakeBase), m_type.width));
	} else {
		value = m_type.decode(m_base);
	}
	return value & m_mask;
}

Datum::operator Variant() const {
	if (m_variant) {
		return *m_variant;
	}
	return static_cast<int64_t>(*this);
}

DynamicMemoryView::DynamicMemoryView(void* buffer, size_t bytes, const DataType& dtype, const MemoryOverlay& overlay)
	: dtype(dtype)
	, overlay(overlay) {
	m_mem.open(buffer, bytes);
}

Datum DynamicMemoryView::operator[](size_t offset) {
	return dtype(m_mem.offset(0), offset, overlay);
}

int64_t DynamicMemoryView::operator[](size_t offset) const {
	if (overlay.width > 1) {
		uint8_t fakeBase[16]{};
		return dtype.decode(overlay.parse(m_mem.offset(0), offset, reinterpret_cast<void*>(fakeBase), dtype.width));
	}
	return dtype.decode(m_mem.offset(offset));
}

const DataType AddressSpace::s_type{ "|u1" };

void AddressSpace::addBlock(size_t offset, size_t size, void* data) {
	if (data) {
		m_blocks[offset].open(data, size);
	} else {
		m_blocks[offset].open(size);
	}
}

void AddressSpace::addBlock(size_t offset, size_t size, const void* data) {
	if (data) {
		m_blocks[offset].clone(data, size);
	} else {
		m_blocks[offset].open(size);
	}
}

void AddressSpace::addBlock(size_t offset, const MemoryView<>& base) {
	m_blocks[offset].clone(base);
}

void AddressSpace::updateBlock(size_t offset, void* data) {
	m_blocks[offset].open(data, m_blocks[offset].size());
}

void AddressSpace::updateBlock(size_t offset, const void* data) {
	m_blocks[offset].clone(data, m_blocks[offset].size());
}

void AddressSpace::updateBlock(size_t offset, const MemoryView<>& base) {
	m_blocks[offset].clone(base);
}

bool AddressSpace::hasBlock(size_t offset) const {
	for (const auto& block : m_blocks) {
		if (offset < block.first) {
			continue;
		}
		if (offset < block.first + block.second.size()) {
			return true;
		}
	}
	return false;
}

const MemoryView<>& AddressSpace::block(size_t offset) const {
	for (const auto& block : m_blocks) {
		if (offset < block.first) {
			continue;
		}
		if (offset < block.first + block.second.size()) {
			return block.second;
		}
	}
	throw std::out_of_range("No known mapping 1");
}

MemoryView<>& AddressSpace::block(size_t offset) {
	for (auto& block : m_blocks) {
		if (offset < block.first) {
			continue;
		}
		if (offset < block.first + block.second.size()) {
			return block.second;
		}
	}
	throw std::out_of_range("No known mapping 2");
}

bool AddressSpace::ok() const {
	return m_blocks.size() > 0;
}

void AddressSpace::reset() {
	m_blocks.clear();
}

void AddressSpace::clone(const AddressSpace& as) {
	m_blocks.clear();
	m_overlay = make_unique<MemoryOverlay>(*as.m_overlay);
	for (auto& kv : as.m_blocks) {
		m_blocks[kv.first].clone(kv.second);
	}
}

void AddressSpace::clone() {
	for (auto& kv : m_blocks) {
		kv.second.clone();
	}
}

void AddressSpace::setOverlay(const MemoryOverlay& overlay) {
	m_overlay = make_unique<MemoryOverlay>(overlay);
}

Datum AddressSpace::operator[](size_t offset) {
	for (auto& kv : m_blocks) {
		if (offset < kv.first) {
			throw std::out_of_range("No known mapping 3");
		}
		if (offset - kv.first >= kv.second.size()) {
			continue;
		}
		return Datum(kv.second.offset(0), offset - kv.first, s_type, *m_overlay);
	}
	throw std::out_of_range("No known mapping 4");
}

Datum AddressSpace::operator[](const Variable& var) {
	for (auto& kv : m_blocks) {
		if (var.address < kv.first) {
			throw std::out_of_range("No known mapping 5");
		}
		if (var.address - kv.first >= kv.second.size()) {
			continue;
		}
		return Datum(kv.second.offset(0), Variable{ var.type, var.address - kv.first, var.mask }, *m_overlay);
	}
	throw std::out_of_range("No known mapping 6");
}

uint8_t AddressSpace::operator[](size_t offset) const {
	for (const auto& kv : m_blocks) {
		if (offset < kv.first) {
			throw std::out_of_range("No known mapping 7");
		}
		if (offset - kv.first >= kv.second.size()) {
			continue;
		}
		uint8_t fakeBase[16]{};
		return s_type.decode(m_overlay->parse(kv.second.offset(0), offset - kv.first, reinterpret_cast<void*>(fakeBase), s_type.width));
	}
	throw std::out_of_range("No known mapping 8");
}

int64_t AddressSpace::operator[](const Variable& var) const {
	for (const auto& kv : m_blocks) {
		if (var.address < kv.first) {
			throw std::out_of_range("No known mapping 9");
		}
		if (var.address - kv.first >= kv.second.size()) {
			continue;
		}
		int64_t value;
		if (m_overlay->width > 1) {
			uint8_t fakeBase[16];
			value = var.type.decode(m_overlay->parse(kv.second.offset(0), var.address - kv.first, reinterpret_cast<void*>(fakeBase), var.type.width));
		} else {
			value = var.type.decode(kv.second.offset(var.address - kv.first));
		}
		value &= var.mask;
		return value;
	}
	throw std::out_of_range("No known mapping 10");
}

AddressSpace& AddressSpace::operator=(AddressSpace&& as) {
	m_blocks.clear();
	m_overlay = move(as.m_overlay);
	for (auto& kv : as.m_blocks) {
		m_blocks[kv.first] = move(as.m_blocks[kv.first]);
	}
	as.m_blocks.clear();
	return *this;
}

int64_t Retro::toBcd(int64_t value) {
	int64_t out = 0;
	int shift = 0;
	while (value) {
		out |= (value % 10) << (shift * 4);
		++shift;
		value /= 10;
	}
	return out;
}

int64_t Retro::toLNBcd(int64_t value) {
	int64_t out = 0;
	int shift = 0;
	while (value) {
		out |= (value % 10) << (shift * 8);
		++shift;
		value /= 10;
	}
	return out;
}

bool Retro::isBcd(uint64_t value) {
	uint64_t halfdigits = (value >> 1) & 0x7777777777777777;
	return !((halfdigits + 0x3333333333333333) & 0x8888888888888888);
}
