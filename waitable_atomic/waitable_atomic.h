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

#ifndef SHERLOCK_WAITABLE_ATOMIC_H
#define SHERLOCK_WAITABLE_ATOMIC_H

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace sherlock {

class CustomWaitableAtomicDestructor {
 public:
  virtual void WaitableAtomicDestructing() = 0;
  virtual ~CustomWaitableAtomicDestructor() = default;
};

class ScopedUniqueLock {
 public:
  inline explicit ScopedUniqueLock(std::mutex& mutex) : lock_(mutex) {}
  inline ~ScopedUniqueLock() {}
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
    virtual bool IsNotDestructing() const = 0;
    friend class IntrusiveClient;
  };

  inline explicit IntrusiveClient(Interface* object) : intrusive_object_(object) {
    if (intrusive_object_) {
      if (!intrusive_object_->RefCounterTryIncrease()) {
        intrusive_object_ = nullptr;
      }
    }
  }

  IntrusiveClient(IntrusiveClient&& rhs) : intrusive_object_(nullptr) {
    std::swap(intrusive_object_, rhs.intrusive_object_);
  }

  inline ~IntrusiveClient() {
    if (intrusive_object_) {
      intrusive_object_->RefCounterDecrease();
    }
  }

  inline operator bool() const { return intrusive_object_ && intrusive_object_->IsNotDestructing(); }

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

    inline BasicImpl() : data_() {}

    inline explicit BasicImpl(const T_DATA& data) : data_(data) {}

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

    template <class PARENT>
    class TemplatedScopedAccessor : private ScopedUniqueLock, public NotifyIfMutable<PARENT>::type {
     public:
      typedef PARENT T_PARENT;
      typedef typename NotifyIfMutable<PARENT>::type T_OPTIONAL_NOTIFIER;

      typedef typename std::conditional<std::is_const<T_PARENT>::value,
                                        const typename T_PARENT::T_DATA,
                                        typename T_PARENT::T_DATA>::type T_DATA;

      explicit inline TemplatedScopedAccessor(T_PARENT* parent)
          : ScopedUniqueLock(parent->data_mutex_), T_OPTIONAL_NOTIFIER(parent), pdata_(&parent->data_) {}

      inline TemplatedScopedAccessor(TemplatedScopedAccessor&& rhs)
          : ScopedUniqueLock(std::move(rhs)), T_OPTIONAL_NOTIFIER(rhs), pdata_(rhs.pdata_) {}

      inline ~TemplatedScopedAccessor() {}

      TemplatedScopedAccessor() = delete;
      TemplatedScopedAccessor(const TemplatedScopedAccessor&) = delete;
      void operator=(const TemplatedScopedAccessor&) = delete;

      inline T_DATA* operator->() { return pdata_; }
      inline T_DATA& operator*() { return *pdata_; }

     private:
      T_DATA* pdata_;
    };

    typedef TemplatedScopedAccessor<BasicImpl> MutableAccessor;
    typedef TemplatedScopedAccessor<const BasicImpl> ImmutableAccessor;

    friend class TemplatedScopedAccessor<BasicImpl>;
    friend class TemplatedScopedAccessor<const BasicImpl>;

    inline ImmutableAccessor ImmutableScopedAccessor() const { return ImmutableAccessor(this); }

    inline MutableAccessor MutableScopedAccessor() { return MutableAccessor(this); }

    inline void Notify() { data_condition_variable_.notify_all(); }

    inline void UseAsLock(std::function<void()> f) const {
      std::unique_lock<std::mutex> lock(T_DATA::data_mutex_);
      f();
    }

    inline bool Wait(std::function<bool(const T_DATA&)> predicate) const {
      std::unique_lock<std::mutex> lock(data_mutex_);
      if (!predicate(data_)) {
        const T_DATA& data = std::ref(data_);
        data_condition_variable_.wait(lock, [&predicate, &data] { return predicate(data); });
      }
      return true;
    }

    inline void ImmutableUse(std::function<void(const T_DATA&)> f) const {
      auto scope = ImmutableScopedAccessor();
      f(*scope);
    }

    inline void MutableUse(std::function<void(T_DATA&)> f) {
      auto scope = MutableScopedAccessor();
      f(*scope);
    }

    inline void PotentiallyMutableUse(std::function<bool(T_DATA&)> f) {
      auto scope = MutableScopedAccessor();
      if (!f(*scope)) {
        scope.MarkAsUnmodified();
      }
    }

    inline T_DATA GetValue() const { return *ImmutableScopedAccessor(); }

    inline void SetValue(const T_DATA& data) { *MutableScopedAccessor() = data; }

    inline void SetValueIf(std::function<bool(const T_DATA&)> predicate, const T_DATA& data) {
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

    inline explicit IntrusiveImpl(const T_DATA& data, CustomWaitableAtomicDestructor* destructor_ptr = nullptr)
        : BasicImpl(data), destructor_ptr_(destructor_ptr) {
      RefCounterTryIncrease();
    }

    inline virtual ~IntrusiveImpl() {
      {
        std::lock_guard<std::mutex> guard(BasicImpl::data_mutex_);
        not_destructing_ = false;
        BasicImpl::Notify();
      }
      RefCounterDecrease();
      if (destructor_ptr_) {
        destructor_ptr_->WaitableAtomicDestructing();
      }
      ref_count_is_nonzero_mutex_.lock();
    }

    inline bool Wait(std::function<bool(const T_DATA&)> predicate) const {
      std::unique_lock<std::mutex> lock(BasicImpl::data_mutex_);
      if (!not_destructing_) {
        return false;
      } else {
        if (!predicate(BasicImpl::data_)) {
          const T_DATA& data = std::ref(BasicImpl::data_);
          BasicImpl::data_condition_variable_.wait(
              lock, [this, &predicate, &data] { return !not_destructing_ || predicate(data); });
        }
        return not_destructing_;
      }
    }

    IntrusiveClient RegisterScopedClient() { return IntrusiveClient(this); }

    virtual bool RefCounterTryIncrease() override {
      std::lock_guard<std::mutex> guard(BasicImpl::data_mutex_);
      if (!not_destructing_) {
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

    virtual bool IsNotDestructing() const override {
      std::lock_guard<std::mutex> guard(BasicImpl::data_mutex_);
      return not_destructing_;
    }

   protected:
    size_t ref_count_ = 0;
    bool not_destructing_ = true;
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

}  // namespace sherlock

#endif  // SHERLOCK_WAITABLE_ATOMIC_H
