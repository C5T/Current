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

#ifndef BRICKS_STRINGS_DISTANCE_H
#define BRICKS_STRINGS_DISTANCE_H

#include <vector>
#include <algorithm>

namespace bricks {
namespace strings {

// Computes the Levenshtein distance between two strings.
// Runs with the complexity of O(A.length * B.length).
inline size_t SlowEditDistance(const std::string& a, const std::string& b) {
  struct SlowImpl {
    const std::string& a;
    const std::string& b;
    const size_t la;
    const size_t lb;
    mutable std::vector<std::vector<size_t>> D;

    SlowImpl(const std::string& a, const std::string& b)
        : a(a),
          b(b),
          la(a.length()),
          lb(b.length()),
          D(la + 1, std::vector<size_t>(lb + 1, static_cast<size_t>(-1))) {}

    size_t Compute(size_t i, size_t j) const {
      size_t& R = D[i][j];
      if (R == static_cast<size_t>(-1)) {
        if (!i) {
          R = j;
        } else if (!j) {
          R = i;
        } else if (a[i - 1] == b[j - 1]) {
          R = Compute(i - 1, j - 1);
        } else {
          R = 1u + std::min(Compute(i - 1, j - 1), std::min(Compute(i, j - 1), Compute(i - 1, j)));
        }
      }
      return R;
    }

    size_t Compute() const { return Compute(la, lb); }
  };
  SlowImpl impl(a, b);
  return impl.Compute();
}

// Returns the edit distance between two strings, assuming it is forbidden
// to 'skew' the strings by more than `max_offset` apart while matching them letter-by-letter.
// The result is equal to Levenshtein distance if it is <= max_offset.
// Runs with the complexity of O((A.length + B.length) * max_offset).
inline size_t FastEditDistance(const std::string& a, const std::string& b, size_t max_offset) {
  struct FastImpl {
    const std::string& a;
    const std::string& b;
    const size_t la;
    const size_t lb;
    const size_t max_offset;
    mutable std::vector<std::vector<size_t>> D;

    FastImpl(const std::string& a, const std::string& b, size_t max_offset)
        : a(a),
          b(b),
          la(a.length()),
          lb(b.length()),
          max_offset(max_offset),
          D(la + 1, std::vector<size_t>(max_offset * 2 + 1, static_cast<size_t>(-1))) {}

    size_t Compute(size_t i, size_t j) const {
      const size_t d = j + max_offset - i;
      if (d <= max_offset * 2) {
        size_t& R = D[i][d];
        if (R == static_cast<size_t>(-1)) {
          if (!i) {
            R = j;
          } else if (!j) {
            R = i;
          } else if (a[i - 1] == b[j - 1]) {
            R = Compute(i - 1, j - 1);
          } else {
            R = 1u + std::min(Compute(i - 1, j - 1), std::min(Compute(i, j - 1), Compute(i - 1, j)));
          }
        }
        return R;
      } else {
        return static_cast<size_t>(-1);
      }
    }
    size_t Compute() const { return Compute(la, lb); }
  };
  FastImpl impl(a, b, max_offset);
  return impl.Compute();
}

}  // namespace strings
}  // namespace bricks

#endif  // BRICKS_STRINGS_DISTANCE_H
