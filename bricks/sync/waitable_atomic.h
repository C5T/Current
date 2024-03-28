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
// Additionally, if the second template parameter is set to true, WaitableAtomic<T, true> allows
// creating scoped clients via `IntrusiveClient client = my_waitable_atomic.RegisterScopedClient()`.
// When an instance of intrusive WaitableAtomic is going out of scope:
// 1) Its registered clients will be notified.           (IntrusiveClient supports `operator bool()`).
// 2) Pending wait operations, if any, will be aborted.  (And return `false`.)
// 3) WaitableAtomic will wait for all the clients to gracefully terminate (go out of scope)
//    before destructing the data object contained within this WaitableAtomic.

#ifndef BRICKS_WAITABLE_ATOMIC_H
#define BRICKS_WAITABLE_ATOMIC_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>

#include "../time/chrono.h"

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

template <typename DATA, bool INTRUSIVE = false>
class WaitableAtomicImpl {
 public:
  class BasicImpl {
   public:
    using data_t = DATA;
    enum { IS_INTRUSIVE = false };

    template <typename... ARGS>
    BasicImpl(ARGS&&... args) : data_(std::forward<ARGS>(args)...) {}

    BasicImpl(const DATA& data) : data_(data) {}

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

    using MutableAccessor = ScopedAccessorImpl<BasicImpl>;
    using ImmutableAccessor = ScopedAccessorImpl<const BasicImpl>;

    friend class ScopedAccessorImpl<BasicImpl>;
    friend class ScopedAccessorImpl<const BasicImpl>;

    ImmutableAccessor ImmutableScopedAccessor() const { return ImmutableAccessor(this); }

    MutableAccessor MutableScopedAccessor() { return MutableAccessor(this); }

    void Notify() { data_condition_variable_.notify_all(); }

    void UseAsLock(std::function<void()> f) const {
      std::unique_lock<std::mutex> lock(data_t::data_mutex_);
      f();
    }

    bool Wait(std::function<bool(const data_t&)> predicate) const {
      std::unique_lock<std::mutex> lock(data_mutex_);
      if (!predicate(data_)) {
        const data_t& data = data_;
        data_condition_variable_.wait(lock, [&predicate, &data] { return predicate(data); });
      }
      return true;
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

   protected:
    data_t data_;
    mutable std::mutex data_mutex_;
    mutable std::condition_variable data_condition_variable_;

   private:
    BasicImpl(const BasicImpl&) = delete;
    void operator=(const BasicImpl&) = delete;
    BasicImpl(BasicImpl&&) = delete;
  };

  class IntrusiveImpl : public BasicImpl, public IntrusiveClient::Interface {
   public:
    using data_t = DATA;
    enum { IS_INTRUSIVE = true };

    explicit IntrusiveImpl(CustomWaitableAtomicDestructor* destructor_ptr = nullptr)
        : destructing_(false), destructor_ptr_(destructor_ptr) {
      RefCounterTryIncrease();
    }

    explicit IntrusiveImpl(const data_t& data, CustomWaitableAtomicDestructor* destructor_ptr = nullptr)
        : BasicImpl(data), destructing_(false), destructor_ptr_(destructor_ptr) {
      RefCounterTryIncrease();
    }

    virtual ~IntrusiveImpl() {
      {
        std::lock_guard<std::mutex> guard(BasicImpl::data_mutex_);
        destructing_ = true;
        BasicImpl::Notify();
      }
      RefCounterDecrease();
      if (destructor_ptr_) {
        destructor_ptr_->WaitableAtomicDestructing();  // LCOV_EXCL_LINE
      }
      {
        // Wait for the registered scoped clients to leave their respective scopes.
        std::unique_lock<std::mutex> lock(BasicImpl::data_mutex_);
        if (ref_count_ > 0u) {
          cv_.wait(lock, [this]() { return ref_count_ == 0u; });
        }
      }
    }

    bool Wait(std::function<bool(const data_t&)> predicate) const {
      std::unique_lock<std::mutex> lock(BasicImpl::data_mutex_);
      if (destructing_) {
        return false;
      } else {
        if (!predicate(BasicImpl::data_)) {
          const data_t& data = BasicImpl::data_;
          BasicImpl::data_condition_variable_.wait(
              lock, [this, &predicate, &data] { return destructing_ || predicate(data); });
        }
        return !destructing_;
      }
    }

    IntrusiveClient RegisterScopedClient() { return IntrusiveClient(this); }

    virtual bool RefCounterTryIncrease() override {
      {
        std::lock_guard<std::mutex> guard(BasicImpl::data_mutex_);
        if (destructing_) {
          return false;
        } else {
          ++ref_count_;
        }
      }
      cv_.notify_one();
      return true;
    }

    virtual void RefCounterDecrease() override {
      {
        std::lock_guard<std::mutex> guard(BasicImpl::data_mutex_);
        --ref_count_;
      }
      cv_.notify_one();
    }

    virtual bool IsDestructing() const override {
      std::lock_guard<std::mutex> guard(BasicImpl::data_mutex_);
      return destructing_;
    }

   protected:
    std::atomic_bool destructing_;
    size_t ref_count_ = 0;
    std::condition_variable cv_;
    CustomWaitableAtomicDestructor* destructor_ptr_ = nullptr;

   private:
    IntrusiveImpl(const IntrusiveImpl&) = delete;
    void operator=(const IntrusiveImpl&) = delete;
    IntrusiveImpl(IntrusiveImpl&&) = delete;
  };

  using type = std::conditional_t<INTRUSIVE, IntrusiveImpl, BasicImpl>;
};

template <typename DATA, bool INTRUSIVE = false>
using WaitableAtomic = typename WaitableAtomicImpl<DATA, INTRUSIVE>::type;

}  // namespace current

#endif  // BRICKS_WAITABLE_ATOMIC_H
