/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// Fixed-sized, zero-padded serialization and de-serialization for unsigned types of two or more bytes.
//
// Ported into Bricks from TailProduce.

#ifndef BRICKS_STRINGS_FIXED_SIZE_SERIALIZER_H
#define BRICKS_STRINGS_FIXED_SIZE_SERIALIZER_H

#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>

namespace current {
namespace strings {

struct FixedSizeSerializerEnabler {};
template <typename T>
struct FixedSizeSerializer
    : std::enable_if_t<std::is_unsigned<T>::value && std::is_integral<T>::value && (sizeof(T) > 1),
                       FixedSizeSerializerEnabler> {
  enum { size_in_bytes = std::numeric_limits<T>::digits10 + 1 };
  static std::string PackToString(T x) {
    std::ostringstream os;
    os << std::setfill('0') << std::setw(size_in_bytes) << x;
    return os.str();
  }
  static T UnpackFromString(std::string const& s) {
    T x;
    std::istringstream is(s);
    is >> x;
    return x;
  }
};

// To allow implicit type specialization wherever possible.
template <typename T>
inline std::string PackToString(T x) {
  return FixedSizeSerializer<T>::PackToString(x);
}
template <typename T>
inline const T& UnpackFromString(std::string const& s, T& x) {
  x = FixedSizeSerializer<T>::UnpackFromString(s);
  return x;
}

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_FIXED_SIZE_SERIALIZER_H
