/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_SHERLOCK_FILTER_H
#define CURRENT_SHERLOCK_FILTER_H

#include "../port.h"

#include "../TypeSystem/variant.h"
#include "../Blocks/SS/ss.h"

namespace current {
namespace sherlock {

// The "generic" case: Assume `SUBSCRIBER_TYPE` is a specific type, and `ENTRY` is a variant containing it.
// This case is not generic by itself, it just happens to be the most general template "specialization".
template <typename SUBSCRIBER_TYPE, typename ENTRY>
struct SubscriberFilter {
  static_assert(TypeListContains<typename ENTRY::T_TYPELIST, SUBSCRIBER_TYPE>::value,
                "Sherlock subscription filter by type requires the top-level type of the stream"
                " to be a `Variant<>` containing the desired type as a sub-type.");

  using entry_t = SUBSCRIBER_TYPE;

  template <typename F, typename E>
  static ss::EntryResponse ProcessEntry(F&& f, E&& entry, idxts_t current, idxts_t last) {
    const E& entry_cref = entry;
    if (Exists<SUBSCRIBER_TYPE>(entry_cref)) {
      return f(Value<SUBSCRIBER_TYPE>(std::forward<E>(entry)), current, last);
    } else {
      return ss::EntryResponse::More;
    }
  }
};

// The "special" case, which actually *is* the generic, default one: pass records unchanged.

struct SubscribeToAllTypes {};  // The class for the policy which is the default one.

template <typename ENTRY>
struct SubscriberFilter<SubscribeToAllTypes, ENTRY> {
  using entry_t = ENTRY;

  template <typename F, typename E>
  static ss::EntryResponse ProcessEntry(F&& f, E&& entry, idxts_t current, idxts_t last) {
    return f(std::forward<E>(entry), current, last);
  }
};

}  // namespace sherlock
}  // namespace current

#endif  // CURRENT_SHERLOCK_FILTER_H
