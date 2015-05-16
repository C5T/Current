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
#include "../../policy.h"
#include "../../exceptions.h"

namespace yoda {

using namespace sfinae;

// The implementation for the logic of `allow_nonthrowing_get` for `MatrixEntry`
// type.
template <typename T_ENTRY, typename T_CELL_NOT_FOUND_EXCEPTION, bool>
struct MatrixEntrySetPromiseToNullEntryOrThrow {};

template <typename T_ENTRY, typename T_CELL_NOT_FOUND_EXCEPTION>
struct MatrixEntrySetPromiseToNullEntryOrThrow<T_ENTRY, T_CELL_NOT_FOUND_EXCEPTION, false> {
  typedef ENTRY_ROW_TYPE<T_ENTRY> T_ROW;
  typedef ENTRY_COL_TYPE<T_ENTRY> T_COL;
  static void DoIt(const T_ROW& row, const T_COL& col, std::promise<T_ENTRY>& pr) {
    pr.set_exception(std::make_exception_ptr(T_CELL_NOT_FOUND_EXCEPTION(row, col)));
  }
};

template <typename T_ENTRY, typename UNUSED_T_CELL_NOT_FOUND_EXCEPTION>
struct MatrixEntrySetPromiseToNullEntryOrThrow<T_ENTRY, UNUSED_T_CELL_NOT_FOUND_EXCEPTION, true> {
  typedef ENTRY_ROW_TYPE<T_ENTRY> T_ROW;
  typedef ENTRY_COL_TYPE<T_ENTRY> T_COL;
  static void DoIt(const T_ROW& row, const T_COL& col, std::promise<T_ENTRY>& pr) {
    T_ENTRY null_entry(NullEntry);
    SetRow(null_entry, row);
    SetCol(null_entry, col);
    pr.set_value(null_entry);
  }
};

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
  typedef EntryShouldExistException<T_ENTRY> T_ENTRY_SHOULD_EXIST_EXCEPTION;
};

template <typename YT, typename ENTRY>
struct YodaImpl<YT, MatrixEntry<ENTRY>> {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  typedef MatrixEntry<ENTRY> YET;  // "Yoda entry type".

  YodaImpl() = delete;
  explicit YodaImpl(typename YT::T_MQ& mq) : mq_(mq) {}

  struct MQMessageGet : YodaMMQMessage<YT> {
    const typename YET::T_ROW row;
    const typename YET::T_COL col;
    std::promise<typename YET::T_ENTRY> pr;
    typename YET::T_ENTRY_CALLBACK on_success;
    typename YET::T_CELL_CALLBACK on_failure;

    explicit MQMessageGet(const typename YET::T_ROW& row,
                          const typename YET::T_COL& col,
                          std::promise<typename YET::T_ENTRY>&& pr)
        : row(row), col(col), pr(std::move(pr)) {}
    explicit MQMessageGet(const typename YET::T_ROW& row,
                          const typename YET::T_COL& col,
                          typename YET::T_ENTRY_CALLBACK on_success,
                          typename YET::T_CELL_CALLBACK on_failure)
        : row(row), col(col), on_success(on_success), on_failure(on_failure) {}
    virtual void Process(YodaContainer<YT>& container,
                         ContainerWrapper<YT>,
                         typename YT::T_STREAM_TYPE&) override {
      container(std::ref(*this));
    }
  };

  struct MQMessageAdd : YodaMMQMessage<YT> {
    const typename YET::T_ENTRY e;
    std::promise<void> pr;
    typename YET::T_VOID_CALLBACK on_success;
    typename YET::T_VOID_CALLBACK on_failure;

    explicit MQMessageAdd(const typename YET::T_ENTRY& e, std::promise<void>&& pr) : e(e), pr(std::move(pr)) {}
    explicit MQMessageAdd(const typename YET::T_ENTRY& e,
                          typename YET::T_VOID_CALLBACK on_success,
                          typename YET::T_VOID_CALLBACK on_failure)
        : e(e), on_success(on_success), on_failure(on_failure) {}

    // Important note: The entry added will eventually reach the storage via the stream.
    // Thus, in theory, `MQMessageAdd::Process()` could be a no-op.
    // This code still updates the storage, to have the API appear more lively to the user.
    // Since the actual implementation of `Add` pushes the `MQMessageAdd` message before publishing
    // an update to the stream, the final state will always be [eventually] consistent.
    // The practical implication here is that an API `Get()` after an api `Add()` may and will return data,
    // that might not yet have reached the storage, and thus relying on the fact that an API `Get()` call
    // reflects updated data is not reliable from the point of data synchronization.
    virtual void Process(YodaContainer<YT>& container,
                         ContainerWrapper<YT>,
                         typename YT::T_STREAM_TYPE& stream) override {
      container(std::ref(*this), std::ref(stream));
    }
  };

  Future<typename YET::T_ENTRY> operator()(apicalls::AsyncGet,
                                           const typename YET::T_ROW& row,
                                           const typename YET::T_COL& col) {
    std::promise<typename YET::T_ENTRY> pr;
    Future<typename YET::T_ENTRY> future = pr.get_future();
    mq_.EmplaceMessage(new MQMessageGet(row, col, std::move(pr)));
    return future;
  }

  void operator()(apicalls::AsyncGet,
                  const typename YET::T_ROW& row,
                  const typename YET::T_COL& col,
                  typename YET::T_ENTRY_CALLBACK on_success,
                  typename YET::T_CELL_CALLBACK on_failure) {
    mq_.EmplaceMessage(new MQMessageGet(row, col, on_success, on_failure));
  }

  typename YET::T_ENTRY operator()(apicalls::Get,
                                   const typename YET::T_ROW& row,
                                   const typename YET::T_COL& col) {
    return operator()(apicalls::AsyncGet(),
                      std::forward<const typename YET::T_ROW>(row),
                      std::forward<const typename YET::T_COL>(col)).Go();
  }

  Future<void> operator()(apicalls::AsyncAdd, const typename YET::T_ENTRY& entry) {
    std::promise<void> pr;
    Future<void> future = pr.get_future();

    mq_.EmplaceMessage(new MQMessageAdd(entry, std::move(pr)));
    return future;
  }

  void operator()(apicalls::AsyncAdd,
                  const typename YET::T_ENTRY& entry,
                  typename YET::T_VOID_CALLBACK on_success,
                  typename YET::T_VOID_CALLBACK on_failure = []() {}) {
    mq_.EmplaceMessage(new MQMessageAdd(entry, on_success, on_failure));
  }

  void operator()(apicalls::Add, const typename YET::T_ENTRY& entry) {
    operator()(apicalls::AsyncAdd(), entry).Go();
  }

 private:
  typename YT::T_MQ& mq_;
};

template <typename YT, typename ENTRY>
struct Container<YT, MatrixEntry<ENTRY>> {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  typedef MatrixEntry<ENTRY> YET;

  template <typename T>
  using CF = bricks::copy_free<T>;

  // Event: The entry has been scanned from the stream.
  void operator()(const typename YET::T_ENTRY& entry) {
    forward_[GetRow(entry)][GetCol(entry)] = entry;
    transposed_[GetCol(entry)][GetRow(entry)] = entry;
  }

  // Event: `Get()`.
  void operator()(typename YodaImpl<YT, MatrixEntry<typename YET::T_ENTRY>>::MQMessageGet& msg) {
    bool cell_exists = false;
    const auto rit = forward_.find(msg.row);
    if (rit != forward_.end()) {
      const auto cit = rit->second.find(msg.col);
      if (cit != rit->second.end()) {
        cell_exists = true;
        // The entry has been found.
        if (msg.on_success) {
          // Callback semantics.
          msg.on_success(cit->second);
        } else {
          // Promise semantics.
          msg.pr.set_value(cit->second);
        }
      }
    }
    if (!cell_exists) {
      // The entry has not been found.
      if (msg.on_failure) {
        // Callback semantics.
        msg.on_failure(msg.row, msg.col);
      } else {
        // Promise semantics.
        MatrixEntrySetPromiseToNullEntryOrThrow<typename YET::T_ENTRY,
                                                typename YET::T_CELL_NOT_FOUND_EXCEPTION,
                                                false  // Was `T_POLICY::allow_nonthrowing_get>::DoIt(key, pr);`
                                                >::DoIt(msg.row, msg.col, msg.pr);
      }
    }
  }

  // Event: `Add()`.
  void operator()(typename YodaImpl<YT, MatrixEntry<typename YET::T_ENTRY>>::MQMessageAdd& msg,
                  typename YT::T_STREAM_TYPE& stream) {
    bool cell_exists = false;
    const auto rit = forward_.find(GetRow(msg.e));
    if (rit != forward_.end()) {
      cell_exists = static_cast<bool>(rit->second.count(GetCol(msg.e)));
    }
    if (cell_exists) {
      if (msg.on_failure) {  // Callback function defined.
        msg.on_failure();
      } else {  // Throw.
        msg.pr.set_exception(std::make_exception_ptr(typename YET::T_CELL_ALREADY_EXISTS_EXCEPTION(msg.e)));
      }
    } else {
      forward_[GetRow(msg.e)][GetCol(msg.e)] = msg.e;
      transposed_[GetCol(msg.e)][GetRow(msg.e)] = msg.e;
      stream.Publish(msg.e);
      if (msg.on_success) {
        msg.on_success();
      } else {
        msg.pr.set_value();
      }
    }
  }

  // Synchronous `Get()` to be used in user functions.
  const EntryWrapper<typename YET::T_ENTRY> operator()(container_wrapper::Get,
                                                       const typename YET::T_ROW& row,
                                                       const typename YET::T_COL& col) const {
    const auto rit = forward_.find(row);
    if (rit != forward_.end()) {
      const auto cit = rit->second.find(col);
      if (cit != rit->second.end()) {
        return EntryWrapper<typename YET::T_ENTRY>(cit->second);
      }
    }
    return EntryWrapper<typename YET::T_ENTRY>();
  }

  // Synchronous `Add()` to be used in user functions.
  // NOTE: `stream` is passed via const reference to make `decltype()` work.
  void operator()(container_wrapper::Add,
                  const typename YT::T_STREAM_TYPE& stream,
                  const typename YET::T_ENTRY& entry) {
    bool cell_exists = false;
    const auto rit = forward_.find(GetRow(entry));
    if (rit != forward_.end()) {
      cell_exists = static_cast<bool>(rit->second.count(GetCol(entry)));
    }
    if (cell_exists) {
      throw typename YET::T_CELL_ALREADY_EXISTS_EXCEPTION(entry);
    } else {
      forward_[GetRow(entry)][GetCol(entry)] = entry;
      transposed_[GetCol(entry)][GetRow(entry)] = entry;
      const_cast<typename YT::T_STREAM_TYPE&>(stream).Publish(entry);
    }
  }

  class Accessor {
   public:
    Accessor() = delete;
    Accessor(Container<YT, YET>& container) : immutable_(container) {}

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
      mutable_.forward_[GetRow(entry)][GetCol(entry)] = entry;
      mutable_.transposed_[GetCol(entry)][GetRow(entry)] = entry;
      // NOTE: runtime error - mutex lock failed.
      // stream_.Publish(entry);
    }

   private:
    Container<YT, YET>& mutable_;
    typename YT::T_STREAM_TYPE& stream_;
  };

  Accessor operator()(container_wrapper::RetrieveAccessor<YET>) const { return Accessor(*this); }

  Mutator operator()(container_wrapper::RetrieveMutator<YET>, typename YT::T_STREAM_TYPE& stream) {
    return Mutator(*this, std::ref(stream));
  }

 private:
  // TODO(dkorolev)+TODO(mzhurovich): Eventually we'll think of storing each entry only once.
  T_MAP_TYPE<typename YET::T_ROW, T_MAP_TYPE<typename YET::T_COL, typename YET::T_ENTRY>> forward_;
  T_MAP_TYPE<typename YET::T_COL, T_MAP_TYPE<typename YET::T_ROW, typename YET::T_ENTRY>> transposed_;
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_API_MATRIX_H
