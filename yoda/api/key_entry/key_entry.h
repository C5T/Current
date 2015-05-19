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

#ifndef SHERLOCK_YODA_API_KEY_ENTRY_H
#define SHERLOCK_YODA_API_KEY_ENTRY_H

#include <future>

#include "exceptions.h"
#include "metaprogramming.h"

#include "../../exceptions.h"
#include "../../metaprogramming.h"
#include "../../policy.h"
#include "../../types.h"

#include "../../../../Bricks/template/pod.h"

namespace yoda {

// User type interface: Use `KeyEntry<MyKeyEntry>` in Yoda's type list for required storage types
// for Yoda to support key-entry (key-value) accessors over the type `MyKeyEntry`.
template <typename ENTRY>
struct KeyEntry {
  static_assert(std::is_base_of<Padawan, ENTRY>::value, "Entry type must be derived from `yoda::Padawan`.");

  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<T_ENTRY> T_KEY;

  typedef std::function<void(const T_ENTRY&)> T_ENTRY_CALLBACK;
  typedef std::function<void(const T_KEY&)> T_KEY_CALLBACK;
  typedef std::function<void()> T_VOID_CALLBACK;

  typedef KeyNotFoundException<T_KEY> T_KEY_NOT_FOUND_EXCEPTION;
  typedef KeyAlreadyExistsException<T_KEY> T_KEY_ALREADY_EXISTS_EXCEPTION;
  // typedef EntryShouldExistException<T_KEY> T_ENTRY_SHOULD_EXIST_EXCEPTION;

  template <typename DATA>
  static decltype(std::declval<DATA>().template Accessor<KeyEntry<ENTRY>>()) Accessor(DATA&& c) {
    return c.template Accessor<KeyEntry<ENTRY>>();
  }

  template <typename DATA>
  static decltype(std::declval<DATA>().template Mutator<KeyEntry<ENTRY>>()) Mutator(DATA&& c) {
    return c.template Mutator<KeyEntry<ENTRY>>();
  }
};

template <typename YT, typename ENTRY>
struct YodaImpl<YT, KeyEntry<ENTRY>> {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  typedef KeyEntry<ENTRY> YET;  // "Yoda entry type".

  YodaImpl() = delete;
  explicit YodaImpl(typename YT::T_MQ& mq) : mq_(mq) {}

  struct MQMessageGet : YodaMMQMessage<YT> {
    const typename YET::T_KEY key;
    std::promise<EntryWrapper<typename YET::T_ENTRY>> pr;
    typename YET::T_ENTRY_CALLBACK on_success;
    typename YET::T_KEY_CALLBACK on_failure;

    explicit MQMessageGet(const typename YET::T_KEY& key,
                          std::promise<EntryWrapper<typename YET::T_ENTRY>>&& pr)
        : key(key), pr(std::move(pr)) {}
    explicit MQMessageGet(const typename YET::T_KEY& key,
                          typename YET::T_ENTRY_CALLBACK on_success,
                          typename YET::T_KEY_CALLBACK on_failure)
        : key(key), on_success(on_success), on_failure(on_failure) {}
    virtual void Process(YodaContainer<YT>& container, YodaData<YT>, typename YT::T_STREAM_TYPE&) override {
      container(std::ref(*this));
    }
  };

  struct MQMessageAdd : YodaMMQMessage<YT> {
    const typename YET::T_ENTRY e;
    const bool overwrite_allowed;
    std::promise<void> pr;
    typename YET::T_VOID_CALLBACK on_success;
    typename YET::T_VOID_CALLBACK on_failure;

    explicit MQMessageAdd(const typename YET::T_ENTRY& e, bool overwrite_allowed, std::promise<void>&& pr)
        : e(e), overwrite_allowed(overwrite_allowed), pr(std::move(pr)) {}
    explicit MQMessageAdd(const typename YET::T_ENTRY& e,
                          bool overwrite_allowed,
                          typename YET::T_VOID_CALLBACK on_success,
                          typename YET::T_VOID_CALLBACK on_failure)
        : e(e), overwrite_allowed(overwrite_allowed), on_success(on_success), on_failure(on_failure) {}

    // Important note: The entry added will eventually reach the storage via the stream.
    // Thus, in theory, `MQMessageAdd::Process()` could be a no-op.
    // This code still updates the storage, to have the API appear more lively to the user.
    // Since the actual implementation of `Add` pushes the `MQMessageAdd` message before publishing
    // an update to the stream, the final state will always be [eventually] consistent.
    // The practical implication here is that an API `Get()` after an api `Add()` may and will return data,
    // that might not yet have reached the storage, and thus relying on the fact that an API `Get()` call
    // reflects updated data is not reliable from the point of data synchronization.
    virtual void Process(YodaContainer<YT>& container,
                         YodaData<YT>,
                         typename YT::T_STREAM_TYPE& stream) override {
      container(std::ref(*this), std::ref(stream));
    }
  };

  Future<EntryWrapper<typename YET::T_ENTRY>> operator()(apicalls::AsyncGet, const typename YET::T_KEY& key) {
    std::promise<EntryWrapper<typename YET::T_ENTRY>> pr;
    Future<EntryWrapper<typename YET::T_ENTRY>> future = pr.get_future();
    mq_.EmplaceMessage(new MQMessageGet(key, std::move(pr)));
    return future;
  }

  void operator()(apicalls::AsyncGet,
                  const typename YET::T_KEY& key,
                  typename YET::T_ENTRY_CALLBACK on_success,
                  typename YET::T_KEY_CALLBACK on_failure) {
    mq_.EmplaceMessage(new MQMessageGet(key, on_success, on_failure));
  }

  /// TODO(dkorolev): Remove old&unused code. Work in progress.
  /// const EntryWrapper<typename YET::T_ENTRY> operator()(apicalls::Get, const typename YET::T_KEY& key) {
  ///   return operator()(apicalls::AsyncGet(), std::forward<const typename YET::T_KEY>(key)).Go();
  /// }

  Future<void> operator()(apicalls::AsyncAdd, const typename YET::T_ENTRY& entry) {
    std::promise<void> pr;
    Future<void> future = pr.get_future();

    mq_.EmplaceMessage(new MQMessageAdd(entry, true, std::move(pr)));
    return future;
  }

  void operator()(apicalls::AsyncAdd,
                  const typename YET::T_ENTRY& entry,
                  typename YET::T_VOID_CALLBACK on_success,
                  typename YET::T_VOID_CALLBACK on_failure = [](const typename YET::T_KEY&) {}) {
    mq_.EmplaceMessage(new MQMessageAdd(entry, true, on_success, on_failure));
  }

  void operator()(apicalls::Add, const typename YET::T_ENTRY& entry) {
    operator()(apicalls::AsyncAdd(), entry).Go();
  }

  YET operator()(apicalls::template ExtractYETFromE<typename YET::T_ENTRY>);
  YET operator()(apicalls::template ExtractYETFromK<typename YET::T_KEY>);

 private:
  typename YT::T_MQ& mq_;
};

template <typename YT, typename ENTRY>
struct Container<YT, KeyEntry<ENTRY>> {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  typedef KeyEntry<ENTRY> YET;

  // Event: The entry has been scanned from the stream.
  // Save a copy! Stream provides copies of entries, that are desined to be `std::move()`-d away.
  void operator()(ENTRY& entry, size_t index) {
    EntryWithIndex<ENTRY>& placeholder = map_[GetKey(entry)];
    if (index > placeholder.index) {
      placeholder.Update(index, std::move(entry));
    }
  }

  // Event: `Get()`.
  void operator()(typename YodaImpl<YT, YET>::MQMessageGet& msg) {
    const auto cit = map_.find(msg.key);
    if (cit != map_.end()) {
      // The entry has been found.
      if (msg.on_success) {
        // Callback semantics.
        msg.on_success(cit->second.entry);
      } else {
        // Promise semantics.
        msg.pr.set_value(EntryWrapper<ENTRY>(cit->second.entry));
      }
    } else {
      // The entry has not been found.
      if (msg.on_failure) {
        // Callback semantics.
        msg.on_failure(msg.key);
      } else {
        // Promise semantics.
        msg.pr.set_value(EntryWrapper<ENTRY>());
        /// TODO(dkorolev): Remove old and unused code.
        /// SetPromiseToNullEntryOrThrow<typename YET::T_KEY,
        ///                              typename YET::T_ENTRY,
        ///                              typename YET::T_KEY_NOT_FOUND_EXCEPTION,
        ///                              false  // Was `T_POLICY::allow_nonthrowing_get>::DoIt(key, pr);`
        ///                              >::DoIt(msg.key, msg.pr);
      }
    }
  }

  // Event: `Add()`.
  void operator()(typename YodaImpl<YT, YET>::MQMessageAdd& msg, typename YT::T_STREAM_TYPE& stream) {
    const bool unacceptable_overwrite = !msg.overwrite_allowed && static_cast<bool>(map_.count(GetKey(msg.e)));
    if (unacceptable_overwrite) {
      if (msg.on_failure) {  // Callback function defined.
        msg.on_failure();
      } else {  // Throw.
        msg.pr.set_exception(
            std::make_exception_ptr(typename YET::T_KEY_ALREADY_EXISTS_EXCEPTION(GetKey(msg.e))));
      }
    } else {
      const size_t index = stream.Publish(msg.e);
      map_[GetKey(msg.e)].Update(index, msg.e);
      if (msg.on_success) {
        msg.on_success();
      } else {
        msg.pr.set_value();
      }
    }
  }

  /// TODO(dkorolev): Remove this code, it's been replaced by an `Accessor`.
  /// Synchronous `Get()` to be used in user functions.
  /// const EntryWrapper<ENTRY> operator()(container_data::Get, const typename YET::T_KEY& key) const {
  ///   const auto cit = map_.find(key);
  ///   if (cit != map_.end()) {
  ///     // The entry has been found.
  ///     return EntryWrapper<ENTRY>(cit->second);
  ///   } else {
  ///     // The entry has not been found.
  ///     return EntryWrapper<ENTRY>();
  ///   }
  /// }

  /// TODO(dkorolev): Remove this code, it's been replaced by a `Mutator`.
  /// Synchronous `Add()` to be used in user functions.
  /// NOTE: `stream` is passed via const reference to make `decltype()` work.
  /// void operator()(container_data::Add,
  ///                 const typename YT::T_STREAM_TYPE& stream,
  ///                 const typename YET::T_ENTRY& entry) {
  ///   const bool key_exists = static_cast<bool>(map_.count(GetKey(entry)));
  ///   if (key_exists) {
  ///     throw typename YET::T_KEY_ALREADY_EXISTS_EXCEPTION(entry);
  ///   } else {
  ///     map_[GetKey(entry)] = entry;
  ///     const_cast<typename YT::T_STREAM_TYPE&>(stream).Publish(entry);
  ///   }
  /// }

  class Accessor {
   public:
    Accessor() = delete;
    explicit Accessor(const Container<YT, YET>& container) : immutable_(container) {}

    bool Exists(bricks::copy_free<typename YET::T_KEY> key) const { return immutable_.map_.count(key); }

    // Non-throwing getter. Returns a wrapped null entry if not found.
    const EntryWrapper<ENTRY> Get(bricks::copy_free<typename YET::T_KEY> key) const {
      const auto cit = immutable_.map_.find(key);
      if (cit != immutable_.map_.end()) {
        return EntryWrapper<ENTRY>(cit->second.entry);
      } else {
        return EntryWrapper<ENTRY>();
      }
    }

    // Throwing getter.
    const ENTRY& operator[](bricks::copy_free<typename YET::T_KEY> key) const {
      const auto cit = immutable_.map_.find(key);
      if (cit != immutable_.map_.end()) {
        return cit->second.entry;
      } else {
        throw typename YET::T_KEY_NOT_FOUND_EXCEPTION(key);
      }
    }

    // Iteration.
    struct Iterator {
      typedef decltype(std::declval<Container<YT, YET>>().map_.cbegin()) T_ITERATOR;
      T_ITERATOR iterator;
      explicit Iterator(T_ITERATOR&& iterator) : iterator(std::move(iterator)) {}
      void operator++() { ++iterator; }
      bool operator==(const Iterator& rhs) const { return iterator == rhs.iterator; }
      bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
      const ENTRY& operator*() const { return iterator->second.entry; }
      const ENTRY* operator->() const { return &iterator->second.entry; }
    };

    Iterator begin() const { return Iterator(immutable_.map_.cbegin()); }

    Iterator end() const { return Iterator(immutable_.map_.cend()); }

    size_t size() const { return immutable_.map_.size(); }

   private:
    const Container<YT, YET>& immutable_;
  };

  class Mutator : public Accessor {
   public:
    Mutator() = delete;
    Mutator(Container<YT, YET>& container, typename YT::T_STREAM_TYPE& stream)
        : Accessor(container), mutable_(container), stream_(stream) {}

    // Non-throwing adder. Silently overwrites if already exists.
    void Add(const ENTRY& entry) {
      const size_t index = stream_.Publish(entry);
      mutable_.map_[GetKey(entry)].Update(index, entry);
    }

    // Throwing adder.
    Mutator& operator<<(const ENTRY& entry) {
      // TODO(dkorolev): Make one, not two lookups in `map`.
      auto key = GetKey(entry);
      if (mutable_.map_.count(key)) {
        throw typename YET::T_KEY_ALREADY_EXISTS_EXCEPTION(std::move(key));
      } else {
        Add(entry);
        return *this;
      }
    }

   private:
    Container<YT, YET>& mutable_;
    typename YT::T_STREAM_TYPE& stream_;
  };

  Accessor operator()(container_data::RetrieveAccessor<YET>) const { return Accessor(*this); }

  Mutator operator()(container_data::RetrieveMutator<YET>, typename YT::T_STREAM_TYPE& stream) {
    return Mutator(*this, std::ref(stream));
  }

  // TODO(dkorolev): This is duplication. We certainly don't need it.
  YET operator()(apicalls::template ExtractYETFromE<typename YET::T_ENTRY>);
  YET operator()(apicalls::template ExtractYETFromK<typename YET::T_KEY>);

 private:
  T_MAP_TYPE<typename YET::T_KEY, EntryWithIndex<typename YET::T_ENTRY>> map_;
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_API_KEY_ENTRY_H
