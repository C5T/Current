/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_STRINGS_ROUNDING_H
#define BRICKS_STRINGS_ROUNDING_H

#include "../../port.h"

#include <cstring>
#include <string>
#include <sstream>
#include <cmath>

namespace current {
namespace strings {

// Rounds the number to have `n_digits` significant digits.
inline std::string RoundDoubleToString(double value, size_t n_digits, bool plus_sign = false) {
  CURRENT_ASSERT(n_digits >= 1);
  CURRENT_ASSERT(n_digits <= 15);

  bool negative = false;
  if (value < 0) {
    value = -value;
    negative = true;
  }

  if (value < 1e-16) {
    return "0";
  }

  const int dim = static_cast<int>(std::ceil(std::log(value) / std::log(10.0) + 1e-10));
  uint64_t mod = static_cast<uint64_t>(std::pow(10.0, n_digits));

  const int log10_multiplier = static_cast<int>(n_digits) - dim;
  uint64_t integerized = static_cast<uint64_t>(value * std::pow(10.0, log10_multiplier) + 0.5);

  int digit_index = n_digits - log10_multiplier;

  if (integerized >= mod) {
    // The corner case of `1.0 - 1e-7`.
    mod *= 10;
    ++digit_index;
  }

  std::ostringstream os;
  if (negative) {
    os << '-';
  } else if (integerized > 0 && plus_sign) {
    os << '+';
  }
  if (digit_index < 0) {
    os << "0.";
    for (int i = 0; i < -digit_index; ++i) {
      os << '0';
    }
  } else if (digit_index == 0) {
    os << '0';
  }
  for (size_t d = 0u; d < n_digits; ++d) {
    if (digit_index == 0) {
      os << '.';
    }
    mod /= 10;
    os << (integerized / mod) % 10;
    integerized %= mod;
    --digit_index;
    if (digit_index <= 0 && !integerized) {
      break;
    }
  }
  while (digit_index-- > 0) {
    os << '0';
  }

  return os.str();
}

inline std::string RoundDoubleToString(double value) {
  static constexpr size_t DEFAULT_NUMBER_OF_SIGNIFICANT_DIGITS = 2;
  return RoundDoubleToString(value, DEFAULT_NUMBER_OF_SIGNIFICANT_DIGITS);
}

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_ROUNDING_H
