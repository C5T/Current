#ifndef BRICKS_RTTI_EXCEPTIONS_H
#define BRICKS_RTTI_EXCEPTIONS_H

#include "../exception.h"

namespace bricks {
namespace rtti {

struct UnrecognizedPolymorphicType : Exception {};

}  // namespace rtti
}  // namespace bricks

#endif  // BRICKS_RTTI_EXCEPTIONS_H
