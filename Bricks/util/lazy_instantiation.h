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

#ifndef BRICKS_UTIL_LAZY_INITIALIZATION_H
#define BRICKS_UTIL_LAZY_INITIALIZATION_H

#include "../port.h"
#include "../template/variadic_indexes.h"

#include <memory>
#include <tuple>
#include <utility>

namespace current {

template <typename T, typename T_EXTRA_PARAMETER>
struct LazyInstantiatorAbstract {
  virtual ~LazyInstantiatorAbstract() = default;
  virtual std::shared_ptr<T> InstantiateAsSharedPtr(const T_EXTRA_PARAMETER&) const = 0;
  virtual std::unique_ptr<T> InstantiateAsUniquePtr(const T_EXTRA_PARAMETER&) const = 0;
};

template <typename T>
struct LazyInstantiatorAbstract<T, void> {
  virtual ~LazyInstantiatorAbstract() = default;
  virtual std::shared_ptr<T> InstantiateAsSharedPtr() const = 0;
  virtual std::unique_ptr<T> InstantiateAsUniquePtr() const = 0;
};

template <typename T, typename T_EXTRA_PARAMETER, typename... ARGS>
class LazyInstantiatorPerType : public LazyInstantiatorAbstract<T, T_EXTRA_PARAMETER> {
 public:
  template <typename PASSED_IN_TUPLE>
  LazyInstantiatorPerType(PASSED_IN_TUPLE&& args_as_tuple)
      : constructor_parameters_(std::forward<std::tuple<ARGS...>>(args_as_tuple)) {}

  std::shared_ptr<T> InstantiateAsSharedPtr(const T_EXTRA_PARAMETER& parameter) const override {
    return DoInstantiateShared(parameter, current::variadic_indexes::generate_indexes<sizeof...(ARGS)>());
  }

  std::unique_ptr<T> InstantiateAsUniquePtr(const T_EXTRA_PARAMETER& parameter) const override {
    return DoInstantiateUnique(parameter, current::variadic_indexes::generate_indexes<sizeof...(ARGS)>());
  }

 private:
  template <int... XS>
  std::shared_ptr<T> DoInstantiateShared(const T_EXTRA_PARAMETER& parameter,
                                         current::variadic_indexes::indexes<XS...>) const {
    return std::make_shared<T>(parameter, std::get<XS>(constructor_parameters_)...);
  }

  template <int... XS>
  std::unique_ptr<T> DoInstantiateUnique(const T_EXTRA_PARAMETER& parameter,
                                         current::variadic_indexes::indexes<XS...>) const {
    return std::make_unique<T>(parameter, std::get<XS>(constructor_parameters_)...);
  }

  std::tuple<ARGS...> constructor_parameters_;
};

template <typename T, typename... ARGS>
class LazyInstantiatorPerType<T, void, ARGS...> : public LazyInstantiatorAbstract<T, void> {
 public:
  template <typename PASSED_IN_TUPLE>
  LazyInstantiatorPerType(PASSED_IN_TUPLE&& args_as_tuple)
      : constructor_parameters_(std::forward<std::tuple<ARGS...>>(args_as_tuple)) {}

  std::shared_ptr<T> InstantiateAsSharedPtr() const override {
    return DoInstantiateShared(current::variadic_indexes::generate_indexes<sizeof...(ARGS)>());
  }

  std::unique_ptr<T> InstantiateAsUniquePtr() const override {
    return DoInstantiateUnique(current::variadic_indexes::generate_indexes<sizeof...(ARGS)>());
  }

 private:
  template <int... XS>
  std::shared_ptr<T> DoInstantiateShared(current::variadic_indexes::indexes<XS...>) const {
    return std::make_shared<T>(std::get<XS>(constructor_parameters_)...);
  }

  template <int... XS>
  std::unique_ptr<T> DoInstantiateUnique(current::variadic_indexes::indexes<XS...>) const {
    return std::make_unique<T>(std::get<XS>(constructor_parameters_)...);
  }

  std::tuple<ARGS...> constructor_parameters_;
};

enum class LazyInstantiationStrategy { Flexible = 0, ShouldNotBeInitialized, ShouldAlreadyBeInitialized };

template <typename T, typename T_EXTRA_PARAMETER = void>
class LazilyInstantiated {
 public:
  LazilyInstantiated(std::unique_ptr<LazyInstantiatorAbstract<T, T_EXTRA_PARAMETER>>&& impl)
      : impl_(std::move(impl)) {}

  // Instantiates as a `shared_ptr<T>`.
  std::shared_ptr<T> InstantiateAsSharedPtr() const { return impl_->InstantiateAsSharedPtr(); }
  template <typename TT = T_EXTRA_PARAMETER>
  std::shared_ptr<T> InstantiateAsSharedPtrWithExtraParameter(const TT& parameter) const {
    return impl_->InstantiateAsSharedPtr(parameter);
  }

  // Instantiates as a `unique_ptr<T>`.
  std::unique_ptr<T> InstantiateAsUniquePtr() const { return impl_->InstantiateAsUniquePtr(); }
  template <typename TT = T_EXTRA_PARAMETER>
  std::unique_ptr<T> InstantiateAsUniquePtrWithExtraParameter(const TT& parameter) const {
    return impl_->InstantiateAsUniquePtr(parameter);
  }

  // Instantiates and returns a `T&`, using a passed in `shared_ptr<T>` as shared storage.
  T& InstantiateAsSharedPtr(std::shared_ptr<T>& shared_instance,
                            LazyInstantiationStrategy strategy = LazyInstantiationStrategy::Flexible) const {
    if (strategy == LazyInstantiationStrategy::ShouldNotBeInitialized) {
      assert(!shared_instance);
    }
    if (strategy == LazyInstantiationStrategy::ShouldAlreadyBeInitialized) {
      assert(shared_instance);
    }
    if (!shared_instance) {
      shared_instance = InstantiateAsSharedPtr();
    }
    return *shared_instance;
  }

 private:
  std::unique_ptr<LazyInstantiatorAbstract<T, T_EXTRA_PARAMETER>> impl_;
};

// Construction from variadic future constructor parameters.
// NOTE: Use `std::ref()` for parameters passed in by reference. Perfect forwarding does not play well
// with delayed instantiation, since it captures constants as rvalue references, which do
// get out of scope before the instantiation takes place.
template <typename T, typename... ARGS>
LazilyInstantiated<T, void> DelayedInstantiate(ARGS... args) {
  return LazilyInstantiated<T, void>(
      std::move(std::make_unique<LazyInstantiatorPerType<T, void, ARGS...>>(std::forward_as_tuple(args...))));
}

template <typename T, typename EXTRA_PARAMETER, typename... ARGS>
LazilyInstantiated<T, EXTRA_PARAMETER> DelayedInstantiateWithExtraParameter(ARGS... args) {
  return LazilyInstantiated<T, EXTRA_PARAMETER>(std::move(
      std::make_unique<LazyInstantiatorPerType<T, EXTRA_PARAMETER, ARGS...>>(std::forward_as_tuple(args...))));
}

// Construction from future constructor parameters passed in as a tuple.
// NOTE: Use `std::ref()` for parameters passed in by reference. Perfect forwarding does not play well
// with delayed instantiation, since it captures constants as rvalue references, which do
// get out of scope before the instantiation takes place.
template <typename T, typename... ARGS>
LazilyInstantiated<T, void> DelayedInstantiateFromTuple(std::tuple<ARGS...>&& args_as_tuple) {
  return LazilyInstantiated<T, void>(std::move(std::make_unique<LazyInstantiatorPerType<T, void, ARGS...>>(
      std::forward<std::tuple<ARGS...>>(args_as_tuple))));
}
template <typename T, typename EXTRA_PARAMETER, typename... ARGS>
LazilyInstantiated<T, EXTRA_PARAMETER> DelayedInstantiateWithExtraParameterFromTuple(
    std::tuple<ARGS...>&& args_as_tuple) {
  return LazilyInstantiated<T, EXTRA_PARAMETER>(
      std::move(std::make_unique<LazyInstantiatorPerType<T, EXTRA_PARAMETER, ARGS...>>(
          std::forward<std::tuple<ARGS...>>(args_as_tuple))));
}

}  // namespace current

#endif  // BRICKS_UTIL_LAZY_INITIALIZATION_H
