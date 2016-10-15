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

// When defining the RipCurrent with `|` and `+` C++ operators, each sub-expression is a building block.
// Building blocks can be output-only ("LHS"), input-only ("RHS"), in/out ("VIA"), or end-to-end ("E2E").
// No buliding block, simple or composite, can be left hanging. Each one should be used, run, described,
// or dismissed.
//
// End-to-end blocks can be run with `(...).RipCurrent().Join()`. Another option is to save the return value
// of `(...).RipCurrent()` into some `scope` variable, which must be `.Join()`-ed at a later time. Finally,
// the `scope` variable can be called `.Async()` on, which would eliminate the need to explicitly call `.Join()`
// at the end of its lifetime; and the syntax of `auto scope = (...).RipCurrent().Async();` is supported as well.
//
// HI-PRI:
// TODO(dkorolev): ParseFileByLines() and/or TailFileForever() as possible LHS.
// TODO(dkorolev): Sherlock listener as possible LHS.
// TODO(dkorolev): The `+`-combiner.
// TODO(dkorolev): Syntax for no-MMQ and no-multithreading message passing (`| !foo`, `| ~foo`).
//
// LO-PRI:
// TODO(dkorolev): Add debug output counters / HTTP endpoint for # of messages per typeid.
// TODO(dkorolev): Add GraphViz-based visualization.

#ifndef CURRENT_RIPCURRENT_H
#define CURRENT_RIPCURRENT_H

#include "../port.h"

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "../TypeSystem/struct.h"
#include "../TypeSystem/remove_parentheses.h"

#include "../Blocks/MMQ/mmpq.h"

#include "../Bricks/strings/join.h"
#include "../Bricks/sync/waitable_atomic.h"
#include "../Bricks/template/rtti_dynamic_call.h"
#include "../Bricks/template/typelist.h"
#include "../Bricks/util/lazy_instantiation.h"
#include "../Bricks/util/singleton.h"

namespace current {
namespace ripcurrent {

using movable_message_t = std::unique_ptr<CurrentSuper, CurrentSuperDeleter>;

template <typename... TS>
class LHSTypes;

template <typename... TS>
class RHSTypes;

template <typename... TS>
struct VoidOrLHSImpl {
  using type = LHSTypes<TS...>;
};

template <typename... TS>
struct VoidOrRHSImpl {
  using type = RHSTypes<TS...>;
};

template <>
struct VoidOrLHSImpl<void> {
  using type = LHSTypes<>;
};

template <>
struct VoidOrRHSImpl<void> {
  using type = RHSTypes<>;
};

template <typename... TS>
using VoidOrLHS = typename VoidOrLHSImpl<TS...>::type;

template <typename... TS>
using VoidOrRHS = typename VoidOrRHSImpl<TS...>::type;

template <typename... TS>
class VIATypes;

// A singleton wrapping error handling logic, to allow mocking for the unit test.
class RipCurrentMockableErrorHandler {
 public:
  using handler_t = std::function<void(const std::string& error_message)>;

  // LCOV_EXCL_START
  RipCurrentMockableErrorHandler()
      : handler_([](const std::string& error_message) {
          std::cerr << error_message;
          std::exit(-1);
        }) {}
  // LCOV_EXCL_STOP

  void HandleError(const std::string& error_message) const { handler_(error_message); }

  void SetHandler(handler_t f) { handler_ = f; }
  handler_t GetHandler() const { return handler_; }

  class InjectedHandlerScope final {
   public:
    explicit InjectedHandlerScope(RipCurrentMockableErrorHandler* parent, handler_t handler)
        : parent_(parent), save_handler_(parent->GetHandler()) {
      parent_->SetHandler(handler);
    }
    ~InjectedHandlerScope() { parent_->SetHandler(save_handler_); }

   private:
    RipCurrentMockableErrorHandler* parent_;
    handler_t save_handler_;
  };

  InjectedHandlerScope ScopedInjectHandler(handler_t injected_handler) {
    return InjectedHandlerScope(this, injected_handler);
  }

 private:
  handler_t handler_;
};

// The wrapper to store `__FILE__` and `__LINE__` for human-readable verbose explanations.
struct FileLine final {
  const char* const file;
  const size_t line;
};

// `Definition` traces down to the original statement in user code that defined a particular building block.
// Used for diagnostic messages in runtime errors when some Current building block was left hanging.
struct Definition {
  struct Pipe final {};
  std::string statement;
  std::vector<std::pair<std::string, FileLine>> sources;
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
  virtual ~Definition() = default;
};

// `UniqueDefinition` is a `Definition` that:
// * Is templated with `LHSTypes<...>` and `RHSTypes<...>` for strict typing.
// * Exposes user-friendly decorators.
// * Can be marked as used in various ways.
// * Generates an error upon destructing if it was not marked as used in at least one way.
enum class BlockUsageBit : size_t { Described = 0, Run = 1, UsedInLargerBlock = 2, Dismissed = 3 };
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
  void MarkAs(BlockUsageBit bit) { mask_ = BlockUsageBitMask(size_t(mask_) | (1 << size_t(bit))); }

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

class GenericEmitDestination {
 public:
  virtual ~GenericEmitDestination() = default;
  virtual void OnThreadUnsafeEmitted(movable_message_t&&, std::chrono::microseconds) = 0;
  virtual void OnThreadUnsafeScheduled(movable_message_t&&, std::chrono::microseconds) = 0;
  virtual void OnThreadUnsafeHeadUpdated(std::chrono::microseconds) = 0;
};

template <class LHS_TYPELIST>
class EmitDestination;

template <class... LHS_XS>
class EmitDestination<LHSTypes<LHS_XS...>> : public GenericEmitDestination {};

template <>
class EmitDestination<LHSTypes<>> : public GenericEmitDestination {
 public:
  void OnThreadUnsafeEmitted(movable_message_t&&, std::chrono::microseconds) override {
    std::cerr << "Not expecting any entries to be sent to a non-consuming \"consumer\".\n";
    CURRENT_ASSERT(false);
  }
  void OnThreadUnsafeScheduled(movable_message_t&&, std::chrono::microseconds) override {
    std::cerr << "Not expecting any entries to be sent to a non-consuming \"consumer\".\n";
    CURRENT_ASSERT(false);
  }
  virtual void OnThreadUnsafeHeadUpdated(std::chrono::microseconds) override {
    std::cerr << "Not expecting HEAD to be updated for a non-consuming \"consumer\".\n";
    CURRENT_ASSERT(false);
  }
};

template <class LHS_TYPELIST, class RHS_TYPELIST>
class SubCurrentScope;

template <class... LHS_TYPES, class... RHS_TYPES>
class SubCurrentScope<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> : public EmitDestination<LHSTypes<LHS_TYPES...>> {
 public:
  virtual ~SubCurrentScope() = default;
};

// The run context for RipCurrent to allow running it synchronously (via `.RipCurrent().Join()`), or
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

// Template logic to wrap the above implementations into abstract classes of proper templated types.
template <class LHS_TYPELIST, class RHS_TYPELIST>
class AbstractCurrent;

template <typename... LHS_TYPES, typename... RHS_TYPES>
class AbstractCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>
    : public SharedDefinition<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> {
 public:
  struct Traits final {
    using input_t = LHSTypes<LHS_TYPES...>;
    using output_t = RHSTypes<RHS_TYPES...>;
  };

  using definition_t = SharedDefinition<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>;

  explicit AbstractCurrent(definition_t definition) : definition_t(definition) {}
  virtual ~AbstractCurrent() = default;
  virtual std::shared_ptr<SubCurrentScope<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>> SpawnAndRun(
      std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>>) const = 0;

  Traits UnderlyingType() const;  // Never called, used from `decltype()`.
};

// The implementation of the "Instantiated building block == shared_ptr<Abstract building block>" paradigm.
template <class LHS_TYPELIST, class RHS_TYPELIST>
class SharedCurrent;

template <class... LHS_TYPES, class... RHS_TYPES>
class SharedCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>
    : public AbstractCurrent<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>> {
 public:
  using input_t = LHSTypes<LHS_TYPES...>;
  using output_t = RHSTypes<RHS_TYPES...>;
  using super_t = AbstractCurrent<input_t, output_t>;

  explicit SharedCurrent(std::shared_ptr<super_t> spawner)
      : super_t(spawner->GetUniqueDefinition()), shared_impl_spawner_(spawner) {}

  std::shared_ptr<SubCurrentScope<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>> SpawnAndRun(
      std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>> next) const override {
    return shared_impl_spawner_->SpawnAndRun(next);
  }

  // User-facing `RipCurrent()` method.
  template <int IN_N = sizeof...(LHS_TYPES), int OUT_N = sizeof...(RHS_TYPES)>
  std::enable_if_t<IN_N == 0 && OUT_N == 0, RipCurrentScope> RipCurrent() const {
    std::ostringstream os;
    super_t::GetDefinition().FullDescription(os);

    return RipCurrentScope(SpawnAndRun(std::make_shared<EmitDestination<LHSTypes<>>>()), os.str());
  }

 private:
  std::shared_ptr<super_t> shared_impl_spawner_;
};

// Helper code to initialize the next handler in the chain before the user code is constructed.
// The user should be able to use `emit()` right away from the constructor, no strings attached.
// Thus, the destination for this `emit()` should be initialized before the user code is. Hence an extra base class.
class AbstractHasEmit {
 public:
  virtual ~AbstractHasEmit() = default;
};

class EventConsumersManager final {
 public:
  void Add(const AbstractHasEmit* key, GenericEmitDestination* value) {
    std::lock_guard<std::mutex> lock(mutex_);
    CURRENT_ASSERT(key);
    CURRENT_ASSERT(value);
    CURRENT_ASSERT(!map_.count(key));
    map_[key] = value;
  }
  void Remove(const AbstractHasEmit* key, GenericEmitDestination* value) {
    std::lock_guard<std::mutex> lock(mutex_);
    CURRENT_ASSERT(key);
    CURRENT_ASSERT(value);
    CURRENT_ASSERT(map_.count(key));
    CURRENT_ASSERT(map_[key] == value);
    map_.erase(key);
  }
  template <typename SPECIFIC_EMITTER_TYPE>
  SPECIFIC_EMITTER_TYPE* Get(const AbstractHasEmit* key) {
    std::lock_guard<std::mutex> lock(mutex_);
    CURRENT_ASSERT(key);
    CURRENT_ASSERT(map_.count(key));
    GenericEmitDestination* result = map_[key];
    SPECIFIC_EMITTER_TYPE* specific_result = dynamic_cast<SPECIFIC_EMITTER_TYPE*>(result);
    CURRENT_ASSERT(specific_result);
    return specific_result;
  }

  class EventConsumerLifetimeScope final {
   public:
    explicit EventConsumerLifetimeScope(const AbstractHasEmit* key, GenericEmitDestination* value)
        : key(key), value(value) {
      Singleton<EventConsumersManager>().Add(key, value);
    }
    ~EventConsumerLifetimeScope() { Singleton<EventConsumersManager>().Remove(key, value); }

   private:
    const AbstractHasEmit* const key;
    GenericEmitDestination* const value;

    EventConsumerLifetimeScope() = delete;
    EventConsumerLifetimeScope(const EventConsumerLifetimeScope&) = delete;
    EventConsumerLifetimeScope(EventConsumerLifetimeScope&&) = delete;
    EventConsumerLifetimeScope& operator=(const EventConsumerLifetimeScope&) = delete;
    EventConsumerLifetimeScope& operator=(EventConsumerLifetimeScope&&) = delete;
  };

 private:
  std::mutex mutex_;
  std::map<const AbstractHasEmit*, GenericEmitDestination*> map_;
};

template <class LHS_TYPELIST>
class HasEmit;

template <class... NEXT_TYPES>
class HasEmit<LHSTypes<NEXT_TYPES...>> : public AbstractHasEmit {
 public:
  using emit_destination_t = EmitDestination<LHSTypes<NEXT_TYPES...>>;

  HasEmit() : next_handler_(Singleton<EventConsumersManager>().template Get<emit_destination_t>(this)) {}
  virtual ~HasEmit() = default;

 protected:
  template <typename T, typename... ARGS>
  std::enable_if_t<TypeListContains<TypeListImpl<NEXT_TYPES...>, T>::value> emit(ARGS&&... args) const {
    // A seemingly unnecessary `release()` is due to `std::make_unique()` not supporting a custom deleter. -- D.K
    next_handler_->OnThreadUnsafeEmitted(
        std::move(movable_message_t(std::make_unique<T>(std::forward<ARGS>(args)...).release())), time::Now());
  }

  template <typename T, typename... ARGS>
  std::enable_if_t<TypeListContains<TypeListImpl<NEXT_TYPES...>, T>::value> post(std::chrono::microseconds t,
                                                                                 ARGS&&... args) const {
    // A seemingly unnecessary `release()` is due to `std::make_unique()` not supporting a custom deleter. -- D.K
    next_handler_->OnThreadUnsafeEmitted(
        std::move(movable_message_t(std::make_unique<T>(std::forward<ARGS>(args)...).release())), t);
  }

  template <typename T, typename... ARGS>
  std::enable_if_t<TypeListContains<TypeListImpl<NEXT_TYPES...>, T>::value> schedule(std::chrono::microseconds t,
                                                                                     ARGS&&... args) const {
    // A seemingly unnecessary `release()` is due to `std::make_unique()` not supporting a custom deleter. -- D.K
    next_handler_->OnThreadUnsafeScheduled(
        std::move(movable_message_t(std::make_unique<T>(std::forward<ARGS>(args)...).release())), t);
  }

  void head(std::chrono::microseconds t) const { next_handler_->OnThreadUnsafeHeadUpdated(t); }

 private:
  emit_destination_t* next_handler_;
};

template <typename LHS_TYPELIST, typename RHS_TYPELIST, typename USER_CLASS>
class EventConsumerInitializer;

template <class... LHS_TYPES, class... RHS_TYPES, typename USER_CLASS>
class EventConsumerInitializer<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS> final {
 public:
  // TODO(dkorolev): Owned/borrowed instead of `.get()`.
  template <typename... ARGS>
  EventConsumerInitializer(std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>> next, ARGS&&... args)
      : scope_(&impl_, next.get()), impl_(std::forward<ARGS>(args)...) {}

  void OnThreadSafeEmitted(movable_message_t&& x) {
    RTTIDynamicCall<TypeListImpl<LHS_TYPES...>, CurrentSuper>(std::move(*x), *this);
  }

  template <typename X>
  void operator()(X&& x) {
    impl_.f(std::forward<X>(x));
  }

  void operator()(CurrentSuper&&) {
    // Should define this method to make sure the RTTI call compiles.
    // Assuming type list magic is done right at compile time (TODO(dkorolev): !), it should never get called.
    CURRENT_ASSERT(false);
  }

 private:
  const EventConsumersManager::EventConsumerLifetimeScope scope_;
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
    : public UserCodeBase<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>, public HasEmit<LHSTypes<RHS_TYPES...>> {
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
  static_assert(std::is_base_of<UserCodeBase<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>, USER_CLASS>::value,
                "User class for RipCurrent data processor should use `RIPCURRENT_NODE()` + `RIPCURRENT_MACRO()`.");

  using instantiator_input_t = LHSTypes<LHS_TYPES...>;
  using instantiator_output_t = RHSTypes<RHS_TYPES...>;

  template <class ARGS_AS_TUPLE>
  UserCodeInstantiator(Definition definition, ARGS_AS_TUPLE&& params)
      : AbstractCurrent<instantiator_input_t, instantiator_output_t>(definition),
        lazy_instance_(current::DelayedInstantiateWithExtraParameterFromTuple<
            EventConsumerInitializer<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS>,
            std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>>>(std::forward<ARGS_AS_TUPLE>(params))) {}

  class Scope final : public SubCurrentScope<instantiator_input_t, instantiator_output_t> {
   public:
    explicit Scope(const current::LazilyInstantiated<
                       EventConsumerInitializer<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS>,
                       std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>>>& lazy_instance,
                   std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>> next)
        : next_(next),
          spawned_user_class_instance_(lazy_instance.InstantiateAsUniquePtrWithExtraParameter(next_)),
          waitable_counters_(0u, 0u),
          single_threaded_processor_(waitable_counters_, *spawned_user_class_instance_),
          mmq_(single_threaded_processor_) {}

    virtual ~Scope() {
      // TODO(dkorolev): Consider other termination strategies, not just the plain "process everything" one.
      waitable_counters_.Wait([](const ThreadMessageCounters& p) { return p.ProcessedEverything(); });
    }

    void OnThreadUnsafeEmitted(movable_message_t&& x, std::chrono::microseconds t) override {
      waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportMessagePublished(); });
      try {
        mmq_.Publish(std::move(x), t);
      } catch (const ss::InconsistentTimestampException& e) {
        current::Singleton<RipCurrentMockableErrorHandler>().HandleError(e.What());
        waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportMessageNotQuitePublished(); });
      }
    }

    void OnThreadUnsafeScheduled(movable_message_t&& x, std::chrono::microseconds t) override {
      waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportMessagePublished(); });
      try {
        mmq_.PublishIntoTheFuture(std::move(x), t);
      } catch (const ss::InconsistentTimestampException& e) {
        current::Singleton<RipCurrentMockableErrorHandler>().HandleError(e.What());
        waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportMessageNotQuitePublished(); });
      }
    }

    virtual void OnThreadUnsafeHeadUpdated(std::chrono::microseconds t) override {
      try {
        mmq_.UpdateHead(t);
      } catch (const ss::InconsistentTimestampException& e) {
        current::Singleton<RipCurrentMockableErrorHandler>().HandleError(e.What());
      }
    }

   private:
    class ThreadMessageCounters {
     public:
      ThreadMessageCounters(int a, int b) : published_(a), processed_(b) {}
      void ReportMessagePublished() { ++published_; }
      void ReportMessageNotQuitePublished() { --published_; }
      void ReportMessageProcessed() { ++processed_; }
      bool ProcessedEverything() const { return published_ == processed_; }

     private:
      int published_;
      int processed_;
    };
    using instance_t = EventConsumerInitializer<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS>;
    using waitable_counters_t = WaitableAtomic<ThreadMessageCounters>;

    struct SingleThreadedProcessorImpl {
      SingleThreadedProcessorImpl(waitable_counters_t& waitable_counters, instance_t& instance)
          : waitable_counters_(waitable_counters), instance_(instance) {}
      ss::EntryResponse operator()(movable_message_t&& e, idxts_t, idxts_t) {
        instance_.OnThreadSafeEmitted(std::move(e));
        waitable_counters_.MutableUse([](ThreadMessageCounters& p) { p.ReportMessageProcessed(); });
        return ss::EntryResponse::More;
      }
      waitable_counters_t& waitable_counters_;
      instance_t& instance_;
    };
    using single_threaded_processor_t = current::ss::EntrySubscriber<SingleThreadedProcessorImpl, movable_message_t>;

    std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>> next_;
    std::unique_ptr<instance_t> spawned_user_class_instance_;
    waitable_counters_t waitable_counters_;
    single_threaded_processor_t single_threaded_processor_;
    mmq::MMPQ<movable_message_t, single_threaded_processor_t> mmq_;
  };

  std::shared_ptr<SubCurrentScope<instantiator_input_t, instantiator_output_t>> SpawnAndRun(
      std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>> next) const override {
    return std::make_shared<Scope>(lazy_instance_, next);
  }

 private:
  current::LazilyInstantiated<EventConsumerInitializer<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>, USER_CLASS>,
                              std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>>> lazy_instance_;
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

    Scope(const SharedSequenceImpl* self, std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>> next)
        : next_(next), into_(self->Into().SpawnAndRun(next_)), from_(self->From().SpawnAndRun(into_)) {
      self->MarkAs(BlockUsageBit::Run);
    }

    void OnThreadUnsafeEmitted(movable_message_t&& x, std::chrono::microseconds t) override {
      from_->OnThreadUnsafeEmitted(std::move(x), t);
    }

    void OnThreadUnsafeScheduled(movable_message_t&& x, std::chrono::microseconds t) override {
      from_->OnThreadUnsafeScheduled(std::move(x), t);
    }

    virtual void OnThreadUnsafeHeadUpdated(std::chrono::microseconds t) override {
      from_->OnThreadUnsafeHeadUpdated(t);
    }

   private:
    // Construction / destruction order matters: { next, into, from }.
    std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>> next_;
    std::shared_ptr<SubCurrentScope<LHSTypes<VIA_X, VIA_XS...>, RHSTypes<RHS_TYPES...>>> into_;
    std::shared_ptr<SubCurrentScope<LHSTypes<LHS_TYPES...>, RHSTypes<VIA_X, VIA_XS...>>> from_;
  };

  std::shared_ptr<SubCurrentScope<LHSTypes<LHS_TYPES...>, RHSTypes<RHS_TYPES...>>> SpawnAndRun(
      std::shared_ptr<EmitDestination<LHSTypes<RHS_TYPES...>>> next) const override {
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
      : base_t(std::make_shared<SharedSequenceImpl<LHS_TYPELIST, RHS_TYPELIST, VIATypes<VIA_XS...>>>(from, into)){};
};

// SharedCurrent sequence combiner, `A | B`.
template <class LHS_TYPELIST, class RHS_TYPELIST, typename VIA_X, typename... VIA_XS>
SharedCurrent<LHS_TYPELIST, RHS_TYPELIST> operator|(SharedCurrent<LHS_TYPELIST, RHSTypes<VIA_X, VIA_XS...>> from,
                                                    SharedCurrent<LHSTypes<VIA_X, VIA_XS...>, RHS_TYPELIST> into) {
  return SharedSequence<LHS_TYPELIST, RHS_TYPELIST, VIATypes<VIA_X, VIA_XS...>>(from, into);
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
    super_t::template emit<current::decay<X>>(std::forward<X>(x));
  }
};

// Note: `struct Pass` will only be used if the uses chooses it over the `RIPCURRENT_PASS` one. The latter option
// includes the names of the types in the type list into the description, which is better for practical purposes.
template <typename T, typename... TS>
struct Pass : UserCodeImpl<LHSTypes<T, TS...>, RHSTypes<T, TS...>, PassImpl<T, TS...>> {
  using super_t = UserCodeImpl<LHSTypes<T, TS...>, RHSTypes<T, TS...>, PassImpl<T, TS...>>;
  Pass() : super_t(Definition("PASS", __FILE__, __LINE__), std::make_tuple()) {}
};

}  // namespace current::ripcurrent
}  // namespace current

// Macros to wrap user code into RipCurrent building blocks.
#define RIPCURRENT_NODE(USER_CLASS, LHS_TS, RHS_TS)                                                           \
  struct USER_CLASS##_RIPCURRENT_CLASS_NAME {                                                                 \
    static const char* RIPCURRENT_CLASS_NAME() { return #USER_CLASS; }                                        \
  };                                                                                                          \
  struct USER_CLASS final                                                                                     \
      : USER_CLASS##_RIPCURRENT_CLASS_NAME,                                                                   \
        ::current::ripcurrent::UserCode<::current::ripcurrent::VoidOrLHS<CURRENT_REMOVE_PARENTHESES(LHS_TS)>, \
                                        ::current::ripcurrent::VoidOrRHS<CURRENT_REMOVE_PARENTHESES(RHS_TS)>, \
                                        USER_CLASS>

#define RIPCURRENT_MACRO(USER_CLASS, ...)                                                                       \
  ::current::ripcurrent::UserCodeImpl<typename USER_CLASS::input_t, typename USER_CLASS::output_t, USER_CLASS>( \
      ::current::ripcurrent::Definition(#USER_CLASS "(" #__VA_ARGS__ ")", __FILE__, __LINE__),                  \
      std::make_tuple(__VA_ARGS__))

// A shortcut for `current::ripcurrent::Pass<...>()`, with the benefit of listing types as RipCurrent node name.
#define RIPCURRENT_PASS(...)                                                                                     \
  ::current::ripcurrent::UserCodeImpl<::current::ripcurrent::LHSTypes<CURRENT_REMOVE_PARENTHESES(__VA_ARGS__)>,  \
                                      ::current::ripcurrent::RHSTypes<CURRENT_REMOVE_PARENTHESES(__VA_ARGS__)>,  \
                                      ::current::ripcurrent::PassImpl<CURRENT_REMOVE_PARENTHESES(__VA_ARGS__)>>( \
      ::current::ripcurrent::Definition("Pass(" #__VA_ARGS__ ")", __FILE__, __LINE__), std::make_tuple())

// A helper macro to extract the underlying type of the user class, now registered as a RipCurrent block type.
#define RIPCURRENT_UNDERLYING_TYPE(USER_CLASS) decltype(USER_CLASS.UnderlyingType())

#endif  // CURRENT_RIPCURRENT_H
