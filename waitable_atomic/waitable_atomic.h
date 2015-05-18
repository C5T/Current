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
#include <mutex>

#include "../time/chrono.h"

namespace bricks {

class CustomWaitableAtomicDestructor {
 public:
  virtual void WaitableAtomicDestructing() = 0;
  virtual ~CustomWaitableAtomicDestructor() = default;
};

// A wrapper around `std::unique_lock<std::mutex>` that allows being inherited from itself.
class ScopedUniqueLock {
 public:
  explicit ScopedUniqueLock(std::mutex& mutex) : lock_(mutex) {}
  ~ScopedUniqueLock() {}
  ScopedUniqueLock(ScopedUniqueLock&& rhs) { lock_ = std::move(rhs.lock_); }

 private:
  std::unique_lock<std::mutex> lock_;

  ScopedUniqueLock(const ScopedUniqueLock&) = delete;
  void operator=(const ScopedUniqueLock&) = delete;
};

class IntrusiveClient {
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
    typedef DATA T_DATA;
    enum { IS_INTRUSIVE = false };

    BasicImpl() : data_() {}

    explicit BasicImpl(const T_DATA& data) : data_(data) {}

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
      typedef typename std::conditional<std::is_const<POINTER>::value,
                                        ImmutableAccessorDoesNotNotify,
                                        MutableAccessorDoesNotify>::type type;
    };

    // A generic implementation for both mutable and immutable scoped accessors.
    template <class PARENT>
    class ScopedAccessorImpl : private ScopedUniqueLock, public NotifyIfMutable<PARENT>::type {
     public:
      typedef PARENT T_PARENT;
      typedef typename NotifyIfMutable<PARENT>::type T_OPTIONAL_NOTIFIER;

      typedef typename std::conditional<std::is_const<T_PARENT>::value,
                                        const typename T_PARENT::T_DATA,
                                        typename T_PARENT::T_DATA>::type T_DATA;

      explicit ScopedAccessorImpl(T_PARENT* parent)
          : ScopedUniqueLock(parent->data_mutex_), T_OPTIONAL_NOTIFIER(parent), pdata_(&parent->data_) {}

      ScopedAccessorImpl(ScopedAccessorImpl&& rhs)
          : ScopedUniqueLock(std::move(rhs)), T_OPTIONAL_NOTIFIER(rhs), pdata_(rhs.pdata_) {}

      ~ScopedAccessorImpl() {}

      ScopedAccessorImpl() = delete;
      ScopedAccessorImpl(const ScopedAccessorImpl&) = delete;
      void operator=(const ScopedAccessorImpl&) = delete;

      T_DATA* operator->() { return pdata_; }
      T_DATA& operator*() { return *pdata_; }

     private:
      T_DATA* pdata_;
    };

    typedef ScopedAccessorImpl<BasicImpl> MutableAccessor;
    typedef ScopedAccessorImpl<const BasicImpl> ImmutableAccessor;

    friend class ScopedAccessorImpl<BasicImpl>;
    friend class ScopedAccessorImpl<const BasicImpl>;

    ImmutableAccessor ImmutableScopedAccessor() const { return ImmutableAccessor(this); }

    MutableAccessor MutableScopedAccessor() { return MutableAccessor(this); }

    void Notify() { data_condition_variable_.notify_all(); }

    void UseAsLock(std::function<void()> f) const {
      std::unique_lock<std::mutex> lock(T_DATA::data_mutex_);
      f();
    }

    bool Wait(std::function<bool(const T_DATA&)> predicate) const {
      std::unique_lock<std::mutex> lock(data_mutex_);
      if (!predicate(data_)) {
        const T_DATA& data = std::ref(data_);
        data_condition_variable_.wait(lock, [&predicate, &data] { return predicate(data); });
      }
      return true;
    }

    bool WaitFor(std::function<bool(const T_DATA&)> predicate, bricks::time::MILLISECONDS_INTERVAL ms) const {
      std::unique_lock<std::mutex> lock(data_mutex_);
      if (!predicate(data_)) {
        const T_DATA& data = std::ref(data_);
        data_condition_variable_.wait_for(lock,
                                          std::chrono::milliseconds(static_cast<uint64_t>(ms)),
                                          [&predicate, &data] { return predicate(data); });
      }
      return true;
    }

    void ImmutableUse(std::function<void(const T_DATA&)> f) const {
      auto scope = ImmutableScopedAccessor();
      f(*scope);
    }

    void MutableUse(std::function<void(T_DATA&)> f) {
      auto scope = MutableScopedAccessor();
      f(*scope);
    }

    void PotentiallyMutableUse(std::function<bool(T_DATA&)> f) {
      auto scope = MutableScopedAccessor();
      if (!f(*scope)) {
        scope.MarkAsUnmodified();
      }
    }

    T_DATA GetValue() const { return *ImmutableScopedAccessor(); }

    void SetValue(const T_DATA& data) { *MutableScopedAccessor() = data; }

    void SetValueIf(std::function<bool(const T_DATA&)> predicate, const T_DATA& data) {
      auto a = MutableScopedAccessor();
      if (predicate(*a)) {
        *a = data;
      } else {
        a.MarkAsUnmodified();
      }
    }

   protected:
    T_DATA data_;
    mutable std::mutex data_mutex_;
    mutable std::condition_variable data_condition_variable_;

   private:
    BasicImpl(const BasicImpl&) = delete;
    void operator=(const BasicImpl&) = delete;
    BasicImpl(BasicImpl&&) = delete;
  };

  class IntrusiveImpl : public BasicImpl, public IntrusiveClient::Interface {
   public:
    typedef DATA T_DATA;
    enum { IS_INTRUSIVE = true };

    explicit IntrusiveImpl(CustomWaitableAtomicDestructor* destructor_ptr = nullptr)
        : destructor_ptr_(destructor_ptr) {
      RefCounterTryIncrease();
    }

    explicit IntrusiveImpl(const T_DATA& data, CustomWaitableAtomicDestructor* destructor_ptr = nullptr)
        : BasicImpl(data), destructor_ptr_(destructor_ptr) {
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
      // Wait for the registered scoped clients to leave their respective scopes.
      ref_count_is_nonzero_mutex_.lock();
    }

    bool Wait(std::function<bool(const T_DATA&)> predicate) const {
      std::unique_lock<std::mutex> lock(BasicImpl::data_mutex_);
      if (destructing_) {
        return false;
      } else {
        if (!predicate(BasicImpl::data_)) {
          const T_DATA& data = std::ref(BasicImpl::data_);
          BasicImpl::data_condition_variable_.wait(
              lock, [this, &predicate, &data] { return destructing_ || predicate(data); });
        }
        return !destructing_;
      }
    }

    IntrusiveClient RegisterScopedClient() { return IntrusiveClient(this); }

    virtual bool RefCounterTryIncrease() override {
      std::lock_guard<std::mutex> guard(BasicImpl::data_mutex_);
      if (destructing_) {
        return false;
      } else {
        if (!ref_count_++) {
          ref_count_is_nonzero_mutex_.lock();
        }
        return true;
      }
    }

    virtual void RefCounterDecrease() override {
      std::lock_guard<std::mutex> guard(BasicImpl::data_mutex_);
      if (!--ref_count_) {
        ref_count_is_nonzero_mutex_.unlock();
      }
    }

    virtual bool IsDestructing() const override {
      std::lock_guard<std::mutex> guard(BasicImpl::data_mutex_);
      return destructing_;
    }

   protected:
    size_t ref_count_ = 0;
    bool destructing_ = false;
    std::mutex ref_count_is_nonzero_mutex_;
    CustomWaitableAtomicDestructor* destructor_ptr_ = nullptr;

   private:
    IntrusiveImpl(const IntrusiveImpl&) = delete;
    void operator=(const IntrusiveImpl&) = delete;
    IntrusiveImpl(IntrusiveImpl&&) = delete;
  };

  typedef typename std::conditional<INTRUSIVE, IntrusiveImpl, BasicImpl>::type type;
};

template <typename DATA, bool INTRUSIVE = false>
using WaitableAtomic = typename WaitableAtomicImpl<DATA, INTRUSIVE>::type;

}  // namespace bricks

#endif  // BRICKS_WAITABLE_ATOMIC_H
