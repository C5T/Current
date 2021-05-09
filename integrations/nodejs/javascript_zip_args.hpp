#pragma once

#include <tuple>

namespace current {
namespace javascript {
namespace impl {

template <size_t I, class T>
struct ZippedArgWithIndex final {
  constexpr static size_t index = I;
  using type = T;
};

template <class ZIPPED_TUPLE, size_t ZIPPED_TUPLE_SIZE, class UNZIPPED_TUPLE>
struct ZipArgsWithIndexesImpl;

template <typename... ZIPPED_TYPES, size_t I, typename UNZIPPED_TYPE, typename... UNZIPPED_TYPES>
struct ZipArgsWithIndexesImpl<std::tuple<ZIPPED_TYPES...>, I, std::tuple<UNZIPPED_TYPE, UNZIPPED_TYPES...>> final {
  using type = typename ZipArgsWithIndexesImpl<std::tuple<ZIPPED_TYPES..., ZippedArgWithIndex<I, UNZIPPED_TYPE>>,
                                               I + 1,
                                               std::tuple<UNZIPPED_TYPES...>>::type;
};

template <class TUPLE_OF_ZIPPED_TYPES, size_t I>
struct ZipArgsWithIndexesImpl<TUPLE_OF_ZIPPED_TYPES, I, std::tuple<>> final {
  using type = TUPLE_OF_ZIPPED_TYPES;
};

template <typename... ARGS>
struct ZipArgsWithIndexes final {
  using type = typename ZipArgsWithIndexesImpl<std::tuple<>, 0u, std::tuple<ARGS...>>::type;
};

static_assert(
    std::is_same<
        ZipArgsWithIndexes<int, bool, std::string>::type,
        std::tuple<ZippedArgWithIndex<0, int>, ZippedArgWithIndex<1, bool>, ZippedArgWithIndex<2, std::string>>>::value,
    "");

}  // namespace impl
}  // namespace javascript
}  // namespace current
