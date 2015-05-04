/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

// The implementation for `KeyEntry` storage type.

#ifndef SHERLOCK_YODA_APY_KEY_ENTRY_H
#define SHERLOCK_YODA_APY_KEY_ENTRY_H

#include <future>

#include "../../types.h"
#include "../../meta_yoda.h"
#include "../../policy.h"
#include "../../exceptions.h"

namespace yoda {

// User type interface: Use `KeyEntry<MyKeyEntry>` in Yoda's type list for required storage types
// for Yoda to support key-entry (key-value) accessors over the type `MyKeyEntry`.
template <typename ENTRY>
struct KeyEntry {};

template <typename ENTRY>
struct StorageTypeExtractor<KeyEntry<ENTRY>> {
  typedef ENTRY type;
};

template <typename ENTRY, typename SUPPORTED_TYPES_AS_TUPLE, typename VISITABLE_TYPES_AS_TUPLE>
struct Container<KeyEntry<ENTRY>, SUPPORTED_TYPES_AS_TUPLE, VISITABLE_TYPES_AS_TUPLE>
    : bricks::metaprogramming::visitor<VISITABLE_TYPES_AS_TUPLE> {
  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<T_ENTRY> T_KEY;

  T_MAP_TYPE<T_KEY, T_ENTRY> data;

  // The container itself is visitable by entries.
  // The entries passed in this way are the entries scanned from the Sherlock stream.
  // The default behavior is to update the contents of the container.
  virtual void visit(ENTRY& entry) override { data[GetKey(entry)] = entry; }
};

template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE, typename ENTRY>
struct YodaImpl<ENTRY_BASE_TYPE, SUPPORTED_TYPES_AS_TUPLE, KeyEntry<ENTRY>> {
  typedef YodaTypes<ENTRY_BASE_TYPE, SUPPORTED_TYPES_AS_TUPLE> YT;

  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<T_ENTRY> T_KEY;

  typedef std::function<void(const T_ENTRY&)> T_ENTRY_CALLBACK;
  typedef std::function<void(const T_KEY&)> T_KEY_CALLBACK;
  typedef std::function<void()> T_VOID_CALLBACK;

  typedef KeyNotFoundException<T_ENTRY> T_KEY_NOT_FOUND_EXCEPTION;
  typedef KeyAlreadyExistsException<T_ENTRY> T_KEY_ALREADY_EXISTS_EXCEPTION;
  typedef EntryShouldExistException<T_ENTRY> T_ENTRY_SHOULD_EXIST_EXCEPTION;

  YodaImpl() = delete;
  explicit YodaImpl(typename YT::T_MQ& mq) : mq_(mq) {}

  struct MQMessageGet : YT::T_MQ_MESSAGE {
    const T_KEY key;
    std::promise<T_ENTRY> pr;
    T_ENTRY_CALLBACK on_success;
    T_KEY_CALLBACK on_failure;

    explicit MQMessageGet(const T_KEY& key, std::promise<T_ENTRY>&& pr) : key(key), pr(std::move(pr)) {}
    explicit MQMessageGet(const T_KEY& key, T_ENTRY_CALLBACK on_success, T_KEY_CALLBACK on_failure)
        : key(key), on_success(on_success), on_failure(on_failure) {}
    virtual void Process(typename YT::T_CONTAINER& container, typename YT::T_STREAM_TYPE&) override {
      const auto cit = container.data.find(key);
      if (cit != container.data.end()) {
        // The entry has been found.
        if (on_success) {
          // Callback semantics.
          on_success(cit->second);
        } else {
          // Promise semantics.
          pr.set_value(cit->second);
        }
      } else {
        // The entry has not been found.
        if (on_failure) {
          // Callback semantics.
          on_failure(key);
        } else {
          // Promise semantics.
          SetPromiseToNullEntryOrThrow<T_KEY,
                                       T_ENTRY,
                                       T_KEY_NOT_FOUND_EXCEPTION,
                                       false  // Was `T_POLICY::allow_nonthrowing_get>::DoIt(key, pr);`
                                       >::DoIt(key, pr);
        }
      }
    }
  };

  struct MQMessageAdd : YT::T_MQ_MESSAGE {
    const T_ENTRY e;
    std::promise<void> pr;
    T_VOID_CALLBACK on_success;
    T_VOID_CALLBACK on_failure;

    explicit MQMessageAdd(const T_ENTRY& e, std::promise<void>&& pr) : e(e), pr(std::move(pr)) {}
    explicit MQMessageAdd(const T_ENTRY& e, T_VOID_CALLBACK on_success, T_VOID_CALLBACK on_failure)
        : e(e), on_success(on_success), on_failure(on_failure) {}

    // Important note: The entry added will eventually reach the storage via the stream.
    // Thus, in theory, `MQMessageAdd::Process()` could be a no-op.
    // This code still updates the storage, to have the API appear more lively to the user.
    // Since the actual implementation of `Add` pushes the `MQMessageAdd` message before publishing
    // an update to the stream, the final state will always be [eventually] consistent.
    // The practical implication here is that an API `Get()` after an api `Add()` may and will return data,
    // that might not yet have reached the storage, and thus relying on the fact that an API `Get()` call
    // reflects updated data is not reliable from the point of data synchronization.
    virtual void Process(typename YT::T_CONTAINER& container,
                         typename YT::T_STREAM_TYPE& stream) override {
      const bool key_exists = static_cast<bool>(container.data.count(GetKey(e)));
      if (key_exists) {
        if (on_failure) {  // Callback function defined.
          on_failure();
        } else {  // Throw.
          pr.set_exception(std::make_exception_ptr(T_KEY_ALREADY_EXISTS_EXCEPTION(e)));
        }
      } else {
        container.data[GetKey(e)] = e;
        stream.Publish(e);
        if (on_success) {
          on_success();
        } else {
          pr.set_value();
        }
      }
    }
  };

  std::future<T_ENTRY> AsyncGet(const T_KEY& key) {
    std::promise<T_ENTRY> pr;
    std::future<T_ENTRY> future = pr.get_future();
    mq_.EmplaceMessage(new MQMessageGet(key, std::move(pr)));
    return future;
  }

  void AsyncGet(const T_KEY& key, T_ENTRY_CALLBACK on_success, T_KEY_CALLBACK on_failure) {
    mq_.EmplaceMessage(new MQMessageGet(key, on_success, on_failure));
  }

  T_ENTRY Get(const T_KEY& key) { return AsyncGet(std::forward<const T_KEY>(key)).get(); }

  std::future<void> AsyncAdd(const T_ENTRY& entry) {
    std::promise<void> pr;
    std::future<void> future = pr.get_future();

    mq_.EmplaceMessage(new MQMessageAdd(entry, std::move(pr)));
    return future;
  }

  void AsyncAdd(const T_ENTRY& entry,
                T_VOID_CALLBACK on_success,
                T_VOID_CALLBACK on_failure = [](const T_KEY&) {}) {
    mq_.EmplaceMessage(new MQMessageAdd(entry, on_success, on_failure));
  }

  void Add(const T_ENTRY& entry) { AsyncAdd(entry).get(); }

 private:
  typename YT::T_MQ& mq_;
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_APY_KEY_ENTRY_H
