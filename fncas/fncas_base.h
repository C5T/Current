// https://github.com/dkorolev/fncas

#ifndef FNCAS_BASE_H
#define FNCAS_BASE_H

#include <vector>
#include <cstdlib>
#include <cstdint>

namespace fncas {

typedef int64_t node_index_type;  // Allow 4B+ nodes, keep the type signed for evaluation algorithms.
typedef double fncas_value_type;

struct noncopyable {
  noncopyable() = default;
  noncopyable(const noncopyable&) = delete;
  noncopyable& operator=(const noncopyable&) = delete;
  noncopyable(noncopyable&&) = delete;
  void operator=(noncopyable&&) = delete;
};

template <typename T>
T& growing_vector_access(std::vector<T>& vector, node_index_type index, const T& fill) {
  if (static_cast<node_index_type>(vector.size()) <= index) {
    vector.resize(static_cast<size_t>(index + 1), fill);
  }
  return vector[index];
}

enum class type_t : uint8_t { variable, value, operation, function };
enum class operation_t : uint8_t { add, subtract, multiply, divide, end };
enum class function_t : uint8_t { sqrt, exp, log, sin, cos, tan, asin, acos, atan, end };

}  // namespace fncas

#endif  // #ifndef FNCAS_BASE_H
