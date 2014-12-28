#ifndef BRICKS_RTTI_EXCEPTIONS_H
#define BRICKS_RTTI_EXCEPTIONS_H

#include "../exception.h"

namespace bricks {
namespace rtti {

// TODO(dkorolev): Add a test for it.
struct UnrecognizedPolymorphicType : Exception {};

}  // namespace rtti
}  // namespace bricks

#endif  // BRICKS_RTTI_EXCEPTIONS_H
