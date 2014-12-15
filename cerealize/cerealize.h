#ifndef BRICKS_CEREALIZE_H
#define BRICKS_CEREALIZE_H

#include <fstream>

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

// Helper code to simplify wrapping objects into `unique_ptr`-s of their base types
// for Cereal to serialize them as polymorphic types.
struct EmptyDeleterForAnyType {
  template <typename T>
  void operator()(T&) {
  }
  template <typename T>
  void operator()(T*) {
  }
};

template <typename BASE, typename ENTRY>
typename std::enable_if<std::is_base_of<BASE, ENTRY>::value,
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
};

template <>
struct CerealStreamType<CerealFormat::JSON> {
  typedef cereal::JSONInputArchive Input;
  typedef cereal::JSONOutputArchive Output;
};

// `CerealFileAppender` appends cereal-ized records to a file.
// The format is selected by a template parameter, defaults to binary. All formats are supported.
// Writes are performed using templated `operator <<(const T& entry)`.
// Is type `T` defines a typedef of `CEREAL_BASE_TYPE`, polymorphic serialization is used.
template <CerealFormat T_CEREAL_FORMAT>
class GenericCerealFileAppender {
 public:
  explicit GenericCerealFileAppender(const std::string& filename, bool append = true)
      : fo_(filename, (append ? std::ofstream::app : std::ofstream::trunc) | std::ofstream::binary), so_(fo_) {
  }

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
  explicit GenericCerealFileParser(const std::string& filename) : fi_(filename), si_(fi_) {
  }

  // `Next` calls `T_PROCESSOR::operator()(const T_ENTRY&)` for the next entry, or returns false.
  template <typename T_PROCESSOR>
  bool Next(T_PROCESSOR& processor) {
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

  // `NextLambda` calls `lambda(const T_ENTRY&)` for the next entry, or returns false.
  // TODO(dkorolev): Chat with Alex -- I did not find a way to support lambdas (which are passed by value)
  // and user-defined classes (which are passed by reference) under the same method name.
  // Since copying user-defined objects is not only inefficient but plain wrong,
  // I went with two methods instead.
  template <typename T_PROCESSOR>
  bool NextLambda(T_PROCESSOR processor) {
    return Next(processor);
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

}  // namespace cerealize
}  // namespace bricks

#endif  // BRICKS_CEREALIZE_H
