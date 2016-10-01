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
// End-to-end blocks can be run with `(...).RipCurrent().Sync()`. TODO(dkorolev): Not only `.Sync()`.
//
// HI-PRI:
// TODO(dkorolev): ParseFileByLines() and/or TailFileForever() as possible LHS.
// TODO(dkorolev): Sherlock listener as possible LHS.
// TODO(dkorolev): MMQ.
// TODO(dkorolev): Threads and joins, run forever.
// TODO(dkorolev): The `+`-combiner.
// TODO(dkorolev): Syntax for no-MMQ and no-multithreading message passing (`| !foo`, `| ~foo`).
// TODO(dkorolev): Run scoping strategies others than `.Sync()`.
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

#include "../Bricks/rtti/dispatcher.h"
#include "../Bricks/strings/join.h"
#include "../Bricks/template/typelist.h"
#include "../Bricks/util/lazy_instantiation.h"
#include "../Bricks/util/singleton.h"

namespace current {
namespace ripcurrent {

template <typename... TS>
class LHS;

template <typename... TS>
class RHS;

template <typename... TS>
class VIA;

// A singleton wrapping error handling logic, to allow mocking for the unit test.
class RipCurrentMockableErrorHandler {
 public:
  typedef std::function<void(const std::string& error_message)> handler_t;

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
// * Templated with `LHS<...>` and `RHS<...>` for strict typing.
// * Exposes user-friendly decorators.
// * Can be marked as used in various ways.
// * Generates an error upon destructing if it was not marked as used in at least one way.
enum class BlockUsageBit : size_t { Described = 0, Run = 1, UsedInLargerBlock = 2, Dismissed = 3 };
enum class BlockUsageBitMask : size_t { Unused = 0 };

template <class LHS_TYPELIST, class RHS_TYPELIST>
class UniqueDefinition;

template <typename... LHS_TYPES, typename... RHS_TYPES>
class UniqueDefinition<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>> final : public Definition {
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

// `SharedUniqueDefinition` is a helper class for an `std::shared_ptr<UniqueDefinition>`.
// The premise is that building blocks that should be used or run can be liberally copied over,
// with the test that they have been used or run performed at the very end of their lifetime.
template <class LHS_TYPELIST, class RHS_TYPELIST>
class SharedUniqueDefinition;

template <typename... LHS_TYPES, typename... RHS_TYPES>
class SharedUniqueDefinition<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>> {
 public:
  using impl_t = UniqueDefinition<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>>;

  SharedUniqueDefinition(Definition definition) : unique_definition_(std::make_shared<impl_t>(definition)) {}
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

  // For expressive initialized lists of shared instances.
  const SharedUniqueDefinition& GetUniqueDefinition() const { return *this; }
  const UniqueDefinition<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>>& GetDefinition() const {
    return *unique_definition_.get();
  }

 private:
  mutable std::shared_ptr<impl_t> unique_definition_;
};

class GenericEntriesConsumer {
 public:
  virtual ~GenericEntriesConsumer() = default;
  virtual void ConsumeEntry(const CurrentSuper&) = 0;
};

template <class LHS_TYPELIST>
class EntriesConsumer;

template <class... LHS_XS>
class EntriesConsumer<LHS<LHS_XS...>> : public GenericEntriesConsumer {};

class NoEntryShallPassFakeConsumer final : public EntriesConsumer<LHS<>> {
 public:
  void ConsumeEntry(const CurrentSuper&) override {
    std::cerr << "Not expecting any entries to be sent to a non-consuming \"consumer\".\n";
    CURRENT_ASSERT(false);
  }
};

// Run context for RipCurrent allows to run it in background, foreground, or scoped.
// TODO(dkorolev): Only supports `.Sync()` now. Implement everything else.
class RipCurrentRunContext final {
 public:
  explicit RipCurrentRunContext(const std::string& error_message)
      : sync_called_(false), error_message_(error_message) {}
  RipCurrentRunContext(RipCurrentRunContext&& rhs) : error_message_(rhs.error_message_) {
    CURRENT_ASSERT(!rhs.sync_called_);
    rhs.sync_called_ = true;
  }
  void Sync() {
    CURRENT_ASSERT(!sync_called_);
    sync_called_ = true;
  }
  ~RipCurrentRunContext() {
    if (!sync_called_) {
      current::Singleton<RipCurrentMockableErrorHandler>().HandleError(error_message_);  // LCOV_EXCL_LINE
    }
  }

 private:
  bool sync_called_ = false;
  std::string error_message_;
};

template <class LHS_TYPELIST, class RHS_TYPELIST>
class InstanceBeingRun;

template <class... LHS_TYPES, class... RHS_TYPES>
class InstanceBeingRun<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>> : public EntriesConsumer<LHS<LHS_TYPES...>> {
 public:
  virtual ~InstanceBeingRun() = default;
};

// Template logic to wrap the above implementations into abstract classes of proper templated types.
template <class LHS_TYPELIST, class RHS_TYPELIST>
class AbstractCurrent;

template <typename... LHS_TYPES, typename... RHS_TYPES>
class AbstractCurrent<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>>
    : public SharedUniqueDefinition<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>> {
 public:
  struct Traits final {
    using input_t = LHS<LHS_TYPES...>;
    using output_t = RHS<RHS_TYPES...>;
  };

  using definition_t = SharedUniqueDefinition<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>>;

  explicit AbstractCurrent(definition_t definition) : definition_t(definition) {}
  virtual ~AbstractCurrent() = default;
  virtual std::shared_ptr<InstanceBeingRun<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>>> SpawnAndRun(
      std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>>) const = 0;

  Traits UnderlyingType() const;  // Never called, used from `decltype()`.
};

// The implementation of the "Instantiated building block == shared_ptr<Abstract building block>" paradigm.
template <class LHS_TYPELIST, class RHS_TYPELIST>
class SharedCurrent;

template <class... LHS_TYPES, class... RHS_TYPES>
class SharedCurrent<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>>
    : public AbstractCurrent<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>> {
 public:
  using input_t = LHS<LHS_TYPES...>;
  using output_t = RHS<RHS_TYPES...>;
  using super_t = AbstractCurrent<input_t, output_t>;

  explicit SharedCurrent(std::shared_ptr<super_t> spawner)
      : super_t(spawner->GetUniqueDefinition()), shared_impl_spawner_(spawner) {}

  std::shared_ptr<InstanceBeingRun<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>>> SpawnAndRun(
      std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>> next) const override {
    return shared_impl_spawner_->SpawnAndRun(next);
  }

  // User-facing `RipCurrent()` method.
  template <int IN_N = sizeof...(LHS_TYPES), int OUT_N = sizeof...(RHS_TYPES)>
  std::enable_if_t<IN_N == 0 && OUT_N == 0, RipCurrentRunContext> RipCurrent() {
    SpawnAndRun(std::make_shared<NoEntryShallPassFakeConsumer>());

    // TODO(dkorolev): Return proper run context. So far, just require `.Sync()` to be called on it.
    std::ostringstream os;
    os << "RipCurrent run context was left hanging. Call `.Sync()`, or, well, something else. -- D.K.\n";
    super_t::GetDefinition().FullDescription(os);
    return std::move(RipCurrentRunContext(os.str()));
  }

 private:
  std::shared_ptr<super_t> shared_impl_spawner_;
};

// Helper code to initialize the next handler in the chain before the user code is constructed.
// TL;DR: The user should be able to use `emit()` from constructor, no strings attached. Thus,
// the destination for this `emit()` should be initialized before the user code is.
class NextHandlerContainerBase {
 public:
  virtual ~NextHandlerContainerBase() = default;
};

class NextHandlersCollection final {
 public:
  void Add(const NextHandlerContainerBase* key, GenericEntriesConsumer* value) {
    std::lock_guard<std::mutex> lock(mutex_);
    CURRENT_ASSERT(key);
    CURRENT_ASSERT(value);
    CURRENT_ASSERT(!map_.count(key));
    map_[key] = value;
  }
  void Remove(const NextHandlerContainerBase* key, GenericEntriesConsumer* value) {
    std::lock_guard<std::mutex> lock(mutex_);
    CURRENT_ASSERT(key);
    CURRENT_ASSERT(value);
    CURRENT_ASSERT(map_.count(key));
    CURRENT_ASSERT(map_[key] == value);
    map_.erase(key);
  }
  template <typename T>
  T* Get(const NextHandlerContainerBase* key) {
    std::lock_guard<std::mutex> lock(mutex_);
    CURRENT_ASSERT(key);
    CURRENT_ASSERT(map_.count(key));
    GenericEntriesConsumer* result = map_[key];
    T* result2 = dynamic_cast<T*>(result);
    CURRENT_ASSERT(result2);
    return result2;
  }

  class Scope final {
   public:
    explicit Scope(const NextHandlerContainerBase* key, GenericEntriesConsumer* value) : key(key), value(value) {
      Singleton<NextHandlersCollection>().Add(key, value);
    }
    ~Scope() { Singleton<NextHandlersCollection>().Remove(key, value); }

   private:
    const NextHandlerContainerBase* const key;
    GenericEntriesConsumer* const value;

    Scope() = delete;
    Scope(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope& operator=(Scope&&) = delete;
  };

 private:
  std::mutex mutex_;
  std::map<const NextHandlerContainerBase*, GenericEntriesConsumer*> map_;
};

template <class LHS_TYPELIST>
class NextHandlerContainer;

template <class... NEXT_TYPES>
class NextHandlerContainer<LHS<NEXT_TYPES...>> : public NextHandlerContainerBase {
 public:
  NextHandlerContainer() : next_handler_(Singleton<NextHandlersCollection>().template Get<next_handler_t>(this)) {}
  virtual ~NextHandlerContainer() = default;

 protected:
  template <typename T>
  std::enable_if_t<TypeListContains<TypeListImpl<NEXT_TYPES...>, current::decay<T>>::value> emit(const T& x) const {
    next_handler_->ConsumeEntry(x);
  }

 private:
  using next_handler_t = EntriesConsumer<LHS<NEXT_TYPES...>>;
  EntriesConsumer<LHS<NEXT_TYPES...>>* const next_handler_;
};

template <typename LHS_TYPELIST, typename RHS_TYPELIST, typename USER_CLASS>
class NextHandlerInitializer;

template <class... LHS_TYPES, class... RHS_TYPES, typename USER_CLASS>
class NextHandlerInitializer<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS> final {
 public:
  template <typename... ARGS>
  NextHandlerInitializer(std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>> next, ARGS&&... args)
      : scope_(&impl_, next.get()), impl_(std::forward<ARGS>(args)...) {}

  template <typename X>
  void operator()(const X& x) {
    impl_.f(x);
  }

  void operator()(const CurrentSuper&) {
    // Should define this method to make sure the RTTI call compiles.
    // Assuming type list magic is done right at compile time (TODO(dkorolev): !), it should never get called.
    CURRENT_ASSERT(false);
  }

  void Accept(const CurrentSuper& x) {
    current::rtti::RuntimeTypeListDispatcher<CurrentSuper, TypeListImpl<LHS_TYPES...>>::DispatchCall(x, *this);
  }

 private:
  const NextHandlersCollection::Scope scope_;
  USER_CLASS impl_;
};

// Base classes for user-defined code, for `is_base_of<>` `static_assert()`-s.
template <class LHS_TYPELIST, class RHS_TYPELIST>
class UserClassTopLevelBase;

template <class... LHS_TYPES, class... RHS_TYPES>
class UserClassTopLevelBase<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>> {};

template <class LHS_TYPELIST, class RHS_TYPELIST, typename USER_CLASS>
class UserClassBase;

template <class... LHS_TYPES, class... RHS_TYPES, typename USER_CLASS>
class UserClassBase<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS>
    : public UserClassTopLevelBase<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>>,
      public NextHandlerContainer<LHS<RHS_TYPES...>> {
 public:
  virtual ~UserClassBase() = default;
  using input_t = LHS<LHS_TYPES...>;
  using output_t = RHS<RHS_TYPES...>;
};

// Helper code to support the declaration and running of user-defined classes.
template <class LHS_TYPELIST, class RHS_TYPELIST, typename USER_CLASS>
class UserClassInstantiator;

template <typename... LHS_TYPES, typename... RHS_TYPES, typename USER_CLASS>
class UserClassInstantiator<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS>
    : public AbstractCurrent<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>> {
 public:
  static_assert(std::is_base_of<UserClassTopLevelBase<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>>, USER_CLASS>::value,
                "User class for RipCurrent data processor should use `RIPCURRENT_NODE()` + `RIPCURRENT_MACRO()`.");

  using input_t = LHS<LHS_TYPES...>;
  using output_t = RHS<RHS_TYPES...>;

  template <class ARGS_AS_TUPLE>
  UserClassInstantiator(Definition definition, ARGS_AS_TUPLE&& params)
      : AbstractCurrent<input_t, output_t>(definition),
        lazy_instance_(current::DelayedInstantiateWithExtraParameterFromTuple<
            NextHandlerInitializer<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS>,
            std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>>>(std::forward<ARGS_AS_TUPLE>(params))) {}

  class Instance : public InstanceBeingRun<input_t, output_t> {
   public:
    virtual ~Instance() = default;

    explicit Instance(
        const current::LazilyInstantiated<NextHandlerInitializer<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS>,
                                          std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>>>& lazy_instance,
        std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>> next)
        : next_(next), spawned_user_class_instance_(lazy_instance.InstantiateAsUniquePtrWithExtraParameter(next_)) {}

    void ConsumeEntry(const CurrentSuper& x) override { spawned_user_class_instance_->Accept(x); }

   private:
    std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>> next_;
    std::unique_ptr<NextHandlerInitializer<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS>>
        spawned_user_class_instance_;
  };

  std::shared_ptr<InstanceBeingRun<input_t, output_t>> SpawnAndRun(
      std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>> next) const override {
    return std::make_shared<Instance>(lazy_instance_, next);
  }

 private:
  current::LazilyInstantiated<NextHandlerInitializer<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS>,
                              std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>>> lazy_instance_;
};

// `SharedUserClassInstantiator` is the `shared_ptr<>` holder of the wrapper class
// containing initialization parameters for the user class.
template <class LHS_TYPELIST, class RHS_TYPELIST, typename USER_CLASS>
class UserClass;

template <class LHS_TYPELIST, class RHS_TYPELIST, typename USER_CLASS>
class SharedUserClassInstantiator;

template <typename... LHS_TYPES, typename... RHS_TYPES, typename USER_CLASS>
class SharedUserClassInstantiator<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS> {
 public:
  typedef UserClassInstantiator<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS> impl_t;
  template <class ARGS_AS_TUPLE>
  SharedUserClassInstantiator(Definition definition, ARGS_AS_TUPLE&& params)
      : shared_spawner_(std::make_shared<impl_t>(definition, std::forward<ARGS_AS_TUPLE>(params))) {}

 private:
  friend class UserClass<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS>;
  std::shared_ptr<impl_t> shared_spawner_;
};

// `UserClass<LHS<...>, RHS<...>, IMPL>` initializes `SharedUserClassInstantiator<LHS<...>, RHS<...>, IMPL>`
// before constructing the parent `SharedCurrent<LHS<...>, RHS<...>>` object, thus allowing
// the latter to reuse the `shared_ptr<>` containing the user code constructed in the former.
template <class LHS_TYPELIST, class RHS_TYPELIST, typename USER_CLASS>
class UserClass;

template <typename... LHS_TYPES, typename... RHS_TYPES, typename USER_CLASS>
class UserClass<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS>
    : public SharedUserClassInstantiator<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS>,
      public SharedCurrent<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>> {
 public:
  typedef SharedUserClassInstantiator<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, USER_CLASS> impl_t;
  template <class ARGS_AS_TUPLE>
  UserClass(Definition definition, ARGS_AS_TUPLE&& params)
      : impl_t(definition, std::forward<ARGS_AS_TUPLE>(params)),
        SharedCurrent<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>>(impl_t::shared_spawner_) {}

  // For the `RIPCURRENT_UNDERLYING_TYPE` macro to be able to extract the underlying user-defined type.
  USER_CLASS UnderlyingType() const;
};

// The implementation of the `A | B` combiner building block.
template <class LHS_TYPELIST, class RHS_TYPELIST, class VIA_TYPELIST>
class AbstractCurrentSequence;

template <typename LHS_TYPELIST, typename RHS_TYPELIST, typename VIA_X, typename... VIA_XS>
class AbstractCurrentSequence<LHS_TYPELIST, RHS_TYPELIST, VIA<VIA_X, VIA_XS...>>
    : public AbstractCurrent<LHS_TYPELIST, RHS_TYPELIST> {
 public:
  AbstractCurrentSequence(SharedCurrent<LHS_TYPELIST, RHS<VIA_X, VIA_XS...>> from,
                          SharedCurrent<LHS<VIA_X, VIA_XS...>, RHS_TYPELIST> into)
      : AbstractCurrent<LHS_TYPELIST, RHS_TYPELIST>(
            Definition(Definition::Pipe(), from.GetDefinition(), into.GetDefinition())),
        from_(from),
        into_(into) {
    from.MarkAs(BlockUsageBit::UsedInLargerBlock);
    into.MarkAs(BlockUsageBit::UsedInLargerBlock);
  }

 protected:
  const SharedCurrent<LHS_TYPELIST, RHS<VIA_X, VIA_XS...>>& From() const { return from_; }
  const SharedCurrent<LHS<VIA_X, VIA_XS...>, RHS_TYPELIST>& Into() const { return into_; }

 private:
  SharedCurrent<LHS_TYPELIST, RHS<VIA_X, VIA_XS...>> from_;
  SharedCurrent<LHS<VIA_X, VIA_XS...>, RHS_TYPELIST> into_;
};

template <class LHS_TYPELIST, class RHS_TYPELIST, class VIA_TYPELIST>
class GenericCurrentSequence;

template <class... LHS_TYPES, class... RHS_TYPES, typename VIA_X, typename... VIA_XS>
class GenericCurrentSequence<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, VIA<VIA_X, VIA_XS...>>
    : public AbstractCurrentSequence<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, VIA<VIA_X, VIA_XS...>> {
 public:
  GenericCurrentSequence(SharedCurrent<LHS<LHS_TYPES...>, RHS<VIA_X, VIA_XS...>> from,
                         SharedCurrent<LHS<VIA_X, VIA_XS...>, RHS<RHS_TYPES...>> into)
      : AbstractCurrentSequence<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>, VIA<VIA_X, VIA_XS...>>(from, into) {}

  class Instance : public InstanceBeingRun<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>> {
   public:
    virtual ~Instance() = default;

    Instance(const GenericCurrentSequence* self, std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>> next)
        : next_(next), into_(self->Into().SpawnAndRun(next_)), from_(self->From().SpawnAndRun(into_)) {
      self->MarkAs(BlockUsageBit::Run);
    }

    void ConsumeEntry(const CurrentSuper& x) override { from_->ConsumeEntry(x); }

   private:
    // Construction / destruction order matters: { next, into, from }.
    std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>> next_;
    std::shared_ptr<InstanceBeingRun<LHS<VIA_X, VIA_XS...>, RHS<RHS_TYPES...>>> into_;
    std::shared_ptr<InstanceBeingRun<LHS<LHS_TYPES...>, RHS<VIA_X, VIA_XS...>>> from_;
  };

  std::shared_ptr<InstanceBeingRun<LHS<LHS_TYPES...>, RHS<RHS_TYPES...>>> SpawnAndRun(
      std::shared_ptr<EntriesConsumer<LHS<RHS_TYPES...>>> next) const override {
    return std::make_shared<Instance>(this, next);
  }
};

// `SubflowSequence` requires `VIA_TYPELIST` to be a nonempty type list.
// This is enforced by the `operator|()` declaration below.
template <class LHS_TYPELIST, class RHS_TYPELIST, class VIA_TYPELIST>
class SubflowSequence;

template <class LHS_TYPELIST, class RHS_TYPELIST, typename VIA_X, typename... VIA_XS>
class SubflowSequence<LHS_TYPELIST, RHS_TYPELIST, VIA<VIA_X, VIA_XS...>>
    : public SharedCurrent<LHS_TYPELIST, RHS_TYPELIST> {
 public:
  typedef SharedCurrent<LHS_TYPELIST, RHS_TYPELIST> subflow_t;
  SubflowSequence(SharedCurrent<LHS_TYPELIST, RHS<VIA_X, VIA_XS...>> from,
                  SharedCurrent<LHS<VIA_X, VIA_XS...>, RHS_TYPELIST> into)
      : subflow_t(
            std::make_shared<GenericCurrentSequence<LHS_TYPELIST, RHS_TYPELIST, VIA<VIA_X, VIA_XS...>>>(from, into)) {}
};

// SharedCurrent sequence combiner, `A | B`.
template <class LHS_TYPELIST, class RHS_TYPELIST, typename VIA_X, typename... VIA_XS>
SharedCurrent<LHS_TYPELIST, RHS_TYPELIST> operator|(SharedCurrent<LHS_TYPELIST, RHS<VIA_X, VIA_XS...>> from,
                                                    SharedCurrent<LHS<VIA_X, VIA_XS...>, RHS_TYPELIST> into) {
  return SubflowSequence<LHS_TYPELIST, RHS_TYPELIST, VIA<VIA_X, VIA_XS...>>(from, into);
}

}  // namespace current::ripcurrent

// Macros to wrap user code into RipCurrent building blocks.

#define RIPCURRENT_NODE(USER_CLASS, LHS_TYPELIST, RHS_TYPELIST)                                            \
  struct USER_CLASS##_RIPCURRENT_CLASS_NAME {                                                              \
    static const char* RIPCURRENT_CLASS_NAME() { return #USER_CLASS; }                                     \
  };                                                                                                       \
  struct USER_CLASS final : USER_CLASS##_RIPCURRENT_CLASS_NAME,                                            \
                            ::ripcurrent::UserClassBase<ripcurrent::LHS<REMOVE_PARENTHESES(LHS_TYPELIST)>, \
                                                        ripcurrent::RHS<REMOVE_PARENTHESES(RHS_TYPELIST)>, \
                                                        USER_CLASS>

#define RIPCURRENT_MACRO(USER_CLASS, ...)                                                                    \
  ::current::ripcurrent::UserClass<typename USER_CLASS::input_t, typename USER_CLASS::output_t, USER_CLASS>( \
      ::current::ripcurrent::Definition(#USER_CLASS "(" #__VA_ARGS__ ")", __FILE__, __LINE__),               \
      std::make_tuple(__VA_ARGS__))

// A helper macro to extract the underlying type of the user class, now registered as a RipCurrent block type.
#define RIPCURRENT_UNDERLYING_TYPE(USER_CLASS) decltype(USER_CLASS.UnderlyingType())

}  // namespace current

// Expose `ripcurrent::{LHS,RHS}` into a global `RipCurrent` namespace.
namespace ripcurrent {
template <class LHS_TYPELIST, class RHS_TYPELIST, typename USER_CLASS>
using UserClassBase = current::ripcurrent::UserClassBase<LHS_TYPELIST, RHS_TYPELIST, USER_CLASS>;
template <typename... TS>
using LHS = current::ripcurrent::LHS<TS...>;
template <typename... TS>
using RHS = current::ripcurrent::RHS<TS...>;
}  // namespace ripcurrent

// These typedef are the types the user can directly operate with.
// All of them can be liberally copied over, since the logic is concealed within the inner `shared_ptr<>`.
template <typename RHS_TYPELIST>
using RipCurrentLHS = current::ripcurrent::SharedCurrent<ripcurrent::LHS<>, RHS_TYPELIST>;

template <typename LHS_TYPELIST, typename RHS_TYPELIST>
using RipCurrentVIA = current::ripcurrent::SharedCurrent<LHS_TYPELIST, RHS_TYPELIST>;

template <typename LHS_TYPELIST>
using RipCurrentRHS = current::ripcurrent::SharedCurrent<LHS_TYPELIST, ripcurrent::RHS<>>;

using RipCurrent = current::ripcurrent::SharedCurrent<ripcurrent::LHS<>, ripcurrent::RHS<>>;

#endif  // CURRENT_RIPCURRENT_H
