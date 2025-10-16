/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// WaitableAtomic<T> acts as an atomic wrapper over type T with one additional feature: the clients
// can wait for updates on this object instead of using spin locks or other external waiting primitives.

#ifndef BRICKS_WAITABLE_ATOMIC_H
#define BRICKS_WAITABLE_ATOMIC_H

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>

#ifdef CURRENT_FOR_CPP14
#include "../template/weed.h"
#endif  // CURRENT_FOR_CPP14

namespace current {

class CustomWaitableAtomicDestructor {
 public:
  virtual void WaitableAtomicDestructing() = 0;
  virtual ~CustomWaitableAtomicDestructor() = default;
};

// A wrapper around `std::unique_lock<std::mutex>` that allows being inherited from itself.
class ScopedUniqueLock {
 public:
  explicit ScopedUniqueLock(std::mutex& mutex) : lock_(mutex) {}
  ScopedUniqueLock(ScopedUniqueLock&& rhs) : lock_(std::move(rhs.lock_)) {}

 protected:
  ~ScopedUniqueLock() = default;  // The destructor of a non-`final` class should be `virtual` or `protected`.

 private:
  std::unique_lock<std::mutex> lock_;

  ScopedUniqueLock(const ScopedUniqueLock&) = delete;
  void operator=(const ScopedUniqueLock&) = delete;
};

class IntrusiveClient final {
 public:
  class Interface {
   public:
    virtual ~Interface() {}

   private:
    virtual bool RefCounterTryIncrease() = 0;
    virtual void RefCounterDecrease() = 0;
    virtual bool IsDestructing() const = 0;
    friend class IntrusiveClient;
  };

  explicit IntrusiveClient(Interface* object) : intrusive_object_(object) {
    if (intrusive_object_) {
      if (!intrusive_object_->RefCounterTryIncrease()) {
        intrusive_object_ = nullptr;
      }
    }
  }

  IntrusiveClient(IntrusiveClient&& rhs) : intrusive_object_(nullptr) {
    std::swap(intrusive_object_, rhs.intrusive_object_);
  }

  ~IntrusiveClient() {
    if (intrusive_object_) {
      intrusive_object_->RefCounterDecrease();
    }
  }

  operator bool() const { return intrusive_object_ && !intrusive_object_->IsDestructing(); }

 private:
  IntrusiveClient(const IntrusiveClient&) = delete;
  void operator=(const IntrusiveClient&) = delete;

  Interface* intrusive_object_;
};

struct WaitableAtomicSubscriberRemover {
  virtual ~WaitableAtomicSubscriberRemover() = default;
  virtual void Remove() = 0;
};

using WaitableAtomicSubscriberScope = std::unique_ptr<WaitableAtomicSubscriberRemover>;

template <typename DATA>
class WaitableAtomic {
 public:
  using data_t = DATA;

  template <typename... ARGS>
  WaitableAtomic(ARGS&&... args) : data_(std::forward<ARGS>(args)...) {}

  WaitableAtomic(const DATA& data) : data_(data) {}

  template <typename POINTER>
  struct NotifyIfMutable {
    class ImmutableAccessorDoesNotNotify {
     public:
      explicit ImmutableAccessorDoesNotNotify(POINTER*) {}
    };

    class MutableAccessorDoesNotify {
     public:
      explicit MutableAccessorDoesNotify(POINTER* parent) : parent_(parent), mark_as_unmodified_(false) {}
      ~MutableAccessorDoesNotify() {
        if (!mark_as_unmodified_) {
          parent_->Notify();
        }
      }
      void MarkAsUnmodified() { mark_as_unmodified_ = true; }

     private:
      POINTER* parent_;
      bool mark_as_unmodified_;
    };

    using impl_t =
        std::conditional_t<std::is_const<POINTER>::value, ImmutableAccessorDoesNotNotify, MutableAccessorDoesNotify>;
  };

  // A generic implementation for both mutable and immutable scoped accessors.
  template <class PARENT>
  class ScopedAccessorImpl final : private ScopedUniqueLock, public NotifyIfMutable<PARENT>::impl_t {
   public:
    using parent_t = PARENT;
    using optional_notifier_t = typename NotifyIfMutable<PARENT>::impl_t;
    using data_t = std::
        conditional_t<std::is_const<parent_t>::value, const typename parent_t::data_t, typename parent_t::data_t>;

    explicit ScopedAccessorImpl(parent_t* parent)
        : ScopedUniqueLock(parent->data_mutex_), optional_notifier_t(parent), pdata_(&parent->data_) {}

    ScopedAccessorImpl(ScopedAccessorImpl&& rhs)
        : ScopedUniqueLock(std::move(rhs)), optional_notifier_t(rhs), pdata_(rhs.pdata_) {}

    ~ScopedAccessorImpl() {}

    ScopedAccessorImpl() = delete;
    ScopedAccessorImpl(const ScopedAccessorImpl&) = delete;
    void operator=(const ScopedAccessorImpl&) = delete;

    data_t* operator->() const { return pdata_; }
    data_t& operator*() const { return *pdata_; }

   private:
    data_t* pdata_;
  };

  using MutableAccessor = ScopedAccessorImpl<WaitableAtomic>;
  using ImmutableAccessor = ScopedAccessorImpl<const WaitableAtomic>;

  friend class ScopedAccessorImpl<WaitableAtomic>;
  friend class ScopedAccessorImpl<const WaitableAtomic>;

  ImmutableAccessor ImmutableScopedAccessor() const { return ImmutableAccessor(this); }

  MutableAccessor MutableScopedAccessor() { return MutableAccessor(this); }

  void Notify() {
    data_condition_variable_.notify_all();
    {
      // Only lock the subscribers, no need to lock the data.
      // Friendly reminder that the subscribers are expected to return quickly.
      std::lock_guard<std::mutex> lock(subscribers_mutex_);
      for (const auto& unused_and_f : subscribers_) {
        unused_and_f.second();
      }
    }
  }

  void UseAsLock(std::function<void()> f) const {
    std::unique_lock<std::mutex> lock(data_t::data_mutex_);
    f();
  }

  bool Wait(std::function<bool(const data_t&)> pred = [](const data_t& e) { return static_cast<bool>(e); }) const {
    std::unique_lock<std::mutex> lock(data_mutex_);
    if (!pred(data_)) {
      const data_t& data = data_;
      data_condition_variable_.wait(lock, [&pred, &data] { return pred(data); });
    }
    return true;
  }

  // NOTE(dkorolev): Deliberately not bothering with C++14 for this two-lambdas `Wait()`.
  // TODO(dkorolev): The `.Wait()` above always returning `true` could use some TLC.

  template <typename F>
  typename std::result_of<F(data_t&)>::type DoWait(std::function<bool(const data_t&)> wait_predicate, F&& retval_predicate) {
    std::unique_lock<std::mutex> lock(data_mutex_);
    if (!wait_predicate(data_)) {
      const data_t& data = data_;
      data_condition_variable_.wait(lock, [&wait_predicate, &data]() { return wait_predicate(data); });
      return retval_predicate(data_);
    } else {
      return retval_predicate(data_);
    }
  }

  template <typename F, class = std::enable_if_t<std::is_same<typename std::result_of<F(data_t&)>::type, void>::value>>
  void Wait(std::function<bool(const data_t&)> wait_predicate, F&& retval_predicate) {
    DoWait(wait_predicate, std::forward<F>(retval_predicate));
    Notify();
  }

  template <typename F, class = std::enable_if_t<!std::is_same<typename std::result_of<F(data_t&)>::type, void>::value>>
  typename std::result_of<F(data_t&)>::type Wait(std::function<bool(const data_t&)> wait_predicate, F&& retval_predicate) {
    typename std::result_of<F(data_t&)>::type retval = DoWait(wait_predicate, std::forward<F>(retval_predicate));
    Notify();
    return retval;
  }

  template <typename T>
  bool WaitFor(std::function<bool(const data_t&)> predicate, T duration) const {
    std::unique_lock<std::mutex> lock(data_mutex_);
    if (!predicate(data_)) {
      const data_t& data = data_;
      return data_condition_variable_.wait_for(lock, duration, [&predicate, &data] { return predicate(data); });
    }
    return true;
  }

  template <typename T>
  bool WaitFor(T duration) const {
    std::unique_lock<std::mutex> lock(data_mutex_);
    if (!static_cast<bool>(data_)) {
      const data_t& data = data_;
      return data_condition_variable_.wait_for(lock, duration, [&data] { return static_cast<bool>(data); });
    }
    return true;
  }

  template <typename T, typename F>
  typename std::result_of<F(data_t&)>::type WaitFor(std::function<bool(const data_t&)> predicate,
                                                    F&& retval_predicate,
                                                    T duration) {
    std::unique_lock<std::mutex> lock(data_mutex_);
    if (!predicate(data_)) {
      const data_t& data = data_;
      if (data_condition_variable_.wait_for(lock, duration, [&predicate, &data] { return predicate(data); })) {
        return retval_predicate(data_);
      } else {
        // The three-argument `WaitFor()` assumes the default constructor for the return type indicates that
        // the wait should continue. Use the four-argument `WaitFor()` to provide a custom retval initializer.
        // The custom retval predicate can also mutate the waited upon object as it sees fit.
        return typename std::result_of<F(data_t&)>::type();
      }
    } else {
      return retval_predicate(data_);
    }
  }

  template <typename T, typename F, typename G>
  typename std::result_of<F(data_t&)>::type WaitFor(std::function<bool(const data_t&)> predicate,
                                                    F&& retval_predicate,
                                                    G&& wait_unsuccessul_predicate,
                                                    T duration) {
    std::unique_lock<std::mutex> lock(data_mutex_);
    if (!predicate(data_)) {
      const data_t& data = data_;
      if (data_condition_variable_.wait_for(lock, duration, [&predicate, &data] { return predicate(data); })) {
        return retval_predicate(data_);
      } else {
        return wait_unsuccessul_predicate(data_);
      }
    } else {
      return retval_predicate(data_);
    }
  }

#ifndef CURRENT_FOR_CPP14

  template <typename F, typename... ARGS>
  std::invoke_result_t<F, const data_t&> ImmutableUse(F&& f, ARGS&&... args) const {
    auto scope = ImmutableScopedAccessor();
    return f(*scope, std::forward<ARGS>(args)...);
  }

  template <typename F, typename... ARGS>
  std::invoke_result_t<F, data_t&, ARGS...> MutableUse(F&& f, ARGS&&... args) {
    auto scope = MutableScopedAccessor();
    return f(*scope, std::forward<ARGS>(args)...);
  }

#else

  template <typename F, typename... ARGS>
  weed::call_with_type<F, const data_t&, ARGS...> ImmutableUse(F&& f, ARGS&&... args) const {
    auto scope = ImmutableScopedAccessor();
    return f(*scope, std::forward<ARGS>(args)...);
  }

  template <typename F, typename... ARGS>
  weed::call_with_type<F, data_t&, ARGS...> MutableUse(F&& f, ARGS&&... args) {
    auto scope = MutableScopedAccessor();
    return f(*scope, std::forward<ARGS>(args)...);
  }

#endif  // CURRENT_FOR_CPP14

  bool PotentiallyMutableUse(std::function<bool(data_t&)> f) {
    auto scope = MutableScopedAccessor();
    if (f(*scope)) {
      return true;
    } else {
      scope.MarkAsUnmodified();
      return false;
    }
  }
  data_t GetValue() const { return *ImmutableScopedAccessor(); }

  void SetValue(const data_t& data) { *MutableScopedAccessor() = data; }

  void SetValueIf(std::function<bool(const data_t&)> predicate, const data_t& data) {
    auto a = MutableScopedAccessor();
    if (predicate(*a)) {
      *a = data;
    } else {
      a.MarkAsUnmodified();
    }
  }

  struct WaitableAtomicSubscriberRemoverImpl final : WaitableAtomicSubscriberRemover {
    WaitableAtomic& self_;
    const size_t id_;
    // TODO(dkorolev): Use `DISALLOW_COPY_AND_ASSIGN`?!
    WaitableAtomicSubscriberRemoverImpl(WaitableAtomicSubscriberRemoverImpl const&) = delete;
    WaitableAtomicSubscriberRemoverImpl& operator=(WaitableAtomicSubscriberRemoverImpl const&) = delete;
    WaitableAtomicSubscriberRemoverImpl(WaitableAtomic& self, size_t id) : self_(self), id_(id) {}    void Remove() override {
      // Okay to only lock the subscribers map, but not the data.
      std::lock_guard<std::mutex> lock(self_.subscribers_mutex_);
      self_.subscribers_.erase(id_);
    }
  };

  [[nodiscard]]
  WaitableAtomicSubscriberScope Subscribe(std::function<void()> f) {
    // Need to lock both the data and the subscribers map to ensure exactly-once delivery of updates.
    // The order is this way because subscribers are assumed to be locked for a shorter period of time.
    // The assumption is that the clients will not perform slow operations and/or lock anything while notified,
    // but at most schedule some tasks to be executed in their respective threads, thus releasing this lock quickly.
    std::lock_guard<std::mutex> lock_data(data_mutex_);
    std::lock_guard<std::mutex> lock_subscribers(subscribers_mutex_);
    const size_t id = subscriber_next_id_;
    ++subscriber_next_id_;
    subscribers_[id] = f;
    return std::make_unique<WaitableAtomicSubscriberRemoverImpl>(*this, id);
  }

 protected:
  data_t data_;
  std::mutex subscribers_mutex_;  // Declare the innermost mutex first.
  mutable std::mutex data_mutex_;
  mutable std::condition_variable data_condition_variable_;
  std::map<size_t, std::function<void()>> subscribers_;
  size_t subscriber_next_id_ = 0u;

 private:
  WaitableAtomic(const WaitableAtomic&) = delete;
  void operator=(const WaitableAtomic&) = delete;
  WaitableAtomic(WaitableAtomic&&) = delete;
};

}  // namespace current

#endif  // BRICKS_WAITABLE_ATOMIC_H
