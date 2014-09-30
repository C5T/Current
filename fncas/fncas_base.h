// https://github.com/dkorolev/fncas

#ifndef FNCAS_BASE_H
#define FNCAS_BASE_H

#include <vector>

namespace fncas {

typedef double fncas_value_type;

class noncopyable {
 public:
  noncopyable() = default;

 private:
  noncopyable(const noncopyable&) = default;
  noncopyable& operator=(const noncopyable&) = default;
};

template <typename T> T& growing_vector_access(std::vector<T>& vector, int32_t index, const T& fill) {
  if (static_cast<int32_t>(vector.size()) <= index) {
    vector.resize(static_cast<int32_t>(index + 1), fill);
  }
  return vector[index];
}

}  // namespace fncas

#endif  // #ifndef FNCAS_BASE_H
