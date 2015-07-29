/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// Current Flow language implementation.

// When defining the flow with `|` and `+` C++ operators, each sub-expression is a `subflow`.
// Subflow can be output-only ("LHS"), input-only ("RHS"), in/out("VIA"), or a complete end-to-end one ("E2E").
// End-to-end flows can be run with `(...).Flow()` or `(...).AsyncFlow()`. (TODO(dkorolev): Async.)
// Other types of subflows, "LHS", "RHS", and "VIA", are the building blocks.
// No subflow should be left hanging. The user should run it, `Describe()` it, or `DismissWithoutRunning()` it.
// TODO(dkorolev): `DismissWithoutRunning()` and test.

// TODO(dkorolev):
// * Add `+` and `*` combiners.
// * Add a base class and an MMQ.
// * Add RTTI dispatching.
// * Add early initialization, so that events could be sent from the constructor too.
// * Add the means to confirm that dropped messages are indeed dropped. (Per-typeid counters?)
// * Use RAAI, not `std::function<>`-s for dispatching messages between stages.
// * Add file/stream data origins/destinations.

#ifndef CURRENT_FLOW_H
#define CURRENT_FLOW_H

#include "../port.h"

#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "lazy_instantiation.h"

namespace flow {

// Template logic to make dealing with LHS/RHS/VIA/E2E subflows more generic.
enum class InputPolicy { DoesNotAccept = 0, Accepts = 1 };
enum class OutputPolicy { DoesNotEmit = 0, Emits = 1 };

template <InputPolicy, OutputPolicy>
struct BeautifyDescription {};
template <>
struct BeautifyDescription<InputPolicy::DoesNotAccept, OutputPolicy::DoesNotEmit> {
  static std::string run(const std::string& s) { return s; }
};
template <>
struct BeautifyDescription<InputPolicy::Accepts, OutputPolicy::DoesNotEmit> {
  static std::string run(const std::string& s) { return "... | " + s; }
};
template <>
struct BeautifyDescription<InputPolicy::DoesNotAccept, OutputPolicy::Emits> {
  static std::string run(const std::string& s) { return s + " | ..."; }
};
template <>
struct BeautifyDescription<InputPolicy::Accepts, OutputPolicy::Emits> {
  static std::string run(const std::string& s) { return "... | " + s + " | ..."; }
};

// Template logic to spawn message acceptors and message emitters.
struct CanBeSpawnedToAccept {
  virtual ~CanBeSpawnedToAccept() = default;
  virtual std::function<void(int)> SpawnAcceptor(bricks::LazyInstantiationStrategy) const = 0;
};

struct CanBeSpawnedToEmit {
  virtual ~CanBeSpawnedToEmit() = default;
  virtual void SpawnEmitter(std::function<void(int)> f, bricks::LazyInstantiationStrategy) const = 0;
};

template <InputPolicy INPUT, OutputPolicy OUTPUT>
struct SubflowRunner;

template <>
struct SubflowRunner<InputPolicy::DoesNotAccept, OutputPolicy::DoesNotEmit> {
  virtual ~SubflowRunner() = default;
};
template <>
struct SubflowRunner<InputPolicy::DoesNotAccept, OutputPolicy::Emits> : CanBeSpawnedToEmit {
  virtual ~SubflowRunner() = default;
};
template <>
struct SubflowRunner<InputPolicy::Accepts, OutputPolicy::DoesNotEmit> : CanBeSpawnedToAccept {
  virtual ~SubflowRunner() = default;
};
template <>
struct SubflowRunner<InputPolicy::Accepts, OutputPolicy::Emits> : CanBeSpawnedToAccept, CanBeSpawnedToEmit {
  virtual ~SubflowRunner() = default;
};

// General fields for each subflow, complete or incomplete.
template <InputPolicy INPUT, OutputPolicy OUTPUT>
class Subflow : public SubflowRunner<INPUT, OUTPUT> {
 public:
  explicit Subflow(const std::string& description) : description_(description) {}

  // No subflow should be left hanging.
  virtual ~Subflow() {
    if (still_hanging && !describe_called) {
      // Too bad we can't throw an exception here. -- D.K.
      // TODO(dkorolev): Unit-test this with a singleton policy with ostream and std::exit().
      std::cerr << "Current leaked:\n" << Describe() << "\nCall `.Flow()`, etc., don't leave it hanging.\n";
      std::exit(-1);
    }
  }

  // The external, user-facing `Describe()` describes this subflow in human-readable text.
  // Applies beautification, such as "... | Logic()" and/or "Logic() | ...", for to-be-added inputs/outputs.
  // Does not mark this subflow as used, but marks it as the one that was described.
  std::string Describe() const {
    MarkAsDescribeCalled();
    return BeautifyDescription<INPUT, OUTPUT>::run(description_);
  }

  // The internal method to describe the subflow. Used to construct textual description of the complex ones.
  // Marks the subflow as not hanging, as it is now part of a larger one.
  std::string DescribeWithoutBangs() const {
    MarkAsNotHanging();
    return description_;
  }

  // Marks this subflow as "used". Surpasses error message on destruction. Should happen at most once.
  void MarkAsNotHanging() const {
    assert(still_hanging);
    still_hanging = false;
  }

  // Marks this subflow as "described". Surpasses error message on destruction.
  // Can happen multiple times, but not after the flow was run.
  void MarkAsDescribeCalled() const {
    assert(still_hanging);
    describe_called = true;
  }

 private:
  const std::string description_;
  mutable bool still_hanging = true;
  mutable bool describe_called = false;
};

typedef Subflow<InputPolicy::DoesNotAccept, OutputPolicy::DoesNotEmit> SubflowE2E;
typedef Subflow<InputPolicy::DoesNotAccept, OutputPolicy::Emits> SubflowLHS;
typedef Subflow<InputPolicy::Accepts, OutputPolicy::DoesNotEmit> SubflowRHS;
typedef Subflow<InputPolicy::Accepts, OutputPolicy::Emits> SubflowVIA;

// Base classes for user code definition.
class SetEmitHelper;
class EmitterImpl {
 protected:
  // `emit()` is the implementation of `emit(...)` called by the user from their code.
  void emit(int x) {
    assert(f_);
    f_(x);
  }

 private:
  friend class SetEmitHelper;
  template <typename F>
  void InternalSetEmitter(F&& f) {
    f_ = f;
  }

  std::function<void(int)> f_ = nullptr;
};

// Extra layer of security.
class SetEmitHelper {
 public:
  template <typename F>
  static void SetEmitter(EmitterImpl& emitter, F&& f) {
    emitter.InternalSetEmitter(std::forward<F>(f));
  }
};

struct CurrentLHSBase : EmitterImpl {};
struct CurrentRHSBase {};
struct CurrentVIABase : EmitterImpl {};

template <typename USER_CLASS>
struct CurrentLHS : CurrentLHSBase {};

template <typename USER_CLASS>
struct CurrentVIA : CurrentVIABase {};

template <typename USER_CLASS>
struct CurrentRHS : CurrentRHSBase {};

// Top-level class to wrap user-defined "LHS" code into a corresponding subflow.
template <typename USER_CLASS, typename ARGS_AS_TUPLE>
class UserCodeLHS : public SubflowLHS {
 private:
  static_assert(std::is_base_of<CurrentLHSBase, USER_CLASS>::value,
                "User code for Current Flow data origin should use CURRENT_LHS() + REGISTER_CURRENT_LHS()");

 public:
  UserCodeLHS(const char* description, ARGS_AS_TUPLE&& params)
      : SubflowLHS(description),
        lazy_instance_(bricks::DelayedInstantiateFromTuple<USER_CLASS>(std::forward<ARGS_AS_TUPLE>(params))) {}

  void SpawnEmitter(std::function<void(int)> f, bricks::LazyInstantiationStrategy strategy) const override {
    auto& instance = lazy_instance_.Instantiate(shared_instance_, strategy);
    SetEmitHelper::SetEmitter(instance, f);
    instance.run();
  }

  // For the `CURRENT_IMPL` macro to be able to extract the underlying user-defined type.
  USER_CLASS UnderlyingTypeExtractor();

 private:
  bricks::LazilyInstantiated<USER_CLASS> lazy_instance_;
  mutable std::shared_ptr<USER_CLASS> shared_instance_;
};

// Top-level class to wrap user-defined "VIA" code into a corresponding subflow.
template <typename USER_CLASS, typename ARGS_AS_TUPLE>
class UserCodeVIA : public SubflowVIA {
 private:
  static_assert(std::is_base_of<CurrentVIABase, USER_CLASS>::value,
                "User code for Current Flow data processor should use CURRENT_VIA() + REGISTER_CURRENT_VIA()");

 public:
  UserCodeVIA(const char* description, ARGS_AS_TUPLE&& params)
      : SubflowVIA(description),
        lazy_instance_(bricks::DelayedInstantiateFromTuple<USER_CLASS>(std::forward<ARGS_AS_TUPLE>(params))) {}

  void SpawnEmitter(std::function<void(int)> f, bricks::LazyInstantiationStrategy strategy) const override {
    auto& instance = lazy_instance_.Instantiate(shared_instance_, strategy);
    SetEmitHelper::SetEmitter(instance, f);
    instance.run();
  }

  std::function<void(int)> SpawnAcceptor(bricks::LazyInstantiationStrategy strategy) const override {
    auto& instance = lazy_instance_.Instantiate(shared_instance_, strategy);
    return [&instance](int x) { instance.f(x); };
  }

  // For the `CURRENT_IMPL` macro to be able to extract the underlying user-defined type.
  USER_CLASS UnderlyingTypeExtractor();

 private:
  bricks::LazilyInstantiated<USER_CLASS> lazy_instance_;
  mutable std::shared_ptr<USER_CLASS> shared_instance_;
};

// Top-level class to wrap user-defined "RHS" code into a corresponding subflow.
template <typename USER_CLASS, typename ARGS_AS_TUPLE>
class UserCodeRHS : public SubflowRHS {
 private:
  static_assert(
      std::is_base_of<CurrentRHSBase, USER_CLASS>::value,
      "User code for Current Flow data destination should use CURRENT_RHS() + REGISTER_CURRENT_RHS()");

 public:
  UserCodeRHS(const char* description, ARGS_AS_TUPLE&& params)
      : SubflowRHS(description),
        lazy_instance_(bricks::DelayedInstantiateFromTuple<USER_CLASS>(std::forward<ARGS_AS_TUPLE>(params))) {}

  // For the `CURRENT_IMPL` macro to be able to extract the underlying user-defined type.
  USER_CLASS UnderlyingTypeExtractor();

  std::function<void(int)> SpawnAcceptor(bricks::LazyInstantiationStrategy strategy) const override {
    auto& instance = lazy_instance_.Instantiate(shared_instance_, strategy);
    return [&instance](int x) { instance.f(x); };
  }

 private:
  bricks::LazilyInstantiated<USER_CLASS> lazy_instance_;
  mutable std::shared_ptr<USER_CLASS> shared_instance_;
};

// The implementation of subflow sequence combiner, `A | B`.
template <InputPolicy INPUT, OutputPolicy OUTPUT>
class SubflowSequenceStorage : public Subflow<INPUT, OUTPUT> {
 public:
  template <typename LHS, typename RHS>
  SubflowSequenceStorage(const LHS& lhs, const RHS& rhs)
      : Subflow<INPUT, OUTPUT>(lhs.DescribeWithoutBangs() + " | " + rhs.DescribeWithoutBangs()),
        from_(lhs),
        into_(rhs) {}

 protected:
  const SubflowRunner<INPUT, OutputPolicy::Emits>& From() const { return from_; }
  const SubflowRunner<InputPolicy::Accepts, OUTPUT>& Into() const { return into_; }

 private:
  const SubflowRunner<INPUT, OutputPolicy::Emits>& from_;
  const SubflowRunner<InputPolicy::Accepts, OUTPUT>& into_;
};

template <InputPolicy INPUT, OutputPolicy OUTPUT>
class SubflowSequence;

// `SubflowSequence` with no external inputs and no external outputs is the full Current Flow.
// It can be run via `Flow()`.
template <>
class SubflowSequence<InputPolicy::DoesNotAccept, OutputPolicy::DoesNotEmit>
    : public SubflowSequenceStorage<InputPolicy::DoesNotAccept, OutputPolicy::DoesNotEmit> {
 public:
  template <typename LHS, typename RHS>
  SubflowSequence(const LHS& lhs, const RHS& rhs)
      : SubflowSequenceStorage<InputPolicy::DoesNotAccept, OutputPolicy::DoesNotEmit>(lhs, rhs) {}

  void Flow() {
    Subflow<InputPolicy::DoesNotAccept, OutputPolicy::DoesNotEmit>::MarkAsNotHanging();
    From().SpawnEmitter(Into().SpawnAcceptor(bricks::LazyInstantiationStrategy::ShouldNotBeInitialized),
                        bricks::LazyInstantiationStrategy::ShouldNotBeInitialized);
  }
};

// Other partial specializations of `SubflowSequence` can spawn their respective Acceptors and Emitters.
// They are defined as individual template specializations for proper `override`-ing.
template <>
class SubflowSequence<InputPolicy::DoesNotAccept, OutputPolicy::Emits>
    : public SubflowSequenceStorage<InputPolicy::DoesNotAccept, OutputPolicy::Emits> {
 public:
  template <typename LHS, typename RHS>
  SubflowSequence(const LHS& lhs, const RHS& rhs)
      : SubflowSequenceStorage<InputPolicy::DoesNotAccept, OutputPolicy::Emits>(lhs, rhs) {}

  void SpawnEmitter(std::function<void(int)> f, bricks::LazyInstantiationStrategy strategy) const override {
    Into().SpawnEmitter(f, strategy);
    From().SpawnEmitter(Into().SpawnAcceptor(bricks::LazyInstantiationStrategy::ShouldAlreadyBeInitialized),
                        strategy);
  }
};

template <>
class SubflowSequence<InputPolicy::Accepts, OutputPolicy::DoesNotEmit>
    : public SubflowSequenceStorage<InputPolicy::Accepts, OutputPolicy::DoesNotEmit> {
 public:
  template <typename LHS, typename RHS>
  SubflowSequence(const LHS& lhs, const RHS& rhs)
      : SubflowSequenceStorage<InputPolicy::Accepts, OutputPolicy::DoesNotEmit>(lhs, rhs) {}

  std::function<void(int)> SpawnAcceptor(bricks::LazyInstantiationStrategy strategy) const override {
    return Into().SpawnAcceptor(strategy);
  }
};

template <>
class SubflowSequence<InputPolicy::Accepts, OutputPolicy::Emits>
    : public SubflowSequenceStorage<InputPolicy::Accepts, OutputPolicy::Emits> {
 public:
  template <typename LHS, typename RHS>
  SubflowSequence(const LHS& lhs, const RHS& rhs)
      : SubflowSequenceStorage<InputPolicy::Accepts, OutputPolicy::Emits>(lhs, rhs) {}

  void SpawnEmitter(std::function<void(int)> f, bricks::LazyInstantiationStrategy strategy) const override {
    Into().SpawnEmitter(f, strategy);
    From().SpawnEmitter(Into().SpawnAcceptor(bricks::LazyInstantiationStrategy::ShouldAlreadyBeInitialized),
                        strategy);
  }

  std::function<void(int)> SpawnAcceptor(bricks::LazyInstantiationStrategy strategy) const override {
    return Into().SpawnAcceptor(strategy);
  }
};

// Subflow sequence combiner, `A | B`.
template <InputPolicy LHSInput, OutputPolicy RHSOutput>
SubflowSequence<LHSInput, RHSOutput> operator|(const Subflow<LHSInput, OutputPolicy::Emits>& lhs,
                                               const Subflow<InputPolicy::Accepts, RHSOutput>& rhs) {
  return SubflowSequence<LHSInput, RHSOutput>(lhs, rhs);
}

}  // namespace flow

// Macros to wrap user code into Current Flow workers.

// Define a user class ready to be used as part of Current Flows.
#define CURRENT_LHS(name) struct name final : flow::CurrentLHS<name>
#define CURRENT_RHS(name) struct name final : flow::CurrentRHS<name>
#define CURRENT_VIA(name) struct name final : flow::CurrentVIA<name>

// Declare a user class as a source of Flow messages.
#define REGISTER_CURRENT_LHS(name, ...)                                                             \
  flow::UserCodeLHS<name, decltype(std::forward_as_tuple(__VA_ARGS__))>(#name "(" #__VA_ARGS__ ")", \
                                                                        std::forward_as_tuple(__VA_ARGS__))

// Declare a user class as a destination of Flow messages.
#define REGISTER_CURRENT_RHS(name, ...)                                                             \
  flow::UserCodeRHS<name, decltype(std::forward_as_tuple(__VA_ARGS__))>(#name "(" #__VA_ARGS__ ")", \
                                                                        std::forward_as_tuple(__VA_ARGS__))

// Declare a user class as a processor of Flow messages.
#define REGISTER_CURRENT_VIA(name, ...)                                                             \
  flow::UserCodeVIA<name, decltype(std::forward_as_tuple(__VA_ARGS__))>(#name "(" #__VA_ARGS__ ")", \
                                                                        std::forward_as_tuple(__VA_ARGS__))

// A helper macro to extract the underlying type of the user class, now registered as a Current Flow type.
#define CURRENT_IMPL(name) decltype(name().UnderlyingTypeExtractor())

#endif  // CURRENT_FLOW_H
