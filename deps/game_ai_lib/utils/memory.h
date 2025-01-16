// Adapted from OpenAI's retro source code:
// https://github.com/openai/retro

#pragma once

//#include "gtest/gtest.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string.h>

#include <fcntl.h>
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#else
#include <windows.h>
#include "unistd.h"
#endif



#ifdef VOID
#undef VOID
#endif

namespace Retro {

template<typename T = uint8_t>
class MemoryView {
public:
	MemoryView() {}
	MemoryView(const MemoryView<T>&) = delete;
	~MemoryView();

	bool open(const std::string& file, size_t bytes = 0);
	void open(void* buffer, size_t bytes);
	void open(size_t bytes);
	void open(std::initializer_list<T>);
	void close();

	bool ok() const;
	void clone(const void* buffer, size_t bytes);
	void clone(const MemoryView<T>&);
	void clone();

	T& operator[](size_t);
	const T& operator[](size_t) const;
	MemoryView<T>& operator=(MemoryView<T>&&);

	void* offset(size_t);
	const void* offset(size_t) const;

	size_t size() const;

private:
	T* m_buffer = nullptr;
	int m_backingFd = -1;
	bool m_managed = false;
	size_t m_size = 0;
#ifdef _WIN32
	HANDLE m_mapView;
#endif
};

template<typename T>
MemoryView<T>::~MemoryView() {
	close();
}

template<typename T>
bool MemoryView<T>::open(const std::string& file, size_t bytes) {
	if (ok()) {
		close();
	}
	int flags = O_RDWR;
	if (bytes) {
		flags |= O_CREAT;
	}
	m_backingFd = ::open(file.c_str(), flags, 0600);
	if (m_backingFd < 0) {
		return false;
	}
	if (bytes) {
		ftruncate(m_backingFd, bytes);
		m_size = bytes;
	} else {
		m_size = lseek(m_backingFd, 0, SEEK_END);
	}
	m_managed = true;
#ifdef _WIN32
	m_mapView = CreateFileMapping(reinterpret_cast<HANDLE>(_get_osfhandle(m_backingFd)), 0, PAGE_READWRITE, 0, m_size, 0);
	m_buffer = reinterpret_cast<T*>(static_cast<uint8_t*>(MapViewOfFile(m_mapView, FILE_MAP_WRITE, 0, 0, m_size)));
#else
	m_buffer = reinterpret_cast<T*>(static_cast<uint8_t*>(mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_backingFd, 0)));
#endif
	if (m_buffer == reinterpret_cast<T*>(-1)) {
		m_buffer = nullptr;
		m_managed = false;
		::close(m_backingFd);
		return false;
	}
	return true;
}

template<typename T>
void MemoryView<T>::open(void* buffer, size_t bytes) {
	if (ok()) {
		close();
	}
	m_backingFd = -1;
	m_size = bytes;
	m_managed = false;
	m_buffer = static_cast<T*>(buffer);
}

template<typename T>
void MemoryView<T>::open(size_t bytes) {
	if (ok()) {
		close();
	}
	m_backingFd = -1;
	m_size = bytes;
	m_managed = true;
#ifdef _WIN32
	m_buffer = static_cast<T*>(VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
	m_buffer = static_cast<T*>(mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0));
#endif
}

template<typename T>
void MemoryView<T>::open(std::initializer_list<T> list) {
	open(list.size());
	std::copy(list.begin(), list.end(), m_buffer);
}

template<typename T>
void MemoryView<T>::close() {
	if (!ok()) {
		return;
	}
	if (m_managed) {
		if (m_buffer) {
#ifdef _WIN32
			if (m_backingFd >= 0) {
				UnmapViewOfFile(m_buffer);
				CloseHandle(m_mapView);
			} else {
				VirtualFree(m_buffer, 0, MEM_RELEASE);
			}
#else
			munmap(m_buffer, m_size);
#endif
		}
		if (m_backingFd >= 0) {
			::close(m_backingFd);
			m_backingFd = -1;
		}
	}
	m_buffer = nullptr;
	m_size = 0;
	m_managed = false;
}

template<typename T>
bool MemoryView<T>::ok() const {
	return m_buffer && m_size;
}

template<typename T>
void MemoryView<T>::clone() {
	if (!ok() || m_managed) {
		return;
	}

	clone(static_cast<void*>(m_buffer), m_size);
}

template<typename T>
void MemoryView<T>::clone(const void* buffer, size_t bytes) {
	if (m_managed && bytes == m_size) {
		memmove(m_buffer, buffer, bytes);
		return;
	}
	if (static_cast<void*>(m_buffer) != buffer || !m_managed) {
		close();
	}

#ifdef _WIN32
	T* newBuffer = static_cast<T*>(VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
	T* newBuffer = static_cast<T*>(mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0));
#endif
	memcpy(newBuffer, buffer, bytes);
	m_buffer = newBuffer;
	m_size = bytes;
	m_managed = true;
}

template<typename T>
void MemoryView<T>::clone(const MemoryView<T>& other) {
	clone(static_cast<const void*>(other.m_buffer), other.m_size);
}

template<typename T>
T& MemoryView<T>::operator[](size_t index) {
	return m_buffer[index];
}

template<typename T>
const T& MemoryView<T>::operator[](size_t index) const {
	return m_buffer[index];
}

template<typename T>
MemoryView<T>& MemoryView<T>::operator=(MemoryView<T>&& other) {
	close();
	m_buffer = other.m_buffer;
	m_backingFd = other.m_backingFd;
	m_managed = other.m_managed;
	m_size = other.m_size;
	other.m_managed = false;
	return *this;
}

template<typename T>
void* MemoryView<T>::offset(size_t index) {
	return reinterpret_cast<void*>(&m_buffer[index]);
}

template<typename T>
const void* MemoryView<T>::offset(size_t index) const {
	return reinterpret_cast<const void*>(&m_buffer[index]);
}

template<typename T>
size_t MemoryView<T>::size() const {
	return m_size;
}

enum class Endian : char {
	BIG = 0b01,
	LITTLE = 0b10,
	NATIVE = 0b11,
	MIXED_BL = 0b1001,
	MIXED_LB = 0b0110,
	MIXED_BN = 0b1101,
	MIXED_LN = 0b1110,
#if defined(__LITTLE_ENDIAN__) || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	REAL_NATIVE = LITTLE,
	REAL_MIXED_BN = MIXED_BL,
	REAL_MIXED_LN = LITTLE,
#else
	REAL_NATIVE = BIG,
	REAL_MIXED_BN = BIG,
	REAL_MIXED_LN = MIXED_LN,
#endif
	UNDEF = 0
};

Endian reduce(Endian);
bool reduceCompare(Endian, Endian);

enum class Repr : char {
	SIGNED = 'i',
	UNSIGNED = 'u',
	BCD = 'd',
	LN_BCD = 'n'
};

class Datum;
class MemoryOverlay;
class DataType {
public:
	DataType(const char*);
	DataType(const std::string&);
	DataType(const DataType&) = default;

	Datum operator()(void*) const;
	Datum operator()(void*, size_t offset, const MemoryOverlay&) const;
	bool operator==(const DataType&) const;
	bool operator!=(const DataType&) const;

	void encode(void* buffer, int64_t value) const;
	int64_t decode(const void* buffer) const;

	const size_t width;
	const Endian endian;
	const Repr repr;

	const char type[5];

private:
#if 0
	FRIEND_TEST(DataTypeShift, 1);
	FRIEND_TEST(DataTypeShift, 2);
	FRIEND_TEST(DataTypeShift, 3);
	FRIEND_TEST(DataTypeShift, 4);
	FRIEND_TEST(DataTypeShift, 5);
	FRIEND_TEST(DataTypeShift, 6);
	FRIEND_TEST(DataTypeShift, 7);
	FRIEND_TEST(DataTypeShift, 8);
#endif

	const uint8_t maskLo;
	const uint8_t maskHi;
	const unsigned cvt;
	int64_t shift[8]{};
};

struct Variable {
	Variable(const DataType&, size_t address, uint64_t mask = UINT64_MAX);
	Variable(const Variable&) = default;
	bool operator==(const Variable&) const;

	const DataType type;
	const size_t address;
	const uint64_t mask = UINT64_MAX;
};

class MemoryOverlay {
public:
	MemoryOverlay(Endian backing = Endian::NATIVE, Endian real = Endian::NATIVE, size_t width = 1);
	MemoryOverlay(char backing, char real, size_t width = 1);

	void* parse(const void* in, size_t offset, void* out, size_t size) const;
	void unparse(void* out, size_t offset, const void* in, size_t size) const;

	const size_t width;

private:
	DataType m_backing;
	DataType m_real;
};

class Variant {
public:
	enum class Type {
		BOOL,
		INT,
		FLOAT,
		VOID
	};

	Variant() {}
	Variant(int64_t);
	Variant(double);
	Variant(bool);

	template<typename T>
	T cast() const {
		switch (m_type) {
		case Type::BOOL:
			return m_vb;
		case Type::INT:
			return m_vi;
		case Type::FLOAT:
			return m_vf;
		case Type::VOID:
		default:
			return T();
		}
	}

	operator int() const;
	operator int64_t() const;
	operator float() const;
	operator double() const;
	operator bool() const;

	void clear();
	Variant& operator=(int64_t);
	Variant& operator=(double);
	Variant& operator=(bool);

	Type type() { return m_type; }

private:
	Type m_type = Type::VOID;
	union {
		bool m_vb;
		int64_t m_vi;
		double m_vf;
	};
};

class Datum {
public:
	Datum() {}
	Datum(void*, const DataType&);
	Datum(void* base, const Variable&, const MemoryOverlay& overlay = {});
	Datum(void* base, size_t offset, const DataType&, const MemoryOverlay& overlay = {});
	Datum(Variant*);

	Datum& operator=(int64_t);
	operator int64_t() const;
	operator Variant() const;
	bool operator==(int64_t);

private:
	void* const m_base = nullptr;
	const size_t m_offset = 0;
	const DataType m_type{ "|u1" };
	const uint64_t m_mask = UINT64_MAX;
	const MemoryOverlay m_overlay{};
	Variant* m_variant = nullptr;
};

class DynamicMemoryView {
public:
	DynamicMemoryView(void* buffer, size_t bytes, const DataType&, const MemoryOverlay& = {});

	Datum operator[](size_t);
	int64_t operator[](size_t) const;

	const DataType dtype;
	const MemoryOverlay overlay;

private:
	MemoryView<> m_mem;
};

class AddressSpace {
public:
	void addBlock(size_t offset, size_t size, void* data = nullptr);
	void addBlock(size_t offset, size_t size, const void* data);
	void addBlock(size_t offset, const MemoryView<>& base);
	void updateBlock(size_t offset, void* data);
	void updateBlock(size_t offset, const void* data);
	void updateBlock(size_t offset, const MemoryView<>& base);

	bool hasBlock(size_t offset) const;
	const MemoryView<>& block(size_t offset) const;
	MemoryView<>& block(size_t offset);

	const std::map<size_t, MemoryView<>>& blocks() const { return m_blocks; }
	std::map<size_t, MemoryView<>>& blocks() { return m_blocks; }

	bool ok() const;
	void reset();
	void clone(const AddressSpace&);
	void clone();

	void setOverlay(const MemoryOverlay& overlay);
	const MemoryOverlay& overlay() const { return *m_overlay; };

	Datum operator[](size_t);
	Datum operator[](const Variable&);
	uint8_t operator[](size_t) const;
	int64_t operator[](const Variable&) const;

	AddressSpace& operator=(AddressSpace&&);

private:
	static const DataType s_type;
	;
	std::map<size_t, MemoryView<>> m_blocks;
	std::unique_ptr<MemoryOverlay> m_overlay = std::make_unique<MemoryOverlay>();
};

int64_t toBcd(int64_t);
int64_t toLNBcd(int64_t);
bool isBcd(uint64_t);
}

namespace std {
template<>
struct hash<Retro::DataType> {
	size_t operator()(const Retro::DataType&) const;
};
}
