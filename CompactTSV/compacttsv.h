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

#ifndef COMPACTTSV_COMPACTTSV_H
#define COMPACTTSV_COMPACTTSV_H

// TODO(dkorolev): Endianness.
// TODO(dkorolev): Move to fast strings.
// TODO(batman): Exceptions.

#include <cassert>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../Bricks/template/weed.h"
#include "../Bricks/util/singleton.h"

namespace efficient_tsv_parser_dispatcher {

template <typename T>
struct DispatcherStorage {
  std::vector<T> row;
  bool Empty() const { return row.empty(); }
  size_t Dim() const { return row.size(); }
  template <typename F>
  void Emit(F&& f) const {
    f(row);
  }
  void GrowAsNeeded(size_t index) {
    if (index >= row.size()) {
      row.resize(index + 1);
    }
  }
};

template <bool string, bool pchar, bool pchar_length_pair>
struct DispatcherImpl {};

template <>
struct DispatcherImpl<true, false, false> : DispatcherStorage<std::string> {
  template <typename L>
  void Update(size_t index, const char* string, L length) {
    GrowAsNeeded(index);
    row[index] = std::string(string, length);
  }
};

// Note: The dispatcher using a bare `const char*` pointer loses information about the length
// of strings containing '\0'-s in the middle. For those cases, the dispatcher passing in rows
// as an `std::pair<const char*, size_t>` is the safest and fastest solution.
template <>
struct DispatcherImpl<false, true, false> : DispatcherStorage<const char*> {
  template <typename L>
  void Update(size_t index, const char* string, L) {
    GrowAsNeeded(index);
    row[index] = string;
  }
};

template <>
struct DispatcherImpl<false, false, true> : DispatcherStorage<std::pair<const char*, size_t>> {
  template <typename L>
  void Update(size_t index, const char* string, L length) {
    GrowAsNeeded(index);
    row[index] = std::make_pair(string, static_cast<size_t>(length));
  }
};

template <typename T, typename... ARGS>
using CW = bricks::weed::call_with<T, ARGS...>;

template <typename F>
using DispatcherImplSelector = DispatcherImpl<CW<F, std::vector<std::string>>::implemented,
                                              CW<F, std::vector<const char*>>::implemented,
                                              CW<F, std::vector<std::pair<const char*, size_t>>>::implemented>;

}  // namespace efficient_tsv_parser_dispatcher

// At most 254 columns, at most 64KB per entry, at most 4B of distinct strings + metadata in total.
// Rationale behind the number "254": 0..253 => update value for this col, 254 => row ready, 255 => new string.
class CompactTSV {
 public:
  // `dim_` can be initialized at construction time or later.
  CompactTSV(size_t dim = 0u) : dim_(dim) {
    assert(dim <= static_cast<size_t>(static_cast<index_type>(-2)));  // TODO(batman): Exception.
    current_.resize(dim_);
  }

  void operator()(const std::vector<std::string>& row) {
    assert(!done_);        // TODO(batman): Exception.
    assert(!row.empty());  // TODO(batman): Exception.
    if (!dim_) {
      dim_ = row.size();
      assert(dim_ <= static_cast<size_t>(static_cast<index_type>(-2)));  // TODO(batman): Exception.
      current_.resize(dim_);
    } else {
      assert(row.size() == dim_);  // TODO(batman): Exception.
    }
    for (index_type i = 0; i < row.size(); ++i) {
      if (row[i] != current_[i] || first_) {
        current_[i] = row[i];
        const offset_type offset = GetOffsetOf(row[i]);
        data_.append(reinterpret_cast<const char*>(&i), sizeof(index_type));
        data_.append(reinterpret_cast<const char*>(&offset), sizeof(offset_type));
      }
    }
    data_.append(reinterpret_cast<const char*>(&markers().row_done), sizeof(index_type));
    first_ = false;
    AssertStillSmall();
  }

  void Finalize() {
    assert(!done_);  // TODO(batman): Exception.
    done_ = true;
  }

  const std::string& GetPackedString() const {
    assert(done_);  // TODO(batman): Exception.
    return data_;
  }

  template <typename F>
  static size_t Unpack(F&& f, const uint8_t* data, size_t length) {
    efficient_tsv_parser_dispatcher::DispatcherImplSelector<F> dispatcher;
    size_t dim = 0u;
    const uint8_t* p = data;
    const uint8_t* end = data + length;
    size_t total = 0u;
    while (p != end) {
      assert(p < end);  // TODO(batman): Exception.
      const index_type index = *reinterpret_cast<const index_type*>(p);
      p += sizeof(index_type);
      assert(p <= end);  // TODO(batman): Exception.
      if (index == markers().storage) {
        const length_type length = *reinterpret_cast<const length_type*>(p);
        p += sizeof(length_type);
        p += length;
        ++p;
        assert(p <= end);  // TODO(batman): Exception.
      } else if (index == markers().row_done) {
        assert(!dispatcher.Empty());
        if (!dim) {
          dim = dispatcher.Dim();
        } else {
          assert(dim == dispatcher.Dim());  // TODO(batman): Exception.
        }
        dispatcher.Emit(std::forward<F>(f));
        ++total;
      } else {
        const offset_type offset = *reinterpret_cast<const offset_type*>(p);
        p += sizeof(offset_type);
        dispatcher.Update(index,
                          reinterpret_cast<const char*>(data + offset + sizeof(length_type)),
                          *reinterpret_cast<const length_type*>(data + offset));
        assert(p <= end);  // TODO(batman): Exception.
      }
    }
    return total;
  }

  template <typename F>
  static size_t Unpack(F&& f, const std::string& input) {
    return Unpack(std::forward<F>(f), reinterpret_cast<const uint8_t*>(&input[0]), input.length());
  }

  template <typename F>
  static size_t Unpack(F&& f, const std::vector<uint8_t>& input) {
    return Unpack(std::forward<F>(f), &input[0], input.size());
  }

 private:
  // Types and helpers.
  using index_type = uint8_t;    // Type to store column index, strictly < 254, 2^8 minus two special markers.
  using length_type = uint16_t;  // Type to store string length, <= 64K, 2^16 - 1.
  using offset_type = uint32_t;  // Type to store offset of a string within `data_`, <= 4B, 2^32 - 1.

  static_assert(std::is_unsigned<index_type>::value, "");
  static_assert(std::is_unsigned<length_type>::value, "");
  static_assert(std::is_unsigned<offset_type>::value, "");

  struct Markers {
    const index_type row_done = static_cast<index_type>(0) - 2;  // 0xfe.
    const index_type storage = static_cast<index_type>(0) - 1;   // 0xff.
  };

  static const Markers& markers() { return bricks::Singleton<Markers>(); }

  // Members.
  bool done_ = false;                 // Whether the TSV data is done.
  size_t dim_ = 0u;                   // Number of rows in input data.
  bool first_ = true;                 // If `operator()` is called for the 1st time and should dump all cols.
  std::vector<std::string> current_;  // Previous/current values of rows, to eliminate redundant data.
  std::string data_;                  // Compressed contents of the "TSV".

  // Offsets of strings already packed into `data_`.
  std::unordered_map<std::string, offset_type> offsets_;

  void AssertStillSmall() {
    const offset_type offset = static_cast<offset_type>(data_.size());
    assert(static_cast<size_t>(offset) == data_.size());  // TODO(batman): Exception.
  }

  offset_type StoreString(const std::string& s) {
    const length_type length = static_cast<length_type>(s.length());
    assert(static_cast<size_t>(length) == s.length());  // TODO(batman): Exception.
    data_.append(reinterpret_cast<const char*>(&markers().storage), sizeof(index_type));
    const offset_type result = static_cast<offset_type>(data_.size());
    data_.append(reinterpret_cast<const char*>(&length), sizeof(length_type));
    data_.append(s.c_str(), length + 1);  // Including the null character.
    AssertStillSmall();
    return result;
  }

  offset_type GetOffsetOf(const std::string& s) {
    offset_type& offset = offsets_[s];
    if (!offset) {
      offset = StoreString(s);
    }
    return offset;
  }
};

#endif  // COMPACTTSV_COMPACTTSV_H
