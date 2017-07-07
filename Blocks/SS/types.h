/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2017 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BLOCKS_SS_TYPES_H
#define BLOCKS_SS_TYPES_H

#include "../../port.h"

#include "../../TypeSystem/variant.h"
#include "../../Bricks/time/chrono.h"
#include "../../Bricks/sync/locks.h"

namespace current {
namespace ss {

template <typename ENTRY, typename STREAM_ENTRY>
struct CanPublish {
  constexpr static bool value = false;
};

template <typename E>
struct CanPublish<E, E> {
  constexpr static bool value = true;
};

template <typename E, typename NAME, typename... TS>
struct CanPublish<E, VariantImpl<NAME, TypeListImpl<TS...>>> {
  constexpr static bool value = TypeListContains<TypeListImpl<TS...>, E>::value;
};

// Special case, mostly for unit tests: can publish C-strings as C++ strings.
template <>
struct CanPublish<char*, std::string> {
  constexpr static bool value = true;
};
template <int N>
struct CanPublish<char[N], std::string> {
  constexpr static bool value = true;
};

}  // namespace current::ss
}  // namespace current

#endif  // BLOCKS_SS_TYPES_H
