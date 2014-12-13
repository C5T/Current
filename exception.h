#ifndef BRICKS_EXCEPTIONS_H
#define BRICKS_EXCEPTIONS_H

#include <exception>

namespace bricks {

// TODO(dkorolev): Add string descriptions.
struct BricksException : std::exception {};

}  // namespace bricks

#endif  // BRICKS_EXCEPTIONS_H
