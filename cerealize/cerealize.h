/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_CEREALIZE_H
#define BRICKS_CEREALIZE_H

#include <fstream>
#include <string>
#include <sstream>

#include "exceptions.h"

#include "../3party/cereal/include/types/string.hpp"
#include "../3party/cereal/include/types/vector.hpp"
#include "../3party/cereal/include/types/map.hpp"

#include "../3party/cereal/include/types/polymorphic.hpp"

#include "../3party/cereal/include/archives/binary.hpp"
#include "../3party/cereal/include/archives/json.hpp"
#include "../3party/cereal/include/archives/xml.hpp"

#include "../rtti/dispatcher.h"

namespace bricks {
namespace cerealize {

// Helper compile-type test to tell string-like types from cerealizable types.
template <typename T>
struct is_string_type_impl {
  constexpr static bool value = false;
};
template <>
struct is_string_type_impl<std::string> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<char> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<std::vector<char>> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<std::vector<int8_t>> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<std::vector<uint8_t>> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<char*> {
  constexpr static bool value = true;
};
template <size_t N>
struct is_string_type_impl<char[N]> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<const char*> {
  constexpr static bool value = true;
};
template <size_t N>
struct is_string_type_impl<const char[N]> {
  constexpr static bool value = true;
};
// Microsoft Visual Studio compiler is strict with overloads,
// explicitly exclude string-related types from cereal-based implementations.
template <typename TOP_LEVEL_T>
struct is_string_type {
  constexpr static bool value = is_string_type_impl<
      typename std::remove_cv<typename std::remove_reference<TOP_LEVEL_T>::type>::type>::value;
};

// Helper compile-time test that certain type can be serialized via cereal.
template <typename T>
struct is_read_cerealizable {
  constexpr static bool value =
      !is_string_type<T>::value && cereal::traits::is_input_serializable<T, cereal::JSONInputArchive>::value;
};

template <typename T>
struct is_write_cerealizable {
  constexpr static bool value =
      !is_string_type<T>::value && cereal::traits::is_output_serializable<T, cereal::JSONOutputArchive>::value;
};

template <typename T>
struct is_cerealizable {
  constexpr static bool value = is_read_cerealizable<T>::value && is_write_cerealizable<T>::value;
};

// Helper code to simplify wrapping objects into `unique_ptr`-s of their base types
// for Cereal to serialize them as polymorphic types.
struct EmptyDeleterForAnyType {
  template <typename T>
  void operator()(T&) {}
  template <typename T>
  void operator()(T*) {}
};

template <typename BASE, typename ENTRY>
inline typename std::enable_if<std::is_base_of<BASE, ENTRY>::value,
                               std::unique_ptr<const BASE, EmptyDeleterForAnyType>>::type
WithBaseType(const ENTRY& object) {
  return std::unique_ptr<const BASE, EmptyDeleterForAnyType>(reinterpret_cast<const BASE*>(&object),
                                                             EmptyDeleterForAnyType());
}

// Enumeration for compile-time format selection.
enum class CerealFormat { Default = 0, Binary = 0, JSON };

// Templated stream types.
template <CerealFormat>
struct CerealStreamType {};

template <>
struct CerealStreamType<CerealFormat::Binary> {
  typedef cereal::BinaryInputArchive Input;
  typedef cereal::BinaryOutputArchive Output;
  template <typename OSTREAM>
  inline static Output CreateOutputArchive(OSTREAM& os) {
    return Output(os);
  }
};

template <>
struct CerealStreamType<CerealFormat::JSON> {
  typedef cereal::JSONInputArchive Input;
  typedef cereal::JSONOutputArchive Output;
  template <typename OSTREAM>
  inline static Output CreateOutputArchive(OSTREAM& os) {
    // TODO(dkorolev): Add NoIndent as a policy once that cereal change is merged.
    return Output(os, cereal::JSONOutputArchive::Options::NoIndent());
  }
};

// `CerealFileAppender` appends cereal-ized records to a file.
// The format is selected by a template parameter, defaults to binary. All formats are supported.
// Writes are performed using templated `operator <<(const T& entry)`.
// Is type `T` defines a typedef of `CEREAL_BASE_TYPE`, polymorphic serialization is used.
template <CerealFormat T_CEREAL_FORMAT>
class GenericCerealFileAppender {
 public:
  explicit GenericCerealFileAppender(const std::string& filename, bool append = true)
      : fo_(filename, (append ? std::ofstream::app : std::ofstream::trunc) | std::ofstream::binary),
        so_(CerealStreamType<T_CEREAL_FORMAT>::CreateOutputArchive(fo_)) {}

  template <typename T>
  typename std::enable_if<sizeof(typename T::CEREAL_BASE_TYPE) != 0, GenericCerealFileAppender&>::type
  operator<<(const T& entry) {
    so_(WithBaseType<typename T::CEREAL_BASE_TYPE>(entry));
    return *this;
  }

 private:
  GenericCerealFileAppender() = delete;
  GenericCerealFileAppender(const GenericCerealFileAppender&) = delete;
  void operator=(const GenericCerealFileAppender&) = delete;
  GenericCerealFileAppender(GenericCerealFileAppender&&) = delete;
  void operator=(GenericCerealFileAppender&&) = delete;

  std::ofstream fo_;
  typename CerealStreamType<T_CEREAL_FORMAT>::Output so_;
};
typedef GenericCerealFileAppender<CerealFormat::Default> CerealFileAppender;

// `CerealFileParser` de-cereal-izes records from file given their type and passes them over to `T_PROCESSOR`.
template <typename T_ENTRY, CerealFormat T_CEREAL_FORMAT>
class GenericCerealFileParser {
 public:
  explicit GenericCerealFileParser(const std::string& filename) : fi_(filename), si_(fi_) {}

  // `Next` calls `T_PROCESSOR::operator()(const T_ENTRY&)` for the next entry, or returns false.
  template <typename T_PROCESSOR>
  bool Next(T_PROCESSOR&& processor) {
    try {
      std::unique_ptr<T_ENTRY> entry;
      si_(entry);
      processor(*entry.get());
      return true;
    } catch (cereal::Exception&) {
      // TODO(dkorolev): Should check whether we have reached the end of the file here, otherwise
      // distinguishing between EOF vs. de-cereal-ization issue in the middle of the file will be
      // pain in the ass.
      return false;
    }
  }

  // `Next` calls `T_PROCESSOR::operator()(const T_ACTUAL_ENTRY_TYPE&)` for the next entry, or returns false.
  // Note that the actual entry type is being used in the calling method signature.
  // This also makes it implausible to pass in a lambda here, thus the parameter is only passed by reference.
  // In order for RTTI dispatching to work, class T_PROCESSOR should define two type:
  // 1) BASE_TYPE: The type of the base entry to de-serialize from the stream, and
  // 2) DERIVED_TYPE_LIST: An std::tuple<TYPE1, TYPE2, TYPE3, ...> of all the types that have to be matched.
  template <typename T_PROCESSOR>
  bool NextWithDispatching(T_PROCESSOR& processor) {
    try {
      std::unique_ptr<T_ENTRY> entry;
      si_(entry);
      bricks::rtti::RuntimeTupleDispatcher<typename T_PROCESSOR::BASE_TYPE,
                                           typename T_PROCESSOR::DERIVED_TYPE_LIST>::DispatchCall(*entry.get(),
                                                                                                  processor);
      return true;
    } catch (cereal::Exception&) {
      // TODO(dkorolev): Should check whether we have reached the end of the file here, otherwise
      // distinguishing between EOF vs. de-cereal-ization issue in the middle of the file will be
      // pain in the ass.
      return false;
    }
  }

 private:
  GenericCerealFileParser() = delete;
  GenericCerealFileParser(const GenericCerealFileParser&) = delete;
  void operator=(const GenericCerealFileParser&) = delete;
  GenericCerealFileParser(GenericCerealFileParser&&) = delete;
  void operator=(GenericCerealFileParser&&) = delete;

  std::ifstream fi_;
  typename CerealStreamType<T_CEREAL_FORMAT>::Input si_;
};
template <typename T_ENTRY>
using CerealFileParser = GenericCerealFileParser<T_ENTRY, CerealFormat::Default>;

template <typename OSTREAM, typename T>
inline OSTREAM& AppendAsJSON(OSTREAM& os, T&& object) {
  cerealize::CerealStreamType<cerealize::CerealFormat::JSON>::CreateOutputArchive(os)(object);
  return os;
}

struct AsConstCharPtr {
  static inline const char* Run(const char* s) { return s; }
  static inline const char* Run(const std::string& s) { return s.c_str(); }
};

template <typename OSTREAM, typename T, typename S>
inline OSTREAM& AppendAsJSON(OSTREAM& os, T&& object, S&& name) {
  cerealize::CerealStreamType<cerealize::CerealFormat::JSON>::CreateOutputArchive(os)(
      cereal::make_nvp<typename std::remove_reference<T>::type>(AsConstCharPtr::Run(name), object));
  return os;
}

template <typename T>
inline std::string JSON(T&& object) {
  std::ostringstream os;
  AppendAsJSON(os, std::forward<T>(object));
  return os.str();
}

template <typename T, typename S>
inline std::string JSON(T&& object, S&& name) {
  std::ostringstream os;
  AppendAsJSON(os, std::forward<T>(object), name);
  return os.str();
}

// JSON parse error handling logic.
// By default, an exception is thrown.
// If a user class defines the `FromInvalidJSON()` method, ParseJSON() is a non-throwing call,
// and that method will be called instead.

template <typename T>
struct HasFromInvalidJSON {
  typedef char one;
  typedef long two;

  template <typename C>
  static one test(decltype(&C::FromInvalidJSON));
  template <typename C>
  static two test(...);

  constexpr static bool value = (sizeof(test<T>(0)) == sizeof(one));
};

template <typename T, bool B>
struct BricksParseJSONError {};

template <typename T>
struct BricksParseJSONError<T, false> {
  static void HandleParseJSONError(const std::string& input_json, T&) {
    BRICKS_THROW(bricks::ParseJSONException(input_json));
  }
};

template <typename T>
struct BricksParseJSONError<T, true> {
  static void HandleParseJSONError(const std::string& input_json, T& output_object) {
    output_object.FromInvalidJSON(input_json);
  }
};

template <typename T>
inline const T& ParseJSON(const std::string& input_json, T& output_object) {
  try {
    std::istringstream is(input_json);
    cereal::JSONInputArchive ar(is);
    ar(output_object);
  } catch (cereal::Exception&) {
    BricksParseJSONError<T, HasFromInvalidJSON<typename std::remove_reference<T>::type>::value>::
        HandleParseJSONError(input_json, output_object);
  }
  return output_object;
}

template <typename T>
inline T ParseJSON(const std::string& input_json) {
  T placeholder;
  ParseJSON(input_json, placeholder);
  // Can not just do `return ParseJSON()`, since it would not handle ownership transfer for `std::unique_ptr<>`.
  return placeholder;
}

}  // namespace cerealize
}  // namespace bricks

using bricks::cerealize::JSON;
using bricks::cerealize::ParseJSON;
using bricks::cerealize::WithBaseType;

#endif  // BRICKS_CEREALIZE_H
