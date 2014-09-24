// https://github.com/dkorolev/fncas

#ifndef FNCAS_BASE_H
#define FNCAS_BASE_H

namespace fncas {

typedef double fncas_value_type;

class noncopyable {
 public:
  noncopyable() = default;

 private:
  noncopyable(const noncopyable&) = default;
  noncopyable& operator=(const noncopyable&) = default;
};

}  // namespace fncas

#endif  // #ifndef FNCAS_BASE_H
