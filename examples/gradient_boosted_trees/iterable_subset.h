/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2017 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef EXAMPLES_GRADIENT_BOOSTED_TREES_ITERABLE_SUBSET_H
#define EXAMPLES_GRADIENT_BOOSTED_TREES_ITERABLE_SUBSET_H

#include "../../current.h"

#ifndef GBT_EXTRA_CHECK
#define GBT_EXTRA_CHECK(...)
#define MUST_UNDEFINE_GBT_EXTRA_CHECK
#endif  // !defined(GBT_EXTRA_CHECK)

class IterableSubset {
 private:
  const size_t n_;

  size_t current_color_;
  std::vector<size_t> color_;  // `.size()` == n_, `color_[i] == current_color_` <=> point `i` is in the subset.

  // The list of indexes, possibly rearranged, so that the { `begin_`, `end_` } span contains all the points
  // presently in the subset. Invariant: `color_[i] == current_color_` <=> `i` can be found in the span.
  std::vector<size_t> indexes_;
  size_t* begin_;
  size_t* end_;

 public:
  explicit IterableSubset(size_t n_)
      : n_(n_),
        current_color_(0u),
        color_(n_, current_color_),
        indexes_(n_),
        begin_(&indexes_[0]),
        end_(&indexes_[0] + n_) {
    for (size_t i = 0; i < n_; ++i) {
      indexes_[i] = i;
    }
  }

  size_t const* begin() const { return begin_; }
  size_t const* end() const { return end_; }
  bool empty() const { return end_ == begin_; }
  size_t size() const { return static_cast<size_t>(end_ - begin_); }

  bool operator[](size_t i) const {
    GBT_EXTRA_CHECK(CURRENT_ASSERT(i < n_));
    return color_[i] == current_color_;
  }

  template <class PREDICATE, class YES, class NO>
  void Partition(PREDICATE&& p, YES&& f_yes, NO&& f_no) {
    static_cast<void>(n_);  // Unused in optimized compilation. -- D.K.
    GBT_EXTRA_CHECK(SlowInternalCheckInvariant());
    size_t* midpoint = std::partition(begin_, end_, std::forward<PREDICATE>(p));
    size_t* save_begin = begin_;
    size_t* save_end = end_;
    size_t save_current_color = current_color_;
    end_ = midpoint;
    ++current_color_;
    GBT_EXTRA_CHECK(CURRENT_ASSERT(current_color_ = save_current_color + 1u));
    for (size_t index : *this) {
      GBT_EXTRA_CHECK(CURRENT_ASSERT(color_[index] == save_current_color));
      color_[index] = current_color_;
    }
    f_yes();
    GBT_EXTRA_CHECK({
      CURRENT_ASSERT(current_color_ = save_current_color + 1u);
      for (size_t index : *this) {
        CURRENT_ASSERT(color_[index] == current_color_);
      }
    });
    begin_ = midpoint;
    end_ = save_end;
    ++current_color_;
    GBT_EXTRA_CHECK(CURRENT_ASSERT(current_color_ = save_current_color + 2u));
    for (size_t index : *this) {
      GBT_EXTRA_CHECK(CURRENT_ASSERT(color_[index] == save_current_color));
      color_[index] = current_color_;
    }
    f_no();
    GBT_EXTRA_CHECK(CURRENT_ASSERT(current_color_ = save_current_color + 2u));
    GBT_EXTRA_CHECK(for (size_t index : *this) { CURRENT_ASSERT(color_[index] == current_color_); });
    begin_ = save_begin;
    for (size_t index : *this) {
      GBT_EXTRA_CHECK(
          CURRENT_ASSERT(color_[index] == save_current_color + 1u || color_[index] == save_current_color + 2u));
      color_[index] = save_current_color;
    }
    current_color_ = save_current_color;
  }

 private:
#ifdef GBT_EXTRA_CHECKS
  void SlowInternalCheckInvariant() const {
    std::vector<size_t> sorted_list_1(begin_, end_);
    std::sort(sorted_list_1.begin(), sorted_list_1.end());

    std::vector<size_t> sorted_list_2;
    for (size_t i = 0; i < n_; ++i) {
      if (color_[i] == current_color_) {
        sorted_list_2.push_back(i);
      }
    }

    CURRENT_ASSERT(sorted_list_1.size() == size());
    CURRENT_ASSERT(sorted_list_2.size() == size());
    CURRENT_ASSERT(sorted_list_1 == sorted_list_2);
  }
#endif  // #ifdef GBT_EXTRA_CHECKS

  IterableSubset(const IterableSubset&) = delete;
  IterableSubset(IterableSubset&&) = delete;
  IterableSubset& operator=(const IterableSubset&) = delete;
  IterableSubset& operator=(IterableSubset&&) = delete;
};

#ifdef MUST_UNDEFINE_GBT_EXTRA_CHECK
#undef GBT_EXTRA_CHECK
#endif  // defined(MUST_UNDEFINE_GBT_EXTRA_CHECK)

#endif  // EXAMPLES_GRADIENT_BOOSTED_TREES_ITERABLE_SUBSET_H
