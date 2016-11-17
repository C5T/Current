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

#include <cmath>
#include <cstring>
#include <sstream>
#include <string>

namespace current {
namespace strings {

// Rounds the number to have `n_digits` significant digits.
inline std::string RoundDoubleToString(double value, size_t n_digits) {
  CURRENT_ASSERT(n_digits >= 1);
  CURRENT_ASSERT(n_digits <= 100);
  // `value` will be between `10^dim` and `10^(dim+1)`.
  const int dim = static_cast<int>(std::floor((std::log(value) / std::log(10.0)) + 1e-6));
  const double k = std::pow(10.0, static_cast<double>(dim - static_cast<int>(n_digits) + 1));
  std::ostringstream os;
  os << (k * std::round(value / k));
  return os.str();
}

inline std::string RoundDoubleToString(double value) {
  static constexpr size_t DEFAULT_NUMBER_OF_SIGNIFICANT_DIGITS = 2;
  return RoundDoubleToString(value, DEFAULT_NUMBER_OF_SIGNIFICANT_DIGITS);
}

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_ROUNDING_H
