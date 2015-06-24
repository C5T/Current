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

#ifndef BRICKS_CEREALIZE_CEREALIZE_H
#define BRICKS_CEREALIZE_CEREALIZE_H

#include <fstream>
#include <string>
#include <sstream>

#include "json.h"
#include "constants.h"
#include "exceptions.h"

#include "../strings/is_string_type.h"
#include "../rtti/dispatcher.h"
#include "../template/decay.h"

#include "../../Blocks/SS/ss.h"

#include "../../3rdparty/cereal/include/types/string.hpp"
#include "../../3rdparty/cereal/include/types/vector.hpp"
#include "../../3rdparty/cereal/include/types/map.hpp"
#include "../../3rdparty/cereal/include/types/set.hpp"

#include "../../3rdparty/cereal/include/types/polymorphic.hpp"

#include "../../3rdparty/cereal/include/archives/binary.hpp"
#include "../../3rdparty/cereal/include/archives/json.hpp"
#include "../../3rdparty/cereal/include/archives/xml.hpp"

#include "../../3rdparty/cereal/include/external/base64.hpp"

namespace bricks {
namespace cerealize {

// Helper compile-time test that certain type can be serialized via cereal.
template <typename T>
struct is_read_cerealizable {
  constexpr static bool value =
      !strings::is_string_type<T>::value &&
      cereal::traits::is_input_serializable<decay<T>, cereal::JSONInputArchive>::value;
};

template <typename T>
struct is_write_cerealizable {
  constexpr static bool value =
      !strings::is_string_type<T>::value &&
      cereal::traits::is_output_serializable<decay<T>, cereal::JSONOutputArchive>::value;
};

template <typename T>
struct is_cerealizable {
  constexpr static bool value = is_read_cerealizable<T>::value && is_write_cerealizable<T>::value;
};

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
  // This "ios" here has nothing to do with Apple, but with Microsoft: in Windows
  // `initial_stream_position_` gets initialized with zero without it. -- D.K.
  explicit CerealFileAppenderBase(const std::string& filename, bool append = true)
      : fo_(filename, (append ? std::ofstream::app : std::ofstream::trunc) | std::ofstream::binary),
        initial_stream_position_(static_cast<size_t>((fo_.seekp(0, std::ios_base::end), fo_.tellp()))) {}

  inline size_t EntriesAppended() const { return entries_appended_; }
  inline size_t BytesAppended() const {
    const size_t current = current_stream_position();
    assert(current >= initial_stream_position_);  // TFU if it's the case.
    return current - initial_stream_position_;
  }
  inline size_t TotalFileSize() const { return current_stream_position(); }

 protected:
  mutable std::ofstream fo_;
  size_t entries_appended_ = 0;

 private:
  const size_t initial_stream_position_;

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
template <typename ENTRY, class CLONER>
class CerealBinaryFileAppenderImpl : public CerealFileAppenderBase {
 public:
  typedef ENTRY T_ENTRY;
  explicit CerealBinaryFileAppenderImpl(const std::string& filename, bool append = true)
      : CerealFileAppenderBase(filename, append), so_(cereal::BinaryOutputArchive(fo_)) {}

  size_t DoPublish(const T_ENTRY& entry) {
    so_(entry);
    return ++entries_appended_;
  }

  CerealBinaryFileAppenderImpl& operator<<(const T_ENTRY& entry) {
    DoPublish(entry);
    return *this;
  }

  // Support direct serialization of derived types in polymorphic cases.
  template <typename E,
            typename UNIQUE_PTR = T_ENTRY,
            typename UNIQUE_PTR_ENTRY = typename UNIQUE_PTR::element_type>
  typename std::enable_if<std::is_same<UNIQUE_PTR_ENTRY, typename E::CEREAL_BASE_TYPE>::value, size_t>::type
  DoPublish(const E& entry) {
    so_(WithBaseType<typename T_ENTRY::element_type>(entry));
    return ++entries_appended_;
  }
  template <typename E,
            typename UNIQUE_PTR = T_ENTRY,
            typename UNIQUE_PTR_ENTRY = typename UNIQUE_PTR::element_type>
  typename std::enable_if<std::is_same<UNIQUE_PTR_ENTRY, typename E::CEREAL_BASE_TYPE>::value,
                          CerealBinaryFileAppenderImpl&>::type
  operator<<(const E& entry) {
    DoPublish<E, UNIQUE_PTR, UNIQUE_PTR_ENTRY>(entry);
    return *this;
  }

 private:
  cereal::BinaryOutputArchive so_;
};

template <typename E, class CLONER>
using CerealBinaryFileAppender = blocks::ss::Publisher<CerealBinaryFileAppenderImpl<E, CLONER>, E>;

// `CerealJSONFileAppender` appends cereal-ized records to a file in JSON format.
// Each entry is written as a separate line containing full JSON record.
// Writes are performed using templated `operator <<(const T& entry)`.
// If type `T` defines a typedef of `CEREAL_BASE_TYPE`, polymorphic serialization is used.
template <typename ENTRY, class CLONER>
class CerealJSONFileAppenderImpl : public CerealFileAppenderBase {
 public:
  typedef ENTRY T_ENTRY;
  explicit CerealJSONFileAppenderImpl(const std::string& filename, bool append = true)
      : CerealFileAppenderBase(filename, append) {}

  size_t DoPublish(const T_ENTRY& entry) {
    {
      cereal::JSONOutputArchive so_(fo_, cereal::JSONOutputArchive::Options::NoIndent());
      so_(cereal::make_nvp("e", entry));  // "e" for "entry".
    }
    fo_ << '\n';
    return ++entries_appended_;
  }

  CerealJSONFileAppenderImpl& operator<<(const T_ENTRY& entry) {
    DoPublish(entry);
    return *this;
    {
      cereal::JSONOutputArchive so_(fo_, cereal::JSONOutputArchive::Options::NoIndent());
      so_(cereal::make_nvp(Constants::DefaultJSONSerializeNonPolymorphicEntryName(), entry));
    }
    fo_ << '\n';
    ++entries_appended_;
    return *this;
  }

  // Support direct serialization of derived types in polymorphic cases.
  template <typename E,
            typename UNIQUE_PTR = T_ENTRY,
            typename UNIQUE_PTR_ENTRY = typename UNIQUE_PTR::element_type>
  typename std::enable_if<std::is_same<UNIQUE_PTR_ENTRY, typename E::CEREAL_BASE_TYPE>::value, size_t>::type
  DoPublish(const E& entry) {
    {
      cereal::JSONOutputArchive so_(fo_, cereal::JSONOutputArchive::Options::NoIndent());
      so_(cereal::make_nvp(Constants::DefaultJSONSerializePolymorphicEntryName(),
                           WithBaseType<typename T_ENTRY::element_type>(entry)));
    }
    fo_ << '\n';
    return ++entries_appended_;
  }
  template <typename E,
            typename UNIQUE_PTR = T_ENTRY,
            typename UNIQUE_PTR_ENTRY = typename UNIQUE_PTR::element_type>
  typename std::enable_if<std::is_same<UNIQUE_PTR_ENTRY, typename E::CEREAL_BASE_TYPE>::value,
                          CerealJSONFileAppenderImpl&>::type
  operator<<(const E& entry) {
    DoPublish<E, UNIQUE_PTR, UNIQUE_PTR_ENTRY>(entry);
    return *this;
  }
};

template <typename E, class CLONER>
using CerealJSONFileAppender = blocks::ss::Publisher<CerealJSONFileAppenderImpl<E, CLONER>, E>;

template <typename E, class CLONER, CerealFormat>
struct CerealGenericFileAppender {};

template <typename E, class CLONER>
struct CerealGenericFileAppender<E, CLONER, CerealFormat::Binary> {
  typedef CerealBinaryFileAppender<E, CLONER> type;
};

template <typename E, typename CLONER>
struct CerealGenericFileAppender<E, CLONER, CerealFormat::JSON> {
  typedef CerealJSONFileAppender<E, CLONER> type;
};

template <typename E, class CLONER, CerealFormat FORMAT = CerealFormat::Default>
using CerealFileAppender = typename CerealGenericFileAppender<E, CLONER, FORMAT>::type;

// `CerealBinaryFileParser` de-cereal-izes records from binary file given their type
// and passes them over to `T_PROCESSOR`.
template <typename ENTRY>
class CerealBinaryFileParser {
 public:
  typedef ENTRY T_ENTRY;
  explicit CerealBinaryFileParser(const std::string& filename) : fi_(filename), si_(fi_) {}

  // `Next` calls `T_PROCESSOR::operator()(T_ENTRY)` for the next entry, or returns false.
  // The handler can taking an rvalue reference and own the freshly deconstructed entry.
  template <typename T_PROCESSOR>
  bool Next(T_PROCESSOR&& processor) {
    if (fi_.peek() != std::char_traits<char>::eof()) {  // This is safe with any next byte in file.
      T_ENTRY entry;
      si_(entry);
      processor(std::move(entry));
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
  typename std::enable_if<bricks::is_unique_ptr<T_ENTRY>::value, bool>::type NextWithDispatching(
      T_PROCESSOR& processor) {
    if (fi_.peek() != std::char_traits<char>::eof()) {  // This is safe with any next byte in file.
      T_ENTRY entry;
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
template <typename ENTRY>
class CerealJSONFileParser {
 public:
  typedef ENTRY T_ENTRY;
  explicit CerealJSONFileParser(const std::string& filename) : fi_(filename) {}

  // `Next` calls `T_PROCESSOR::operator()(const T_ENTRY&)` for the next entry, or returns false.
  template <typename T_PROCESSOR>
  bool Next(T_PROCESSOR&& processor) {
    try {
      processor(std::move(GetNextEntryOrThrow()));
      return true;
    } catch (const CerealizeFileStreamErrorException&) {
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
  template <typename T_PROCESSOR, typename T_ENTRY = ENTRY>
  typename std::enable_if<bricks::is_unique_ptr<T_ENTRY>::value, bool>::type NextWithDispatching(
      T_PROCESSOR& processor) {
    try {
      bricks::rtti::RuntimeTupleDispatcher<
          typename T_PROCESSOR::BASE_TYPE,
          typename T_PROCESSOR::DERIVED_TYPE_LIST>::DispatchCall(*GetNextEntryOrThrow().get(), processor);
    } catch (const CerealizeFileStreamErrorException&) {
      return false;
    }
  }

 private:
  CerealJSONFileParser() = delete;
  CerealJSONFileParser(const CerealJSONFileParser&) = delete;
  void operator=(const CerealJSONFileParser&) = delete;
  CerealJSONFileParser(CerealJSONFileParser&&) = delete;
  void operator=(CerealJSONFileParser&&) = delete;

  T_ENTRY GetNextEntryOrThrow() {
    std::string line;
    if (std::getline(fi_, line)) {
      std::istringstream iss(line);
      T_ENTRY entry;
      cereal::JSONInputArchive si_(iss);
      si_(entry);
      return entry;
    } else {
      BRICKS_THROW(CerealizeFileStreamErrorException());
    }
  }

  std::ifstream fi_;
};

template <typename ENTRY, CerealFormat>
struct CerealGenericFileParser {};

template <typename ENTRY>
struct CerealGenericFileParser<ENTRY, CerealFormat::Binary> {
  typedef CerealBinaryFileParser<ENTRY> type;
};

template <typename ENTRY>
struct CerealGenericFileParser<ENTRY, CerealFormat::JSON> {
  typedef CerealJSONFileParser<ENTRY> type;
};

template <typename ENTRY, CerealFormat FORMAT = CerealFormat::Default>
using CerealFileParser = typename CerealGenericFileParser<ENTRY, FORMAT>::type;

}  // namespace cerealize
}  // namespace bricks

#endif  // BRICKS_CEREALIZE_CEREALIZE_H
