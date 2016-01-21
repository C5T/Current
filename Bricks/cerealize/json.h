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

#ifndef BRICKS_CEREALIZE_JSON_H
#define BRICKS_CEREALIZE_JSON_H

#include <memory>
#include <string>
#include <sstream>

#include "exceptions.h"
#include "constants.h"

#include "../template/enable_if.h"
#include "../template/decay.h"
#include "../template/is_unique_ptr.h"
#include "../util/null_deleter.h"

#include "../../3rdparty/cereal/include/types/string.hpp"
#include "../../3rdparty/cereal/include/types/vector.hpp"
#include "../../3rdparty/cereal/include/types/map.hpp"
#include "../../3rdparty/cereal/include/types/set.hpp"

#include "../../3rdparty/cereal/include/archives/json.hpp"

// Empirically discovered to be necessary to enable serialization of `std::unique_ptr<>`-s. -- D.K.
#include "../../3rdparty/cereal/include/types/polymorphic.hpp"

#include "../../3rdparty/cereal/include/external/base64.hpp"

namespace current {
namespace cerealize {

template <typename BASE, typename ENTRY>
inline ENABLE_IF<std::is_base_of<BASE, ENTRY>::value, std::unique_ptr<const BASE, current::NullDeleter>>
WithBaseType(const ENTRY& object) {
  return std::unique_ptr<const BASE, NullDeleter>(reinterpret_cast<const BASE*>(&object),
                                                  current::NullDeleter());
}

template <typename T>
constexpr bool HasJSONEntryName(char) {
  return false;
}

template <typename T>
constexpr auto HasJSONEntryName(int) -> decltype(T::JSONEntryName(), bool()) {
  return true;
}

template <typename T, bool HAS_JSON_ENTRY_NAME>
struct ExtractJSONEntryNameImpl {};

template <typename T>
struct ExtractJSONEntryNameImpl<T, false> {
  static std::string DoIt() { return Constants::DefaultJSONEntryName(); }
};

template <typename T>
struct ExtractJSONEntryNameImpl<T, true> {
  static std::string DoIt() { return T::JSONEntryName(); }
};

template <typename UNDECAYED_T>
inline std::string ExtractJSONEntryName() {
  typedef decay<UNDECAYED_T> T;
  return ExtractJSONEntryNameImpl<T, HasJSONEntryName<T>(0)>::DoIt();
}

template <typename OSTREAM, typename T>
inline OSTREAM& AppendAsJSON(OSTREAM& os, T&& object) {
  cereal::JSONOutputArchive so(os);
  so(cereal::make_nvp<rmref<T>>(ExtractJSONEntryName<T>().c_str(), object));
  return os;
}

struct AsConstCharPtr {
  static inline const char* Run(const char* s) { return s; }
  static inline const char* Run(const std::string& s) { return s.c_str(); }
};

template <typename OSTREAM, typename T, typename S>
inline OSTREAM& AppendAsJSON(OSTREAM& os, T&& object, S&& name) {
  cereal::JSONOutputArchive so(os);
  so(cereal::make_nvp<rmref<T>>(AsConstCharPtr::Run(name), object));
  return os;
}

template <typename T>
inline std::string CerealizeJSON(T&& object) {
  std::ostringstream os;
  AppendAsJSON(os, std::forward<T>(object), ExtractJSONEntryName<T>());
  return os.str();
}

template <typename T, typename S>
inline std::string CerealizeJSON(T&& object, S&& name) {
  std::ostringstream os;
  AppendAsJSON(os, std::forward<T>(object), std::forward<S>(name));
  return os.str();
}

// CerealizeJSON parse error handling logic.
// By default, an exception is thrown.
// If a user class defines the `FromInvalidJSON()` method, CerealizeParseJSON() is a non-throwing call,
// and that method will be called instead.

template <typename T>
constexpr bool HasFromInvalidJSON(char) {
  return false;
}

template <typename T>
constexpr auto HasFromInvalidJSON(int)
    -> decltype(std::declval<T>().FromInvalidJSON(std::declval<std::string>()), bool()) {
  return true;
}

template <typename T, bool B>
struct ParseJSONErrorHandler {};

// LCOV_EXCL_START
template <typename T>
struct ParseJSONErrorHandler<T, false> {
  static void HandleError(const std::string& input_json, T&) {
    CURRENT_THROW(current::ParseJSONException(input_json));
  }
};
// LCOV_EXCL_STOP

template <typename T>
struct ParseJSONErrorHandler<T, true> {
  static void HandleError(const std::string& input_json, T& output_object) {
    output_object.FromInvalidJSON(input_json);
  }
};

template <typename T>
inline const T& CerealizeParseJSON(const std::string& input_json, T& output_object) {
  try {
    std::istringstream is(input_json);
    cereal::JSONInputArchive ar(is);
    ar(output_object);
  } catch (cereal::Exception&) {
    ParseJSONErrorHandler<T, HasFromInvalidJSON<decay<T>>(0)>::HandleError(input_json, output_object);
  }
  return output_object;
}

template <typename T>
inline T CerealizeParseJSON(const std::string& input_json) {
  T placeholder;
  CerealizeParseJSON(input_json, placeholder);
  // Can not just do `return CerealizeParseJSON()`, since
  // it would not handle ownership transfer for `std::unique_ptr<>`.
  return placeholder;
}

inline std::string Base64Encode(const std::string& s) {
  return base64::encode(reinterpret_cast<const unsigned char*>(s.c_str()), s.length());
}

}  // namespace cerealize
}  // namespace current

using current::cerealize::CerealizeJSON;
using current::cerealize::CerealizeParseJSON;
using current::cerealize::WithBaseType;
using current::cerealize::ExtractJSONEntryName;

#endif  // BRICKS_CEREALIZE_JSON_H
