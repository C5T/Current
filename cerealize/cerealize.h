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
#include "../3party/cereal/include/types/set.hpp"

#include "../3party/cereal/include/types/polymorphic.hpp"

#include "../3party/cereal/include/archives/binary.hpp"
#include "../3party/cereal/include/archives/json.hpp"
#include "../3party/cereal/include/archives/xml.hpp"

#include "../3party/cereal/include/external/base64.hpp"

#include "../rtti/dispatcher.h"
#include "../template/rmref.h"

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
  constexpr static bool value = is_string_type_impl<rmconstref<TOP_LEVEL_T>>::value;
};

// Helper compile-time test that certain type can be serialized via cereal.
template <typename T>
struct is_read_cerealizable {
  constexpr static bool value =
      !is_string_type<T>::value &&
      cereal::traits::is_input_serializable<rmconstref<T>, cereal::JSONInputArchive>::value;
};

template <typename T>
struct is_write_cerealizable {
  constexpr static bool value =
      !is_string_type<T>::value &&
      cereal::traits::is_output_serializable<rmconstref<T>, cereal::JSONOutputArchive>::value;
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

// ***** !!! ATTENTION !!! *****
// `CerealFileAppenderBase`, which handles the stream for all file serialization
// classes, DOES NOT guarantee an exclusive access to the file.
// THIS MAY CAUSE DATA CORRUPTION.
// It is user responsibility to ensure that there is no simultaneous access.
// TODO(dkorolev): Think of platform-independent layer for filesystem locks.
class CerealFileAppenderBase {
 public:
  explicit CerealFileAppenderBase(const std::string& filename, bool append = true)
      : fo_(filename, (append ? std::ofstream::app : std::ofstream::trunc) | std::ofstream::binary),
        initial_stream_position_(fo_.tellp()) {}

  inline size_t EntriesAppended() const { return entries_appended_; }
  inline size_t BytesAppended() const { return current_stream_position() - initial_stream_position_; }
  inline size_t TotalFileSize() const { return current_stream_position(); }

 protected:
  mutable std::ofstream fo_;
  size_t entries_appended_ = 0;

 private:
  const std::streampos initial_stream_position_;

  size_t current_stream_position() const {
    const std::streampos p = fo_.tellp();
    if (p >= 0) {
      return static_cast<size_t>(p);
    } else {
      BRICKS_THROW(bricks::CerealizeFileStreamErrorException());  // LCOV_EXCL_LINE
    }
  }

  CerealFileAppenderBase() = delete;
  CerealFileAppenderBase(const CerealFileAppenderBase&) = delete;
  void operator=(const CerealFileAppenderBase&) = delete;
  CerealFileAppenderBase(CerealFileAppenderBase&&) = delete;
  void operator=(CerealFileAppenderBase&&) = delete;
};

// `CerealBinaryFileAppender` appends cereal-ized records to a file in binary format.
// Writes are performed using templated `operator <<(const T& entry)`.
// If type `T` defines a typedef of `CEREAL_BASE_TYPE`, polymorphic serialization is used.
// TODO(dkorolev): Support for non-polymorhic types.
class CerealBinaryFileAppender : public CerealFileAppenderBase {
 public:
  explicit CerealBinaryFileAppender(const std::string& filename, bool append = true)
      : CerealFileAppenderBase(filename, append), so_(cereal::BinaryOutputArchive(fo_)) {}

  template <typename T>
  typename std::enable_if<sizeof(typename T::CEREAL_BASE_TYPE) != 0, CerealBinaryFileAppender&>::type
  operator<<(const T& entry) {
    so_(WithBaseType<typename T::CEREAL_BASE_TYPE>(entry));
    ++entries_appended_;
    return *this;
  }

 private:
  cereal::BinaryOutputArchive so_;
};

// `CerealJSONFileAppender` appends cereal-ized records to a file in JSON format.
// Each entry is written as a separate line containing full JSON record.
// Writes are performed using templated `operator <<(const T& entry)`.
// If type `T` defines a typedef of `CEREAL_BASE_TYPE`, polymorphic serialization is used.
// TODO(dkorolev): Support for non-polymorhic types.
class CerealJSONFileAppender : public CerealFileAppenderBase {
 public:
  explicit CerealJSONFileAppender(const std::string& filename, bool append = true)
      : CerealFileAppenderBase(filename, append) {}

  template <typename T>
  typename std::enable_if<sizeof(typename T::CEREAL_BASE_TYPE) != 0, CerealJSONFileAppender&>::type operator<<(
      const T& entry) {
    // One entry per line format.
    // JSONOutputArchive writes the final '}' after going out of the scope.
    {
      cereal::JSONOutputArchive so_(fo_, cereal::JSONOutputArchive::Options::NoIndent());
      so_(WithBaseType<typename T::CEREAL_BASE_TYPE>(entry));
    }
    fo_ << '\n';
    ++entries_appended_;
    return *this;
  }
};

template <CerealFormat>
struct CerealGenericFileAppender {};

template <>
struct CerealGenericFileAppender<CerealFormat::Binary> {
  typedef CerealBinaryFileAppender type;
};

template <>
struct CerealGenericFileAppender<CerealFormat::JSON> {
  typedef CerealJSONFileAppender type;
};

template <CerealFormat T_FORMAT = CerealFormat::Default>
using CerealFileAppender = typename CerealGenericFileAppender<T_FORMAT>::type;

// `CerealBinaryFileParser` de-cereal-izes records from binary file given their type
// and passes them over to `T_PROCESSOR`.
template <typename T_ENTRY>
class CerealBinaryFileParser {
 public:
  explicit CerealBinaryFileParser(const std::string& filename) : fi_(filename), si_(fi_) {}

  // `Next` calls `T_PROCESSOR::operator()(const T_ENTRY&)` for the next entry, or returns false.
  template <typename T_PROCESSOR>
  bool Next(T_PROCESSOR&& processor) {
    if (fi_.peek() != std::char_traits<char>::eof()) {  // This is safe with any next byte in file.
      std::unique_ptr<T_ENTRY> entry;
      si_(entry);
      processor(*entry.get());
      return true;
    } else {
      return false;
    }
  }

  // `NextWithDispatching` calls `T_PROCESSOR::operator()(const T_ACTUAL_ENTRY_TYPE&)`
  // for the next entry, or returns false.
  // Note that the actual entry type is being used in the calling method signature.
  // This also makes it implausible to pass in a lambda here, thus the parameter is only passed by reference.
  // In order for RTTI dispatching to work, class T_PROCESSOR should define two type:
  // 1) BASE_TYPE: The type of the base entry to de-serialize from the stream, and
  // 2) DERIVED_TYPE_LIST: An std::tuple<TYPE1, TYPE2, TYPE3, ...> of all the types that have to be matched.
  template <typename T_PROCESSOR>
  bool NextWithDispatching(T_PROCESSOR& processor) {
    if (fi_.peek() != std::char_traits<char>::eof()) {  // This is safe with any next byte in file.
      std::unique_ptr<T_ENTRY> entry;
      si_(entry);
      bricks::rtti::RuntimeTupleDispatcher<typename T_PROCESSOR::BASE_TYPE,
                                           typename T_PROCESSOR::DERIVED_TYPE_LIST>::DispatchCall(*entry.get(),
                                                                                                  processor);
      return true;
    } else {
      return false;
    }
  }

 private:
  CerealBinaryFileParser() = delete;
  CerealBinaryFileParser(const CerealBinaryFileParser&) = delete;
  void operator=(const CerealBinaryFileParser&) = delete;
  CerealBinaryFileParser(CerealBinaryFileParser&&) = delete;
  void operator=(CerealBinaryFileParser&&) = delete;

  std::ifstream fi_;
  cereal::BinaryInputArchive si_;
};

// `CerealJSONFileParser` de-cereal-izes records from JSON file given their type
// and passes them over to `T_PROCESSOR`. Each line in the file expected to be a
// full JSON record for one entry.
template <typename T_ENTRY>
class CerealJSONFileParser {
 public:
  explicit CerealJSONFileParser(const std::string& filename) : fi_(filename) {}

  // `Next` calls `T_PROCESSOR::operator()(const T_ENTRY&)` for the next entry, or returns false.
  template <typename T_PROCESSOR>
  bool Next(T_PROCESSOR&& processor) {
    std::unique_ptr<T_ENTRY> entry(GetNextEntry());
    if (entry) {
      processor(*entry.get());
      return true;
    } else {
      return false;
    }
  }

  // `NextWithDispatching` calls `T_PROCESSOR::operator()(const T_ACTUAL_ENTRY_TYPE&)`
  // for the next entry, or returns false.
  // Note that the actual entry type is being used in the calling method signature.
  // This also makes it implausible to pass in a lambda here, thus the parameter is only passed by reference.
  // In order for RTTI dispatching to work, class T_PROCESSOR should define two type:
  // 1) BASE_TYPE: The type of the base entry to de-serialize from the stream, and
  // 2) DERIVED_TYPE_LIST: An std::tuple<TYPE1, TYPE2, TYPE3, ...> of all the types that have to be matched.
  template <typename T_PROCESSOR>
  bool NextWithDispatching(T_PROCESSOR& processor) {
    std::unique_ptr<T_ENTRY> entry(GetNextEntry());
    if (entry) {
      bricks::rtti::RuntimeTupleDispatcher<typename T_PROCESSOR::BASE_TYPE,
                                           typename T_PROCESSOR::DERIVED_TYPE_LIST>::DispatchCall(*entry.get(),
                                                                                                  processor);
      return true;
    } else {
      return false;
    }
  }

 private:
  CerealJSONFileParser() = delete;
  CerealJSONFileParser(const CerealJSONFileParser&) = delete;
  void operator=(const CerealJSONFileParser&) = delete;
  CerealJSONFileParser(CerealJSONFileParser&&) = delete;
  void operator=(CerealJSONFileParser&&) = delete;

  std::unique_ptr<T_ENTRY> GetNextEntry() {
    std::string line;
    if (std::getline(fi_, line)) {
      std::istringstream iss(line);
      std::unique_ptr<T_ENTRY> entry;
      cereal::JSONInputArchive si_(iss);
      si_(entry);
      return std::move(entry);
    } else {
      return nullptr;
    }
  }

  std::ifstream fi_;
};

template <typename T_ENTRY, CerealFormat>
struct CerealGenericFileParser {};

template <typename T_ENTRY>
struct CerealGenericFileParser<T_ENTRY, CerealFormat::Binary> {
  typedef CerealBinaryFileParser<T_ENTRY> type;
};

template <typename T_ENTRY>
struct CerealGenericFileParser<T_ENTRY, CerealFormat::JSON> {
  typedef CerealJSONFileParser<T_ENTRY> type;
};

template <typename T_ENTRY, CerealFormat T_FORMAT = CerealFormat::Default>
using CerealFileParser = typename CerealGenericFileParser<T_ENTRY, T_FORMAT>::type;

template <typename OSTREAM, typename T>
inline OSTREAM& AppendAsJSON(OSTREAM& os, T&& object) {
  cereal::JSONOutputArchive so(os);
  so(object);
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
    BricksParseJSONError<T, HasFromInvalidJSON<rmref<T>>::value>::HandleParseJSONError(input_json,
                                                                                       output_object);
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

inline std::string Base64Encode(const std::string& s) {
  return base64::encode(reinterpret_cast<const unsigned char*>(s.c_str()), s.length());
}

}  // namespace cerealize
}  // namespace bricks

using bricks::cerealize::JSON;
using bricks::cerealize::ParseJSON;
using bricks::cerealize::WithBaseType;

#endif  // BRICKS_CEREALIZE_H
