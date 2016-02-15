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
// * The scope in which the object has been created is the master owner of this object; others are followers.
// * Upon leaving the master scope, the synchronization primitive waits until all its followers terminate.
// * Followers are under no obligation to terminate sooner or later, but they get signaled to wrap up.
//
// Implementation:
//
// * ScopeOwnedByMe<T> master(T_construction_parameter1, ...);
//   Creates `master`, which will live only within its creation scope.
//
// * ScopeOwnedBySomeoneElse<T> follower(master, [&signal]() { signal.SignalTermination(); });
//   Initializes `follower`, likely `std::move()` it into a different thread.
//   The scope of the `follower` object will be strictly inside the scope of `master`.
//   The destructor of `master` will wait until all the `follower`-s have terminated.
//   The callbacks, corresponding to active `follower`-s, will be called as `master` will be destructing.

#ifndef BRICKS_SYNC_SCOPE_OWNED_H
#define BRICKS_SYNC_SCOPE_OWNED_H

#include "../../port.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "../exception.h"

namespace current {
namespace sync {

// Exceptions to be raised if attempting to spin off a new follower for an object already being destructed.
struct InDestructingModeException : Exception {
  using Exception::Exception;
};

template <typename T>
struct InDestructingModeOfTException : InDestructingModeException {
  using InDestructingModeException::InDestructingModeException;
};

struct AttemptedToUnregisterScopeOwnedBySomeoneElseMoreThanOnce : Exception {
  using Exception::Exception;
};

// The container for the actual instance and its environment.
namespace impl {

// Helper to prohibit copying `ScopeOwnedByMe`.
struct ConstructScopeOwnedByMe {};
struct ConstructScopeOwnedBySomeoneElse {};
struct ConstructScopeOwnedObjectViaMoveConstructor {};

struct ConstructActualContainerViaMoveConstructor {};

// The actual instance, kept along with its destructing status, and a list of registered followers.
template <typename T>
struct ActualInstance final {
  // Constructor: Construct an instance of the object.
  template <typename... ARGS>
  ActualInstance(ARGS&&... args)
      : instance_(std::forward<ARGS>(args)...),
        destructing_(false),
        total_followers_spawned_throughout_lifetime_(0u) {}

  // Destructor: Block until all the followers are done.
  ~ActualInstance() {
    // Mark the instance of the object as being destructed.
    destructing_ = true;

    // Collect all the "destructing" callbacks to call.
    std::vector<std::function<void()>> callbacks_to_call;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      for (const auto& cit : followers_) {
        callbacks_to_call.push_back(cit.second);
      }
    }

    // Call all the "destructing" callbacks.
    for (auto& f : callbacks_to_call) {
      f();
    }

    // Wait until all the followers have terminated.
    {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_variable_.wait(lock, [this]() { return followers_.empty(); });
    }
  }

  // Register ("increase the ref-count") a newly spawned follower, along with its termination signal callback.
  // Must be called from within a mutex-locked section.
  size_t RegisterFollowerFromLockedSection(std::function<void()> destruction_callback) {
    followers_[++total_followers_spawned_throughout_lifetime_] = destruction_callback;
    return total_followers_spawned_throughout_lifetime_;
  }

  // Unregister ("decrease the ref-count") a previously spawned follower.
  // Locks its own mutex; must be called from outside a mutex-locked section.
  void UnRegisterFollower(size_t follower_index) {
    bool notify = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = followers_.find(follower_index);
      if (it == followers_.end()) {
        CURRENT_THROW(AttemptedToUnregisterScopeOwnedBySomeoneElseMoreThanOnce());
      }
      followers_.erase(it);
      notify = followers_.empty();
    }
    if (notify) {
      condition_variable_.notify_one();
    }
  }

  ActualInstance() = delete;
  ActualInstance(const ActualInstance&) = delete;
  ActualInstance(ActualInstance&&) = delete;
  ActualInstance& operator=(const ActualInstance&) = delete;
  ActualInstance& operator=(ActualInstance&&) = delete;

  T instance_;                                          // The actual instance.
  std::atomic_bool destructing_;                        // Whether it already is in the "destructing" mode.
  std::mutex mutex_;                                    // The mutex to guard per-follower info.
  std::condition_variable condition_variable_;          // The variable to notify as all the users are done.
  std::map<size_t, std::function<void()>> followers_;   // Map of live follower -> termination signal callback.
  size_t total_followers_spawned_throughout_lifetime_;  // A pre-increment of this value is the key in the map.
};

}  // namespace current::sync::impl

template <typename T>
class ScopeOwnedByMe;
template <typename T>
class ScopeOwnedBySomeoneElse;

// `ScopeOwned<T>` is the  base class for `ScopeOwnedByMe<T>` and `ScopeOwnedBySomeoneElse<T>`.
//
// Unifies and simplifies:
// 1) Basic usage patterns, such as `operator*()`, `operator->()`, and exceptions.
// 2) Parameter passing and initialization, as the callee initializing its own `ScopeOwnedBySomeoneElse<T>`
//    from the passed in parameter can accept `ScopeOwned<T>`, which would enable calling it with
//    either `ScopeOwnedByMe<T>` or `ScopeOwnedBySomeoneElse<T>` as a parameter.
template <typename T>
class ScopeOwned {
 private:
  using T_INSTANCE = impl::ActualInstance<T>;
  // `ScopeOwned<T>` is meant to be initialized by `ScopeOwnedByMe<T>` or `ScopeOwnedBySomeoneElse<T>` only.
  // Its constructors are private to prevent accidental construction.
  friend class ScopeOwnedByMe<T>;
  ScopeOwned(impl::ConstructScopeOwnedByMe, T_INSTANCE& actual_instance)
      : p_actual_instance_(&actual_instance), key_(0u) {}

  friend class ScopeOwnedBySomeoneElse<T>;
  ScopeOwned(impl::ConstructScopeOwnedBySomeoneElse,
             const ScopeOwned& rhs,
             std::function<void()> destruction_callback) {
    std::lock_guard<std::mutex> lock(rhs.p_actual_instance_->mutex_);
    rhs.CheckPrivilege();
    p_actual_instance_ = rhs.p_actual_instance_;
    key_ = p_actual_instance_->RegisterFollowerFromLockedSection(destruction_callback);
  }

  ScopeOwned(impl::ConstructScopeOwnedObjectViaMoveConstructor, ScopeOwned&& rhs) {
    std::lock_guard<std::mutex> lock(rhs.p_actual_instance_->mutex_);
    p_actual_instance_ = rhs.p_actual_instance_;
    key_ = rhs.key_;
    rhs.key_ = 0u;  // Must mark the passed in xvalue of `ScopeOwner<>` as abandoned.
  }

  ScopeOwned(const ScopeOwned&) = delete;
  ScopeOwned(ScopeOwned&&) = delete;
  ScopeOwned& operator=(const ScopeOwned&) = delete;
  ScopeOwned& operator=(ScopeOwned&&) = delete;

 public:
  ~ScopeOwned() {
    if (key_) {
      // `*this` is a valid (non - std::move()-d - away) `ScopeOwnedBySomeoneElse`.
      // As it is being terminated, it should unregister its callback, so that:
      // 1) If the object is still alive, to have this callback not be called upon it being destructed, and/or
      // 2) If the object is now destructing, to indicate this follower has released it, to decrement
      //    the active followers counter, and trigger a condition variable `*p_actual_instance_` is waiting on
      //    if this counter has reached zero with this unregistration.
      p_actual_instance_->UnRegisterFollower(key_);
    }
  }

  // Whether the object is destructing.
  // THREAD-SAFE. NEVER THROW.
  bool IsDestructing() const { return p_actual_instance_->destructing_; }
  operator bool() const { return IsDestructing(); }

  // Smart-pointer-style accessors.
  // NOT THREAD-SAFE. CAN THROW IF ALREADY DESTRUCTING.
  T& operator*() {
    CheckPrivilege();
    return p_actual_instance_->instance_;
  }
  const T& operator*() const {
    CheckPrivilege();
    return p_actual_instance_->instance_;
  }
  T* operator->() {
    CheckPrivilege();
    return &p_actual_instance_->instance_;
  }
  const T* operator->() const {
    CheckPrivilege();
    return &p_actual_instance_->instance_;
  }

  // Exclusive accessors.
  // NOT THREAD-SAFE. CAN THROW IF ALREADY DESTRUCTING.
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

  // Exclusive accessors ignoring the `destructing_` flag.
  // NOT THREAD-SAFE. NEVER THROW.
  void ExclusiveUseDespitePossiblyDestructing(std::function<void(T&)> f) {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    f(p_actual_instance_->instance_);
  }
  void ExclusiveUseDespitePossiblyDestructing(std::function<void(const T&)> f) const {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    f(p_actual_instance_->instance_);
  }
  template <typename V>
  V ExclusiveUseDespitePossiblyDestructing(std::function<V(T&)> f) {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    return f(p_actual_instance_->instance_);
  }
  template <typename V>
  V ExclusiveUseDespitePossiblyDestructing(std::function<V(const T&)> f) const {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    return f(p_actual_instance_->instance_);
  }

  // Return the number of registered follower users besides the master one.
  // For unit-testing purposes mostly, but may end up useful. -- D.K.
  // THREAD-SAFE. NEVER THROWS.
  size_t NumberOfActiveFollowers() const {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    return p_actual_instance_->followers_.size();
  }

  // Return the total number of registered follower users registered, with some possibly already out of scope.
  // For unit-testing purposes mostly, but may end up useful. -- D.K.
  // THREAD-SAFE. NEVER THROWS.
  size_t TotalFollowersSpawnedThroughoutLifetime() const {
    std::lock_guard<std::mutex> lock(p_actual_instance_->mutex_);
    return p_actual_instance_->total_followers_spawned_throughout_lifetime_;
  }

 private:
  void CheckPrivilege() const {
    if (p_actual_instance_->destructing_) {
      CURRENT_THROW(InDestructingModeOfTException<T>());
    }
  }

  T_INSTANCE* p_actual_instance_;
  size_t key_;  // The index of the slave user to mark as left the scope, or `0u` if N/A (master or moved away).
};

namespace impl {

// `ActualInstanceContainer<T>` must be a dedicated struct for `ScopeOwnedByMe<T>` to construct
// the actual object before initializing the `ScopeOwned<T>` associated with it.
// `ActualInstanceContainer<T>` is constructible, movable, and nothing else.
template <typename T>
struct ActualInstanceContainer {
  std::unique_ptr<ActualInstance<T>> movable_instance_;

  ActualInstanceContainer(ConstructActualContainerViaMoveConstructor, ActualInstanceContainer&& rhs)
      : movable_instance_(std::move(rhs.movable_instance_)) {}

  template <typename... ARGS>
  ActualInstanceContainer(ARGS&&... args)
      : movable_instance_(std::make_unique<ActualInstance<T>>(std::forward<ARGS>(args)...)) {}

  ActualInstanceContainer() = delete;
  ActualInstanceContainer(const ActualInstanceContainer&) = delete;
  ActualInstanceContainer(ActualInstanceContainer&&) = delete;
  ActualInstanceContainer& operator=(const ActualInstanceContainer&) = delete;
  ActualInstanceContainer& operator=(ActualInstanceContainer&&) = delete;
};

}  // namespace current::sync::impl

// `ScopedOwnedbyMe<T>` is constructible, movable, and can spin off `ScopeOwnedBySomeoneElse<T>` offspring.
template <typename T>
class ScopeOwnedByMe final : private impl::ActualInstanceContainer<T>, public ScopeOwned<T> {
 private:
  using T_IMPL = impl::ActualInstanceContainer<T>;
  using T_BASE = ScopeOwned<T>;

 public:
  template <typename... ARGS>
  ScopeOwnedByMe(ARGS&&... args)
      : T_IMPL(std::forward<ARGS>(args)...),
        T_BASE(impl::ConstructScopeOwnedByMe(), *T_IMPL::movable_instance_) {}

  ScopeOwnedByMe(ScopeOwnedByMe&& rhs)
      : T_IMPL(impl::ConstructActualContainerViaMoveConstructor(), std::move(static_cast<T_IMPL&&>(rhs))),
        T_BASE(impl::ConstructScopeOwnedByMe(), *T_IMPL::movable_instance_) {}

  ScopeOwnedByMe() = delete;
  ScopeOwnedByMe(const ScopeOwnedByMe&) = delete;
  ScopeOwnedByMe& operator=(const ScopeOwnedByMe&) = delete;
  ScopeOwnedByMe& operator=(ScopeOwnedByMe&&) = delete;
};

// The constructor of `ScopeOwnedBySomeoneElse<T>` requires two parameters:
// 1) `ScopeOwned{ByMe/BySomeoneElse}&`, and
// 2) a [required so far] callback, which will be called should this scope be terminating.
template <typename T>
class ScopeOwnedBySomeoneElse final : public ScopeOwned<T> {
 private:
  using T_BASE = ScopeOwned<T>;

 public:
  ScopeOwnedBySomeoneElse(ScopeOwned<T>& rhs, std::function<void()> destruction_callback)
      : T_BASE(impl::ConstructScopeOwnedBySomeoneElse(), rhs, destruction_callback) {}

  ScopeOwnedBySomeoneElse(ScopeOwnedBySomeoneElse&& rhs)
      : T_BASE(impl::ConstructScopeOwnedObjectViaMoveConstructor(), std::move(static_cast<T_BASE&&>(rhs))) {}

  ScopeOwnedBySomeoneElse() = delete;
  ScopeOwnedBySomeoneElse(const ScopeOwnedBySomeoneElse&) = delete;
  ScopeOwnedBySomeoneElse& operator=(const ScopeOwnedBySomeoneElse&) = delete;
  ScopeOwnedBySomeoneElse& operator=(ScopeOwnedBySomeoneElse&&) = delete;
};

}  // namespace current::sync
}  // namespace current

using current::sync::ScopeOwned;
using current::sync::ScopeOwnedByMe;
using current::sync::ScopeOwnedBySomeoneElse;

#endif  // BRICKS_SYNC_SCOPE_OWNED_H
