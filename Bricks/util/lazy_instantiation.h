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

#include <memory>
#include <tuple>
#include <utility>

namespace bricks {

template <typename T>
struct LazyInstantiatorAbstract {
  virtual ~LazyInstantiatorAbstract() = default;
  virtual std::shared_ptr<T> Instantiate() const = 0;
};

namespace lazy_constructor_parameters {
struct Variadic {};
struct Tuple {};
}  // namespace lazy_constructor_parameters

template <typename T, typename... ARGS>
class LazyInstantiatorPerType : public LazyInstantiatorAbstract<T> {
 public:
  LazyInstantiatorPerType(lazy_constructor_parameters::Variadic, ARGS&&... args)
      : constructor_parameters_(std::forward<ARGS>(args)...) {}

  template <typename PASSED_IN_TUPLE>
  LazyInstantiatorPerType(lazy_constructor_parameters::Tuple, PASSED_IN_TUPLE&& args_as_tuple)
      : constructor_parameters_(std::forward<std::tuple<ARGS...>>(args_as_tuple)) {}

  std::shared_ptr<T> Instantiate() const override {
    return DoInstantiateShared(typename indexes_generator<sizeof...(ARGS)>::type());
  }

 private:
  template <int...>
  struct indexes {};

  template <int X, int... XS>
  struct indexes_generator : indexes_generator<X - 1, X - 1, XS...> {};

  template <int... XS>
  struct indexes_generator<0, XS...> {
    typedef indexes<XS...> type;
  };

  template <int... XS>
  std::shared_ptr<T> DoInstantiateShared(indexes<XS...>) const {
    return std::make_shared<T>(std::get<XS>(constructor_parameters_)...);
  }

  std::tuple<ARGS...> constructor_parameters_;
};

enum class LazyInstantiationStrategy { Flexible = 0, ShouldNotBeInitialized, ShouldAlreadyBeInitialized };

template <typename T>
class LazilyInstantiated {
 public:
  LazilyInstantiated(std::unique_ptr<LazyInstantiatorAbstract<T>>&& impl) : impl_(std::move(impl)) {}

  // Instantiate as a `shared_ptr<T>`.
  std::shared_ptr<T> Instantiate() const { return impl_->Instantiate(); }

  // Instantiate and return a `T&`, using a passed in `shared_ptr<T>` as shared storage.
  T& Instantiate(std::shared_ptr<T>& shared_instance,
                 LazyInstantiationStrategy strategy = LazyInstantiationStrategy::Flexible) const {
    if (strategy == LazyInstantiationStrategy::ShouldNotBeInitialized) {
      assert(!shared_instance);
    }
    if (strategy == LazyInstantiationStrategy::ShouldAlreadyBeInitialized) {
      assert(shared_instance);
    }
    if (!shared_instance) {
      shared_instance = Instantiate();
    }
    return *shared_instance;
  }

 private:
  std::unique_ptr<LazyInstantiatorAbstract<T>> impl_;
};

// Construction from variadic future constructor parameters.
template <typename T, typename... ARGS>
LazilyInstantiated<T> DelayedInstantiate(ARGS&&... args) {
  return LazilyInstantiated<T>(std::move(make_unique<LazyInstantiatorPerType<T, ARGS...>>(
      lazy_constructor_parameters::Variadic(), std::forward<ARGS>(args)...)));
}

// Construction from future constructor parameters passed in as a tuple.
template <typename T, typename... ARGS>
LazilyInstantiated<T> DelayedInstantiateFromTuple(std::tuple<ARGS...>&& args_as_tuple) {
  return LazilyInstantiated<T>(std::move(make_unique<LazyInstantiatorPerType<T, ARGS...>>(
      lazy_constructor_parameters::Tuple(), std::forward<std::tuple<ARGS...>>(args_as_tuple))));
}

}  // namespace bricks

#endif  // BRICKS_UTIL_LAZY_INITIALIZATION_H
