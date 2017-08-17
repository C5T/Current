// The `current.h` file is the one from `https://github.com/C5T/Current`.
// Compile with `-std=c++11` or higher.

#include "current.h"

// clang-format off

namespace current_userspace {

#ifndef CURRENT_SCHEMA_FOR_T9203762249653644123
#define CURRENT_SCHEMA_FOR_T9203762249653644123
namespace t9203762249653644123 {
CURRENT_STRUCT(Integer) {
  CURRENT_FIELD(i, int32_t);
};
}  // namespace t9203762249653644123
#endif  // CURRENT_SCHEMA_FOR_T_9203762249653644123

#ifndef CURRENT_SCHEMA_FOR_T9208325393419959398
#define CURRENT_SCHEMA_FOR_T9208325393419959398
namespace t9208325393419959398 {
CURRENT_STRUCT(Templated_Z) {
  CURRENT_EXPORTED_TEMPLATED_STRUCT(Templated, t9203762249653644123::Integer);
  CURRENT_FIELD(x, t9203762249653644123::Integer);
};
}  // namespace t9208325393419959398
#endif  // CURRENT_SCHEMA_FOR_T_9208325393419959398

}  // namespace current_userspace

#ifndef CURRENT_NAMESPACE_ExposedTemplates_DEFINED
#define CURRENT_NAMESPACE_ExposedTemplates_DEFINED
CURRENT_NAMESPACE(ExposedTemplates) {
  CURRENT_NAMESPACE_TYPE(Integer, current_userspace::t9203762249653644123::Integer);
  CURRENT_NAMESPACE_TYPE(Templated_T9203762249653644123, current_userspace::t9208325393419959398::Templated_Z);

  // Privileged types.
  CURRENT_NAMESPACE_TYPE(TestExposure, current_userspace::t9208325393419959398::Templated_Z);
};  // CURRENT_NAMESPACE(ExposedTemplates)
#endif  // CURRENT_NAMESPACE_ExposedTemplates_DEFINED

namespace current {
namespace type_evolution {

// Default evolution for struct `Integer`.
#ifndef DEFAULT_EVOLUTION_A24ABDE99FCA11BC4103B06A33F151D15BAEA6088742D6736A0394DB59745124  // typename ExposedTemplates::Integer
#define DEFAULT_EVOLUTION_A24ABDE99FCA11BC4103B06A33F151D15BAEA6088742D6736A0394DB59745124  // typename ExposedTemplates::Integer
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedTemplates, typename ExposedTemplates::Integer, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedTemplates;
  template <typename INTO>
  static void Go(const typename FROM::Integer& from,
                 typename INTO::Integer& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Integer>::value == 1,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(i);
  }
};
#endif

// Default evolution for struct `Templated_Z`.
#ifndef DEFAULT_EVOLUTION_A8698CD1708B2AD4E13F8076FE6FFF992C5B98923B5FC54B9667682D842A2892  // typename ExposedTemplates::Templated_T9203762249653644123
#define DEFAULT_EVOLUTION_A8698CD1708B2AD4E13F8076FE6FFF992C5B98923B5FC54B9667682D842A2892  // typename ExposedTemplates::Templated_T9203762249653644123
template <typename CURRENT_ACTIVE_EVOLVER>
struct Evolve<ExposedTemplates, typename ExposedTemplates::Templated_T9203762249653644123, CURRENT_ACTIVE_EVOLVER> {
  using FROM = ExposedTemplates;
  template <typename INTO>
  static void Go(const typename FROM::Templated_T9203762249653644123& from,
                 typename INTO::Templated_T9203762249653644123& into) {
      static_assert(::current::reflection::FieldCounter<typename INTO::Templated_T9203762249653644123>::value == 1,
                    "Custom evolver required.");
      CURRENT_COPY_FIELD(x);
  }
};
#endif

}  // namespace current::type_evolution
}  // namespace current

#if 0  // Boilerplate evolvers.

CURRENT_STRUCT_EVOLVER(CustomEvolver, ExposedTemplates, Integer, {
  CURRENT_COPY_FIELD(i);
});

CURRENT_STRUCT_EVOLVER(CustomEvolver, ExposedTemplates, Templated_T9203762249653644123, {
  CURRENT_COPY_FIELD(x);
});

#endif  // Boilerplate evolvers.

// clang-format on
