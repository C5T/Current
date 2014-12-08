#ifndef BRICKS_STRING_PRINTF_H
#define BRICKS_STRING_PRINTF_H

#include <string>
#include <cstdarg>

namespace bricks {
namespace string {

std::string Printf(const char *fmt, ...) {
  const int max_formatted_output_length = 1024 * 1024;
  static char buf[max_formatted_output_length + 1];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, max_formatted_output_length, fmt, ap);
  va_end(ap);
  return buf;
}

}  // namespace string
}  // namespace bricks

#endif  // BRICKS_STRING_PRINTF_H
