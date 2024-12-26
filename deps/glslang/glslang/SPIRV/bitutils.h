// Copyright (c) 2015-2016 The Khronos Group Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIBSPIRV_UTIL_BITUTILS_H_
#define LIBSPIRV_UTIL_BITUTILS_H_

#include <cstdint>
#include <cstring>

namespace spvutils {

// Performs a bitwise copy of source to the destination type Dest.
template <typename Dest, typename Src>
Dest BitwiseCast(Src source) {
  Dest dest;
  std::memcpy(static_cast<void*>(&dest), static_cast<void*>(&source), sizeof(dest));
  return dest;
}

// SetBits<T, First, Num> returns an integer of type <T> with bits set
// for position <First> through <First + Num - 1>, counting from the least
// significant bit. In particular when Num == 0, no positions are set to 1.
// A static assert will be triggered if First + Num > sizeof(T) * 8, that is,
// a bit that will not fit in the underlying type is set.
template <typename T, size_t First = 0, size_t Num = 0>
struct SetBits {
  const static T get = (T(1) << First) | SetBits<T, First + 1, Num - 1>::get;
};

template <typename T, size_t Last>
struct SetBits<T, Last, 0> {
  const static T get = T(0);
};

}  // namespace spvutils

#endif  // LIBSPIRV_UTIL_BITUTILS_H_
