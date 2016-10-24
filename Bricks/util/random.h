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

#include "../time/chrono.h"
#include "singleton.h"

namespace current {
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
                                                         : current::time::Now().count()) {}
#endif
  std::mt19937_64 instance;
};

}  // namespace impl

inline void SetRandomSeed(const size_t seed) {
  ThreadLocalSingleton<impl::SeedImpl>().seed = seed;
  ThreadLocalSingleton<impl::SeedImpl>().is_set = true;
}

enum class Generator { MT19937_64, CSPRNG };
inline std::mt19937_64& mt19937_64_tls() { return ThreadLocalSingleton<impl::mt19937_64_wrapper>().instance; }
// Cryptographically secure pseudorandom number generator.
// `std::random_device` does its job on Linux and Windows:
//  * https://msdn.microsoft.com/en-us/library/bb982250.aspx#Anchor_2
//  * http://en.cppreference.com/w/cpp/numeric/random/random_device/random_device
inline std::random_device& cspnrg_tls() { return ThreadLocalSingleton<std::random_device>(); }

// Generic templated function for integral types.
template <typename T, Generator G = Generator::MT19937_64>
inline T RandomIntegral(const T a, const T b) {
  static_assert(std::is_integral<T>::value, "`RandomInt` can be used only with integral types.");
  std::uniform_int_distribution<T> distribution(a, b);
  return (G == Generator::MT19937_64) ? distribution(mt19937_64_tls()) : distribution(cspnrg_tls());
}

inline int RandomInt(const int a, const int b) { return RandomIntegral<int>(a, b); }

inline unsigned int RandomUInt(const unsigned int a, const unsigned int b) {
  return RandomIntegral<unsigned int>(a, b);
}

inline long RandomLong(const long a, const long b) { return RandomIntegral<long>(a, b); }

inline unsigned long RandomULong(const unsigned long a, const unsigned long b) {
  return RandomIntegral<unsigned long>(a, b);
}

inline int64_t RandomInt64(const int64_t a, const int64_t b) { return RandomIntegral<int64_t>(a, b); }

inline uint64_t RandomUInt64(const uint64_t a, const uint64_t b) { return RandomIntegral<uint64_t>(a, b); }

// Cryptographically secure functions for integral types.
inline int CSRandomInt(const int a, const int b) { return RandomIntegral<int, Generator::CSPRNG>(a, b); }

inline unsigned int CSRandomUInt(const unsigned int a, const unsigned int b) {
  return RandomIntegral<unsigned int, Generator::CSPRNG>(a, b);
}

inline long CSRandomLong(const long a, const long b) { return RandomIntegral<long, Generator::CSPRNG>(a, b); }

inline unsigned long CSRandomULong(const unsigned long a, const unsigned long b) {
  return RandomIntegral<unsigned long, Generator::CSPRNG>(a, b);
}

inline int64_t CSRandomInt64(const int64_t a, const int64_t b) {
  return RandomIntegral<int64_t, Generator::CSPRNG>(a, b);
}

inline uint64_t CSRandomUInt64(const uint64_t a, const uint64_t b) {
  return RandomIntegral<uint64_t, Generator::CSPRNG>(a, b);
}

// Generic templated function for floating point types.
template <typename T, Generator G = Generator::MT19937_64>
inline T RandomReal(const T a, const T b) {
  static_assert(std::is_floating_point<T>::value, "`RandomReal` can be used only with floating point types.");
  std::uniform_real_distribution<T> distribution(a, b);
  return (G == Generator::MT19937_64) ? distribution(mt19937_64_tls()) : distribution(cspnrg_tls());
}

inline float RandomFloat(const float a, const float b) { return RandomReal<float>(a, b); }

inline double RandomDouble(const double a, const double b) { return RandomReal<double>(a, b); }

// Cryptographically secure functions for floating point types.
inline float CSRandomFloat(const float a, const float b) { return RandomReal<float, Generator::CSPRNG>(a, b); }

inline double CSRandomDouble(const double a, const double b) { return RandomReal<double, Generator::CSPRNG>(a, b); }

}  // namespace random
}  // namespace current

using current::random::RandomInt;
using current::random::RandomLong;
using current::random::RandomULong;
using current::random::RandomInt64;
using current::random::RandomUInt64;
using current::random::RandomFloat;
using current::random::RandomDouble;

using current::random::SetRandomSeed;

#endif  // BRICKS_UTIL_RANDOM_H
