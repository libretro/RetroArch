#pragma once
#include <ostream>
#include <type_traits>
#include "utils.h"

template<typename Type>
class be_val
{
public:
   static_assert(!std::is_array<Type>::value, "be_val invalid type: array");
   static_assert(!std::is_pointer<Type>::value, "be_val invalid type: pointer");
   static_assert(sizeof(Type) == 1 || sizeof(Type) == 2 || sizeof(Type) == 4 || sizeof(Type) == 8, "be_val invalid type size");

   be_val()
   {
   }

   be_val(Type value)
   {
      *this = value;
   }

   Type value() const
   {
      return byte_swap(mValue);
   }

   operator Type() const
   {
      return value();
   }

   template<typename Other> std::enable_if_t<std::is_assignable<Type&, Other>::value, be_val&>
   operator =(const Other &rhs)
   {
      mValue = byte_swap(static_cast<Type>(rhs));
      return *this;
   }

   be_val &operator++() { *this = value() + 1; return *this; }
   be_val &operator--() { *this = value() - 1; return *this; }
   be_val operator--(int) { auto old = *this; *this = value() - 1; return old; }
   be_val operator++(int) { auto old = *this; *this = value() + 1; return old; }

   template<typename Other> bool operator == (const Other &rhs) const { return value() == static_cast<Type>(rhs); }
   template<typename Other> bool operator != (const Other &rhs) const { return value() != static_cast<Type>(rhs); }
   template<typename Other> bool operator >= (const Other &rhs) const { return value() >= static_cast<Type>(rhs); }
   template<typename Other> bool operator <= (const Other &rhs) const { return value() <= static_cast<Type>(rhs); }
   template<typename Other> bool operator  > (const Other &rhs) const { return value()  > static_cast<Type>(rhs); }
   template<typename Other> bool operator  < (const Other &rhs) const { return value()  < static_cast<Type>(rhs); }

   template<typename Other> be_val &operator+=(const Other &rhs) { *this = static_cast<Type>(value() + rhs); return *this; }
   template<typename Other> be_val &operator-=(const Other &rhs) { *this = static_cast<Type>(value() - rhs); return *this; }
   template<typename Other> be_val &operator*=(const Other &rhs) { *this = static_cast<Type>(value() * rhs); return *this; }
   template<typename Other> be_val &operator/=(const Other &rhs) { *this = static_cast<Type>(value() / rhs); return *this; }
   template<typename Other> be_val &operator%=(const Other &rhs) { *this = static_cast<Type>(value() % rhs); return *this; }
   template<typename Other> be_val &operator|=(const Other &rhs) { *this = static_cast<Type>(value() | rhs); return *this; }
   template<typename Other> be_val &operator&=(const Other &rhs) { *this = static_cast<Type>(value() & rhs); return *this; }
   template<typename Other> be_val &operator^=(const Other &rhs) { *this = static_cast<Type>(value() ^ rhs); return *this; }

   template<typename Other> Type operator+(const Other &rhs) const { return static_cast<Type>(value() + rhs); }
   template<typename Other> Type operator-(const Other &rhs) const { return static_cast<Type>(value() - rhs); }
   template<typename Other> Type operator*(const Other &rhs) const { return static_cast<Type>(value() * rhs); }
   template<typename Other> Type operator/(const Other &rhs) const { return static_cast<Type>(value() / rhs); }
   template<typename Other> Type operator%(const Other &rhs) const { return static_cast<Type>(value() % rhs); }
   template<typename Other> Type operator|(const Other &rhs) const { return static_cast<Type>(value() | rhs); }
   template<typename Other> Type operator&(const Other &rhs) const { return static_cast<Type>(value() & rhs); }
   template<typename Other> Type operator^(const Other &rhs) const { return static_cast<Type>(value() ^ rhs); }

protected:
   Type mValue {};
};
