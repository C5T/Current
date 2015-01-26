// https://github.com/dkorolev/fncas

#ifndef FNCAS_BASE_H
#define FNCAS_BASE_H

#include <vector>
#include <cstdlib>

namespace fncas {

typedef int64_t node_index_type;  // Allow 4B+ nodes, keep the type signed for evaluation algorithms.
typedef double fncas_value_type;

class noncopyable {
 public:
  noncopyable() = default;

 private:
  noncopyable(const noncopyable&) = default;
  noncopyable& operator=(const noncopyable&) = default;
};

template <typename T>
T& growing_vector_access(std::vector<T>& vector, node_index_type index, const T& fill) {
  if (static_cast<node_index_type>(vector.size()) <= index) {
    vector.resize(static_cast<size_t>(index + 1), fill);
  }
  return vector[index];
}

}  // namespace fncas

#endif  // #ifndef FNCAS_BASE_H
