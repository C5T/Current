/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef SHERLOCK_YODA_TYPES_H
#define SHERLOCK_YODA_TYPES_H

#include <future>
#include <map>
#include <type_traits>
#include <unordered_map>

#include "../Blocks/HTTP/api.h"

#include "../Bricks/exception.h"
#include "../Bricks/cerealize/cerealize.h"
#include "../Bricks/time/chrono.h"
#include "../Bricks/util/future.h"

namespace yoda {

// All user entries, which are supposed to be stored in Yoda, should be derived from this base class.
struct Padawan {
  using CEREAL_BASE_TYPE = Padawan;

  int64_t us;  // Can't change this into `std::chrono::microseconds`, it's Cereal here. -- D.K.
  Padawan() : us(current::time::Now().count()) {}
  virtual ~Padawan() = default;

  template <typename F>
  void ReportTimestamp(F&& f) const {
    f(us);
  }

  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(us));
  }
};

struct NonexistentEntryAccessed : current::Exception {};

// Wrapper to have in-memory view not only contain the entries, but also the index with which this entry
// has been pushed to the stream. This is to ensure that the in-memory view is kept up to date
// in case of overwrites, when the value written earlier can come from the stream after it was overwritten.
// TODO(dkorolev): Retire this index-based logic, as it's no longer going to be current.
template <typename ENTRY>
struct EntryWithIndex {
  size_t index;
  ENTRY entry;
  EntryWithIndex() : index(static_cast<size_t>(-1)) {}
  EntryWithIndex(size_t index, const ENTRY& entry) : index(index), entry(entry) {}
  EntryWithIndex(size_t index, ENTRY&& entry) : index(index), entry(std::move(entry)) {}
  void Update(size_t i, const ENTRY& e) {
    index = i;
    entry = e;
  }
  void Update(size_t i, ENTRY&& e) {
    index = i;
    entry = std::move(e);
  }
};

// Wrapper to return possibly nonexistent entries.
struct EntryNotFoundHTTPResponse {
  const std::string message = "NOT_FOUND";
  EntryNotFoundHTTPResponse() {}
  template <typename A>
  void save(A& ar) const {
    ar(CEREAL_NVP(message));
  }
};

template <typename ENTRY>
struct EntryWrapper {
  EntryWrapper() = default;
  explicit EntryWrapper(const ENTRY& entry) : exists(true), entry(&entry) {}

  operator bool() const { return exists; }

  operator const ENTRY&() const {
    if (exists) {
      return *entry;
    } else {
      throw NonexistentEntryAccessed();
    }
  }
  void RespondViaHTTP(Request r) const {
    if (exists) {
      r(*entry, "entry");
    } else {
      r(EntryNotFoundHTTPResponse(), "error", HTTPResponseCode.NotFound);
    }
  }

 private:
  bool exists = false;
  const ENTRY* entry = nullptr;
};

#if 0

// Helper structures that the user can derive their entries from
// to signal Yoda to behave in a non-default way.

struct AllowNonThrowingGet {
  // Inheriting from this base class, or simply defining the below constant manually,
  // will result in `Get()`-s for non-existing keys to be non-throwing.
  // NOTE: This behavior requires the user class to derive from `Nullable` as well, see below.
  constexpr static bool allow_nonthrowing_get = true;
};

struct AllowOverwriteOnAdd {
  // Inheriting from this base class, or simply defining the below constant manually,
  // will result in `Add()`-s for already existing keys to silently overwrite previous values.
  constexpr static bool allow_overwrite_on_add = true;
};

// Helper structures that the user can derive their entries from
// to implement particular Yoda functionality.

// By deriving from `Nullable` (and adding `using Nullable::Nullable`),
// the user indicates that their entry type supports creation of a non-existing instance.
// This is a requirement for a) non-throwing `Get()`, and b) for `Delete()` part of the API.
// TODO(dkorolev): Add a compile-time check that `Nullable` user types allow constructing themselves as null-s.
enum NullEntryTypeHelper { NullEntry };
struct Nullable {
  const bool exists;
  Nullable() : exists(true) {}
  Nullable(NullEntryTypeHelper) : exists(false) {}
};

// By deriving from `Deletable`, the user commits to serializing the `Nullable::exists` field,
// thus enabling delete-friendly storage.
// The user should derive from both `Nullable` and `Deletable` for full `Delete()` support via Yoda.
// TODO(dkorolev): Add the runtime check that certain type does indeed serialize the `exists` field and test it.
struct Deletable {};

#endif

}  // namespace yoda

#endif  // SHERLOCK_YODA_TYPES_H
