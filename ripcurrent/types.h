/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_RIPCURRENT_TYPES_H
#define CURRENT_RIPCURRENT_TYPES_H

#include "../port.h"

#include <iostream>
#include <functional>

#include "../typesystem/struct.h"
#include "../bricks/template/typelist.h"

namespace current {
namespace ripcurrent {

using movable_message_t = std::unique_ptr<CurrentSuper, CurrentSuperDeleter>;

#if 0

// "Debug-compilation" mode, slower to compile.

template <typename... TS>
class LHSTypes;

template <typename... TS>
struct VoidOrLHSTypesImpl {
  using type = LHSTypes<TS...>;
};

template <>
struct VoidOrLHSTypesImpl<void> {
  using type = LHSTypes<>;
};

template <typename... TS>
using VoidOrLHSTypes = typename VoidOrLHSTypesImpl<TS...>::type;

template <typename... TS>
class RHSTypes;

template <typename... TS>
struct VoidOrRHSTypesImpl {
  using type = RHSTypes<TS...>;
};

template <>
struct VoidOrRHSTypesImpl<void> {
  using type = RHSTypes<>;
};

template <typename... TS>
using VoidOrRHSTypes = typename VoidOrRHSTypesImpl<TS...>::type;

template <typename... TS>
class VIATypes;

template <typename... TS>
class EmittableTypes;

template <typename... TS>
class ThreadUnsafeOutgoingTypes;

template <typename... TS>
class ThreadSafeIncomingTypes;

template <typename T>
struct LHSTypesFromTypeListImpl;

template <typename... TS>
struct LHSTypesFromTypeListImpl<TypeListImpl<TS...>> {
  using type = LHSTypes<TS...>;
};

template <typename T>
using LHSTypesFromTypeList = typename LHSTypesFromTypeListImpl<T>::type;

template <typename T>
struct RHSTypesFromTypeListImpl;

template <typename... TS>
struct RHSTypesFromTypeListImpl<TypeListImpl<TS...>> {
  using type = RHSTypes<TS...>;
};

template <typename T>
using RHSTypesFromTypeList = typename RHSTypesFromTypeListImpl<T>::type;

#else

// Faster compilation with less type differentiation.

template <typename... TS>
using LHSTypes = TypeListImpl<TS...>;
template <typename... TS>
using RHSTypes = TypeListImpl<TS...>;
template <typename... TS>
using VIATypes = TypeListImpl<TS...>;
template <typename... TS>
using EmittableTypes = TypeListImpl<TS...>;
template <typename... TS>
using ThreadUnsafeOutgoingTypes = TypeListImpl<TS...>;
template <typename... TS>
using ThreadSafeIncomingTypes = TypeListImpl<TS...>;

template <typename... TS>
struct VoidOrLHSTypesImpl {
  using type = LHSTypes<TS...>;
};

template <>
struct VoidOrLHSTypesImpl<void> {
  using type = LHSTypes<>;
};

template <typename... TS>
using VoidOrLHSTypes = typename VoidOrLHSTypesImpl<TS...>::type;

template <typename... TS>
struct VoidOrRHSTypesImpl {
  using type = RHSTypes<TS...>;
};

template <>
struct VoidOrRHSTypesImpl<void> {
  using type = RHSTypes<>;
};

template <typename... TS>
using VoidOrRHSTypes = typename VoidOrRHSTypesImpl<TS...>::type;

template <typename T>
struct LHSTypesFromTypeListImpl;

template <typename... TS>
struct LHSTypesFromTypeListImpl<TypeListImpl<TS...> > {
  using type = LHSTypes<TS...>;
};

template <typename T>
using LHSTypesFromTypeList = typename LHSTypesFromTypeListImpl<T>::type;

template <typename T>
struct RHSTypesFromTypeListImpl;

template <typename... TS>
struct RHSTypesFromTypeListImpl<TypeListImpl<TS...> > {
  using type = RHSTypes<TS...>;
};

template <typename T>
using RHSTypesFromTypeList = typename RHSTypesFromTypeListImpl<T>::type;
#endif

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

}  // namespace current::ripcurrent
}  // namespace current

#endif  // CURRENT_RIPCURRENT_TYPES_H
