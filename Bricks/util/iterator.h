/*******************************************************************************
 The MIT License (MIT)

 Copyright (c) 2016 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>

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

#ifndef BRICKS_UTIL_ITERATOR_H
#define BRICKS_UTIL_ITERATOR_H

#include "../template/pod.h"
#include "../template/is_unique_ptr.h"

namespace current {
namespace stl_wrappers {

template <typename MAP>
struct GenericMapIterator final {
  // DIMA_FIXME: Naming.
  using inner_iterator_t = typename MAP::const_iterator;
  using key_t = typename MAP::key_type;
  using mapped_t = typename std::remove_pointer<typename MAP::mapped_type>::type;
  using value_t = typename is_unique_ptr<mapped_t>::underlying_type;
  // Types required by STL to be exposed in iterator.
  using self_type = GenericMapIterator<MAP>;
  using iterator_category = typename inner_iterator_t::iterator_category;
  using value_type = value_t;
  using pointer = value_t*;
  using reference = value_t&;
  using difference_type = typename inner_iterator_t::difference_type;

  inner_iterator_t iterator_;
  explicit GenericMapIterator(inner_iterator_t iterator) : iterator_(iterator) {}
  self_type operator++() {
    ++iterator_;
    return *this;
  }
  self_type operator++(int) {
    self_type result = *this;
    ++iterator_;
    return result;
  }
  self_type operator--() {
    --iterator_;
    return *this;
  }
  self_type operator--(int) {
    self_type result = *this;
    --iterator_;
    return result;
  }
  bool operator==(const GenericMapIterator& rhs) const { return iterator_ == rhs.iterator_; }
  bool operator!=(const GenericMapIterator& rhs) const { return !operator==(rhs); }
  copy_free<key_t> key() const { return iterator_->first; }

  const value_t& operator*() const { return is_unique_ptr<mapped_t>::extract(iterator_->second); }
  const value_t* operator->() const { return is_unique_ptr<mapped_t>::pointer(iterator_->second); }
};

template <typename MAP>
struct GenericMapReverseIterator final {
  // DIMA_FIXME: Naming.
  using inner_iterator_t = typename MAP::const_reverse_iterator;
  using key_t = typename MAP::key_type;
  using mapped_t = typename std::remove_pointer<typename MAP::mapped_type>::type;
  using value_t = typename is_unique_ptr<mapped_t>::underlying_type;
  // Types required by STL to be exposed in iterator.
  using self_type = GenericMapReverseIterator<MAP>;
  using iterator_category = typename inner_iterator_t::iterator_category;
  using value_type = value_t;
  using pointer = value_t*;
  using reference = value_t&;
  using difference_type = typename inner_iterator_t::difference_type;

  inner_iterator_t iterator_;
  explicit GenericMapReverseIterator(inner_iterator_t iterator) : iterator_(iterator) {}
  explicit GenericMapReverseIterator(const GenericMapIterator<MAP>& rhs) : iterator_(rhs.iterator_) {}
  self_type operator++() {
    ++iterator_;
    return *this;
  }
  self_type operator++(int) {
    self_type result = *this;
    ++iterator_;
    return result;
  }
  self_type operator--() {
    --iterator_;
    return *this;
  }
  self_type operator--(int) {
    self_type result = *this;
    --iterator_;
    return result;
  }
  bool operator==(const GenericMapReverseIterator& rhs) const { return iterator_ == rhs.iterator_; }
  bool operator!=(const GenericMapReverseIterator& rhs) const { return !operator==(rhs); }
  copy_free<key_t> key() const { return iterator_->first; }

  const value_t& operator*() const { return is_unique_ptr<mapped_t>::extract(iterator_->second); }
  const value_t* operator->() const { return is_unique_ptr<mapped_t>::pointer(iterator_->second); }
};

namespace sfinae {

template <typename MAP>
struct is_unordered_map {
 private:
  template <typename T>
  constexpr static auto Check(int) -> decltype(std::declval<typename T::hasher>(), bool()) {
    return true;
  }
  template <typename>
  constexpr static bool Check(...) {
    return false;
  }

 public:
  constexpr static bool value = Check<MAP>(0);
};

}  // namespace current::stl_wrappers::sfinae

template <typename MAP>
struct GenericMapAccessor final {
  using const_iterator = GenericMapIterator<MAP>;
  using const_reverse_iterator = GenericMapReverseIterator<MAP>;
  // DIMA_FIXME: Naming.
  using iterator_t = const_iterator;
  using reverse_iterator_t = const_reverse_iterator;
  using key_t = typename MAP::key_type;
  const MAP& map_;

  explicit GenericMapAccessor(const MAP& map) : map_(map) {}

  bool Empty() const { return map_.empty(); }
  size_t Size() const { return map_.size(); }

  bool Has(const key_t& x) const { return map_.find(x) != map_.end(); }

  template <typename U = MAP, class = std::enable_if_t<!sfinae::is_unordered_map<U>::value>>
  iterator_t LowerBound(const key_t& x) const {
    return iterator_t(map_.lower_bound(x));
  }
  template <typename U = MAP, class = std::enable_if_t<!sfinae::is_unordered_map<U>::value>>
  iterator_t UpperBound(const key_t& x) const {
    return iterator_t(map_.upper_bound(x));
  }

  iterator_t begin() const { return iterator_t(map_.cbegin()); }
  iterator_t end() const { return iterator_t(map_.cend()); }

  reverse_iterator_t rbegin() const { return reverse_iterator_t(map_.crbegin()); }
  reverse_iterator_t rend() const { return reverse_iterator_t(map_.crend()); }

  // TODO(dkorolev): Replace this by `Size()` once the REST-related dust settles.
  int64_t TotalElementsForHypermediaCollectionView() const { return static_cast<int64_t>(Size()); }
};

}  // namespace current::stl_wrappers

using stl_wrappers::GenericMapIterator;
using stl_wrappers::GenericMapAccessor;

}  // namespace current

#endif  // BRICKS_UTIL_ITERATOR_H
