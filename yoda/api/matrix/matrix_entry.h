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

#ifndef SHERLOCK_YODA_API_MATRIX_H
#define SHERLOCK_YODA_API_MATRIX_H

#include <future>

#include "exceptions.h"
#include "metaprogramming.h"

#include "../../types.h"
#include "../../metaprogramming.h"

namespace yoda {

using sfinae::ENTRY_ROW_TYPE;
using sfinae::ENTRY_COL_TYPE;
using sfinae::T_MAP_TYPE;
using sfinae::GetRow;
using sfinae::GetCol;

// User type interface: Use `MatrixEntry<MyMatrixEntry>` in Yoda's type list for required storage types
// for Yoda to support key-entry (key-value) accessors over the type `MyMatrixEntry`.
template <typename ENTRY>
struct MatrixEntry {
  static_assert(std::is_base_of<Padawan, ENTRY>::value, "Entry type must be derived from `yoda::Padawan`.");

  typedef ENTRY T_ENTRY;
  typedef ENTRY_ROW_TYPE<T_ENTRY> T_ROW;
  typedef ENTRY_COL_TYPE<T_ENTRY> T_COL;

  typedef std::function<void(const T_ENTRY&)> T_ENTRY_CALLBACK;
  typedef std::function<void(const T_ROW&, const T_COL&)> T_CELL_CALLBACK;
  typedef std::function<void()> T_VOID_CALLBACK;

  typedef CellNotFoundException<T_ENTRY> T_CELL_NOT_FOUND_EXCEPTION;
  typedef CellAlreadyExistsException<T_ENTRY> T_CELL_ALREADY_EXISTS_EXCEPTION;

  template <typename DATA>
  static decltype(std::declval<DATA>().template Accessor<MatrixEntry<ENTRY>>()) Accessor(DATA&& c) {
    return c.template Accessor<MatrixEntry<ENTRY>>();
  }

  template <typename DATA>
  static decltype(std::declval<DATA>().template Mutator<MatrixEntry<ENTRY>>()) Mutator(DATA&& c) {
    return c.template Mutator<MatrixEntry<ENTRY>>();
  }
};

template <typename YT, typename ENTRY>
struct Container<YT, MatrixEntry<ENTRY>> {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  typedef MatrixEntry<ENTRY> YET;

  YET operator()(container_helpers::template ExtractYETFromE<typename YET::T_ENTRY>);
  YET operator()(container_helpers::template ExtractYETFromK<typename YET::T_ROW>);
  YET operator()(container_helpers::template ExtractYETFromK<typename YET::T_COL>);

  template <typename T>
  using CF = bricks::copy_free<T>;

  // Event: The entry has been scanned from the stream.
  void operator()(ENTRY& entry, size_t index) {
    std::unique_ptr<EntryWithIndex<ENTRY>>& placeholder = map_[std::make_pair(GetRow(entry), GetCol(entry))];
    if (index > placeholder->index) {
      placeholder->Update(index, std::move(entry));
      forward_[GetRow(entry)][GetCol(entry)] = &placeholder->entry;
      transposed_[GetCol(entry)][GetRow(entry)] = &placeholder->entry;
    }
  }

  class Accessor {
   public:
    Accessor() = delete;
    Accessor(const Container<YT, YET>& container) : immutable_(container) {}

    bool Exists(CF<typename YET::T_ROW> row, CF<typename YET::T_COL> col) const {
      const auto rit = immutable_.forward_.find(row);
      if (rit != immutable_.forward_.end()) {
        return rit->second.count(col);
      } else {
        return false;
      }
    }

    const EntryWrapper<ENTRY> Get(CF<typename YET::T_ROW> row, CF<typename YET::T_COL> col) const {
      const auto rit = immutable_.forward_.find(row);
      if (rit != immutable_.forward_.end()) {
        const auto cit = rit->second.find(col);
        if (cit != rit->second.end()) {
          return EntryWrapper<typename YET::T_ENTRY>(cit->second);
        }
      }
      return EntryWrapper<typename YET::T_ENTRY>();
    }

    /*
        // `operator[key]` returns entry with the corresponding key and throws, if it's not found.
        const ENTRY& operator[](const copy_free<typename YET::T_KEY> key) const {
          const auto cit = immutable_.map_.find(key);
          if (cit != immutable_.map_.end()) {
            return cit->second;
          } else {
            throw typename YET::T_KEY_NOT_FOUND_EXCEPTION(key);
          }
        }
    */

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
          mutable_.map_[std::make_pair(GetRow(entry), GetCol(entry))];
      placeholder = make_unique<EntryWithIndex<ENTRY>>(index, entry);
      mutable_.forward_[GetRow(entry)][GetCol(entry)] = &placeholder->entry;
      mutable_.transposed_[GetCol(entry)][GetRow(entry)] = &placeholder->entry;
    }

   private:
    Container<YT, YET>& mutable_;
    typename YT::T_STREAM_TYPE& stream_;
  };

  Accessor operator()(container_helpers::RetrieveAccessor<YET>) const { return Accessor(*this); }

  Mutator operator()(container_helpers::RetrieveMutator<YET>, typename YT::T_STREAM_TYPE& stream) {
    return Mutator(*this, std::ref(stream));
  }

 private:
  T_MAP_TYPE<std::pair<typename YET::T_ROW, typename YET::T_COL>,
             std::unique_ptr<EntryWithIndex<typename YET::T_ENTRY>>> map_;
  T_MAP_TYPE<typename YET::T_ROW, T_MAP_TYPE<typename YET::T_COL, const typename YET::T_ENTRY*>> forward_;
  T_MAP_TYPE<typename YET::T_COL, T_MAP_TYPE<typename YET::T_ROW, const typename YET::T_ENTRY*>> transposed_;
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_API_MATRIX_H
