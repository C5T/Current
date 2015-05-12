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

#include <map>
#include <unordered_map>
#include <type_traits>

#include "../../Bricks/cerealize/cerealize.h"

namespace yoda {

// All user entries, which are supposed to be stored in Yoda, should be derived from this base structure.
struct Padawan {
  uint64_t ms;
  virtual ~Padawan() = default;

  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(ms));
  }
};

// Wrapper to return non-existing entries.
template <typename T_ENTRY>
struct EntryWrapper {
  EntryWrapper() = default;
  explicit EntryWrapper(const T_ENTRY& entry) : exists(true), entry(&entry) {}
  operator bool() const { return exists; }
  const T_ENTRY& operator()() const { return *entry; }

 private:
  bool exists = false;
  const T_ENTRY* entry = nullptr;
};

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

}  // namespace yoda

#endif  // SHERLOCK_YODA_TYPES_H
