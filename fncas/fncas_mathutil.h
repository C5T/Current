/*******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * *******************************************************************************/

#ifndef FNCAS_MATHUTIL_H
#define FNCAS_MATHUTIL_H

#include <vector>
#include <numeric>

namespace fncas {

template <typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value, T>::type 
DotProduct(const std::vector<T> &v1, const std::vector<T> &v2) {
  return std::inner_product(std::begin(v1), std::end(v1), std::begin(v2), static_cast<T>(0));
}

template <typename T>
inline typename std::enable_if<std::is_arithmetic<T>::value, T>::type 
L2Norm(const std::vector<T> &v) {
  return DotProduct(v, v);
}

template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
inline void FlipSign(std::vector<T> &v) {
  std::transform(std::begin(v), std::end(v), std::begin(v), std::negate<T>());
}

}

#endif
