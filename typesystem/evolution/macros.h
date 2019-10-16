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

#include "../../bricks/template/decay.h"

// This header file defines the macros to shorten:
// 1) Boilerplate type evolvers (which are dumped as part of the Current schema output format), and
// 2) User-defined evolvers (which the user implements by modifying, ideally, just a few lines of the former).

// `CURRENT_EVOLVE` and `CURRENT_NATURAL_EVOLVE` are the syntaxes to eliminate the need
// to write `::current::type_evolution::Evolve<...>::template Go<...>(from_object, into_object)`.
#define CURRENT_EVOLVE_IMPL(evolver, from_namespace, into_namespace, from_object, into_object)                        \
  ::current::type_evolution::Evolve<from_namespace, ::current::decay_t<decltype(from_object)>, evolver>::template Go< \
      into_namespace>(from_object, into_object)

// `CURRENT_EVOLVE` uses the `evolver` provided explicitly as the first parameter.
#define CURRENT_EVOLVE(evolver, from_namespace, into_namespace, from_object, into_object) \
  CURRENT_EVOLVE_IMPL(evolver, from_namespace, into_namespace, from_object, into_object)

// `CURRENT_NATURAL_EVOLVE` does not require the evolver specified explicitly, it relies on the fact
// that when from within a "scope" of Current evolution, the `CURRENT_ACTIVE_EVOLVER` type is defined
// and it points to the evolver being used right now.
#define CURRENT_NATURAL_EVOLVE(from_namespace, into_namespace, from_object, into_object) \
  CURRENT_EVOLVE_IMPL(CURRENT_ACTIVE_EVOLVER, from_namespace, into_namespace, from_object, into_object)

// Making the syntax shorter. -- D.K.
#define CURRENT_COPY_FIELD(f) \
  ::current::type_evolution::Evolve<FROM, decltype(from.f), CURRENT_ACTIVE_EVOLVER>::template Go<INTO>(from.f, into.f)

#define CURRENT_COPY_SUPER(t)                                                                  \
  ::current::type_evolution::Evolve<FROM, FROM::t, CURRENT_ACTIVE_EVOLVER>::template Go<INTO>( \
      static_cast<const typename FROM::t&>(from), static_cast<typename INTO::t&>(into))

// Macros to shorten variant evolution.
namespace current {
namespace type_evolution {
template <typename DST>
struct CurrentGenericPerCaseVariantEvolver {
  current::decay_t<DST>* p_into;
};
}  // namespace current::type_evolution
}  // namespace current

#define CURRENT_TYPE_EVOLVER_VARIANT(custom_evolver, from_namespace, T, into_namespace)                    \
  template <int COUNTER, typename DST, typename FROM, typename INTO, typename EVOLVER>                     \
  struct CurrentGenericPerCaseVariantEvolverImpl;                                                          \
  CURRENT_TYPE_EVOLVER(custom_evolver,                                                                     \
                       from_namespace,                                                                     \
                       T,                                                                                  \
                       {                                                                                   \
    CurrentGenericPerCaseVariantEvolverImpl<__COUNTER__,                                                   \
                                            decltype(into),                                                \
                                            from_namespace,                                                \
                                            into_namespace,                                                \
                                            custom_evolver> evolver;                                       \
    evolver.p_into = &into;                                                                                \
    from.Call(evolver);                                                                                    \
                       });                                                                                 \
                                                                                                           \
  template <typename DST, typename FROM, typename INTO, typename CURRENT_ACTIVE_EVOLVER>                   \
  struct CurrentGenericPerCaseVariantEvolverImpl<__COUNTER__ - 1, DST, FROM, INTO, CURRENT_ACTIVE_EVOLVER> \
      : ::current::type_evolution::CurrentGenericPerCaseVariantEvolver<DST>

#define CURRENT_TYPE_EVOLVER_VARIANT_CASE(T, ...)                                              \
  void operator()(const typename FROM::T& from) const {                                        \
    auto& into = *::current::type_evolution::CurrentGenericPerCaseVariantEvolver<DST>::p_into; \
    __VA_ARGS__;                                                                               \
  }

// Evolve the variant case into the type of the same name, from the destination namespace.
// Use the natural evolver. This make sure each and every possible inner type
// will be evolved using the evolver specified by the user.
#define CURRENT_TYPE_EVOLVER_NATURAL_VARIANT_CASE(T, ...)                                       \
  void operator()(const typename FROM::T& from) const {                                         \
    auto& into0 = *::current::type_evolution::CurrentGenericPerCaseVariantEvolver<DST>::p_into; \
    using into_t = typename INTO::T;                                                            \
    into0 = into_t();                                                                           \
    auto& into = Value<into_t>(into0);                                                          \
    __VA_ARGS__;                                                                                \
  }

// Shorter syntax. -- D.K.
#define CURRENT_VARIANT_EVOLVER CURRENT_TYPE_EVOLVER_VARIANT
#define CURRENT_EVOLVE_CASE CURRENT_TYPE_EVOLVER_VARIANT_CASE
#define CURRENT_COPY_CASE(T)                                                                    \
  void operator()(const typename FROM::T& from) const {                                         \
    auto& into0 = *::current::type_evolution::CurrentGenericPerCaseVariantEvolver<DST>::p_into; \
    using into_t = typename INTO::T;                                                            \
    into0 = into_t();                                                                           \
    auto& into = Value<into_t>(into0);                                                          \
    CURRENT_NATURAL_EVOLVE(FROM, INTO, from, into);                                             \
  }

#define CURRENT_DROP_CASE(T) \
  void operator()(const typename FROM::T&) const {}

#endif  // CURRENT_TYPE_SYSTEM_EVOLUTION_MACROS_H
