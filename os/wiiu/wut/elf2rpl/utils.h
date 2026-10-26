#pragma once
#include <cstring>
#include <type_traits>

#if defined(WIN32) || defined(_WIN32) || defined(_MSC_VER)
#define PLATFORM_WINDOWS
#elif __APPLE__
#define PLATFORM_APPLE
#define PLATFORM_POSIX
#elif __linux__
#define PLATFORM_LINUX
#define PLATFORM_POSIX
#endif

#ifdef PLATFORM_LINUX
#include <byteswap.h>
#endif

/* reinterpret_cast for value types */
template<typename DstType, typename SrcType>
inline DstType
bit_cast(const SrcType& src)
{
   static_assert(sizeof(SrcType) == sizeof(DstType), "bit_cast must be between same sized types");
   static_assert(std::is_trivially_copyable<SrcType>::value, "SrcType is not trivially copyable.");
   static_assert(std::is_trivially_copyable<DstType>::value, "DstType is not trivially copyable.");

   DstType dst;
   std::memcpy(&dst, &src, sizeof(SrcType));
   return dst;
}

/* Utility class to swap endian for types of size 1, 2, 4, 8 */
/* other type sizes are not supported */
template<typename Type, unsigned Size = sizeof(Type)>
struct byte_swap_t;

template<typename Type>
struct byte_swap_t<Type, 1>
{
   static Type swap(Type src)
   {
      return src;
   }
};

template<typename Type>
struct byte_swap_t<Type, 2>
{
   static Type swap(Type src)
   {
#ifdef PLATFORM_WINDOWS
      return bit_cast<Type>(_byteswap_ushort(bit_cast<uint16_t>(src)));
#elif defined(PLATFORM_APPLE)
      /* Apple has no 16-bit byteswap intrinsic */
      const uint16_t data = bit_cast<uint16_t>(src);
      return bit_cast<Type>((uint16_t)((data >> 8) | (data << 8)));
#elif defined(PLATFORM_LINUX)
      return bit_cast<Type>(bswap_16(bit_cast<uint16_t>(src)));
#endif
   }
};

template<typename Type>
struct byte_swap_t<Type, 4>
{
   static Type swap(Type src)
   {
#ifdef PLATFORM_WINDOWS
      return bit_cast<Type>(_byteswap_ulong(bit_cast<uint32_t>(src)));
#elif defined(PLATFORM_APPLE)
      return bit_cast<Type>(__builtin_bswap32(bit_cast<uint32_t>(src)));
#elif defined(PLATFORM_LINUX)
      return bit_cast<Type>(bswap_32(bit_cast<uint32_t>(src)));
#endif
   }
};

template<typename Type>
struct byte_swap_t<Type, 8>
{
   static Type swap(Type src)
   {
#ifdef PLATFORM_WINDOWS
      return bit_cast<Type>(_byteswap_uint64(bit_cast<uint64_t>(src)));
#elif defined(PLATFORM_APPLE)
      return bit_cast<Type>(__builtin_bswap64(bit_cast<uint64_t>(src)));
#elif defined(PLATFORM_LINUX)
      return bit_cast<Type>(bswap_64(bit_cast<uint64_t>(src)));
#endif
   }
};

/* Swaps endian of src */
template<typename Type>
inline Type
byte_swap(Type src)
{
   return byte_swap_t<Type>::swap(src);
}

/* Alignment helpers */
template<typename Type>
constexpr inline Type
align_up(Type value, size_t alignment)
{
   return static_cast<Type>((static_cast<size_t>(value) + (alignment - 1)) & ~(alignment - 1));
}

template<typename Type>
constexpr inline Type
align_down(Type value, size_t alignment)
{
   return static_cast<Type>(static_cast<size_t>(value) & ~(alignment - 1));
}

#define CHECK_SIZE(Type, Size) \
   static_assert(sizeof(Type) == Size, \
                 #Type " must be " #Size " bytes")
