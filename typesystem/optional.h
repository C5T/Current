/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

// User-friendly types that can be optional are `Optional<T>` and `ImmutableOptional<T>`.
//
// Other types may behave as optionals, for example, an `operator[]`-style getter on a container.
// This adds flexibility by saving on checks. For instance, consider a container within a container, with
// access by double index. Because an optional inner-level container silently reports itself as empty,
// the `if (Exists(matrix[foo][bar]))` construct, as well as the `for (const auto& cit : matrix[foo])` one
// are perfectly valid, despite appearing as they are attempting to access a non-existent inner object.
//
// TODO(dkorolev): Implement the above logic wrt missing inner level data.

#ifndef CURRENT_TYPE_SYSTEM_OPTIONAL_H
#define CURRENT_TYPE_SYSTEM_OPTIONAL_H

#include "../port.h"  // `make_unique<>`.

#include <memory>
#include <type_traits>

#include "types.h"
#include "exceptions.h"

#include "../bricks/template/decay.h"

namespace current {

// A special constructor parameter to indicate `Optional` it should indeed
// construct itself from a bare pointer.
struct FromBarePointer {};

// `ImmutableOptional` class holds immutable optional value of `T`.
template <typename, typename Enable = void>
class ImmutableOptional;

template <typename T>
class ImmutableOptional<T, std::enable_if_t<std::is_pod<T>::value>> final {
 public:
  ImmutableOptional() = delete;

  ImmutableOptional(std::nullptr_t) : value_(), exists_(false) {}

  ImmutableOptional(T value) : value_(value), exists_(true) {}

  bool ExistsImpl() const { return exists_; }

  T ValueImpl() const {
    if (exists_) {
      return value_;
    } else {
      CURRENT_THROW(NoValueOfTypeException<T>());
    }
  }

 private:
  const T value_;
  bool exists_;
};

template <typename T>
class ImmutableOptional<T, std::enable_if_t<!std::is_pod<T>::value>> final {
 public:
  ImmutableOptional() = delete;

  ImmutableOptional(std::nullptr_t) : optional_object_(nullptr) {}

  ImmutableOptional(const FromBarePointer&, const T* ptr) : optional_object_(ptr) {}

  ImmutableOptional(const T& object)
      : owned_optional_object_(std::make_unique<T>(object)), optional_object_(owned_optional_object_.get()) {}

  ImmutableOptional(T&& object)
      : owned_optional_object_(std::make_unique<T>(std::move(object))),
        optional_object_(owned_optional_object_.get()) {}

  ImmutableOptional(std::unique_ptr<T>&& uptr)
      : owned_optional_object_(std::move(uptr)), optional_object_(owned_optional_object_.get()) {}

  bool ExistsImpl() const { return optional_object_ != nullptr; }

  const T& ValueImpl() const {
    if (optional_object_ != nullptr) {
      return *optional_object_;
    } else {
      CURRENT_THROW(NoValueOfTypeException<T>());
    }
  }

 private:
  std::unique_ptr<T> owned_optional_object_;
  const T* optional_object_;
};

// Compare `ImmutableOptional<T>` with `ImmutableOptional<T>`.
template <class T>
bool operator==(const ImmutableOptional<T>& x, const ImmutableOptional<T>& y) {
  return x.ExistsImpl() != y.ExistsImpl() ? false : (x.ExistsImpl() ? x.ValueImpl() == y.ValueImpl() : true);
}

template <class T>
bool operator!=(const ImmutableOptional<T>& x, const ImmutableOptional<T>& y) {
  return !(x == y);
}

template <class T>
bool operator<(const ImmutableOptional<T>& x, const ImmutableOptional<T>& y) {
  return !y.ExistsImpl() ? false : (!x.ExistsImpl() ? true : x.ValueImpl() < y.ValueImpl());
}

template <class T>
bool operator>(const ImmutableOptional<T>& x, const ImmutableOptional<T>& y) {
  return (y < x);
}

template <class T>
bool operator<=(const ImmutableOptional<T>& x, const ImmutableOptional<T>& y) {
  return !(y < x);
}

template <class T>
bool operator>=(const ImmutableOptional<T>& x, const ImmutableOptional<T>& y) {
  return !(x < y);
}

// Compare `ImmutableOptional<T>` with `const T&`.
template <class T>
bool operator==(const ImmutableOptional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() == v : false;
}

template <class T>
bool operator==(const T& v, const ImmutableOptional<T>& x) {
  return x.ExistsImpl() ? v == x.ValueImpl() : false;
}

template <class T>
bool operator!=(const ImmutableOptional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() != v : true;
}

template <class T>
bool operator!=(const T& v, const ImmutableOptional<T>& x) {
  return x.ExistsImpl() ? v != x.ValueImpl() : true;
}

template <class T>
bool operator<(const ImmutableOptional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() < v : true;
}

template <class T>
bool operator<(const T& v, const ImmutableOptional<T>& x) {
  return x.ExistsImpl() ? v < x.ValueImpl() : false;
}

template <class T>
bool operator>(const ImmutableOptional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() > v : false;
}

template <class T>
bool operator>(const T& v, const ImmutableOptional<T>& x) {
  return x.ExistsImpl() ? v > x.ValueImpl() : true;
}

template <class T>
bool operator<=(const ImmutableOptional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() <= v : true;
}

template <class T>
bool operator<=(const T& v, const ImmutableOptional<T>& x) {
  return x.ExistsImpl() ? v <= x.ValueImpl() : false;
}

template <class T>
bool operator>=(const ImmutableOptional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() >= v : false;
}

template <class T>
bool operator>=(const T& v, const ImmutableOptional<T>& x) {
  return x.ExistsImpl() ? v >= x.ValueImpl() : true;
}

// `Optional` class holds mutable optional value of `T`.
template <typename, typename Enable = void>
class Optional;

template <typename T>
class Optional<T, std::enable_if_t<std::is_pod<T>::value>> final {
 public:
  using optional_underlying_t = T;

  Optional() : value_(), exists_(false) {}

  Optional(std::nullptr_t) : value_(), exists_(false) {}

  Optional(T value) : value_(value), exists_(true) {}

  Optional(const Optional<T>& rhs) {
    value_ = rhs.value_;
    exists_ = rhs.exists_;
  }

  Optional(Optional<T>&& rhs) {
    value_ = rhs.value_;
    exists_ = rhs.exists_;
    rhs.exists_ = false;
  }

  Optional(const ImmutableOptional<T>& rhs) {
    exists_ = rhs.ExistsImpl();
    if (exists_) {
      value_ = rhs.ValueImpl();
    }
  }

  Optional<T>& operator=(std::nullptr_t) {
    exists_ = false;
    return *this;
  }

  Optional<T>& operator=(T value) {
    value_ = value;
    exists_ = true;
    return *this;
  }

  // Important to be able to assign `0` to `Optional<double` et. al.
  template <typename TT = T, class ENABLE = std::enable_if_t<std::is_arithmetic_v<TT>>>
  Optional<T>& operator=(int value) {
    value_ = value;
    exists_ = true;
    return *this;
  }

  Optional<T>& operator=(const Optional<T>& rhs) {
    value_ = rhs.value_;
    exists_ = rhs.exists_;
    return *this;
  }

  Optional<T>& operator=(Optional<T>&& rhs) {
    value_ = rhs.value_;
    exists_ = rhs.exists_;
    rhs.exists_ = false;
    return *this;
  }

  Optional<T>& operator=(const ImmutableOptional<T>& rhs) {
    exists_ = rhs.ExistsImpl();
    if (exists_) {
      value_ = rhs.ValueImpl();
    }
    return *this;
  }

  bool ExistsImpl() const { return exists_; }

  T ValueImpl() const {
    if (exists_) {
      return value_;
    } else {
      CURRENT_THROW(NoValueOfTypeException<T>());
    }
  }

  T& ValueImpl() {
    if (exists_) {
      return value_;
    } else {
      CURRENT_THROW(NoValueOfTypeException<T>());
    }
  }

 private:
  T value_;
  bool exists_;
};

template <typename T>
class Optional<T, std::enable_if_t<!std::is_pod<T>::value>> final {
 public:
  using optional_underlying_t = T;

  Optional() = default;

  Optional(std::nullptr_t) : optional_object_(nullptr) {}

  Optional(const FromBarePointer&, T* ptr) : optional_object_(ptr) {}

  Optional(const T& object)
      : owned_optional_object_(std::make_unique<T>(object)), optional_object_(owned_optional_object_.get()) {}

  Optional(T&& object)
      : owned_optional_object_(std::make_unique<T>(std::move(object))),
        optional_object_(owned_optional_object_.get()) {}

  Optional(std::unique_ptr<T>&& uptr)
      : owned_optional_object_(std::move(uptr)), optional_object_(owned_optional_object_.get()) {}

  Optional(const Optional<T>& rhs) {
    if (rhs.ExistsImpl()) {
      owned_optional_object_ = std::make_unique<T>(rhs.ValueImpl());
    } else {
      owned_optional_object_ = nullptr;
    }
    optional_object_ = owned_optional_object_.get();
  }

  Optional(Optional<T>&& rhs) {
    if (rhs.ExistsImpl()) {
      owned_optional_object_ = std::move(rhs.owned_optional_object_);
      rhs = nullptr;
    } else {
      owned_optional_object_ = nullptr;
    }
    optional_object_ = owned_optional_object_.get();
  }

  Optional(const ImmutableOptional<T>& rhs) {
    if (rhs.ExistsImpl()) {
      owned_optional_object_ = std::make_unique<T>(rhs.ValueImpl());
    } else {
      owned_optional_object_ = nullptr;
    }
    optional_object_ = owned_optional_object_.get();
  }

  Optional<T>& operator=(std::nullptr_t) {
    owned_optional_object_ = nullptr;
    optional_object_ = nullptr;
    return *this;
  }

  Optional<T>& operator=(const T& object) {
    if (!owned_optional_object_) {
      owned_optional_object_ = std::make_unique<T>(object);
    } else {
      *owned_optional_object_ = object;
    }
    optional_object_ = owned_optional_object_.get();
    return *this;
  }

  Optional<T>& operator=(T&& object) {
    if (!owned_optional_object_) {
      owned_optional_object_ = std::make_unique<T>(std::move(object));
    } else {
      *owned_optional_object_ = std::move(object);
    }
    optional_object_ = owned_optional_object_.get();
    return *this;
  }

  Optional<T>& operator=(T* ptr) {
    owned_optional_object_ = nullptr;
    optional_object_ = ptr;
    return *this;
  }

  Optional<T>& operator=(std::unique_ptr<T>&& uptr) {
    owned_optional_object_ = std::move(uptr);
    optional_object_ = owned_optional_object_.get();
    return *this;
  }

  Optional<T>& operator=(const Optional<T>& rhs) {
    if (rhs.ExistsImpl()) {
      owned_optional_object_ = std::make_unique<T>(rhs.ValueImpl());
    } else {
      owned_optional_object_ = nullptr;
    }
    optional_object_ = owned_optional_object_.get();
    return *this;
  }

  Optional<T>& operator=(Optional<T>&& rhs) {
    if (rhs.ExistsImpl()) {
      owned_optional_object_ = std::move(rhs.owned_optional_object_);
      rhs = nullptr;
    } else {
      owned_optional_object_ = nullptr;
    }
    optional_object_ = owned_optional_object_.get();
    return *this;
  }

  Optional<T>& operator=(const ImmutableOptional<T>& rhs) {
    if (rhs.ExistsImpl()) {
      owned_optional_object_ = std::make_unique<T>(rhs.ValueImpl());
    } else {
      owned_optional_object_ = nullptr;
    }
    optional_object_ = owned_optional_object_.get();
    return *this;
  }

  bool ExistsImpl() const { return optional_object_ != nullptr; }

  const T& ValueImpl() const {
    if (optional_object_ != nullptr) {
      return *optional_object_;
    } else {
      CURRENT_THROW(NoValueOfTypeException<T>());  // LCOV_EXCL_LINE
    }
  }

  T& ValueImpl() {
    if (optional_object_ != nullptr) {
      return *optional_object_;
    } else {
      CURRENT_THROW(NoValueOfTypeException<T>());
    }
  }

 private:
  std::unique_ptr<T> owned_optional_object_;
  T* optional_object_ = nullptr;
};

// Compare `Optional<T>` with `Optional<T>`.
template <class T>
bool operator==(const Optional<T>& x, const Optional<T>& y) {
  return x.ExistsImpl() != y.ExistsImpl() ? false : (x.ExistsImpl() ? x.ValueImpl() == y.ValueImpl() : true);
}

template <class T>
bool operator!=(const Optional<T>& x, const Optional<T>& y) {
  return !(x == y);
}

template <class T>
bool operator<(const Optional<T>& x, const Optional<T>& y) {
  return !y.ExistsImpl() ? false : (!x.ExistsImpl() ? true : x.ValueImpl() < y.ValueImpl());
}

template <class T>
bool operator>(const Optional<T>& x, const Optional<T>& y) {
  return (y < x);
}

template <class T>
bool operator<=(const Optional<T>& x, const Optional<T>& y) {
  return !(y < x);
}

template <class T>
bool operator>=(const Optional<T>& x, const Optional<T>& y) {
  return !(x < y);
}

// Compare `Optional<T>` with `const T&`.
template <class T>
bool operator==(const Optional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() == v : false;
}

template <class T>
bool operator==(const T& v, const Optional<T>& x) {
  return x.ExistsImpl() ? v == x.ValueImpl() : false;
}

template <class T>
bool operator!=(const Optional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() != v : true;
}

template <class T>
bool operator!=(const T& v, const Optional<T>& x) {
  return x.ExistsImpl() ? v != x.ValueImpl() : true;
}

template <class T>
bool operator<(const Optional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() < v : true;
}

template <class T>
bool operator<(const T& v, const Optional<T>& x) {
  return x.ExistsImpl() ? v < x.ValueImpl() : false;
}

template <class T>
bool operator>(const Optional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() > v : false;
}

template <class T>
bool operator>(const T& v, const Optional<T>& x) {
  return x.ExistsImpl() ? v > x.ValueImpl() : true;
}

template <class T>
bool operator<=(const Optional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() <= v : true;
}

template <class T>
bool operator<=(const T& v, const Optional<T>& x) {
  return x.ExistsImpl() ? v <= x.ValueImpl() : false;
}

template <class T>
bool operator>=(const Optional<T>& x, const T& v) {
  return x.ExistsImpl() ? x.ValueImpl() >= v : false;
}

template <class T>
bool operator>=(const T& v, const Optional<T>& x) {
  return x.ExistsImpl() ? v >= x.ValueImpl() : true;
}

}  // namespace current

using current::FromBarePointer;
using current::ImmutableOptional;
using current::Optional;

#endif  // CURRENT_TYPE_SYSTEM_OPTIONAL_H
