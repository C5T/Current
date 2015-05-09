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
struct KeyEntry {
  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<T_ENTRY> T_KEY;

  typedef std::function<void(const T_ENTRY&)> T_ENTRY_CALLBACK;
  typedef std::function<void(const T_KEY&)> T_KEY_CALLBACK;
  typedef std::function<void()> T_VOID_CALLBACK;

  typedef KeyNotFoundException<T_ENTRY> T_KEY_NOT_FOUND_EXCEPTION;
  typedef KeyAlreadyExistsException<T_ENTRY> T_KEY_ALREADY_EXISTS_EXCEPTION;
  typedef EntryShouldExistException<T_ENTRY> T_ENTRY_SHOULD_EXIST_EXCEPTION;
};

template <typename YT, typename ENTRY>
struct YodaImpl<YT, KeyEntry<ENTRY>> {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  typedef KeyEntry<ENTRY> YET;  // "Yoda entry type".

  YodaImpl() = delete;
  explicit YodaImpl(typename YT::T_MQ& mq) : mq_(mq) {}

  struct MQMessageGet : YodaMMQMessage<YT> {
    const typename YET::T_KEY key;
    std::promise<typename YET::T_ENTRY> pr;
    typename YET::T_ENTRY_CALLBACK on_success;
    typename YET::T_KEY_CALLBACK on_failure;

    explicit MQMessageGet(const typename YET::T_KEY& key, std::promise<typename YET::T_ENTRY>&& pr)
        : key(key), pr(std::move(pr)) {}
    explicit MQMessageGet(const typename YET::T_KEY& key,
                          typename YET::T_ENTRY_CALLBACK on_success,
                          typename YET::T_KEY_CALLBACK on_failure)
        : key(key), on_success(on_success), on_failure(on_failure) {}
    virtual void Process(YodaContainer<YT>& container, typename YT::T_STREAM_TYPE&) override {
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
    virtual void Process(YodaContainer<YT>& container, typename YT::T_STREAM_TYPE& stream) override {
      container(std::ref(*this), std::ref(stream));
    }
  };

  std::future<typename YET::T_ENTRY> operator()(apicalls::AsyncGet, const typename YET::T_KEY& key) {
    std::promise<typename YET::T_ENTRY> pr;
    std::future<typename YET::T_ENTRY> future = pr.get_future();
    mq_.EmplaceMessage(new MQMessageGet(key, std::move(pr)));
    return future;
  }

  void operator()(apicalls::AsyncGet,
                  const typename YET::T_KEY& key,
                  typename YET::T_ENTRY_CALLBACK on_success,
                  typename YET::T_KEY_CALLBACK on_failure) {
    mq_.EmplaceMessage(new MQMessageGet(key, on_success, on_failure));
  }

  typename YET::T_ENTRY operator()(apicalls::Get, const typename YET::T_KEY& key) {
    return operator()(apicalls::AsyncGet(), std::forward<const typename YET::T_KEY>(key)).get();
  }

  std::future<void> operator()(apicalls::AsyncAdd, const typename YET::T_ENTRY& entry) {
    std::promise<void> pr;
    std::future<void> future = pr.get_future();

    mq_.EmplaceMessage(new MQMessageAdd(entry, std::move(pr)));
    return future;
  }

  void operator()(apicalls::AsyncAdd,
                  const typename YET::T_ENTRY& entry,
                  typename YET::T_VOID_CALLBACK on_success,
                  typename YET::T_VOID_CALLBACK on_failure = [](const typename YET::T_KEY&) {}) {
    mq_.EmplaceMessage(new MQMessageAdd(entry, on_success, on_failure));
  }

  void operator()(apicalls::Add, const typename YET::T_ENTRY& entry) {
    operator()(apicalls::AsyncAdd(), entry).get();
  }

 private:
  typename YT::T_MQ& mq_;
};

template <typename YT, typename ENTRY>
struct Container<YT, KeyEntry<ENTRY>> {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  typedef KeyEntry<ENTRY> YET;

  T_MAP_TYPE<typename YET::T_KEY, typename YET::T_ENTRY> container;

  // Event: The entry has been scanned from the stream.
  void operator()(const typename YET::T_ENTRY& entry) { container[GetKey(entry)] = entry; }

  // Event: `Get()`.
  void operator()(typename YodaImpl<YT, KeyEntry<typename YET::T_ENTRY>>::MQMessageGet& msg) {
    const auto cit = container.find(msg.key);
    if (cit != container.end()) {
      // The entry has been found.
      if (msg.on_success) {
        // Callback semantics.
        msg.on_success(cit->second);
      } else {
        // Promise semantics.
        msg.pr.set_value(cit->second);
      }
    } else {
      // The entry has not been found.
      if (msg.on_failure) {
        // Callback semantics.
        msg.on_failure(msg.key);
      } else {
        // Promise semantics.
        SetPromiseToNullEntryOrThrow<typename YET::T_KEY,
                                     typename YET::T_ENTRY,
                                     typename YET::T_KEY_NOT_FOUND_EXCEPTION,
                                     false  // Was `T_POLICY::allow_nonthrowing_get>::DoIt(key, pr);`
                                     >::DoIt(msg.key, msg.pr);
      }
    }
  }

  // Event: `Add()`.
  void operator()(typename YodaImpl<YT, KeyEntry<typename YET::T_ENTRY>>::MQMessageAdd& msg,
                  typename YT::T_STREAM_TYPE& stream) {
    const bool key_exists = static_cast<bool>(container.count(GetKey(msg.e)));
    if (key_exists) {
      if (msg.on_failure) {  // Callback function defined.
        msg.on_failure();
      } else {  // Throw.
        msg.pr.set_exception(std::make_exception_ptr(typename YET::T_KEY_ALREADY_EXISTS_EXCEPTION(msg.e)));
      }
    } else {
      container[GetKey(msg.e)] = msg.e;
      stream.Publish(msg.e);
      if (msg.on_success) {
        msg.on_success();
      } else {
        msg.pr.set_value();
      }
    }
  }
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_APY_KEY_ENTRY_H
