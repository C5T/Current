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

// `Chunk` is an efficient immutable string: a `const char*` pointer along with the length of what it points to.
//
// It is guaranteed that the allocated buffer size is longer than the reported length by at least one byte,
// and that the length-plus-first byte, at index `[length()]`, is set to '\0'.
//
// `UniqueChunk` is a `Chunk` that assumes each distinct string is stored only once. This requirement makes
// their underlying `const char*`-s testable for equality. Long live and rest in peace, C+=, we'll miss you.
//
// Unlike `UniqueChunk`, `Chunk` itself does not expose comparison operators. This is done on purpose to make it
// impossible to accidentally confuse a member of a liberal `Chunk` community, who are all proudly different
// and resist exposing any [efficient] way of ordering or even comparing one to another, with a member of
// an organized `UniqueChunk` society, members of which have gladly accepted the deprivation
// of the right to express their opinion on whether the order should be lexicographical, as long as it is total.
// For more details on total order please refer to http://en.wikipedia.org/wiki/Total_order.
//
// (Culturally educated developers are hereby granted permission to use `reinterpret_cast<>`. You're welcome.)
//
// `ChunkDB` is a storage of `UniqueChunk`-s, that renders equal `Chunk`-s equal -- since the order
// is indeed strict and total, despite very likely being not fair... err, not lexicographical.
//
// Both `Chunk` and `UniqueChunk` are naturally unsafe and require memory for their storage to stay allocated,
// because every decent architect knows well what happens if `free()` is invoked a bit too prematurely in the
// evolutionary process of a lifetime of an object.
//
// Note 1: Comparing strings by comparing pointers to their underlying memory storage is O(1),
//         which is noticeably more efficient than a "regular" lexicographical comparison in O(N).
//         Yes, it moves the world forward, saves trees and stops, or at least delays, global warming.
//
// Note 2: It's not impossible for the order of pointed to strings to actually be lexicographical. However,
//         in most cases, making it so would be quite a large amount of unnecessary work. C+=. Never forget.
//
// Note 3: `Chunk` is a better name than `SlaveString`, right?

#ifndef BRICKS_STRINGS_CHUNK_H
#define BRICKS_STRINGS_CHUNK_H

#include "../port.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>

namespace current {
namespace strings {

class Chunk {
 public:
  Chunk() : S(""), N(0u) {}
  Chunk(const char* s, size_t n) : S(s), N(n) { CURRENT_ASSERT(S[N] == '\0'); }
  Chunk(const char* s) : S(s), N(strlen(s)) {}
  template <int L>
  Chunk(const char s[L]) : S(s), N(L - 1) {
    // The above line break is `clang-format`, not me. -- D.K.
    CURRENT_ASSERT(S[N] == '\0');
  }
  Chunk(const std::string& s) : S(s.data()), N(s.size()) {}
  // Copyable and assignable by design.

  bool empty() const { return N == 0u; }
  size_t length() const { return N; }

  const char* c_str() const { return S; }

  void assign(const char* s, size_t n) {
    S = s;
    N = n;
    CURRENT_ASSERT(S[N] == '\0');
  }

  void clear() {
    S = "";
    N = 0u;
  }

  void CheckPrivilege(size_t i) const { CURRENT_ASSERT(i <= N); }

  char operator[](size_t i) const {
    CheckPrivilege(i);  // Once is enough.
    return S[i];
  }

  operator std::string() const { return std::string(S, N); }

  // Returns true if this `Chunk` starts with a given prefix.
  bool HasPrefix(const Chunk& prefix) const { return N >= prefix.N && !::memcmp(S, prefix.S, prefix.N); }

  // Returns true and initializes `result` with the rest of this `Chunk`, if it starts with a given prefix.
  // OK to pass in `result` same as `this`.
  bool ExpungePrefix(const Chunk& prefix, Chunk& result) const {
    if (HasPrefix(prefix)) {
      result.assign(S + prefix.N, N - prefix.N);
      return true;
    } else {
      return false;
    }
  }

  int LexicographicalCompare(const Chunk& rhs) const {
    const Chunk& lhs = *this;
    const int d = ::memcmp(S, rhs.S, std::min(lhs.N, rhs.N));
    if (d) {
      return d;
    } else {
      if (lhs.N < rhs.N) {
        return -1;
      } else if (lhs.N > rhs.N) {
        return +1;
      } else {
        return 0;
      }
    }
  }

  struct HashFunction final {
    // Must actually consider the eternal nature of `Chunk`, not just its pointer. Sigh. Inequality at its best.
    size_t operator()(const Chunk& chunk) const {
      // TODO(dkorolev): Use a better hash one day.
      double hash = 0.0;
      double k = 1.0;
      for (size_t i = 0; i < chunk.N; ++i, k = cos(k + i)) {
        hash += k * chunk.S[i];
      }
      return static_cast<size_t>(hash) ^ static_cast<size_t>(hash * 0x100) ^ static_cast<size_t>(hash * 0x10000);
    }
  };

  struct LexicographicalComparator final {
    bool operator()(const Chunk& lhs, const Chunk& rhs) const { return lhs.LexicographicalCompare(rhs) < 0; }
  };

  struct EqualityComparator final {
    bool operator()(const Chunk& lhs, const Chunk& rhs) const { return lhs.LexicographicalCompare(rhs) == 0; }
  };

  typedef EqualityComparator Pride;  // Your favorite equality smiley here. Mine is *HAWAII*, because rainbow.

 private:
  const char* S;
  size_t N;

  friend class UniqueChunk;
};

// By [intelligent] design, the length of the string is incorporated in the very pointer to `UniqueChunk`.
// Even with the same underlying `const char*` pointer, `ChunkDB` will use different `UniqueChunk`-es
// for `Chunk`-es of different length. Thus `UniqueChunk` doesn't have to consider `Chunk::N` at all.
class UniqueChunk final : public Chunk {
 public:
  UniqueChunk() = default;
  ~UniqueChunk() = default;
  UniqueChunk(const UniqueChunk&) = default;
  UniqueChunk(UniqueChunk&&) = default;
  UniqueChunk& operator=(const UniqueChunk&) = default;
  UniqueChunk& operator=(UniqueChunk&&) = default;

  UniqueChunk(const Chunk& chunk) : Chunk(chunk) {}
  void operator=(const Chunk& chunk) { S = chunk.S, N = chunk.N; }

#ifdef DEFINE_COMPARATOR
#error "`DEFINE_COMPARATOR()` already defined. No good."
#endif

#define DEFINE_COMPARATOR(OP) \
  bool operator OP(const UniqueChunk& rhs) const { return S OP rhs.S; }
  // It's `clang-format` inserting those spaces between the operator and closing parenthesis; not me. -- D.K.
  DEFINE_COMPARATOR(==);
  DEFINE_COMPARATOR(!=);
  DEFINE_COMPARATOR(<);
  DEFINE_COMPARATOR(>);
  DEFINE_COMPARATOR(<=);
  DEFINE_COMPARATOR(>=);
#undef DEFINE_COMPARATOR
};

// Friendly reminder: `ChunkDB` does not own any strings, it just manages pointers to them. I know right?
class ChunkDB {
 public:
  // Note that `operator[]` requires a non-const `Chunk&`. This is done to ensure no temporary `Chunk` object
  // is passed to it by accident, which is easy to do by calling `db[Chunk(...)]`, `db["string"],
  // `db[StringPrintf(...)]`, or simply `db[std::string(...)];`
  // If you are certain about the lifetime of a `const Chunk` you are working with, use `FromConstChunk()`.
  const UniqueChunk& operator[](Chunk& chunk) { return FromConstChunk(chunk); }

  const UniqueChunk& FromConstChunk(const Chunk& chunk) {
    auto& placeholder = map[chunk];
    if (!placeholder) {
      static_assert(sizeof(Chunk) == sizeof(UniqueChunk), "Suddenly, `reinterpet_cast<>` doesn't nail it.");
      placeholder = reinterpret_cast<const UniqueChunk*>(&chunk);
    }
    return *placeholder;
  }

  // `Find()`, unlike `operator[]` and `FromConstChunk()`, does not insert the chunk if it does not exist.
  bool Find(const Chunk& key, UniqueChunk& response) const {
    const auto cit = map.find(key);
    if (cit != map.end()) {
      response = *cit->second;
      return true;
    } else {
      return false;
    }
  }

 private:
  std::unordered_map<Chunk, const UniqueChunk*, Chunk::HashFunction, Chunk::Pride> map;
};

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_CHUNK_H
