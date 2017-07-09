/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

// The synchronization primitive to leverage RAII and enable creating scope-based long-lived objects.
//
// Philosophy:
// * The scope in which the object has been created is the master owner of this object; others are borrowers.
// * Upon leaving the master scope, the synchronization primitive waits until all its borrowers terminate.
// * Borrowers are under no obligation to terminate sooner or later, but they get signaled to wrap up.
//
// Implementation:
//
// * auto master = current::MakeOwned<T>( [ ... T's constructor arguments ]);
//   Creates `master`, which will live only within its creation scope.
//   The basic usage of `master` is `master->MemberFunctionOfT(...)`, much like with C++'s `shared_ptr<T>`.
//
// * Borrowed<T> borrower(master);  // Or `Borrowed<T> another_borrower(borrower);`.
//   Initializes `borrower`, which can be used same way as `master`.
//   Casting it to `bool` (or just checking for `if (!borrower) { ... }`) would result in `false`
//   if and only if the master scope (the scope of the `Owned<T>` is being terminated, and is currently
//   awaiting until all the borrowers are done.
//
// * BorrowedWithCallback<T> borrower(master, [&signal]() { signal.SignalTermination(); });
//   Initializes `borrower` passing it an external signal that will be likely `std::move()` it into a different thread.
//   The lifetime of the `borrower` object will be strictly inside the lifetime of `master`.
//   The destructor of `master` will wait until all the `borrower`-s have terminated.
//   The callbacks, corresponding to active `borrower`-s, will be called as `master` will be destructing.

#ifndef BRICKS_SYNC_OWNED_BORROWED_H
#define BRICKS_SYNC_OWNED_BORROWED_H

#include "../../port.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "../exception.h"

#include "../template/decay.h"

namespace current {
namespace sync {

// The container for the actual instance and its environment.
namespace impl {

// Helper to prohibit copying `Owned`.
struct ConstructOwned {};
struct ConstructBorrowable {};
struct ConstructBorrowableObjectViaMoveConstructor {};

struct ConstructUniqueContainerViaMoveConstructor {};

// The actual instance, kept along with its destructing status, and a list of registered borrowers.
template <typename T>
struct UniqueInstance final {
  // Constructor: Construct an instance of the object.
  UniqueInstance() : destructing_(false), instance_(), total_borrowers_spawned_throughout_lifetime_(0u) {}
  template <typename... ARGS>
  UniqueInstance(ARGS&&... args)
      : destructing_(false), instance_(std::forward<ARGS>(args)...), total_borrowers_spawned_throughout_lifetime_(0u) {}

  // Destructor: Block until all the borrowers are done.
  ~UniqueInstance() {
    // Mark the instance of the object as being destructed.
    destructing_ = true;

    // Collect all the "destructing" callbacks to call.
    std::vector<std::function<void()>> callbacks_to_call;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      for (const auto& cit : borrowers_) {
        callbacks_to_call.push_back(cit.second);
      }
    }

    // Call all the "destructing" callbacks.
    for (auto& f : callbacks_to_call) {
      f();
    }

    // Wait until all the borrowers have terminated.
    {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_variable_.wait(lock, [this]() { return borrowers_.empty(); });
    }
  }

  // Register ("increase the ref-count") a newly spawned borrower, along with its termination signal callback.
  // Must be called from within a mutex-locked section.
  size_t RegisterBorrowerFromLockedSection(std::function<void()> destruction_callback) {
    borrowers_[++total_borrowers_spawned_throughout_lifetime_] = destruction_callback;
    return total_borrowers_spawned_throughout_lifetime_;
  }

  void UpdateBorrowersCallbackFromLockedSection(size_t key, std::function<void()> destruction_callback) {
    borrowers_[key] = destruction_callback;
  }

  // Unregister ("decrease the ref-count") a previously spawned borrower.
  // Locks its own mutex; must be called from outside a mutex-locked section.
  void UnRegisterBorrower(size_t borrower_index) {
    bool notify = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = borrowers_.find(borrower_index);
      CURRENT_ASSERT(it != borrowers_.end());
      borrowers_.erase(it);
      notify = borrowers_.empty();
    }
    if (notify) {
      condition_variable_.notify_one();
    }
  }

  UniqueInstance(const UniqueInstance&) = delete;
  UniqueInstance(UniqueInstance&&) = delete;
  UniqueInstance& operator=(const UniqueInstance&) = delete;
  UniqueInstance& operator=(UniqueInstance&&) = delete;

  std::atomic_bool destructing_;                        // Whether it already is in the "destructing" mode.
  T instance_;                                          // The actual instance.
  std::mutex mutex_;                                    // The mutex to guard per-borrower info.
  std::condition_variable condition_variable_;          // The variable to notify as all the borrowers are gone.
  std::map<size_t, std::function<void()>> borrowers_;   // Map of live borrower -> termination signal callback.
  size_t total_borrowers_spawned_throughout_lifetime_;  // A pre-increment of this value is the key in the above map.
};

}  // namespace current::sync::impl

template <typename T>
class Owned;
template <typename T>
class Borrowed;
template <typename T>
class BorrowedWithCallback;
template <typename T>
class BorrowedOfGuaranteedLifetime;

// `WeakBorrowed<T>` is the base class for `Owned<T>`, `Borrowed<T>`, and `BorrowedWithCallback<T>`.
template <typename T>
class WeakBorrowed {
 private:
  using instance_t = impl::UniqueInstance<T>;

 protected:
  // `WeakBorrowed<T>` is meant to be initialized by `Owned<T>` | `BorrowedWithCallback<T>` | `Borrowed<T>` only.
  WeakBorrowed(impl::ConstructOwned, instance_t& actual_instance) : p_actual_instance_(&actual_instance), key_(0u) {}

  WeakBorrowed(impl::ConstructBorrowable, const WeakBorrowed& rhs, std::function<void()> destruction_callback) {
    std::lock_guard<std::mutex> lock(rhs.p_actual_instance_->mutex_);
    p_actual_instance_ = rhs.p_actual_instance_;
    key_ = p_actual_instance_->RegisterBorrowerFromLockedSection(destruction_callback);
  }

  WeakBorrowed(impl::ConstructBorrowableObjectViaMoveConstructor,
               WeakBorrowed&& rhs,
               std::function<void()> destruction_callback) {
    MoveFrom(std::move(rhs), destruction_callback);
  }

  void InitializedMovedOwned(instance_t& actual_instance) {
    p_actual_instance_ = &actual_instance;
    key_ = 0u;
  }

  void MoveFrom(WeakBorrowed&& rhs, std::function<void()> destruction_callback) {
    std::lock_guard<std::mutex> lock(rhs.p_actual_instance_->mutex_);
    p_actual_instance_ = rhs.p_actual_instance_;
    key_ = rhs.key_;
    p_actual_instance_->UpdateBorrowersCallbackFromLockedSection(key_, destruction_callback);
    rhs.key_ = 0u;  // Must mark the passed in xvalue of `WeakBorrowed<>` as abandoned.
    rhs.p_actual_instance_ = nullptr;
  }

  WeakBorrowed(const WeakBorrowed&) = delete;
  WeakBorrowed(WeakBorrowed&&) = delete;
  WeakBorrowed& operator=(const WeakBorrowed&) = delete;
  WeakBorrowed& operator=(WeakBorrowed&&) = delete;

 public:
  ~WeakBorrowed() { InternalUnRegister(); }

  // Whether the object is valid and has not yet been marked for destruction.
  // The only way to invalidate the object is to move away from it.
  // The only way to mark it for destruction is for the `Owned` object to get out of scope.
  // THREAD-SAFE. NEVER THROWS.
  bool IsValid() const noexcept { return p_actual_instance_ && !p_actual_instance_->destructing_; }
  operator bool() const noexcept { return IsValid(); }

  // Smart-pointer-style accessors.
  // THREAD-SAFE. NEVER THROWS.
  T& operator*() { return p_actual_instance_->instance_; }
  const T& operator*() const { return p_actual_instance_->instance_; }
  T* operator->() { return &p_actual_instance_->instance_; }
  const T* operator->() const { return &p_actual_instance_->instance_; }

  // Exclusive accessors.
  // THREAD-SAFE WITH RESPECT TO MAKING SURE UNDERLYING OBJECT IS ACCESSED EXCLUSIVELY BY THE PASSED IN FUNCTOR.
  // NEVER THROWS.
  void ExclusiveUse(std::function<void(T&)> f) {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    f(operator*());
  }
  void ExclusiveUse(std::function<void(const T&)> f) const {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    f(operator*());
  }
  template <typename V>
  V ExclusiveUse(std::function<V(T&)> f) {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    return f(operator*());
  }
  template <typename V>
  V ExclusiveUse(std::function<V(const T&)> f) const {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    return f(operator*());
  }

  // Return the number of registered borrower users besides the master one.
  // For unit-testing purposes mostly, but may end up useful. -- D.K.
  // THREAD-SAFE. NEVER THROWS.
  size_t NumberOfActiveBorrowers() const noexcept {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    return p_actual_instance_->borrowers_.size();
  }

  // Return the total number of registered borrower users registered, with some possibly already out of scope.
  // For unit-testing purposes mostly, but may end up useful. -- D.K.
  // THREAD-SAFE. NEVER THROWS.
  size_t TotalBorrowersSpawnedThroughoutLifetime() const noexcept {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    return p_actual_instance_->total_borrowers_spawned_throughout_lifetime_;
  }

 protected:
  instance_t* p_actual_instance_;  // Is set to `nullptr` externally when moved.

  void InternalUnRegister() {
    if (key_) {
      CURRENT_ASSERT(p_actual_instance_);
      // `*this` is a valid (non - std::move()-d - away) `BorrowedWithCallback`.
      // As it is being terminated, it should unregister its callback, so that:
      // 1) If the object is still alive, to have this callback not be called upon it being destructed, and/or
      // 2) If the object is now destructing, to indicate this borrower has released it, to decrement
      //    the active borrowers counter, and trigger a condition variable `*p_actual_instance_` is waiting on
      //    if this counter has reached zero with this unregistration.
      p_actual_instance_->UnRegisterBorrower(key_);
      key_ = 0u;
    }
    p_actual_instance_ = nullptr;
  }

 private:
  size_t key_;  // The index of the slave user to mark as left the scope, or `0u` if N/A (master or moved away).
};

namespace impl {

// `UniqueInstanceContainer<T>` must be a dedicated struct for `Owned<T>` to construct
// the actual object before initializing the `WeakBorrowed<T>` associated with it.
// `UniqueInstanceContainer<T>` is constructible, movable, and nothing else.
template <typename T>
struct UniqueInstanceContainer {
  std::unique_ptr<UniqueInstance<T>> movable_instance_;

  UniqueInstanceContainer(ConstructUniqueContainerViaMoveConstructor, UniqueInstanceContainer&& rhs)
      : movable_instance_(std::move(rhs.movable_instance_)) {}

  UniqueInstanceContainer() : movable_instance_(std::make_unique<UniqueInstance<T>>()) {}

  template <typename... ARGS>
  UniqueInstanceContainer(ARGS&&... args)
      : movable_instance_(std::make_unique<UniqueInstance<T>>(std::forward<ARGS>(args)...)) {}

  UniqueInstanceContainer(UniqueInstanceContainer&&) = default;
  UniqueInstanceContainer& operator=(UniqueInstanceContainer&&) = default;

  UniqueInstanceContainer(const UniqueInstanceContainer&) = delete;
  UniqueInstanceContainer& operator=(const UniqueInstanceContainer&) = delete;
};

}  // namespace current::sync::impl

template <typename T>
struct ConstructOwned {};

// `Owned<T>` is constructible, movable, and can spin off `BorrowedWithCallback<T>` offspring.
template <typename T>
class Owned final : private impl::UniqueInstanceContainer<T>, public WeakBorrowed<T> {
 private:
  static_assert(std::is_same<T, decay<T>>::value, "`Owned<>` requires a clean type.");

  using impl_t = impl::UniqueInstanceContainer<T>;
  using base_t = WeakBorrowed<T>;

 public:
  Owned() : impl_t(), base_t(impl::ConstructOwned(), *impl_t::movable_instance_) {}

#ifndef CURRENT_OWNED_BORROWED_EXPLICIT_ONLY_CONSTRUCTION
  // For the end users of Current, we allow "manual" `Owned` construction via `ARGS...`.
  // (It's really for "backwards compatibility" reasons, we could revisit this. -- D.K.)
  // Within Current, `MakeOwned<T>(...)` or `Owned<T>(ConstructOwned<T>(), ...)` are used, and thus
  // the "liberal" constructor is unnecessary.
  // Original background/rationale: Easier type-level development/debugging of Current itself.
  template <typename... ARGS>
  Owned(ARGS&&... args)
      : impl_t(std::forward<ARGS>(args)...), base_t(impl::ConstructOwned(), *impl_t::movable_instance_) {}
#endif  // CURRENT_OWNED_BORROWED_EXPLICIT_ONLY_CONSTRUCTION

  template <typename... ARGS>
  Owned(ConstructOwned<T>, ARGS&&... args)
      : impl_t(std::forward<ARGS>(args)...), base_t(impl::ConstructOwned(), *impl_t::movable_instance_) {}

  Owned(Owned&& rhs)
      : impl_t(impl::ConstructUniqueContainerViaMoveConstructor(), std::move(static_cast<impl_t&&>(rhs))),
        base_t(impl::ConstructOwned(), *impl_t::movable_instance_) {
    rhs.base_t::p_actual_instance_ = nullptr;
  }

  Owned& operator=(Owned&& rhs) {
    impl_t::operator=(std::move(rhs));
    base_t::InitializedMovedOwned(*impl_t::movable_instance_);
    rhs.p_actual_instance_ = nullptr;
    return *this;
  }

  Owned(const Owned&) = delete;
  Owned& operator=(const Owned&) = delete;
};

// The constructor of `BorrowedWithCallback<T>` requires two parameters:
// 1) `const WeakBorrowed<T>&`, and
// 2) a required callback, which will be called once the scope of the `Owned<T>` will be terminating.
// Obviously, an instance of `BorrowedWithCallback` can't be moved natively, as the
// destruction callback it uses must point to the respective owner object, not to the "old" one.
template <typename T>
class BorrowedWithCallback final : public WeakBorrowed<T> {
 private:
  static_assert(std::is_same<T, decay<T>>::value, "`BorrowedWithCallback<>` requires a clean type.");

  using base_t = WeakBorrowed<T>;

 public:
  BorrowedWithCallback(const WeakBorrowed<T>& rhs, std::function<void()> destruction_callback)
      : base_t(impl::ConstructBorrowable(), rhs, destruction_callback) {}

  BorrowedWithCallback(WeakBorrowed<T>&& rhs, std::function<void()> destruction_callback)
      : base_t(impl::ConstructBorrowableObjectViaMoveConstructor(), std::move(rhs), destruction_callback) {}

  void operator=(std::nullptr_t) { base_t::InternalUnRegister(); }

  BorrowedWithCallback() = delete;
  BorrowedWithCallback(const BorrowedWithCallback&) = delete;
  BorrowedWithCallback(BorrowedWithCallback&&) = delete;
  BorrowedWithCallback& operator=(const BorrowedWithCallback&) = delete;
  BorrowedWithCallback& operator=(BorrowedWithCallback&& rhs) = delete;
};

// `Borrowed<T>` is just like `BorrowedWithCallback<T>`, but it does not have an associated destruction callback.
// Thus:
// 1) The user is responsible for periodically checking its validity
//    (otherwise the `Owned` scope will wait forever in the destructor), and
// 2) Unlike `BorrowedWithCallback<T>`, instances of `Borrowed<T>` can be safely moved
//    (which makes them ideal for returning as lightweight values, passing as parameters to threads, etc.)
template <typename T>
class Borrowed final : public WeakBorrowed<T> {
 private:
  static_assert(std::is_same<T, decay<T>>::value, "`Borrowed<>` requires a clean type.");

  using base_t = WeakBorrowed<T>;

 public:
  Borrowed(const Borrowed& rhs) : base_t(impl::ConstructBorrowable(), rhs, []() {}) {}

  Borrowed(Borrowed&& rhs) : base_t(impl::ConstructBorrowableObjectViaMoveConstructor(), std::move(rhs), []() {}) {}

  Borrowed& operator=(Borrowed&& rhs) {
    base_t::MoveFrom(std::move(rhs), []() {});
    return *this;
  }

  Borrowed(const WeakBorrowed<T>& rhs) : base_t(impl::ConstructBorrowable(), rhs, []() {}) {}

  Borrowed(WeakBorrowed<T>&& rhs)
      : base_t(impl::ConstructBorrowableObjectViaMoveConstructor(), std::move(rhs), []() {}) {}

  void operator=(std::nullptr_t) { base_t::InternalUnRegister(); }

  Borrowed& operator=(const Borrowed&) = delete;  // Hmm, maybe we'd need this? -- D.K.
  Borrowed() = delete;
};

// `BorrowedOfGuaranteedLifetime<T>` is a borrower that by definition can not outlive its owner,
// as it is supposed to be contained in the scope inner to the scope of the owner.
// If `BorrowedOfGuaranteedLifetime<T>` is requested to release the underlying object, a major ownership
// invariant is broken. An example of such a case would be the storage being requested to let go of a stream.
// To simplify debugging of possible nasty deadlocks, `BorrowedOfGuaranteedLifetime<T>` just dumps an error message
// and terminates.
inline void BorrowedOfGuaranteedLifetimeInvariantErrorCallback() {
  std::cerr << "BorrowedOfGuaranteedLifetimeInvariantErrorCallback.\n";
  std::exit(-1);
}

template <typename T>
class BorrowedOfGuaranteedLifetime final : public WeakBorrowed<T> {
 private:
  static_assert(std::is_same<T, decay<T>>::value, "`BorrowedOfGuaranteedLifetime<>` requires a clean type.");

  using base_t = WeakBorrowed<T>;

 public:
  explicit BorrowedOfGuaranteedLifetime(const WeakBorrowed<T>& rhs)
      : base_t(impl::ConstructBorrowable(), rhs, BorrowedOfGuaranteedLifetimeInvariantErrorCallback) {}

  BorrowedOfGuaranteedLifetime(WeakBorrowed<T>&& rhs)
      : base_t(impl::ConstructBorrowableObjectViaMoveConstructor(),
               std::move(rhs),
               BorrowedOfGuaranteedLifetimeInvariantErrorCallback) {}

  void operator=(std::nullptr_t) { base_t::InternalUnRegister(); }

  BorrowedOfGuaranteedLifetime(BorrowedOfGuaranteedLifetime&& rhs)
      : base_t(impl::ConstructBorrowableObjectViaMoveConstructor(), std::move(rhs), []() {}) {}

  BorrowedOfGuaranteedLifetime& operator=(BorrowedOfGuaranteedLifetime&& rhs) {
    base_t::MoveFrom(std::move(rhs), BorrowedOfGuaranteedLifetimeInvariantErrorCallback);
    return *this;
  }

  BorrowedOfGuaranteedLifetime() = delete;
  BorrowedOfGuaranteedLifetime(const BorrowedOfGuaranteedLifetime&) = delete;
  BorrowedOfGuaranteedLifetime& operator=(const BorrowedOfGuaranteedLifetime&) = delete;
};

template <typename T, typename... ARGS>
Owned<T> MakeOwned(ARGS&&... args) {
  return Owned<T>(ConstructOwned<T>(), std::forward<ARGS>(args)...);
}

}  // namespace current::sync

template <typename T>
using ConstructOwned = sync::ConstructOwned<T>;

template <typename T>
using Owned = sync::Owned<T>;

using sync::MakeOwned;

template <typename T>
using Borrowed = sync::Borrowed<T>;

template <typename T>
using BorrowedWithCallback = sync::BorrowedWithCallback<T>;

template <typename T>
using BorrowedOfGuaranteedLifetime = sync::BorrowedOfGuaranteedLifetime<T>;

template <typename T>
using WeakBorrowed = sync::WeakBorrowed<T>;

}  // namespace current

#endif  // BRICKS_SYNC_OWNED_BORROWED_H
