/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#ifndef BRICKS_UTIL_RANDOM_H
#define BRICKS_UTIL_RANDOM_H

#include <random>

#include "singleton.h"
#include "../time/chrono.h"

namespace bricks {
namespace random {

#ifdef BRICKS_RANDOM_FIX_SEED
static constexpr size_t FIXED_SEED = 42u;
#endif

namespace impl {

struct SeedImpl {
  size_t seed;
  bool is_set = false;
};

struct mt19937_64_wrapper {
#ifdef BRICKS_RANDOM_FIX_SEED
  mt19937_64_wrapper() : instance(FIXED_SEED) {}
#else
  mt19937_64_wrapper()
      : instance(ThreadLocalSingleton<SeedImpl>().is_set ? ThreadLocalSingleton<SeedImpl>().seed
                                                         : static_cast<size_t>(bricks::time::Now())) {}
#endif
  std::mt19937_64 instance;
};

}  // namespace impl

inline void SetSeed(const size_t seed) {
  ThreadLocalSingleton<impl::SeedImpl>().seed = seed;
  ThreadLocalSingleton<impl::SeedImpl>().is_set = true;
}

inline std::mt19937_64& mt19937_64_tls() { return ThreadLocalSingleton<impl::mt19937_64_wrapper>().instance; }

template <typename T>
inline T RandomInt(const T a, const T b) {
  static_assert(std::is_integral<T>::value, "`RandomInt` can be used only with integral types.");
  std::uniform_int_distribution<T> distribution(a, b);
  return distribution(mt19937_64_tls());
}

template <typename T>
inline T RandomReal(const T a, const T b) {
  static_assert(std::is_floating_point<T>::value, "`RandomReal` can be used only with floating point types.");
  std::uniform_real_distribution<T> distribution(a, b);
  return distribution(mt19937_64_tls());
}

}  // namespace random
}  // namespace bricks

#endif  // BRICKS_UTIL_RANDOM_H
