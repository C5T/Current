/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_EVOLUTION_TYPE_EVOLUTION_H
#define CURRENT_TYPE_SYSTEM_EVOLUTION_TYPE_EVOLUTION_H

#include <chrono>

#include "macros.h"

#include "../struct.h"
#include "../variant.h"
#include "../optional.h"

#include "../../Bricks/template/pod.h"

#define CURRENT_NAMESPACE(ns)                                   \
  struct CURRENT_NAMESPACE_HELPER_##ns {                        \
    static const char* CURRENT_NAMESPACE_NAME() { return #ns; } \
  };                                                            \
  struct ns : CURRENT_NAMESPACE_HELPER_##ns

#define CURRENT_NAMESPACE_TYPE(external, ...) using external = __VA_ARGS__

// TODO(dkorolev): `__VA_ARGS__` magic here.
#define CURRENT_DERIVED_NAMESPACE(ns, parent)                   \
  struct CURRENT_NAMESPACE_HELPER_##ns {                        \
    static const char* CURRENT_NAMESPACE_NAME() { return #ns; } \
  };                                                            \
  struct ns : parent, CURRENT_NAMESPACE_HELPER_##ns

namespace current {
namespace type_evolution {

struct NaturalEvolver {};

template <typename FROM_NAMESPACE, typename FROM_TYPE, typename EVOLVER = NaturalEvolver>
struct Evolve;

// Identity evolvers for primitive types.
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type) \
  template <typename FROM_NAMESPACE, typename EVOLVER>                                         \
  struct Evolve<FROM_NAMESPACE, cpp_type, EVOLVER> {                                           \
    template <typename>                                                                        \
    static void Go(current::copy_free<cpp_type> from, cpp_type& into) {                        \
      into = from;                                                                             \
    }                                                                                          \
  };
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

// Boilerplate default generic evolver for `std::vector<T>`.
template <typename FROM_NAMESPACE, typename EVOLVER, typename VECTOR_ELEMENT_TYPE>
struct Evolve<FROM_NAMESPACE, std::vector<VECTOR_ELEMENT_TYPE>, EVOLVER> {
  template <typename INTO, typename OUTPUT>
  static void Go(const std::vector<VECTOR_ELEMENT_TYPE>& from, OUTPUT& into) {
    into.resize(from.size());
    auto placeholder = into.begin();
    for (const auto& e : from) {
      Evolve<FROM_NAMESPACE, VECTOR_ELEMENT_TYPE, EVOLVER>::template Go<INTO>(e, *placeholder++);
    }
  }
};

// Boilerplate default generic evolver for `std::pair<T1, T2>`.
template <typename FROM_NAMESPACE, typename EVOLVER, typename FIRST_TYPE, typename SECOND_TYPE>
struct Evolve<FROM_NAMESPACE, std::pair<FIRST_TYPE, SECOND_TYPE>, EVOLVER> {
  template <typename INTO, typename OUTPUT>
  static void Go(const std::pair<FIRST_TYPE, SECOND_TYPE>& from, OUTPUT& into) {
    Evolve<FROM_NAMESPACE, FIRST_TYPE, EVOLVER>::template Go<INTO>(from.first, into.first);
    Evolve<FROM_NAMESPACE, SECOND_TYPE, EVOLVER>::template Go<INTO>(from.second, into.second);
  }
};

// Boilerplate default generic evolver for `std::map<K, V>`.
template <typename FROM_NAMESPACE, typename EVOLVER, typename MAP_KEY, typename MAP_VALUE>
struct Evolve<FROM_NAMESPACE, std::map<MAP_KEY, MAP_VALUE>, EVOLVER> {
  template <typename INTO, typename OUTPUT>
  static void Go(const std::map<MAP_KEY, MAP_VALUE>& from, OUTPUT& into) {
    for (const auto& e : from) {
      typename OUTPUT::key_type key;
      Evolve<FROM_NAMESPACE, MAP_KEY, EVOLVER>::template Go<INTO>(e.first, key);
      Evolve<FROM_NAMESPACE, MAP_VALUE, EVOLVER>::template Go<INTO>(e.second, into[key]);
    }
  }
};

// Boilerplate default generic evolver for `Optional<T>`.
template <typename FROM_NAMESPACE, typename EVOLVER, typename OPTIONAL_INNER_TYPE>
struct Evolve<FROM_NAMESPACE, Optional<OPTIONAL_INNER_TYPE>, EVOLVER> {
  template <typename INTO>
  static void Go(const Optional<OPTIONAL_INNER_TYPE>& from, Optional<OPTIONAL_INNER_TYPE>& into) {
    if (Exists(from)) {
      into = Value(from);
    } else {
      into = nullptr;
    }
  }
};

}  // namespace current::type_evolution
}  // namespace current

#define CURRENT_TYPE_EVOLVER(evolver, from_namespace, type_name, ...)                                \
  namespace current {                                                                                \
  namespace type_evolution {                                                                         \
  struct evolver;                                                                                    \
  template <>                                                                                        \
  struct Evolve<from_namespace, from_namespace::type_name, evolver> {                                \
    using CURRENT_ACTIVE_EVOLVER = evolver;                                                          \
    template <typename INTO>                                                                         \
    static void Go(const typename from_namespace::type_name& from, typename INTO::type_name& into) { \
      __VA_ARGS__;                                                                                   \
    }                                                                                                \
  };                                                                                                 \
  }                                                                                                  \
  }

#endif  // CURRENT_TYPE_SYSTEM_EVOLUTION_TYPE_EVOLUTION_H
