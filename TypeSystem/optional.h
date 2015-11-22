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

// Current-friendly data accessors:
//
// * bool Exists(x) : non-throwing, always `true` for non-optionals.
// * T Value(x)     : can throw `NoValue` if and only if `Exists(x)` is `false`.
//
// For primitive types, `Exists()` is always `true`, and `Value(x)` is equivalent to `x`.
// For types that can be optional, `Exists()` may be `false`, and `Value(x)` would then throw.
//
// The exception type is derived from `NoValueException`, alias as `NoValue = const NoValueException&`.
// Catching on this top-level type is type-safe.
//
// TODO(dkorolev): Alternatively, a type-specific `catch (NoSpecificValue<T>) { ... }` can be used.
//
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

#include "exceptions.h"

#include "../Bricks/template/enable_if.h"
#include "../Bricks/template/decay.h"

namespace current {

template <typename T>
class ImmutableOptional final {
 public:
  ImmutableOptional() = delete;
  ImmutableOptional(const T* object) : optional_object_(object) {}

  // NOTE: Constructors taking references or const references are a bad idea,
  // since it makes it very easy to make a mistake of passing in a short-lived temporary.
  // The users are advised to explicitly pass in a pointer if the object is externally owned,
  // or `std::move()` an `std::unique_ptr<>` into an `ImmutableOptional`.
  // Another alternative is construct an `ImmutableOptional<>` directly from `make_unique<>`.

  // TODO(dkorolev): Discuss the semantics with @mzhurovich.

  ImmutableOptional(std::unique_ptr<T>&& rhs)
      : owned_optional_object_(std::move(rhs)), optional_object_(owned_optional_object_.get()) {}
  bool Exists() const { return optional_object_ != nullptr; }
  const T& Value() const {
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

template <typename T>
class Optional final {
 public:
  Optional() = default;
  Optional(const T* object) : optional_object_(object) {}

  // NOTE: Constructors taking references or const references are a bad idea,
  // since it makes it very easy to make a mistake of passing in a short-lived temporary.
  // The users are advised to explicitly pass in a pointer if the object is externally owned,
  // or `std::move()` an `std::unique_ptr<>` into an `Optional`.
  // Another alternative is construct an `Optional<>` directly from `make_unique<>`.

  // TODO(dkorolev): Discuss the semantics with @mzhurovich.

  Optional(std::unique_ptr<T>&& rhs)
      : owned_optional_object_(std::move(rhs)), optional_object_(owned_optional_object_.get()) {}
  bool Exists() const { return optional_object_ != nullptr; }
  const T& Value() const {
    if (optional_object_ != nullptr) {
      return *optional_object_;
    } else {
      throw NoValueOfTypeException<T>();
    }
  }
  T& Value() {
    if (optional_object_ != nullptr) {
      return *optional_object_;
    } else {
      throw NoValueOfTypeException<T>();
    }
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
  Optional<T>& operator=(std::unique_ptr<T>&& value) {
    owned_optional_object_ = std::move(value);
    optional_object_ = owned_optional_object_.get();
    return *this;
  }

  // TODO(dkorolev): Discuss this semantics with Max.
  Optional<T>& operator=(const T& data) {
    owned_optional_object_ = std::move(make_unique<T>(data));
    optional_object_ = owned_optional_object_.get();
    return *this;
  }

 private:
  std::unique_ptr<T> owned_optional_object_;
  T* optional_object_ = nullptr;
};

}  // namespace current

using current::ImmutableOptional;
using current::Optional;

#endif  // CURRENT_TYPE_SYSTEM_OPTIONAL_H
