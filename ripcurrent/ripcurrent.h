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

// RipCurrent dataflow definition language.

// When defining the RipCurrent flow with the `|` and `+` C++ operators, each sub-expression is a building block.
// Building blocks can be output-only ("LHS"), input-only ("RHS"), in/out ("VIA"), or end-to-end ("E2E").
// No buliding block, simple or composite, can be left hanging. Each one should be used, run, described, or dismissed.
//
// End-to-end blocks can be run synchronously with `(...).RipCurrent().Join()`. Another option is to save
// the return value of `(...).RipCurrent()` into some `scope` variable, which must be `.Join()`-ed at a later time.
// Finally, the `scope` variable can be called `.Async()` on, which eliminates the need to explicitly call `.Join()`
// at the end of its lifetime; and the syntax of `auto scope = (...).RipCurrent().Async();` is supported as well.
//
// HI-PRI:
// TOOD(dkorolev): Add `RipCurrent/builtin` for our standard flow blocks library.
//                 Some `ParseFileByLines<T>()`, `StreamSubscriber<T>()`, `Dump<T>()`, `CountDistinct<T>()` would be
//                 prime candidates.
//
// LO-PRI:
// TODO(dkorolev): Add debug output counters / HTTP endpoint for # of messages per typeid.
// TODO(dkorolev): Add GraphViz-based visualization.

#ifndef CURRENT_RIPCURRENT_RIPCURRENT_H
#define CURRENT_RIPCURRENT_RIPCURRENT_H

#include "../port.h"

#include "types.h"

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "../typesystem/struct.h"
#include "../typesystem/remove_parentheses.h"

#include "../blocks/mmq/mmpq.h"

#include "../bricks/strings/join.h"
#include "../bricks/sync/waitable_atomic.h"
#include "../bricks/template/rtti_dynamic_call.h"
#include "../bricks/template/typelist.h"
#include "../bricks/util/lazy_instantiation.h"
#include "../bricks/util/singleton.h"

namespace current {
namespace ripcurrent {

// `Definition` traces down to the original statement in the source code that defined a particular building block.
// It is used for diagnostic messages in runtime errors, or when some Current building block was left hanging.
struct Definition {
  std::string statement;
  std::vector<std::pair<std::string, FileLine>> sources;

  struct Pipe final {};  // A helper to describe a composite block built with '|'.
  struct Plus final {};  // A helper to describe a composite block built with '+'.

  static std::vector<std::pair<std::string, FileLine>> CombineSources(const Definition& a, const Definition& b) {
    std::vector<std::pair<std::string, FileLine>> sources;
    sources.reserve(a.sources.size() + b.sources.size());
    for (const auto& e : a.sources) {
      sources.push_back(e);
    }
    for (const auto& e : b.sources) {
      sources.push_back(e);
    }
    return sources;
  }

  Definition(const std::string& statement, const char* const file, size_t line)
      : statement(statement), sources({{statement, FileLine{file, line}}}) {}
  Definition(Pipe, const Definition& from, const Definition& into)
      : statement(from.statement + " | " + into.statement), sources(CombineSources(from, into)) {}
  Definition(Plus, const Definition& a, const Definition& b)
      : statement(a.statement + " + " + b.statement), sources(CombineSources(a, b)) {}
  virtual ~Definition() = default;
};

// `UniqueDefinition` is a `Definition` that:
// * Is templated with `LHSTypes<...>` and `RHSTypes<...>` for strict typing.
// * Exposes user-friendly decorators.
// * Can be marked as used in various ways.
// * Generates an error upon destructing if it was not marked as used in at least one way.
enum class BlockUsageBit : size_t { Described = 0, HasBeenRun = 1, UsedInLargerBlock = 2, Dismissed = 3 };
enum class BlockUsageBitMask : size_t { Unused = 0 };

template <class LHS_TYPELIST, class RHS_TYPELIST>
class UniqueDefinition;

template <typename... LHS_TYPES, typename... RHS_TYPES>
class UniqueDefinition<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> final : public Definition {
 public:
  // Can be created.
  explicit UniqueDefinition(Definition definition) : Definition(definition) {}
  UniqueDefinition(Definition definition, BlockUsageBitMask mask) : Definition(definition), mask_(mask) {}

  // Is free to copy around.
  UniqueDefinition() = delete;
  explicit UniqueDefinition(const UniqueDefinition&) = default;

  // Can be marked as used in any way.
  void MarkAs(BlockUsageBit bit) { mask_ = BlockUsageBitMask(size_t(mask_) | (static_cast<size_t>(1) << static_cast<size_t>(bit))); }

  // Builds the full description of this `Definition`, down to source code file names and line numbers.
  void FullDescription(std::ostream& os = std::cerr) const {
    os << (sizeof...(LHS_TYPES) ? "... | " : "");
    os << statement;
    os << (sizeof...(RHS_TYPES) ? " | ..." : "");
    os << "\n";
    for (const auto& source : sources) {
      os << source.first << " from " << source.second.file << ":" << source.second.line << "\n";
    }
  }

  // Displays a verbose critical runtime error if the buliding block this definition describes
  // has been left hanging, either as unused or as not run.
  ~UniqueDefinition() {
    if (mask_ == BlockUsageBitMask::Unused) {
      std::ostringstream os;

      os << "RipCurrent building block leaked.\n";
      FullDescription(os);
      os << "Each building block should be run, described, used as part of a larger block, or dismissed.\n";

      current::Singleton<RipCurrentMockableErrorHandler>().HandleError(os.str());
    }
  }

 private:
  BlockUsageBitMask mask_ = BlockUsageBitMask::Unused;
};

// `SharedDefinition` is a helper class for an `std::shared_ptr<UniqueDefinition>`.
// The premise is that building blocks that should be used or run can be liberally copied over,
// with the test that they have been used or run performed at the very end of their lifetime.
template <class LHS_TYPELIST, class RHS_TYPELIST>
class SharedDefinition;

template <typename... LHS_TYPES, typename... RHS_TYPES>
class SharedDefinition<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> {
 public:
  using impl_t = UniqueDefinition<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>;

  SharedDefinition(Definition definition) : unique_definition_(std::make_shared<impl_t>(definition)) {}
  void MarkAs(BlockUsageBit bit) const { unique_definition_->MarkAs(bit); }

  // User-facing methods.
  std::string Describe() const {
    MarkAs(BlockUsageBit::Described);
    return (sizeof...(LHS_TYPES) ? "... | " : "") + unique_definition_->statement +
           (sizeof...(RHS_TYPES) ? " | ..." : "");
  }

  template <typename T>
  struct AppendTypeName {
    AppendTypeName(std::vector<std::string>& types) { types.push_back(reflection::CurrentTypeName<T>()); }
  };

  std::string DescribeWithTypes() const {
    MarkAs(BlockUsageBit::Described);
    std::string result;
    if (sizeof...(LHS_TYPES)) {
      std::vector<std::string> types;
      metaprogramming::call_all_constructors_with<AppendTypeName, std::vector<std::string>, TypeListImpl<LHS_TYPES...>>(
          types);
      result.append("... | { " + strings::Join(types, ", ") + " } => ");
    }
    result.append(unique_definition_->statement);
    if (sizeof...(RHS_TYPES)) {
      std::vector<std::string> types;
      metaprogramming::call_all_constructors_with<AppendTypeName, std::vector<std::string>, TypeListImpl<RHS_TYPES...>>(
          types);
      result.append(" => { " + strings::Join(types, ", ") + " } | ...");
    }
    return result;
  }

  void Dismiss() const { MarkAs(BlockUsageBit::Dismissed); }

  // For expressive initializer lists of shared instances.
  const SharedDefinition& GetUniqueDefinition() const { return *this; }
  const UniqueDefinition<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>& GetDefinition() const {
    return *unique_definition_.get();
  }

 private:
  mutable std::shared_ptr<impl_t> unique_definition_;
};

// The interfaces to pass messages between the blocks. There are two: an asynchronous one and a synchronous one.
// The asynchronous one, which captures thread-unsafe messages from the user code, is `BlockOutgoingInterface`.
// The synchronous one, which thread-safely lines up the messages for the user code, is `BlockIncomingInterface'.

// `GenericBlockOutgoingInterface` is the interface to service the `emit<>`, `post<>`, `schedule<>`, and `head<>`
// calls originating from the block of user code. ASSUMES ALL USER CALLS ARE THREAD-UNSAFE.
class GenericBlockOutgoingInterface {
 public:
  virtual ~GenericBlockOutgoingInterface() = default;
  virtual void OnThreadUnsafeEmitted(movable_message_t&&, std::chrono::microseconds) = 0;
  virtual void OnThreadUnsafeScheduled(movable_message_t&&, std::chrono::microseconds) = 0;
  virtual void OnThreadUnsafeHeadUpdated(std::chrono::microseconds) = 0;
};

template <class>
class BlockOutgoingInterface;

template <class... TYPES>
class BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<TYPES...>> : public GenericBlockOutgoingInterface {};

// TODO(dkorolev): Unnecessary?
template <>
class BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<>> : public GenericBlockOutgoingInterface {
 public:
  void OnThreadUnsafeEmitted(movable_message_t&&, std::chrono::microseconds) override {
    std::cerr << "Not expecting any entries from a non-emitting block.\n";
    CURRENT_ASSERT(false);
  }
  void OnThreadUnsafeScheduled(movable_message_t&&, std::chrono::microseconds) override {
    std::cerr << "Not expecting any entries from a non-emitting block.\n";
    CURRENT_ASSERT(false);
  }
  void OnThreadUnsafeHeadUpdated(std::chrono::microseconds) override {
    std::cerr << "Not expecting HEAD updates to originate from a non-emitting block.\n";
    CURRENT_ASSERT(false);
  }
};

// `GenericBlockIncomingInterface` is the interface to accept the actor model, thread-safe, calls.
// For end-user blocks, these calls are proxied directly to the `USER_CODE::f()` function.
// ASSUMES ALL CALLS ARE THREAD-SAFE.
class GenericBlockIncomingInterface {
 public:
  virtual ~GenericBlockIncomingInterface() = default;
  virtual void OnThreadSafeMessage(movable_message_t&&) = 0;
};

template <class>
class BlockIncomingInterface;

template <class... TYPES>
class BlockIncomingInterface<ThreadSafeIncomingTypes<TYPES...>> : public GenericBlockIncomingInterface {};

// TODO(dkorolev): Unnecessary?
template <>
class BlockIncomingInterface<ThreadSafeIncomingTypes<>> : public GenericBlockIncomingInterface {
 public:
  void OnThreadSafeMessage(movable_message_t&&) override {
    std::cerr << "Not expecting any entries to be sent to a non-accepting block.\n";
    CURRENT_ASSERT(false);
  }
};

// Consider the following setup: `Produce(A, B, C, D) | Consume(A) + Consume(B) + Consume(C) + Consume(D)`.
//
// There are several ways the right hand side of the pipe operator could be constructed, specifically:
// 1) ... | (((A+B)+C)+D)  // left-to-right
// 2) ... | (A+(B+(C+D)))  // right-to-left
// 3) ... | ((A+(B+C))+D)  // middle-first, then left, then right
// 4) ... | (A+((B+C)+D))  // middle-first, then right, then left
// 5) ... | ((A+B)+(C+D))  // two left plus two right.
//
// For all the ways the RipCurrent flow could be initialized, the execution logic is the same: there should be one
// MMPQ to receive the messages from the left hand side. This MMPQ handles handle all `schedule<>` and `head<>` logic,
// leaving only the real messages to reach the corresponding consumers.
//
// Implementation-wise, this implies only the first to initialize "plus combiner" should create an MMPQ, while all the
// other plus-combiners should receive messages from the top-level one synchronously, with no MMPQs or mutexes involved.

template <class LHS_TYPELIST, class RHS_TYPELIST>
class SubCurrentScope;

template <class... LHS_TYPES, class... RHS_TYPES>
class SubCurrentScope<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>
    : public BlockIncomingInterface<ThreadSafeIncomingTypes<LHS_TYPES...>> {
 public:
  virtual ~SubCurrentScope() = default;
};

// The run context of a presently running RipCurrent flow.
// RipCurrent can be run synchronously (via `.RipCurrent().Join()`), or
// asyncronously (via `auto scope = (...).RipCurrent(); ... scope.Join()`).
class RipCurrentScope final {
 public:
  using scope_t = SubCurrentScope<LHSTypes<>, RHSTypes<>>;

  RipCurrentScope(std::shared_ptr<scope_t> scope, const std::string& description_as_text)
      : scope_(scope), legitimately_terminated_(false), description_as_text_(description_as_text) {}

  RipCurrentScope(RipCurrentScope&& rhs)
      : scope_(std::move(rhs.scope_)),
        legitimately_terminated_(rhs.legitimately_terminated_),
        description_as_text_(std::move(rhs.description_as_text_)) {
    rhs.legitimately_terminated_ = true;
  }

  void Join() {
    if (legitimately_terminated_) {
      current::Singleton<RipCurrentMockableErrorHandler>().HandleError(
          "Attempted to call `RipCurrent::Join()` on a scope that has already been legitimately taken care of.\n" +
          description_as_text_);  // LCOV_EXCL_LINE
    }
    legitimately_terminated_ = true;
    scope_ = nullptr;
  }

  RipCurrentScope& Async() {
    if (legitimately_terminated_) {
      current::Singleton<RipCurrentMockableErrorHandler>().HandleError(
          "Attempted to call `RipCurrent::Async()` on a scope that has already been legitimately taken care of.\n" +
          description_as_text_);  // LCOV_EXCL_LINE
    }
    legitimately_terminated_ = true;
    return *this;
  }

  ~RipCurrentScope() {
    if (!legitimately_terminated_) {
      current::Singleton<RipCurrentMockableErrorHandler>().HandleError(
          "RipCurrent run context was left hanging. Call `.Join()` or `.Async()` on it.\n" +
          description_as_text_);  // LCOV_EXCL_LINE
    }
  }

 private:
  std::shared_ptr<scope_t> scope_;
  bool legitimately_terminated_ = false;
  std::string description_as_text_;
};

// Template logic to wrap runnable RipCurrent flows into abstract classes of templated types,
// keeping track of the inbound and outgoing types of the simple or compound block being described.
// Runnable RipCurrent flows would generally be `shared_ptr<AbstractCurrent<LHS, RHS>>`-s, containing references
// to the code to run, its initilization parameters, and an abstract method `Run()` to run the job.
template <class LHS_TYPELIST, class RHS_TYPELIST>
class AbstractCurrent;

template <typename... LHS_TYPES, typename... RHS_TYPES>
class AbstractCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>
    : public SharedDefinition<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> {
 public:
  using definition_t = SharedDefinition<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>;

  explicit AbstractCurrent(definition_t definition) : definition_t(definition) {}
  virtual ~AbstractCurrent() = default;

  virtual std::shared_ptr<SubCurrentScope<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>> Run(
      std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>>) const = 0;

  struct Traits final {
    using input_t = LHSTypes<LHS_TYPES...>;
    using output_t = RHSTypes<RHS_TYPES...>;
  };
  Traits UnderlyingType() const;  // Never called, used for `decltype()` only.
};

// The implementation of the "Instantiated building block == shared_ptr<Abstract building block>" paradigm.
template <class LHS_TYPELIST, class RHS_TYPELIST>
class SharedCurrent;

// TODO(dkorolev): Collapse `SharedCurrent` and `AbstractCurrent`?
template <class... LHS_TYPES, class... RHS_TYPES>
class SharedCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>
    : public AbstractCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> {
 public:
  using input_t = LHSTypes<LHS_TYPES...>;
  using output_t = RHSTypes<RHS_TYPES...>;
  using super_t = AbstractCurrent<input_t, output_t>;

  explicit SharedCurrent(std::shared_ptr<super_t> spawner) : super_t(spawner->GetUniqueDefinition()), super_(spawner) {}

  std::shared_ptr<SubCurrentScope<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>> Run(
      std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>> next) const override {
    return super_->Run(next);
  }

  // User-facing `RipCurrent()` method, only for "closed", end-to-end flows.
  template <int IN_N = sizeof...(LHS_TYPES), int OUT_N = sizeof...(RHS_TYPES)>
  std::enable_if_t<IN_N == 0 && OUT_N == 0, RipCurrentScope> RipCurrent() const {
    std::ostringstream os;
    super_t::GetDefinition().FullDescription(os);

    return RipCurrentScope(Run(std::make_shared<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<>>>()), os.str());
  }

 private:
  std::shared_ptr<super_t> super_;
};

// Helper code to initialize the next handler in the chain before the user code is constructed.
// The user should be able to use `emit<>` and other methods right away from the constructor, no strings attached.
// Thus, the destination for those methods should be initialized before the user code is. Hence an extra base class.
class GenericCallsGeneratingBlock {
 public:
  virtual ~GenericCallsGeneratingBlock() = default;
};

class BlockCallsConsumersManager final {
 public:
  void Add(const GenericCallsGeneratingBlock* key, GenericBlockOutgoingInterface* value) {
    std::lock_guard<std::mutex> lock(mutex_);
    CURRENT_ASSERT(key);
    CURRENT_ASSERT(value);
    CURRENT_ASSERT(!map_.count(key));
    map_[key] = value;
  }
  void Remove(const GenericCallsGeneratingBlock* key, GenericBlockOutgoingInterface* value) {
    std::lock_guard<std::mutex> lock(mutex_);
    CURRENT_ASSERT(key);
    CURRENT_ASSERT(value);
    CURRENT_ASSERT(map_.count(key));
    CURRENT_ASSERT(map_[key] == value);
    map_.erase(key);
  }
  template <typename SPECIFIC_EMITTER_TYPE>
  SPECIFIC_EMITTER_TYPE* Get(const GenericCallsGeneratingBlock* key) {
    std::lock_guard<std::mutex> lock(mutex_);
    CURRENT_ASSERT(key);
    CURRENT_ASSERT(map_.count(key));
    GenericBlockOutgoingInterface* result = map_[key];
    SPECIFIC_EMITTER_TYPE* specific_result = dynamic_cast<SPECIFIC_EMITTER_TYPE*>(result);
    CURRENT_ASSERT(specific_result);
    return specific_result;
  }

  class CallsConsumerLifetimeScope final {
   public:
    explicit CallsConsumerLifetimeScope(const GenericCallsGeneratingBlock* key, GenericBlockOutgoingInterface* value)
        : key(key), value(value) {
      Singleton<BlockCallsConsumersManager>().Add(key, value);
    }
    ~CallsConsumerLifetimeScope() { Singleton<BlockCallsConsumersManager>().Remove(key, value); }

   private:
    const GenericCallsGeneratingBlock* const key;
    GenericBlockOutgoingInterface* const value;

    CallsConsumerLifetimeScope() = delete;
    CallsConsumerLifetimeScope(const CallsConsumerLifetimeScope&) = delete;
    CallsConsumerLifetimeScope(CallsConsumerLifetimeScope&&) = delete;
    CallsConsumerLifetimeScope& operator=(const CallsConsumerLifetimeScope&) = delete;
    CallsConsumerLifetimeScope& operator=(CallsConsumerLifetimeScope&&) = delete;
  };

 private:
  std::mutex mutex_;
  std::map<const GenericCallsGeneratingBlock*, GenericBlockOutgoingInterface*> map_;
};

// The base class for user code, enabling to make the very `emit<>`, `post<>`, `schedule<>`, and `head<>` calls.
template <class EMITTED_TYPES>
class CallsGeneratingBlock;

template <class... EMITTED_TYPES>
class CallsGeneratingBlock<EmittableTypes<EMITTED_TYPES...>> : public GenericCallsGeneratingBlock {
 public:
  using outgoing_interface_t = BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<EMITTED_TYPES...>>;

  CallsGeneratingBlock() : handler_(Singleton<BlockCallsConsumersManager>().template Get<outgoing_interface_t>(this)) {}
  virtual ~CallsGeneratingBlock() = default;

 protected:
  template <typename T, typename... ARGS>
  std::enable_if_t<TypeListContains<TypeListImpl<EMITTED_TYPES...>, T>::value> emit(ARGS&&... args) const {
    // A seemingly unnecessary `release()` is due to `std::make_unique()` not supporting a custom deleter. -- D.K
    handler_->OnThreadUnsafeEmitted(movable_message_t(std::make_unique<T>(std::forward<ARGS>(args)...).release()),
                                    time::Now());
  }

  template <typename T, typename... ARGS>
  std::enable_if_t<TypeListContains<TypeListImpl<EMITTED_TYPES...>, T>::value> post(std::chrono::microseconds t,
                                                                                    ARGS&&... args) const {
    // A seemingly unnecessary `release()` is due to `std::make_unique()` not supporting a custom deleter. -- D.K
    handler_->OnThreadUnsafeEmitted(movable_message_t(std::make_unique<T>(std::forward<ARGS>(args)...).release()), t);
  }

  template <typename T, typename... ARGS>
  std::enable_if_t<TypeListContains<TypeListImpl<EMITTED_TYPES...>, T>::value> schedule(std::chrono::microseconds t,
                                                                                        ARGS&&... args) const {
    // A seemingly unnecessary `release()` is due to `std::make_unique()` not supporting a custom deleter. -- D.K
    handler_->OnThreadUnsafeScheduled(movable_message_t(std::make_unique<T>(std::forward<ARGS>(args)...).release()), t);
  }

  void head(std::chrono::microseconds t) const { handler_->OnThreadUnsafeHeadUpdated(t); }

 private:
  outgoing_interface_t* handler_;
};

// `UserClassInstantiator` instantiates the user class passed in as `USER_CLASS`.
// It serves two purposes:
// 1) Itself, it inherits from `BlockIncomingInterface<ThreadSafeIncomingTypes<LHS_TYPES...>>`, and can accept entries.
//    Those entries are assumed thread safe, and are proxied directly to the user code's `.f()` method.
// 2) It requires the `next_` parameter, which is a `BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>`.
//    Prior to instantiating user class, it uses the `BlockCallsConsumersManager::CallsConsumerLifetimeScope` mechanism
//    to enable user code to make the calls to `emit<>`, `post<>`, `schedule<>`, and `head<>` from its constructor.
template <class LHS_TYPES, class RHS_TYPES, class USER_CLASS>
class UserClassInstantiator;

template <class... LHS_TYPES, class... RHS_TYPES, class USER_CLASS>
class UserClassInstantiator<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS> final
    : BlockIncomingInterface<ThreadSafeIncomingTypes<LHS_TYPES...>> {
 public:
  // TODO(dkorolev): Owned/borrowed instead of `.get()`.
  template <typename... ARGS>
  UserClassInstantiator(std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>> next,
                        ARGS&&... args)
      : scope_(&impl_, next.get()), impl_(std::forward<ARGS>(args)...) {}

  void OnThreadSafeMessage(movable_message_t&& x) override {
    RTTIDynamicCall<TypeListImpl<LHS_TYPES...>, CurrentSuper>(std::move(*x), *this);
  }

  template <typename X>
  void operator()(X&& x) {
    impl_.f(std::forward<X>(x));
  }

  void operator()(CurrentSuper&&) {
    // Should define this method to make sure the `RTTIDynamicCall<>` construct compiles.
    // Given type list magic is done at compile time, this method would never get called.
    CURRENT_ASSERT(false);
  }

 private:
  const BlockCallsConsumersManager::CallsConsumerLifetimeScope scope_;
  USER_CLASS impl_;
};

// Base classes for user-defined code, for `is_base_of<>` `static_assert()`-s.
template <class LHS_TYPELIST, class RHS_TYPELIST>
class UserCodeBase;

template <class... LHS_TYPES, class... RHS_TYPES>
class UserCodeBase<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> {};

template <class LHS_TYPELIST, class RHS_TYPELIST, typename USER_CLASS>
class UserCode;

template <class... LHS_TYPES, class... RHS_TYPES, typename USER_CLASS>
class UserCode<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS>
    : public UserCodeBase<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>,
      public CallsGeneratingBlock<EmittableTypes<RHS_TYPES...>> {
 public:
  virtual ~UserCode() = default;
  using input_t = LHSTypes<LHS_TYPES...>;
  using output_t = RHSTypes<RHS_TYPES...>;
};

// Helper code to support the declaration and running of user-defined classes.
template <class LHS_TYPELIST, class RHS_TYPELIST, typename USER_CLASS>
class UserCodeInstantiator;

template <typename... LHS_TYPES, typename... RHS_TYPES, typename USER_CLASS>
class UserCodeInstantiator<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS>
    : public AbstractCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> {
 public:
  static_assert(std::is_base_of_v<UserCodeBase<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>, USER_CLASS>,
                "User class for RipCurrent data processor should use `RIPCURRENT_NODE()` + `RIPCURRENT_MACRO()`.");

  using instantiator_input_t = LHSTypes<LHS_TYPES...>;
  using instantiator_output_t = RHSTypes<RHS_TYPES...>;

  template <class ARGS_AS_TUPLE>
  UserCodeInstantiator(Definition definition, ARGS_AS_TUPLE&& params)
      : AbstractCurrent<instantiator_input_t, instantiator_output_t>(definition),
        lazy_instance_(current::DelayedInstantiateWithExtraParameterFromTuple<
            UserClassInstantiator<instantiator_input_t, instantiator_output_t, USER_CLASS>,
            std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>>>(
            std::forward<ARGS_AS_TUPLE>(params))) {}

  class Scope final : public SubCurrentScope<instantiator_input_t, instantiator_output_t> {
   public:
    virtual ~Scope() = default;

    explicit Scope(const current::LazilyInstantiated<
                       UserClassInstantiator<instantiator_input_t, instantiator_output_t, USER_CLASS>,
                       std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>>>& lazy_instance,
                   std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>> next)
        : spawned_user_class_instance_(lazy_instance.InstantiateAsUniquePtrWithExtraParameter(next)) {}

    void OnThreadSafeMessage(movable_message_t&& x) override {
      spawned_user_class_instance_->OnThreadSafeMessage(std::move(x));
    }

   private:
    std::unique_ptr<UserClassInstantiator<instantiator_input_t, instantiator_output_t, USER_CLASS>>
        spawned_user_class_instance_;
  };

  std::shared_ptr<SubCurrentScope<instantiator_input_t, instantiator_output_t>> Run(
      std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>> next) const override {
    return std::make_shared<Scope>(lazy_instance_, next);
  }

 private:
  current::LazilyInstantiated<UserClassInstantiator<instantiator_input_t, instantiator_output_t, USER_CLASS>,
                              std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>>>
      lazy_instance_;
};

// `SharedUserCodeInstantiator` is the `shared_ptr<>` holder of the wrapper class
// containing initialization parameters for the user class.
template <class LHS_TYPELIST, class RHS_TYPELIST, typename USER_CLASS>
class UserCodeImpl;

template <class LHS_TYPELIST, class RHS_TYPELIST, typename USER_CLASS>
class SharedUserCodeInstantiator;

template <typename... LHS_TYPES, typename... RHS_TYPES, typename USER_CLASS>
class SharedUserCodeInstantiator<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS> {
 public:
  using impl_t = UserCodeInstantiator<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS>;
  template <class ARGS_AS_TUPLE>
  SharedUserCodeInstantiator(Definition definition, ARGS_AS_TUPLE&& params)
      : shared_spawner_(std::make_shared<impl_t>(definition, std::forward<ARGS_AS_TUPLE>(params))) {}

 private:
  friend class UserCodeImpl<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS>;
  std::shared_ptr<impl_t> shared_spawner_;
};

// `UserCodeImpl<LHSTypes<...>, RHSTypes<...>, IMPL>`
// initializes `SharedUserCodeInstantiator<LHSTypes<...>, RHSTypes<...>, IMPL>`
// before constructing the parent `SharedCurrent<LHSTypes<...>, RHSTypes<...>>` object, thus
// allowing the latter to reuse the `shared_ptr<>` containing the user code constructed in the former.
template <typename... LHS_TYPES, typename... RHS_TYPES, typename USER_CLASS>
class UserCodeImpl<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS>
    : public SharedUserCodeInstantiator<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS>,
      public SharedCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> {
 public:
  using impl_t = SharedUserCodeInstantiator<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS>;
  template <class ARGS_AS_TUPLE>
  UserCodeImpl(Definition definition, ARGS_AS_TUPLE&& params)
      : impl_t(definition, std::forward<ARGS_AS_TUPLE>(params)),
        SharedCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>(impl_t::shared_spawner_) {}

  // For the `RIPCURRENT_UNDERLYING_TYPE` macro to be able to extract the underlying user-defined type.
  USER_CLASS UnderlyingType() const;
};

// The implementation of the `A | B` combiner building block.
template <class LHS_TYPELIST, class RHS_TYPELIST, class VIA_TYPELIST>
class SharedSequenceImpl;

template <class... LHS_TYPES, class... RHS_TYPES, typename VIA_X, typename... VIA_XS>
class SharedSequenceImpl<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, VIATypes<VIA_X, VIA_XS...>>
    : public AbstractCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> {
 public:
  SharedSequenceImpl(SharedCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<VIA_X, VIA_XS...>> from,
                     SharedCurrent<LHSTypes<VIA_X, VIA_XS...>, RHSTypes<RHS_TYPES...>> into)
      : AbstractCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>(
            Definition(Definition::Pipe(), from.GetDefinition(), into.GetDefinition())),
        from_(from),
        into_(into) {
    from.MarkAs(BlockUsageBit::UsedInLargerBlock);
    into.MarkAs(BlockUsageBit::UsedInLargerBlock);
  }

  class Scope final : public SubCurrentScope<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> {
   public:
    virtual ~Scope() = default;

    Scope(const SharedSequenceImpl* self,
          std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>> next)
        : next_(next),
          into_(self->Into().Run(next_)),
          into_mmpq_(std::make_shared<MMPQWrapper>(into_)),
          from_(self->From().Run(into_mmpq_)) {
      self->MarkAs(BlockUsageBit::HasBeenRun);
    }

    void OnThreadSafeMessage(movable_message_t&& x) override { from_->OnThreadSafeMessage(std::move(x)); }

   private:
    class MMPQWrapper final : public BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<VIA_X, VIA_XS...>> {
     public:
      explicit MMPQWrapper(
          std::shared_ptr<BlockIncomingInterface<ThreadSafeIncomingTypes<VIA_X, VIA_XS...>>> destination)
          : single_threaded_processor_(waitable_counters_, destination), mmpq_(single_threaded_processor_) {}

      ~MMPQWrapper() {
        waitable_counters_.Wait([](const ThreadMessageCounters& counters) { return counters.ProcessedEverything(); });
      }

      void OnThreadUnsafeEmitted(movable_message_t&& x, std::chrono::microseconds t) override {
        waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportPublishCalled(); });
        try {
          mmpq_.UpdateHead(t);  // Run `UpdateHead` before `Publish`, as the latter does not validate monotocinity.
          mmpq_.Publish(std::move(x), t);
          waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportMessagePublished(); });
        } catch (const ss::InconsistentTimestampException& e) {
          current::Singleton<RipCurrentMockableErrorHandler>().HandleError(e.DetailedDescription());
          waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportMessageNotPublished(); });
        }
      }

      void OnThreadUnsafeScheduled(movable_message_t&& x, std::chrono::microseconds t) override {
        waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportPublishCalled(); });
        try {
          mmpq_.Publish(std::move(x), t);
          waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportMessagePublished(); });
        } catch (const ss::InconsistentTimestampException& e) {
          current::Singleton<RipCurrentMockableErrorHandler>().HandleError(e.DetailedDescription());
          waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportMessageNotPublished(); });
        }
      }

      void OnThreadUnsafeHeadUpdated(std::chrono::microseconds t) override {
        try {
          mmpq_.UpdateHead(t);
        } catch (const ss::InconsistentTimestampException& e) {
          current::Singleton<RipCurrentMockableErrorHandler>().HandleError(e.DetailedDescription());
        }
      }

     private:
      class ThreadMessageCounters {
       public:
        void ReportPublishCalled() { ++publish_called_count_; }
        void ReportMessagePublished() { ++published_count_; }
        void ReportMessageNotPublished() { ++not_published_count_; }
        void ReportMessageProcessed() { ++processed_count_; }
        bool ProcessedEverything() const { return publish_called_count_ == processed_count_ + not_published_count_; }

       private:
        size_t publish_called_count_ = 0u;
        size_t published_count_ = 0u;
        size_t not_published_count_ = 0u;
        size_t processed_count_ = 0u;
      };

      struct SingleThreadedProcessorImpl {
        SingleThreadedProcessorImpl(
            WaitableAtomic<ThreadMessageCounters>& waitable_counters,
            std::shared_ptr<BlockIncomingInterface<ThreadSafeIncomingTypes<VIA_X, VIA_XS...>>> next)
            : waitable_counters_(waitable_counters), next_(next) {}

        ss::EntryResponse operator()(movable_message_t&& e, idxts_t, idxts_t) {
          next_->OnThreadSafeMessage(std::move(e));
          waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportMessageProcessed(); });
          return ss::EntryResponse::More;
        }

        WaitableAtomic<ThreadMessageCounters>& waitable_counters_;
        std::shared_ptr<BlockIncomingInterface<ThreadSafeIncomingTypes<VIA_X, VIA_XS...>>> next_;
      };

      WaitableAtomic<ThreadMessageCounters> waitable_counters_;
      current::ss::EntrySubscriber<SingleThreadedProcessorImpl, movable_message_t> single_threaded_processor_;
      mmq::MMPQ<movable_message_t, current::ss::EntrySubscriber<SingleThreadedProcessorImpl, movable_message_t>> mmpq_;
    };

    // Construction / destruction order matters: { next, into, from }.
    std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>> next_;
    std::shared_ptr<SubCurrentScope<LHSTypes<VIA_X, VIA_XS...>, RHSTypes<RHS_TYPES...>>> into_;
    std::shared_ptr<MMPQWrapper> into_mmpq_;
    std::shared_ptr<SubCurrentScope<LHSTypes<LHS_TYPES...>, RHSTypes<VIA_X, VIA_XS...>>> from_;
  };

  std::shared_ptr<SubCurrentScope<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>> Run(
      std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<RHS_TYPES...>>> next) const override {
    return std::make_shared<Scope>(this, next);
  }

 protected:
  const SharedCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<VIA_X, VIA_XS...>>& From() const { return from_; }
  const SharedCurrent<LHSTypes<VIA_X, VIA_XS...>, RHSTypes<RHS_TYPES...>>& Into() const { return into_; }

 private:
  SharedCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<VIA_X, VIA_XS...>> from_;
  SharedCurrent<LHSTypes<VIA_X, VIA_XS...>, RHSTypes<RHS_TYPES...>> into_;
};

// `SharedSequence` requires `VIA_TYPELIST` to be a nonempty type list.
// This is enforced by the `operator|()` declaration below.
template <class LHS_TYPELIST, class RHS_TYPELIST, class VIA_TYPELIST>
class SharedSequence;

template <class LHS_TYPELIST, class RHS_TYPELIST, typename... VIA_XS>
class SharedSequence<LHS_TYPELIST, RHS_TYPELIST, VIATypes<VIA_XS...>>
    : public SharedCurrent<LHS_TYPELIST, RHS_TYPELIST> {
 public:
  using base_t = SharedCurrent<LHS_TYPELIST, RHS_TYPELIST>;
  SharedSequence(SharedCurrent<LHS_TYPELIST, RHSTypes<VIA_XS...>> from,
                 SharedCurrent<LHSTypes<VIA_XS...>, RHS_TYPELIST> into)
      : base_t(std::make_shared<SharedSequenceImpl<LHS_TYPELIST, RHS_TYPELIST, VIATypes<VIA_XS...>>>(from, into)) {}
};

// SharedCurrent sequence combiner, `A | B`.
template <class LHS_TYPELIST, class RHS_TYPELIST, typename VIA_X, typename... VIA_XS>
SharedCurrent<LHS_TYPELIST, RHS_TYPELIST> operator|(SharedCurrent<LHS_TYPELIST, RHSTypes<VIA_X, VIA_XS...>> from,
                                                    SharedCurrent<LHSTypes<VIA_X, VIA_XS...>, RHS_TYPELIST> into) {
  return SharedSequence<LHS_TYPELIST, RHS_TYPELIST, VIATypes<VIA_X, VIA_XS...>>(from, into);
}

// The implementation of the `A + B` combiner building block.
template <class LHS_TYPELIST, class RHS_TYPELIST, class A_LHS, class A_RHS, class B_LHS, class B_RHS>
class SharedParallelImpl;

template <class... AB_LHS, class... AB_RHS, class... A_LHS, class... A_RHS, class... B_LHS, class... B_RHS>
class SharedParallelImpl<LHSTypes<AB_LHS...>,
                         RHSTypes<AB_RHS...>,
                         LHSTypes<A_LHS...>,
                         RHSTypes<A_RHS...>,
                         LHSTypes<B_LHS...>,
                         RHSTypes<B_RHS...>> : public AbstractCurrent<LHSTypes<AB_LHS...>, RHSTypes<AB_RHS...>> {
 public:
  SharedParallelImpl(SharedCurrent<LHSTypes<A_LHS...>, RHSTypes<A_RHS...>> a,
                     SharedCurrent<LHSTypes<B_LHS...>, RHSTypes<B_RHS...>> b)
      : AbstractCurrent<LHSTypes<AB_LHS...>, RHSTypes<AB_RHS...>>(
            Definition(Definition::Plus(), a.GetDefinition(), b.GetDefinition())),
        a_(a),
        b_(b) {
    a.MarkAs(BlockUsageBit::UsedInLargerBlock);
    b.MarkAs(BlockUsageBit::UsedInLargerBlock);
  }

  class Scope final : public SubCurrentScope<LHSTypes<AB_LHS...>, RHSTypes<AB_RHS...>> {
   public:
    virtual ~Scope() = default;

    Scope(const SharedParallelImpl* self,
          std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<AB_RHS...>>> next)
        : next_(next),
          next_a_(std::make_shared<PassOnToNextA>(next)),
          next_b_(std::make_shared<PassOnToNextB>(next)),
          a_(self->A().Run(next_a_)),
          b_(self->B().Run(next_b_)) {
      self->MarkAs(BlockUsageBit::HasBeenRun);
    }

    class Router {
     public:
      explicit Router(Scope* self) : self_(self) {}

      template <typename X>
      void operator()(X&& x) {
        constexpr bool a = metaprogramming::TypeListContains<TypeListImpl<A_LHS...>, X>::value;
        constexpr bool b = metaprogramming::TypeListContains<TypeListImpl<B_LHS...>, X>::value;
        static_assert(a != b, "Type X should be either in A's input, or in B's input, but not both.");
        // This is to be cleaned up. -- D.K., TODO(dkorolev), FIXME DIMA.
        auto y = movable_message_t(std::make_unique<X>(std::move(x)).release());
        if (a) {
          self_->a_->OnThreadSafeMessage(std::move(y));
        } else {
          self_->b_->OnThreadSafeMessage(std::move(y));
        }
      }

      void operator()(CurrentSuper&&) {
        // Should define this method to make sure the RTTI call compiles.
        CURRENT_ASSERT(false);
      }

     private:
      Scope* self_;
    };

    void OnThreadSafeMessage(movable_message_t&& x) override {
      RTTIDynamicCall<metaprogramming::TypeListUnion<TypeListImpl<A_LHS...>, TypeListImpl<B_LHS...>>, CurrentSuper>(
          std::move(*x), Router(this));
    }

   private:
    // Helper passthrough `next` handlers.
    struct PassOnToNextA : BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<A_RHS...>> {
      std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<AB_RHS...>>> next;
      PassOnToNextA(std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<AB_RHS...>>> next) : next(next) {}
      void OnThreadUnsafeEmitted(movable_message_t&& x, std::chrono::microseconds t) override {
        next->OnThreadUnsafeEmitted(std::move(x), t);
      }
      void OnThreadUnsafeScheduled(movable_message_t&& x, std::chrono::microseconds t) override {
        next->OnThreadUnsafeScheduled(std::move(x), t);
      }
      void OnThreadUnsafeHeadUpdated(std::chrono::microseconds t) override { next->OnThreadUnsafeHeadUpdated(t); }
    };
    struct PassOnToNextB : BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<B_RHS...>> {
      std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<AB_RHS...>>> next;
      PassOnToNextB(std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<AB_RHS...>>> next) : next(next) {}
      void OnThreadUnsafeEmitted(movable_message_t&& x, std::chrono::microseconds t) override {
        next->OnThreadUnsafeEmitted(std::move(x), t);
      }
      void OnThreadUnsafeScheduled(movable_message_t&& x, std::chrono::microseconds t) override {
        next->OnThreadUnsafeScheduled(std::move(x), t);
      }
      void OnThreadUnsafeHeadUpdated(std::chrono::microseconds t) override { next->OnThreadUnsafeHeadUpdated(t); }
    };

    // Construction / destruction order matters: { `next_a_`, `next_b_` } before { `a_`, `b_` }.
    std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<AB_RHS...>>> next_;
    std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<A_RHS...>>> next_a_;
    std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<B_RHS...>>> next_b_;
    std::shared_ptr<SubCurrentScope<LHSTypes<A_LHS...>, RHSTypes<A_RHS...>>> a_;
    std::shared_ptr<SubCurrentScope<LHSTypes<B_LHS...>, RHSTypes<B_RHS...>>> b_;
  };

  std::shared_ptr<SubCurrentScope<LHSTypes<AB_LHS...>, RHSTypes<AB_RHS...>>> Run(
      std::shared_ptr<BlockOutgoingInterface<ThreadUnsafeOutgoingTypes<AB_RHS...>>> next) const override {
    return std::make_shared<Scope>(this, next);
  }

 protected:
  const SharedCurrent<LHSTypes<A_LHS...>, RHSTypes<A_RHS...>>& A() const { return a_; }
  const SharedCurrent<LHSTypes<B_LHS...>, RHSTypes<B_RHS...>>& B() const { return b_; }

 private:
  SharedCurrent<LHSTypes<A_LHS...>, RHSTypes<A_RHS...>> a_;
  SharedCurrent<LHSTypes<B_LHS...>, RHSTypes<B_RHS...>> b_;
};

// `SharedParallel` requires `VIA_TYPELIST` to be a nonempty type list.
template <class AB_LHS, class AB_RHS, class A_LHS, class A_RHS, class B_LHS, class B_RHS>
class SharedParallel : public SharedCurrent<AB_LHS, AB_RHS> {
 public:
  using base_t = SharedCurrent<AB_LHS, AB_RHS>;
  SharedParallel(SharedCurrent<A_LHS, A_RHS> a, SharedCurrent<B_LHS, B_RHS> b)
      : base_t(std::make_shared<SharedParallelImpl<AB_LHS, AB_RHS, A_LHS, A_RHS, B_LHS, B_RHS>>(a, b)) {}
};

// SharedCurrent parallel combiner, `A + B`.
template <class... A_LHS, class... A_RHS, class... B_LHS, class... B_RHS>
SharedCurrent<LHSTypesFromTypeList<metaprogramming::TypeListCat<TypeListImpl<A_LHS...>, TypeListImpl<B_LHS...>>>,
              RHSTypesFromTypeList<metaprogramming::TypeListUnion<TypeListImpl<A_RHS...>, TypeListImpl<B_RHS...>>>>
operator+(SharedCurrent<LHSTypes<A_LHS...>, RHSTypes<A_RHS...>> a,
          SharedCurrent<LHSTypes<B_LHS...>, RHSTypes<B_RHS...>> b) {
  using lhs_t = LHSTypesFromTypeList<metaprogramming::TypeListCat<TypeListImpl<A_LHS...>, TypeListImpl<B_LHS...>>>;
  using rhs_t = RHSTypesFromTypeList<metaprogramming::TypeListUnion<TypeListImpl<A_RHS...>, TypeListImpl<B_RHS...>>>;
  return SharedParallel<lhs_t, rhs_t, LHSTypes<A_LHS...>, RHSTypes<A_RHS...>, LHSTypes<B_LHS...>, RHSTypes<B_RHS...>>(
      a, b);
}

// These `using`-s are the types the user can directly operate with.
// All of them can be liberally copied over, since the logic is concealed within the inner `shared_ptr<>`.
template <typename RHS_TYPELIST>
using LHS = SharedCurrent<LHSTypes<>, RHS_TYPELIST>;

template <typename LHS_TYPELIST, typename RHS_TYPELIST>
using VIA = SharedCurrent<LHS_TYPELIST, RHS_TYPELIST>;

template <typename LHS_TYPELIST>
using RHS = SharedCurrent<LHS_TYPELIST, RHSTypes<>>;

using E2E = SharedCurrent<LHSTypes<>, RHSTypes<>>;

// The `RIPCURRENT_PASS` built-in building block.
template <typename T, typename... TS>
struct PassImplClassName {
  static const char* RIPCURRENT_CLASS_NAME() { return "PassImpl"; }
};

template <typename T, typename... TS>
struct PassImpl final : UserCode<LHSTypes<T, TS...>, RHSTypes<T, TS...>, PassImplClassName<T, TS...>> {
  using super_t = UserCode<LHSTypes<T, TS...>, RHSTypes<T, TS...>, PassImplClassName<T, TS...>>;
  template <typename X>
  void f(X&& x) {
    super_t::template emit<current::decay_t<X>>(std::forward<X>(x));
  }
};

// Note: `struct Pass` will only be used if the user chooses it over the `RIPCURRENT_PASS` one. The latter option
// includes the names of the types in the type list into the description, which is better for practical purposes.
template <typename T, typename... TS>
struct Pass : UserCodeImpl<LHSTypes<T, TS...>, RHSTypes<T, TS...>, PassImpl<T, TS...>> {
  using super_t = UserCodeImpl<LHSTypes<T, TS...>, RHSTypes<T, TS...>, PassImpl<T, TS...>>;
  Pass() : super_t(Definition("PASS", __FILE__, __LINE__), std::make_tuple()) {}
};

// The `RIPCURRENT_DROP` built-in building block.
template <typename T, typename... TS>
struct DropImplClassName {
  static const char* RIPCURRENT_CLASS_NAME() { return "DropImpl"; }
};

template <typename T, typename... TS>
struct DropImpl final : UserCode<LHSTypes<T, TS...>, RHSTypes<>, DropImplClassName<T, TS...>> {
  using super_t = UserCode<LHSTypes<T, TS...>, RHSTypes<>, DropImplClassName<T, TS...>>;
  template <typename X>
  void f(X&& x) {
    static_cast<void>(x);  // It is drop. Drop, it is.
  }
};

// Note: `struct Drop` will only be used if the user chooses it over the `RIPCURRENT_PASS` one. The latter option
// includes the names of the types in the type list into the description, which is better for practical purposes.
template <typename T, typename... TS>
struct Drop : UserCodeImpl<LHSTypes<T, TS...>, RHSTypes<>, DropImpl<T, TS...>> {
  using super_t = UserCodeImpl<LHSTypes<T, TS...>, RHSTypes<>, DropImpl<T, TS...>>;
  Drop() : super_t(Definition("DROP", __FILE__, __LINE__), std::make_tuple()) {}
};

}  // namespace current::ripcurrent
}  // namespace current

// Macros to wrap user code into RipCurrent building blocks.
#define RIPCURRENT_NODE(USER_CLASS, LHS_TS, RHS_TS)                                                                \
  struct USER_CLASS##_RIPCURRENT_CLASS_NAME {                                                                      \
    static const char* RIPCURRENT_CLASS_NAME() { return #USER_CLASS; }                                             \
  };                                                                                                               \
  struct USER_CLASS final                                                                                          \
      : USER_CLASS##_RIPCURRENT_CLASS_NAME,                                                                        \
        ::current::ripcurrent::UserCode<::current::ripcurrent::VoidOrLHSTypes<CURRENT_REMOVE_PARENTHESES(LHS_TS)>, \
                                        ::current::ripcurrent::VoidOrRHSTypes<CURRENT_REMOVE_PARENTHESES(RHS_TS)>, \
                                        USER_CLASS>

#define RIPCURRENT_MACRO(USER_CLASS, ...)                                                                       \
  ::current::ripcurrent::UserCodeImpl<typename USER_CLASS::input_t, typename USER_CLASS::output_t, USER_CLASS>( \
      ::current::ripcurrent::Definition(#USER_CLASS "(" #__VA_ARGS__ ")", __FILE__, __LINE__),                  \
      std::make_tuple(__VA_ARGS__))

// The macros for templated RipCurrent nodes.
#define RIPCURRENT_NODE_T(USER_CLASS_T, LHS_TS, RHS_TS)                                                            \
  struct USER_CLASS_T##_RIPCURRENT_CLASS_NAME {                                                                    \
    static const char* RIPCURRENT_CLASS_NAME() { return #USER_CLASS_T "<T>"; }                                     \
  };                                                                                                               \
  template <typename T>                                                                                            \
  struct USER_CLASS_T final                                                                                        \
      : USER_CLASS_T##_RIPCURRENT_CLASS_NAME,                                                                      \
        ::current::ripcurrent::UserCode<::current::ripcurrent::VoidOrLHSTypes<CURRENT_REMOVE_PARENTHESES(LHS_TS)>, \
                                        ::current::ripcurrent::VoidOrRHSTypes<CURRENT_REMOVE_PARENTHESES(RHS_TS)>, \
                                        USER_CLASS_T<T>>

#define RIPCURRENT_MACRO_T(USER_CLASS, T, ...)                                                         \
  ::current::ripcurrent::UserCodeImpl<typename USER_CLASS<T>::input_t,                                 \
                                      typename USER_CLASS<T>::output_t,                                \
                                      USER_CLASS<T>>(                                                  \
      ::current::ripcurrent::Definition(#USER_CLASS "<" #T ">(" #__VA_ARGS__ ")", __FILE__, __LINE__), \
      std::make_tuple(__VA_ARGS__))

// A shortcut for `current::ripcurrent::Pass<...>()`, with the benefit of listing types as RipCurrent node name.
#define RIPCURRENT_PASS(...)                                                                                     \
  ::current::ripcurrent::UserCodeImpl<::current::ripcurrent::LHSTypes<CURRENT_REMOVE_PARENTHESES(__VA_ARGS__)>,  \
                                      ::current::ripcurrent::RHSTypes<CURRENT_REMOVE_PARENTHESES(__VA_ARGS__)>,  \
                                      ::current::ripcurrent::PassImpl<CURRENT_REMOVE_PARENTHESES(__VA_ARGS__)>>( \
      ::current::ripcurrent::Definition("Pass(" #__VA_ARGS__ ")", __FILE__, __LINE__), std::make_tuple())

// A shortcut for `current::ripcurrent::Drop<...>()`, with the benefit of listing types as RipCurrent node name.
#define RIPCURRENT_DROP(...)                                                                                     \
  ::current::ripcurrent::UserCodeImpl<::current::ripcurrent::LHSTypes<CURRENT_REMOVE_PARENTHESES(__VA_ARGS__)>,  \
                                      ::current::ripcurrent::RHSTypes<>,                                         \
                                      ::current::ripcurrent::DropImpl<CURRENT_REMOVE_PARENTHESES(__VA_ARGS__)>>( \
      ::current::ripcurrent::Definition("Drop(" #__VA_ARGS__ ")", __FILE__, __LINE__), std::make_tuple())

// A helper macro to extract the underlying type of the user class, now registered as a RipCurrent block type.
// Use `__VA_ARGS__` to support templated constructs with commas inside them.
#define RIPCURRENT_UNDERLYING_TYPE(...) decltype((CURRENT_REMOVE_PARENTHESES(__VA_ARGS__)).UnderlyingType())

// Because `emit<T>` isn't directly visible from a template-instantiated class. Oh well. -- D.K.
#define RIPCURRENT_TEMPLATED_EMIT(TS, T, ...)  \
  ::current::ripcurrent::CallsGeneratingBlock< \
      ::current::ripcurrent::EmittableTypes<CURRENT_REMOVE_PARENTHESES(TS)>>::template emit<T>(__VA_ARGS__)

#endif  // CURRENT_RIPCURRENT_RIPCURRENT_H
