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

#ifndef CURRENT_TYPE_SYSTEM_EVOLUTION_MACROS_H
#define CURRENT_TYPE_SYSTEM_EVOLUTION_MACROS_H

#include "../../port.h"

#include "../../Bricks/template/decay.h"

// This header file defines the macros to shorten:
// 1) Boilerplate type evolutors (which are dumped as part of the Current schema output format), and
// 2) User-defined evolutors (which the user implements by modifying, ideally, just a few lines of the former).

// `CURRENT_EVOLVE` and `CURRENT_NATURAL_EVOLVE` are the syntaxes to eliminate the need
// to write `::current::type_evolution::Evolve<...>::template Go<...>(from_object, into_object)`.
#define CURRENT_EVOLVE_IMPL(evolutor, from_namespace, into_namespace, from_object, into_object) \
  ::current::type_evolution::Evolve<from_namespace,                                             \
                                    ::current::decay<decltype(from_object)>,                    \
                                    evolutor>::template Go<into_namespace>(from_object, into_object)

// `CURRENT_EVOLVE` uses the `evolutor` provided explicitly as the first parameter.
#define CURRENT_EVOLVE(evolutor, from_namespace, into_namespace, from_object, into_object) \
  CURRENT_EVOLVE_IMPL(evolutor, from_namespace, into_namespace, from_object, into_object)

// `CURRENT_NATURAL_EVOLVE` does not require the evolutor specified explicitly, it relies on the fact
// that when from within a "scope" of Current evolution, the `CURRENT_ACTIVE_EVOLUTOR` type is defined
// and it points to the evolutor being used right now.
#define CURRENT_NATURAL_EVOLVE(from_namespace, into_namespace, from_object, into_object) \
  CURRENT_EVOLVE_IMPL(CURRENT_ACTIVE_EVOLUTOR, from_namespace, into_namespace, from_object, into_object)

// Macros to shorten variant evolution.
namespace current {
namespace type_evolution {
template <typename DST>
struct CurrentGenericPerCaseVariantEvolutor {
  current::decay<DST>* p_into;
};
}  // namespace current::type_evolution
}  // namespace current

#define CURRENT_TYPE_EVOLUTOR_VARIANT(custom_evolutor, from_namespace, T, into_namespace)                    \
  template <int COUNTER, typename DST, typename FROM, typename INTO, typename EVOLUTOR>                      \
  struct CurrentGenericPerCaseVariantEvolutorImpl;                                                           \
  CURRENT_TYPE_EVOLUTOR(custom_evolutor,                                                                     \
                        from_namespace,                                                                      \
                        T,                                                                                   \
                        {                                                                                    \
    CurrentGenericPerCaseVariantEvolutorImpl<__COUNTER__,                                                    \
                                             decltype(into),                                                 \
                                             from_namespace,                                                 \
                                             into_namespace,                                                 \
                                             custom_evolutor> evolutor;                                      \
    evolutor.p_into = &into;                                                                                 \
    from.Call(evolutor);                                                                                     \
                        });                                                                                  \
                                                                                                             \
  template <typename DST, typename FROM, typename INTO, typename CURRENT_ACTIVE_EVOLUTOR>                    \
  struct CurrentGenericPerCaseVariantEvolutorImpl<__COUNTER__ - 1, DST, FROM, INTO, CURRENT_ACTIVE_EVOLUTOR> \
      : ::current::type_evolution::CurrentGenericPerCaseVariantEvolutor<DST>

#define CURRENT_TYPE_EVOLUTOR_VARIANT_CASE(T, ...)                                              \
  void operator()(const typename FROM::T& from) const {                                         \
    auto& into = *::current::type_evolution::CurrentGenericPerCaseVariantEvolutor<DST>::p_into; \
    __VA_ARGS__;                                                                                \
  }

// Evolve the variant case into the type of the same name, from the destination namespace.
// Use the natural evolutor. This make sure each and every possible inner type
// will be evolved using the evolutor specified by the user.
#define CURRENT_TYPE_EVOLUTOR_NATURAL_VARIANT_CASE(T, ...)                                       \
  void operator()(const typename FROM::T& from) const {                                          \
    auto& into0 = *::current::type_evolution::CurrentGenericPerCaseVariantEvolutor<DST>::p_into; \
    using into_t = typename INTO::T;                                                             \
    into0 = into_t();                                                                            \
    auto& into = Value<into_t>(into0);                                                           \
    __VA_ARGS__;                                                                                 \
  }

#endif  // CURRENT_TYPE_SYSTEM_EVOLUTION_MACROS_H
