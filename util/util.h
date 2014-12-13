#ifndef BRICKS_UTIL_UTIL_H
#define BRICKS_UTIL_UTIL_H

#include <cstddef>

template <size_t N>
constexpr size_t CompileTimeStringLength(char const (&)[N]) {
  return N - 1;
}

#endif  // BRICKS_UTIL_UTIL_H
