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

#include "types.h"
#include "exceptions.h"

#include "../Bricks/template/enable_if.h"
#include "../Bricks/template/decay.h"

namespace current {

// A special constructor parameter to indicate `Optional` it should indeed
// construct itself from a bare pointer.
struct FromBarePointer {};

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
      throw NoValueOfTypeException<T>();
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
      throw NoValueOfTypeException<T>();
    }
  }

 private:
  std::unique_ptr<T> owned_optional_object_;
  const T* optional_object_;
};

template <typename, typename Enable = void>
class Optional;

template <typename T>
class Optional<T, std::enable_if_t<std::is_pod<T>::value>> final {
 public:
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

  Optional<T>& operator=(std::nullptr_t) {
    exists_ = false;
    return *this;
  }

  Optional<T>& operator=(T value) {
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

  bool ExistsImpl() const { return exists_; }

  T ValueImpl() const {
    if (exists_) {
      return value_;
    } else {
      throw NoValueOfTypeException<T>();
    }
  }

  T& ValueImpl() {
    if (exists_) {
      return value_;
    } else {
      throw NoValueOfTypeException<T>();
    }
  }

 private:
  T value_;
  bool exists_;
};

template <typename T>
class Optional<T, std::enable_if_t<!std::is_pod<T>::value>> final {
 public:
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

  Optional<T>& operator=(std::nullptr_t) {
    owned_optional_object_ = nullptr;
    optional_object_ = nullptr;
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

  Optional<T>& operator=(const T& object) {
    owned_optional_object_ = std::make_unique<T>(object);
    optional_object_ = owned_optional_object_.get();
    return *this;
  }

  Optional<T>& operator=(T&& object) {
    owned_optional_object_ = std::make_unique<T>(std::move(object));
    optional_object_ = owned_optional_object_.get();
    return *this;
  }

  bool ExistsImpl() const { return optional_object_ != nullptr; }

  const T& ValueImpl() const {
    if (optional_object_ != nullptr) {
      return *optional_object_;
    } else {
      throw NoValueOfTypeException<T>();  // LCOV_EXCL_LINE
    }
  }

  T& ValueImpl() {
    if (optional_object_ != nullptr) {
      return *optional_object_;
    } else {
      throw NoValueOfTypeException<T>();
    }
  }

 private:
  std::unique_ptr<T> owned_optional_object_;
  T* optional_object_ = nullptr;
};

}  // namespace current

using current::ImmutableOptional;
using current::Optional;
using current::FromBarePointer;

#endif  // CURRENT_TYPE_SYSTEM_OPTIONAL_H
