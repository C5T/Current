/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef SHERLOCK_YODA_CONTAINER_MATRIX_API_H
#define SHERLOCK_YODA_CONTAINER_MATRIX_API_H

#include <future>

#include "exceptions.h"
#include "metaprogramming.h"

#include "../../types.h"
#include "../../metaprogramming.h"

namespace yoda {

using sfinae::ENTRY_ROW_TYPE;
using sfinae::ENTRY_COL_TYPE;
using sfinae::MAP_CONTAINER_TYPE;
using sfinae::RowCol;
using sfinae::GetRow;
using sfinae::GetCol;

// User type interface: Use `Matrix<MyMatrix>` in Yoda's type list for required storage types
// for Yoda to support key-entry (key-value) accessors over the type `MyMatrix`.
template <typename ENTRY>
struct Matrix {
  static_assert(std::is_base_of<Padawan, ENTRY>::value, "Entry type must be derived from `yoda::Padawan`.");

  typedef ENTRY T_ENTRY;
  typedef ENTRY_ROW_TYPE<ENTRY> T_ROW;
  typedef ENTRY_COL_TYPE<ENTRY> T_COL;

  typedef std::function<void(const ENTRY&)> T_ENTRY_CALLBACK;
  typedef std::function<void(const T_ROW&, const T_COL&)> T_CELL_CALLBACK;
  typedef std::function<void()> T_VOID_CALLBACK;

  typedef CellNotFoundException<ENTRY> T_CELL_NOT_FOUND_EXCEPTION;
  typedef CellAlreadyExistsException<ENTRY> T_CELL_ALREADY_EXISTS_EXCEPTION;
  typedef SubscriptException<ENTRY> T_SUBSCRIPT_EXCEPTION;

  template <typename DATA>
  static decltype(std::declval<DATA>().template Accessor<Matrix<ENTRY>>()) Accessor(DATA&& c) {
    return c.template Accessor<Matrix<ENTRY>>();
  }

  template <typename DATA>
  static decltype(std::declval<DATA>().template Mutator<Matrix<ENTRY>>()) Mutator(DATA&& c) {
    return c.template Mutator<Matrix<ENTRY>>();
  }
};

// `OUTER_KEY` and `INNER_KEY` are { T_ROW, T_COL } or { T_COL, T_ROW }, depending
// on whether it's `forward_` or `transposed_` that is being traversed.
template <typename YET, typename OUTER_KEY, typename SUBMAP, typename INNER_KEY>
class InnerMapAccessor final {
 public:
  using T_SUBMAP = SUBMAP;

  template <typename T>
  using CF = bricks::copy_free<T>;

  InnerMapAccessor(CF<OUTER_KEY> key, const SUBMAP& map) : key_(key), map_(map) {}
  InnerMapAccessor(InnerMapAccessor&&) = default;
  struct Iterator final {
    typedef decltype(std::declval<SUBMAP>().cbegin()) ITERATOR;
    ITERATOR iterator;
    explicit Iterator(ITERATOR&& iterator) : iterator(std::move(iterator)) {}
    void operator++() { ++iterator; }
    bool operator==(const Iterator& rhs) const { return iterator == rhs.iterator; }
    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
    CF<INNER_KEY> key() const { return iterator->first; }
    const typename YET::T_ENTRY& operator*() const { return *iterator->second; }
    const typename YET::T_ENTRY* operator->() const { return iterator->second; }
  };

  const typename YET::T_ENTRY& operator[](CF<INNER_KEY> subkey) const {
    const auto cit = map_.find(subkey);
    if (cit != map_.end()) {
      return *cit->second;
    } else {
      throw typename YET::T_SUBSCRIPT_EXCEPTION();
    }
  }

  bool Has(CF<INNER_KEY> subkey) const { return (map_.find(subkey) != map_.end()); }

  const CF<OUTER_KEY> key() const { return key_; }
  size_t size() const { return map_.size(); }
  bool empty() const { return map_.empty(); }
  Iterator begin() const { return Iterator(map_.cbegin()); }
  Iterator end() const { return Iterator(map_.cend()); }

 private:
  const CF<OUTER_KEY> key_;
  const SUBMAP& map_;
};

// `OUTER_KEY` and `INNER_KEY` are { T_ROW, T_COL } or { T_COL, T_ROW }, depending
// on whether it's `forward_` or `transposed_` that is being traversed.
template <typename YET, typename OUTER_KEY, typename INNER_KEY>
class OuterMapAccessor final {
 public:
  using T_INNER_MAP = MAP_CONTAINER_TYPE<INNER_KEY, const typename YET::T_ENTRY*>;
  using T_INNER_MAP_ACCESSOR = InnerMapAccessor<YET, OUTER_KEY, T_INNER_MAP, INNER_KEY>;
  using T_OUTER_MAP = MAP_CONTAINER_TYPE<OUTER_KEY, T_INNER_MAP>;

  template <typename T>
  using CF = bricks::copy_free<T>;

  explicit OuterMapAccessor(const T_OUTER_MAP& outer_map) : outer_map_(outer_map) {}
  OuterMapAccessor(OuterMapAccessor&&) = default;

  struct OuterIterator final {
    typedef decltype(std::declval<T_OUTER_MAP>().cbegin()) ITERATOR;
    ITERATOR iterator;
    explicit OuterIterator(ITERATOR&& iterator) : iterator(std::move(iterator)) {}
    void operator++() { ++iterator; }
    bool operator==(const OuterIterator& rhs) const { return iterator == rhs.iterator; }
    bool operator!=(const OuterIterator& rhs) const { return !operator==(rhs); }
    CF<OUTER_KEY> key() const { return iterator->first; }
    T_INNER_MAP_ACCESSOR operator*() const { return T_INNER_MAP_ACCESSOR(iterator->first, iterator->second); }
  };

  const T_INNER_MAP_ACCESSOR operator[](CF<OUTER_KEY> key) const {
    const auto cit = outer_map_.find(key);
    if (cit != outer_map_.end()) {
      return T_INNER_MAP_ACCESSOR(cit->first, cit->second);
    } else {
      throw typename YET::T_SUBSCRIPT_EXCEPTION();
    }
  }

  bool Has(CF<OUTER_KEY> key) const { return (outer_map_.find(key) != outer_map_.end()); }

  size_t size() const { return outer_map_.size(); }
  bool empty() const { return outer_map_.empty(); }
  OuterIterator begin() const { return OuterIterator(outer_map_.cbegin()); }
  OuterIterator end() const { return OuterIterator(outer_map_.cend()); }

 private:
  const T_OUTER_MAP& outer_map_;
};

template <typename YT, typename ENTRY>
struct Container<YT, Matrix<ENTRY>> {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  typedef Matrix<ENTRY> YET;

  typedef RowCol<typename YET::T_ROW, typename YET::T_COL> ROW_COL;

  template <typename T>
  using CF = bricks::copy_free<T>;

  YET operator()(type_inference::template YETFromE<typename YET::T_ENTRY>);
  YET operator()(type_inference::template YETFromK<std::tuple<typename YET::T_ROW, typename YET::T_COL>>);
  YET operator()(
      type_inference::template YETFromSubscript<std::tuple<typename YET::T_ROW, typename YET::T_COL>>);
  YET operator()(type_inference::template YETFromSubscript<typename YET::T_ROW>);
  YET operator()(type_inference::template YETFromSubscript<typename YET::T_COL>);

  // Event: The entry has been scanned from the stream.
  void operator()(ENTRY& entry, size_t index) {
    std::unique_ptr<EntryWithIndex<ENTRY>>& placeholder = map_[ROW_COL(GetRow(entry), GetCol(entry))];
    if (placeholder->index == static_cast<size_t>(-1) || index > placeholder->index) {
      placeholder->Update(index, std::move(entry));
      forward_[GetRow(entry)][GetCol(entry)] = &placeholder->entry;
      transposed_[GetCol(entry)][GetRow(entry)] = &placeholder->entry;
    }
  }

  class Accessor {
   public:
    Accessor() = delete;
    Accessor(const Container<YT, YET>& container) : immutable_(container) {}

    // Returns `true` id entry with corresponding `row`\`col` pair exists.
    bool Has(CF<typename YET::T_ROW> row, CF<typename YET::T_COL> col) const {
      const ROW_COL key(row, col);
      const auto cit = immutable_.map_.find(key);
      if (cit != immutable_.map_.end()) {
        return true;
      } else {
        return false;
      }
    }

    bool Has(const std::tuple<CF<typename YET::T_ROW>, CF<typename YET::T_COL>>& key_as_tuple) const {
      return Has(std::get<0>(key_as_tuple), std::get<1>(key_as_tuple));
    }

    // Non-throwing getter, returns a wrapper.
    const EntryWrapper<ENTRY> Get(CF<typename YET::T_ROW> row, CF<typename YET::T_COL> col) const {
      const ROW_COL key(row, col);
      const auto cit = immutable_.map_.find(key);
      if (cit != immutable_.map_.end()) {
        return EntryWrapper<typename YET::T_ENTRY>(cit->second->entry);
      } else {
        return EntryWrapper<typename YET::T_ENTRY>();
      }
    }

    const EntryWrapper<ENTRY> Get(
        const std::tuple<CF<typename YET::T_ROW>, CF<typename YET::T_COL>>& key_as_tuple) const {
      return Get(std::get<0>(key_as_tuple), std::get<1>(key_as_tuple));
    }

    // Throwing getter.
    const ENTRY& operator[](
        const std::tuple<CF<typename YET::T_ROW>, CF<typename YET::T_COL>>& key_as_tuple) const {
      const auto key = ROW_COL(std::get<0>(key_as_tuple), std::get<1>(key_as_tuple));
      const auto cit = immutable_.map_.find(key);
      if (cit != immutable_.map_.end()) {
        return cit->second->entry;
      } else {
        throw typename YET::T_CELL_NOT_FOUND_EXCEPTION(key.row, key.col);
      }
    }

    // TODO(dk+mz): Should per-row / per-col getters throw right away when row/col is not present?
    InnerMapAccessor<YET,
                     typename YET::T_ROW,
                     MAP_CONTAINER_TYPE<typename YET::T_COL, const typename YET::T_ENTRY*>,
                     typename YET::T_COL>
    operator[](CF<typename YET::T_ROW> row) const {
      const auto submap_cit = immutable_.forward_.find(row);
      if (submap_cit != immutable_.forward_.end()) {
        return InnerMapAccessor<YET,
                                typename YET::T_ROW,
                                MAP_CONTAINER_TYPE<typename YET::T_COL, const typename YET::T_ENTRY*>,
                                typename YET::T_COL>(submap_cit->first, submap_cit->second);
      } else {
        throw typename YET::T_SUBSCRIPT_EXCEPTION();
      }
    }

    InnerMapAccessor<YET,
                     typename YET::T_COL,
                     MAP_CONTAINER_TYPE<typename YET::T_ROW, const typename YET::T_ENTRY*>,
                     typename YET::T_ROW>
    operator[](CF<typename YET::T_COL> col) const {
      const auto submap_cit = immutable_.transposed_.find(col);
      if (submap_cit != immutable_.transposed_.end()) {
        return InnerMapAccessor<YET,
                                typename YET::T_COL,
                                MAP_CONTAINER_TYPE<typename YET::T_ROW, const typename YET::T_ENTRY*>,
                                typename YET::T_ROW>(submap_cit->first, submap_cit->second);
      } else {
        throw typename YET::T_SUBSCRIPT_EXCEPTION();
      }
    }

    OuterMapAccessor<YET, typename YET::T_ROW, typename YET::T_COL> Rows() const {
      return OuterMapAccessor<YET, typename YET::T_ROW, typename YET::T_COL>(immutable_.forward_);
    }

    OuterMapAccessor<YET, typename YET::T_COL, typename YET::T_ROW> Cols() const {
      return OuterMapAccessor<YET, typename YET::T_COL, typename YET::T_ROW>(immutable_.transposed_);
    }

   private:
    const Container<YT, YET>& immutable_;
  };

  class Mutator : public Accessor {
   public:
    Mutator(Container<YT, YET>& container, typename YT::T_STREAM_TYPE& stream)
        : Accessor(container), mutable_(container), stream_(stream) {}

    // Non-throwing method. If entry with the same key already exists, performs silent overwrite.
    void Add(const ENTRY& entry) {
      const size_t index = stream_.Publish(entry);
      std::unique_ptr<EntryWithIndex<ENTRY>>& placeholder =
          mutable_.map_[ROW_COL(GetRow(entry), GetCol(entry))];
      placeholder = make_unique<EntryWithIndex<ENTRY>>(index, entry);
      mutable_.forward_[GetRow(entry)][GetCol(entry)] = &placeholder->entry;
      mutable_.transposed_[GetCol(entry)][GetRow(entry)] = &placeholder->entry;
    }
    void Add(const std::tuple<ENTRY>& entry) { Add(std::get<0>(entry)); }

    // Throwing adder.
    Mutator& operator<<(const ENTRY& entry) {
      const auto key = ROW_COL(GetRow(entry), GetCol(entry));
      if (mutable_.map_.count(key)) {
        throw typename YET::T_CELL_ALREADY_EXISTS_EXCEPTION(GetRow(entry), GetCol(entry));
      } else {
        Add(entry);
        return *this;
      }
    }

   private:
    Container<YT, YET>& mutable_;
    typename YT::T_STREAM_TYPE& stream_;
  };

  Accessor operator()(type_inference::RetrieveAccessor<YET>) const { return Accessor(*this); }

  Mutator operator()(type_inference::RetrieveMutator<YET>, typename YT::T_STREAM_TYPE& stream) {
    return Mutator(*this, std::ref(stream));
  }

 private:
  MAP_CONTAINER_TYPE<ROW_COL, std::unique_ptr<EntryWithIndex<typename YET::T_ENTRY>>> map_;
  MAP_CONTAINER_TYPE<typename YET::T_ROW, MAP_CONTAINER_TYPE<typename YET::T_COL, const typename YET::T_ENTRY*>>
      forward_;
  MAP_CONTAINER_TYPE<typename YET::T_COL, MAP_CONTAINER_TYPE<typename YET::T_ROW, const typename YET::T_ENTRY*>>
      transposed_;
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_CONTAINER_MATRIX_API_H
