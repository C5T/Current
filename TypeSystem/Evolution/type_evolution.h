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

#include "../struct.h"
#include "../variant.h"

#include "../../Bricks/template/pod.h"

#define CURRENT_NAMESPACE(ns)                                   \
  struct CURRENT_NAMESPACE_HELPER_##ns {                        \
    static const char* CURRENT_NAMESPACE_NAME() { return #ns; } \
  };                                                            \
  struct ns : CURRENT_NAMESPACE_HELPER_##ns

#define CURRENT_NAMESPACE_TYPE(external, ...) using external = __VA_ARGS__

namespace current {
namespace type_evolution {

struct NaturalEvolutor {};

template <typename FROM_NAMESPACE, typename FROM_TYPE, typename EVOLUTOR = NaturalEvolutor>
struct Evolve;

// Identity evolutors for primitive types.
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type) \
  template <typename FROM_NAMESPACE, typename EVOLUTOR>                                        \
  struct Evolve<FROM_NAMESPACE, cpp_type, EVOLUTOR> {                                          \
    template <typename>                                                                        \
    static cpp_type Go(cpp_type from) {                                                        \
      return from;                                                                             \
    }                                                                                          \
  };
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

// Default evolutor for Variants.
template <typename DESTINATION_VARIANT, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>
struct VariantTypedEvolutor {
  DESTINATION_VARIANT& into;
  explicit VariantTypedEvolutor(DESTINATION_VARIANT& into) : into(into) {}
  template <typename T>
  void operator()(const T& value) {
    into = Evolve<FROM_NAMESPACE, T, EVOLUTOR>::template Go<INTO>(value);
  }
};

}  // namespace current::type_evolution
}  // namespace current

#define CURRENT_TYPE_EVOLUTOR(evolutor, from_namespace, type_name, ...)                  \
  namespace current {                                                                    \
  namespace type_evolution {                                                             \
  struct evolutor;                                                                       \
  template <>                                                                            \
  struct Evolve<from_namespace, from_namespace::type_name, evolutor> {                   \
    template <typename INTO>                                                             \
    static typename INTO::type_name Go(const typename from_namespace::type_name& from) { \
      typename INTO::type_name into;                                                     \
      __VA_ARGS__;                                                                       \
      return into;                                                                       \
    }                                                                                    \
  };                                                                                     \
  }                                                                                      \
  }

#endif  // CURRENT_TYPE_SYSTEM_EVOLUTION_TYPE_EVOLUTION_H
