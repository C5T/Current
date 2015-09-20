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

#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <tuple>

#include "../Bricks/template/typelist.h"
#include "../Bricks/util/lazy_instantiation.h"
#include "../Bricks/util/singleton.h"

namespace ripcurrent {

// A base class for all entries passing through the building blocks.
// TODO(dkorolev): They will have to be serializable for MMQ-based implemetations. Padawan?
/*
struct H2O {
  virtual ~H2O() = default;
};
*/
typedef int H2O;

// A singleton wrapping error handling logic, to allow mocking for the unit test.
class RipCurrentMockableErrorHandler {
 public:
  typedef std::function<void(const std::string& error_message)> handler_type;

  RipCurrentMockableErrorHandler()
      : handler_([](const std::string& error_message) {
          std::cerr << error_message;
          std::exit(-1);
        }) {}

  void HandleError(const std::string& error_message) const { handler_(error_message); }

  void SetHandler(handler_type f) { handler_ = f; }
  handler_type GetHandler() const { return handler_; }

  class InjectedHandlerScope {
   public:
    explicit InjectedHandlerScope(RipCurrentMockableErrorHandler* parent, handler_type handler)
        : parent_(parent), save_handler_(parent->GetHandler()) {
      parent_->SetHandler(handler);
    }
    ~InjectedHandlerScope() { parent_->SetHandler(save_handler_); }

   private:
    RipCurrentMockableErrorHandler* parent_;
    handler_type save_handler_;
  };

  InjectedHandlerScope ScopedInjectHandler(handler_type injected_handler) {
    return InjectedHandlerScope(this, injected_handler);
  }

 private:
  handler_type handler_;
};

// Human-readable symbols for template parameters for one of four open-vs-closed-ended blocks.
enum class InputPolicy { DoesNotAccept = 0, Accepts = 1 };
enum class OutputPolicy { DoesNotEmit = 0, Emits = 1 };

// The wrapper to store `__FILE__` and `__LINE__` for human-readable verbose explanations.
struct FileLine {
  const char* const file;
  const size_t line;
};

// `Definition` traces down to the original statement in user code that defined a particular building block.
// Used for diagnostic messages in runtime errors when some Current building block was left hanging.
struct Definition {
  struct Pipe {};
  std::string statement;
  std::vector<std::pair<std::string, FileLine>> sources;
  static std::vector<std::pair<std::string, FileLine>> CombineSources(const Definition& a,
                                                                      const Definition& b) {
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
};

// `UniqueDefinition` is a `Definition` that:
// * Templated with `InputPolicy` and `OutputPolicy` for strict typing.
// * Exposes user-friendly decorators.
// * Can be marked as used in various ways.
// * Generates an error upon destructing if it was not marked as used in at least one way.
enum class BlockUsageBit : size_t { Described = 0, Run = 1, UsedInLargerBlock = 2, Dismissed = 3 };
enum class BlockUsageBitMask : size_t { Unused = 0 };

template <InputPolicy INPUT, OutputPolicy OUTPUT>
class UniqueDefinition final : public Definition {
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
    os << (INPUT == InputPolicy::Accepts ? "... | " : "");
    os << statement;
    os << (OUTPUT == OutputPolicy::Emits ? " | ..." : "");
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

      bricks::Singleton<RipCurrentMockableErrorHandler>().HandleError(os.str());
    }
  }

 private:
  BlockUsageBitMask mask_ = BlockUsageBitMask::Unused;
};

// `SharedUniqueDefinition` is a helper class for an `std::shared_ptr<UniqueDefinition>`.
// The premise is that building blocks that should be used or run can be liberally copied over,
// with the test that they have been used or run performed at the very end of their lifetime.
template <InputPolicy INPUT, OutputPolicy OUTPUT>
class SharedUniqueDefinition {
 public:
  typedef UniqueDefinition<INPUT, OUTPUT> T_IMPL;

  SharedUniqueDefinition(Definition definition) : unique_definition_(std::make_shared<T_IMPL>(definition)) {}
  void MarkAs(BlockUsageBit bit) const { unique_definition_->MarkAs(bit); }

  // User-facing methods.
  std::string Describe() const {
    MarkAs(BlockUsageBit::Described);
    return (INPUT == InputPolicy::Accepts ? "... | " : "") + unique_definition_->statement +
           (OUTPUT == OutputPolicy::Emits ? " | ..." : "");
  }

  void Dismiss() const { MarkAs(BlockUsageBit::Dismissed); }

  // For expressive initialized lists of shared instances.
  const SharedUniqueDefinition& GetUniqueDefinition() const { return *this; }
  const UniqueDefinition<INPUT, OUTPUT>& GetDefinition() const { return *unique_definition_.get(); }

 private:
  mutable std::shared_ptr<T_IMPL> unique_definition_;
};

template <OutputPolicy>
struct InputPolicyMatchingOutputPolicy;
template <>
struct InputPolicyMatchingOutputPolicy<OutputPolicy::DoesNotEmit> {
  constexpr static InputPolicy RESULT = InputPolicy::DoesNotAccept;
};
template <>
struct InputPolicyMatchingOutputPolicy<OutputPolicy::Emits> {
  constexpr static InputPolicy RESULT = InputPolicy::Accepts;
};

// `DummyNoEntryType` and `ActualOrDummyEntryType<InputPolicy::*>` are to unify templates later on.
// `DummyNoEntryType` is the type of the entry that will never be emitted, because the block
// it is being "sent" to does not accept incoming entries.
struct DummyNoEntryType;

template <InputPolicy>
struct ActualOrDummyEntryTypeImpl;

template <>
struct ActualOrDummyEntryTypeImpl<InputPolicy::DoesNotAccept> {
  typedef DummyNoEntryType type;
};

template <>
struct ActualOrDummyEntryTypeImpl<InputPolicy::Accepts> {
  typedef const H2O& type;
};

template <InputPolicy INPUT>
using ActualOrDummyEntryType = typename ActualOrDummyEntryTypeImpl<INPUT>::type;

template <InputPolicy INPUT>
class EntriesConsumer {
 public:
  virtual ~EntriesConsumer() = default;
  virtual void Accept(const ActualOrDummyEntryType<INPUT>&) = 0;
};

class DummyNoEntryTypeAcceptor : public EntriesConsumer<InputPolicy::DoesNotAccept> {
 public:
  virtual ~DummyNoEntryTypeAcceptor() = default;
  void Accept(const DummyNoEntryType&) override {
    bricks::Singleton<RipCurrentMockableErrorHandler>().HandleError(
        "Internal error: Attepmted to send entries into a non-accepting block.");
  }
};

// Run context for RipCurrent allows to run it in background, foreground, or scoped.
// TODO(dkorolev): Only supports `.Sync()` now. Implement everything else.
class RipCurrentRunContext {
 public:
  explicit RipCurrentRunContext(const std::string& error_message)
      : sync_called_(false), error_message_(error_message) {}
  RipCurrentRunContext(RipCurrentRunContext&& rhs) : error_message_(rhs.error_message_) {
    assert(!rhs.sync_called_);
    rhs.sync_called_ = true;
  }
  void Sync() {
    assert(!sync_called_);
    sync_called_ = true;
  }
  ~RipCurrentRunContext() {
    if (!sync_called_) {
      bricks::Singleton<RipCurrentMockableErrorHandler>().HandleError(error_message_);
    }
  }

 private:
  bool sync_called_ = false;
  std::string error_message_;
};

template <InputPolicy INPUT, OutputPolicy OUTPUT>
class InstanceBeingRun : public EntriesConsumer<INPUT> {
 public:
  virtual ~InstanceBeingRun() = default;
};

// Template logic to wrap the above implementations into abstract classes of proper templated types.
template <InputPolicy INPUT, OutputPolicy OUTPUT>
class AbstractCurrent : public SharedUniqueDefinition<INPUT, OUTPUT> {
 public:
  typedef SharedUniqueDefinition<INPUT, OUTPUT> T_DEFINITION;
  constexpr static InputPolicy NEXT_INPUT = InputPolicyMatchingOutputPolicy<OUTPUT>::RESULT;

  explicit AbstractCurrent(T_DEFINITION definition) : T_DEFINITION(definition) {}
  virtual ~AbstractCurrent() = default;
  virtual std::shared_ptr<InstanceBeingRun<INPUT, OUTPUT>> SpawnAndRun(
      std::shared_ptr<EntriesConsumer<NEXT_INPUT>>) const = 0;

  struct Traits {
    constexpr static InputPolicy INPUT_POLICY = INPUT;
    constexpr static OutputPolicy OUTPUT_POLICY = OUTPUT;
  };

  Traits UnderlyingType() const;  // Never called, used from `decltype()`.
};

// The implementation of the "Instantiated building block == shared_ptr<Abstract building block>" paradigm.
template <InputPolicy INPUT, OutputPolicy OUTPUT>
class SharedCurrent : public AbstractCurrent<INPUT, OUTPUT> {
 public:
  typedef AbstractCurrent<INPUT, OUTPUT> T_BASE;
  constexpr static InputPolicy NEXT_INPUT = InputPolicyMatchingOutputPolicy<OUTPUT>::RESULT;

  explicit SharedCurrent(std::shared_ptr<T_BASE> spawner)
      : T_BASE(spawner->GetUniqueDefinition()), shared_impl_spawner_(spawner) {}

  std::shared_ptr<InstanceBeingRun<INPUT, OUTPUT>> SpawnAndRun(
      std::shared_ptr<EntriesConsumer<NEXT_INPUT>> next) const override {
    return shared_impl_spawner_->SpawnAndRun(next);
  }

  // User-facing `RipCurrent()` method.
  template <InputPolicy IN = INPUT, OutputPolicy OUT = OUTPUT>
  typename std::enable_if<IN == InputPolicy::DoesNotAccept && OUT == OutputPolicy::DoesNotEmit,
                          RipCurrentRunContext>::type
  RipCurrent() {
    SpawnAndRun(std::make_shared<DummyNoEntryTypeAcceptor>());

    // TODO(dkorolev): Return proper run context. So far, just require `.Sync()` to be called on it.
    std::ostringstream os;
    os << "RipCurrent run context was left hanging. Call `.Sync()`, or, well, something else. -- D.K.\n";
    T_BASE::GetDefinition().FullDescription(os);
    return std::move(RipCurrentRunContext(os.str()));
  }

 private:
  std::shared_ptr<T_BASE> shared_impl_spawner_;
};

// Helper code to initialize the next handler in the chain before the user code is constructed.
// TL;DR: The user should be able to use `emit()` from constructor, no strings attached. Thus,
// the destination for this `emit()` should be initialized before the user code is.
template <InputPolicy INPUT>
class NextHandlerContainer {
 public:
  virtual ~NextHandlerContainer() = default;

  void SetNextHandler(std::shared_ptr<EntriesConsumer<INPUT>> next) const { next_handler_ = next.get(); }

 protected:
  void emit(const H2O& x) const { next_handler_->Accept(x); }

 private:
  // NOTE(dkorolev): This field is pre-initialized by an instance of another helper class, which is
  // constructed before the constuctor of the user code, derived indirectly from `NextHandlerContainer`,
  // is run. This is done to enable user code to do `emit()` from within the constructor, or from callbacks
  // initialized from within the constructor (for example, HTTP endpoints).
  // Since such an initialization happens before the constructor has been properly called,
  // the data field it has to be a POD. An `std::shared_ptr<>` would not work due to being uninitialized first
  // (and re-initialized over later).
  // Thus, we use a bare pointer here. Note that the `Instance` classes, that ultimately instantiate user code,
  // do wrap the top-level `next` into its own `std::shared_ptr<>` and make sure to construct it before (and
  // destruct it after) the user code itself.
  mutable EntriesConsumer<INPUT>* next_handler_;
};

template <InputPolicy INPUT, typename T>
class NextHandlerInitializer {
 public:
  template <typename... ARGS>
  NextHandlerInitializer(std::shared_ptr<EntriesConsumer<INPUT>> next, ARGS&&... args)
      : early_initializer_(impl_, next), impl_(std::forward<ARGS>(args)...) {}

  // TODO(dkorolev): Here be RTTI, per type than can be emitted.
  void Accept(const H2O& x) { impl_.f(x); }
  void Accept(const DummyNoEntryType&) {}

 private:
  struct NextHandlerEarlyInitializer {
    NextHandlerEarlyInitializer(NextHandlerContainer<INPUT>& instance_to_initialize,
                                std::shared_ptr<EntriesConsumer<INPUT>> next) {
      instance_to_initialize.SetNextHandler(next);
    }
  };

  NextHandlerEarlyInitializer early_initializer_;
  T impl_;
};

// Base classes for user-defined code, for `is_base_of<>` `static_assert()`-s.
template <OutputPolicy, typename EMITTABLE_TYPES_AS_TYPELIST>
struct ConfirmRHSDoesNotEmit;

template <>
struct ConfirmRHSDoesNotEmit<OutputPolicy::DoesNotEmit, TypeListImpl<>> {};

template <typename T, typename... TS>
struct ConfirmRHSDoesNotEmit<OutputPolicy::Emits, TypeListImpl<T, TS...>> {};

template <InputPolicy INPUT, OutputPolicy OUTPUT>
class UserClassTopLevelBase {};

template <InputPolicy INPUT, OutputPolicy OUTPUT, typename USER_CLASS, typename EMITTABLE_TYPES_AS_TYPELIST>
class UserClassBase : public UserClassTopLevelBase<INPUT, OUTPUT>,
                      public NextHandlerContainer<InputPolicyMatchingOutputPolicy<OUTPUT>::RESULT>,
                      public ConfirmRHSDoesNotEmit<OUTPUT, EMITTABLE_TYPES_AS_TYPELIST> {
 public:
  virtual ~UserClassBase() = default;
  constexpr static InputPolicy INPUT_POLICY = INPUT;
  constexpr static OutputPolicy OUTPUT_POLICY = OUTPUT;
};

// Helper code to support the declaration and running of user-defined classes.
template <InputPolicy INPUT, OutputPolicy OUTPUT, typename USER_CLASS>
class GenericUserClassInstantiator : public AbstractCurrent<INPUT, OUTPUT> {
 public:
  constexpr static InputPolicy NEXT_INPUT = InputPolicyMatchingOutputPolicy<OUTPUT>::RESULT;

  template <class ARGS_AS_TUPLE>
  GenericUserClassInstantiator(Definition definition, ARGS_AS_TUPLE&& params)
      : AbstractCurrent<INPUT, OUTPUT>(definition),
        lazy_instance_(bricks::DelayedInstantiateWithExtraParameterFromTuple<
            NextHandlerInitializer<NEXT_INPUT, USER_CLASS>,
            std::shared_ptr<EntriesConsumer<NEXT_INPUT>>>(std::forward<ARGS_AS_TUPLE>(params))) {}

  class Instance : public InstanceBeingRun<INPUT, OUTPUT> {
   public:
    virtual ~Instance() = default;

    explicit Instance(
        const bricks::LazilyInstantiated<NextHandlerInitializer<NEXT_INPUT, USER_CLASS>,
                                         std::shared_ptr<EntriesConsumer<NEXT_INPUT>>>& lazy_instance,
        std::shared_ptr<EntriesConsumer<NEXT_INPUT>> next)
        : next_(next),
          spawned_user_class_instance_(lazy_instance.InstantiateAsUniquePtrWithExtraParameter(
              std::shared_ptr<EntriesConsumer<NEXT_INPUT>>(next_))) {}

    void Accept(const ActualOrDummyEntryType<INPUT>& x) override { spawned_user_class_instance_->Accept(x); }

   private:
    std::shared_ptr<EntriesConsumer<NEXT_INPUT>> next_;
    std::unique_ptr<NextHandlerInitializer<NEXT_INPUT, USER_CLASS>> spawned_user_class_instance_;
  };

  std::shared_ptr<InstanceBeingRun<INPUT, OUTPUT>> SpawnAndRun(
      std::shared_ptr<EntriesConsumer<NEXT_INPUT>> next) const override {
    return std::make_shared<Instance>(lazy_instance_, next);
  }

 private:
  bricks::LazilyInstantiated<NextHandlerInitializer<NEXT_INPUT, USER_CLASS>,
                             std::shared_ptr<EntriesConsumer<NEXT_INPUT>>> lazy_instance_;
};

template <InputPolicy INPUT, OutputPolicy OUTPUT, typename USER_CLASS>
class UserClassInstantiator;

// Top-level class to wrap user-defined "LHS" code into a corresponding subflow.
template <typename USER_CLASS>
class UserClassInstantiator<InputPolicy::DoesNotAccept, OutputPolicy::Emits, USER_CLASS>
    : public GenericUserClassInstantiator<InputPolicy::DoesNotAccept, OutputPolicy::Emits, USER_CLASS> {
 private:
  static_assert(std::is_base_of<UserClassTopLevelBase<InputPolicy::DoesNotAccept, OutputPolicy::Emits>,
                                USER_CLASS>::value,
                "User class for RipCurrent data origin should use `CURRENT_LHS()` + `REGISTER_LHS()`.");

 public:
  typedef GenericUserClassInstantiator<InputPolicy::DoesNotAccept, OutputPolicy::Emits, USER_CLASS> T_BASE;
  template <class ARGS_AS_TUPLE>
  UserClassInstantiator(Definition definition, ARGS_AS_TUPLE&& params)
      : T_BASE(definition, std::forward<ARGS_AS_TUPLE>(params)) {}
};

// Top-level class to wrap user-defined "VIA" code into a corresponding subflow.
template <typename USER_CLASS>
class UserClassInstantiator<InputPolicy::Accepts, OutputPolicy::Emits, USER_CLASS>
    : public GenericUserClassInstantiator<InputPolicy::Accepts, OutputPolicy::Emits, USER_CLASS> {
 private:
  static_assert(
      std::is_base_of<UserClassTopLevelBase<InputPolicy::Accepts, OutputPolicy::Emits>, USER_CLASS>::value,
      "User class for RipCurrent data processor should use `CURRENT_VIA()` + `REGISTER_VIA()`.");

 public:
  typedef GenericUserClassInstantiator<InputPolicy::Accepts, OutputPolicy::Emits, USER_CLASS> T_BASE;
  template <class ARGS_AS_TUPLE>
  UserClassInstantiator(Definition definition, ARGS_AS_TUPLE&& params)
      : T_BASE(definition, std::forward<ARGS_AS_TUPLE>(params)) {}
};

// Top-level class to wrap user-defined "RHS" code into a corresponding subflow.
template <typename USER_CLASS>
class UserClassInstantiator<InputPolicy::Accepts, OutputPolicy::DoesNotEmit, USER_CLASS>
    : public GenericUserClassInstantiator<InputPolicy::Accepts, OutputPolicy::DoesNotEmit, USER_CLASS> {
 private:
  static_assert(std::is_base_of<UserClassTopLevelBase<InputPolicy::Accepts, OutputPolicy::DoesNotEmit>,
                                USER_CLASS>::value,
                "User class for RipCurrent data destination should use `CURRENT_RHS()` + `REGISTER_RHS()`.");

 public:
  typedef GenericUserClassInstantiator<InputPolicy::Accepts, OutputPolicy::DoesNotEmit, USER_CLASS> T_BASE;
  template <class ARGS_AS_TUPLE>
  UserClassInstantiator(Definition definition, ARGS_AS_TUPLE&& params)
      : T_BASE(definition, std::forward<ARGS_AS_TUPLE>(params)) {}
};

// `SharedUserClassInstantiator` is the `shared_ptr<>` holder of the wrapper class
// containing initialization parameters for the user class.
template <InputPolicy INPUT, OutputPolicy OUTPUT, typename USER_CLASS>
class UserClass;

template <InputPolicy INPUT, OutputPolicy OUTPUT, typename USER_CLASS>
class SharedUserClassInstantiator {
 public:
  typedef UserClassInstantiator<INPUT, OUTPUT, USER_CLASS> T_IMPL;
  template <class ARGS_AS_TUPLE>
  SharedUserClassInstantiator(Definition definition, ARGS_AS_TUPLE&& params)
      : shared_spawner_(std::make_shared<T_IMPL>(definition, std::forward<ARGS_AS_TUPLE>(params))) {}

 private:
  friend class UserClass<INPUT, OUTPUT, USER_CLASS>;
  std::shared_ptr<T_IMPL> shared_spawner_;
};

// `UserClass<INPUT, OUTPUT, USER_CLASS>` initializes `SharedUserClassInstantiator<INPUT, OUTPUT, USER_CLASS>`
// before constructing the parent `SharedCurrent<INPUT, OUTPUT>` object, thus allowing
// the latter to reuse the `shared_ptr<>` containing the user code constructed in the former.
template <InputPolicy INPUT, OutputPolicy OUTPUT, typename USER_CLASS>
class UserClass : public SharedUserClassInstantiator<INPUT, OUTPUT, USER_CLASS>,
                  public SharedCurrent<INPUT, OUTPUT> {
 public:
  typedef SharedUserClassInstantiator<INPUT, OUTPUT, USER_CLASS> T_IMPL;
  template <class ARGS_AS_TUPLE>
  UserClass(Definition definition, ARGS_AS_TUPLE&& params)
      : T_IMPL(definition, std::forward<ARGS_AS_TUPLE>(params)),
        SharedCurrent<INPUT, OUTPUT>(T_IMPL::shared_spawner_) {}

  // For the `CURRENT_USER_TYPE` macro to be able to extract the underlying user-defined type.
  USER_CLASS UnderlyingType() const;
};

// The implementation of the `A | B` combiner building block.
template <InputPolicy INPUT, OutputPolicy OUTPUT>
class AbstractCurrentSequence : public AbstractCurrent<INPUT, OUTPUT> {
 public:
  AbstractCurrentSequence(SharedCurrent<INPUT, OutputPolicy::Emits> from,
                          SharedCurrent<InputPolicy::Accepts, OUTPUT> into)
      : AbstractCurrent<INPUT, OUTPUT>(
            Definition(Definition::Pipe(), from.GetDefinition(), into.GetDefinition())),
        from_(from),
        into_(into) {
    from.MarkAs(BlockUsageBit::UsedInLargerBlock);
    into.MarkAs(BlockUsageBit::UsedInLargerBlock);
  }

 protected:
  const SharedCurrent<INPUT, OutputPolicy::Emits>& From() const { return from_; }
  const SharedCurrent<InputPolicy::Accepts, OUTPUT>& Into() const { return into_; }

 private:
  SharedCurrent<INPUT, OutputPolicy::Emits> from_;
  SharedCurrent<InputPolicy::Accepts, OUTPUT> into_;
};

template <InputPolicy INPUT, OutputPolicy OUTPUT>
class GenericCurrentSequence : public AbstractCurrentSequence<INPUT, OUTPUT> {
 public:
  constexpr static InputPolicy NEXT_INPUT = InputPolicyMatchingOutputPolicy<OUTPUT>::RESULT;

  GenericCurrentSequence(SharedCurrent<INPUT, OutputPolicy::Emits> from,
                         SharedCurrent<InputPolicy::Accepts, OUTPUT> into)
      : AbstractCurrentSequence<INPUT, OUTPUT>(from, into) {}

  class Instance : public InstanceBeingRun<INPUT, OUTPUT> {
   public:
    virtual ~Instance() = default;

    Instance(const GenericCurrentSequence* self, std::shared_ptr<EntriesConsumer<NEXT_INPUT>> next)
        : next_(next), into_(self->Into().SpawnAndRun(next_)), from_(self->From().SpawnAndRun(into_)) {
      self->MarkAs(BlockUsageBit::Run);
    }

    // Can be either a real entry or a dummy type. The former will be handled, the latter will be ignored.
    void Accept(const ActualOrDummyEntryType<INPUT>& x) override { from_->Accept(x); }

   private:
    // Construction / destruction order matters: { next, into, from }.
    std::shared_ptr<EntriesConsumer<NEXT_INPUT>> next_;
    std::shared_ptr<InstanceBeingRun<InputPolicy::Accepts, OUTPUT>> into_;
    std::shared_ptr<InstanceBeingRun<INPUT, OutputPolicy::Emits>> from_;
  };

  std::shared_ptr<InstanceBeingRun<INPUT, OUTPUT>> SpawnAndRun(
      std::shared_ptr<EntriesConsumer<NEXT_INPUT>> next) const override {
    return std::make_shared<Instance>(this, next);
  }
};

template <InputPolicy INPUT, OutputPolicy OUTPUT>
class SubflowSequence : public SharedCurrent<INPUT, OUTPUT> {
 public:
  typedef SharedCurrent<INPUT, OUTPUT> T_SUBFLOW;
  SubflowSequence(SharedCurrent<INPUT, OutputPolicy::Emits> from,
                  SharedCurrent<InputPolicy::Accepts, OUTPUT> into)
      : T_SUBFLOW(std::make_shared<GenericCurrentSequence<INPUT, OUTPUT>>(from, into)) {}
};

// SharedCurrent sequence combiner, `A | B`.
template <InputPolicy INPUT, OutputPolicy OUTPUT>
SharedCurrent<INPUT, OUTPUT> operator|(SharedCurrent<INPUT, OutputPolicy::Emits> from,
                                       SharedCurrent<InputPolicy::Accepts, OUTPUT> into) {
  return SubflowSequence<INPUT, OUTPUT>(from, into);
}

}  // namespace ripcurrent

// Macros to wrap user code into RipCurrent building blocks.

// Define a user class ready to be used as part of RipCurrent data pipeline.
#define CURRENT_LHS(T, ...) \
  struct T final            \
      : ripcurrent::UserClassBase<InputPolicy::DoesNotAccept, OutputPolicy::Emits, T, TypeList<__VA_ARGS__>>
#define CURRENT_RHS(T, ...) \
  struct T final            \
      : ripcurrent::UserClassBase<InputPolicy::Accepts, OutputPolicy::DoesNotEmit, T, TypeList<__VA_ARGS__>>
#define CURRENT_VIA(T, ...) \
  struct T final            \
      : ripcurrent::UserClassBase<InputPolicy::Accepts, OutputPolicy::Emits, T, TypeList<__VA_ARGS__>>

// Declare a user class as a source of data entries.
#define REGISTER_LHS(T, ...)                                                 \
  ripcurrent::UserClass<InputPolicy::DoesNotAccept, OutputPolicy::Emits, T>( \
      ripcurrent::Definition(#T "(" #__VA_ARGS__ ")", __FILE__, __LINE__), std::make_tuple(__VA_ARGS__))

// Declare a user class as a destination of data entries.
#define REGISTER_RHS(T, ...)                                                 \
  ripcurrent::UserClass<InputPolicy::Accepts, OutputPolicy::DoesNotEmit, T>( \
      ripcurrent::Definition(#T "(" #__VA_ARGS__ ")", __FILE__, __LINE__), std::make_tuple(__VA_ARGS__))

// Declare a user class as a processor of data entries.
#define REGISTER_VIA(T, ...)                                           \
  ripcurrent::UserClass<InputPolicy::Accepts, OutputPolicy::Emits, T>( \
      ripcurrent::Definition(#T "(" #__VA_ARGS__ ")", __FILE__, __LINE__), std::make_tuple(__VA_ARGS__))

// A helper macro to extract the underlying type of the user class, now registered as a RipCurrent block type.
#define CURRENT_USER_TYPE(T) decltype(T.UnderlyingType())

// Types exposed to the end user.
using ripcurrent::H2O;
using ripcurrent::InputPolicy;
using ripcurrent::OutputPolicy;
using ripcurrent::RipCurrentMockableErrorHandler;

// These typedef are the types the user can directly operate with.
// All of them can be liberally copied over, since the logic is concealed within the inner `shared_ptr<>`.
using RipCurrentLHS = ripcurrent::SharedCurrent<InputPolicy::DoesNotAccept, OutputPolicy::Emits>;
using RipCurrentVIA = ripcurrent::SharedCurrent<InputPolicy::Accepts, OutputPolicy::Emits>;
using RipCurrentRHS = ripcurrent::SharedCurrent<InputPolicy::Accepts, OutputPolicy::DoesNotEmit>;
using RipCurrent = ripcurrent::SharedCurrent<InputPolicy::DoesNotAccept, OutputPolicy::DoesNotEmit>;

#endif  // CURRENT_RIPCURRENT_H
