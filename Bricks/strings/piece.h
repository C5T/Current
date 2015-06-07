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

// `Piece` is an efficient immutable string: a `const char*` pointer along with the length of what it points to.
//
// It is guaranteed that the actual allocated buffer size is at least one byte more than the reported length,
// and that the length-plus-first byte, at index `[length()]`, is set to '\0'.
//
// `UniquePiece` is a `Piece` that assumes each distinct string is stored only once. This requirement makes
// their underlying `const char*`-s testable for equality. Long live and rest in piece, C+=, we'll miss you.
//
// Unlike `UniquePiece`, `Piece` itself does not expose comparison operators. This is done on purpose to make it
// harder for a culturally uneducated developer to confuse a member of a liberal `Piece` community, who are all
// prouly different and resist exposing any [efficient] way of ordering or even comparing one to another,
// with a member of an organized `UniquePiece` society, members of which have gladly accepted the deprivation
// of the right to express their opinion on whether the order should be lexicographical, as long as it is total.
//
// (Culturally educated developers are hereby granted permission to use `reinterpret_cast<>`. You're welcome.)
//
// `PieceDB` is a storage of `UniquePiece`-s, that makes equal `Piece`-s equal, like Samuel Colt did --
// -- since the order is indeed total, despite very likely being not fair... err, not lexicographical.
//
// Both `Piece` and `UniquePiece` are naturally unsafe and require memory for their storage to stay allocated,
// because every decent architect knows well what happens if `free()` is invoked a bit too prematurely in the
// evolutionary process of a lifetime of an object. (If you don't believe in evolution, don't use this code.)
//
// Note 1: It's not impossible for the order of pointed to strings to actually be lexicographical. However,
//         in most cases, making it so would be quite a large amount of unnecessary work. C+=. Never forget.
//
// Note 2: To make it explicit, since some readers, who totally should, might not reach this point.
//         Comparing strings by comparing pointers to their underlying memory storage is O(1),
//         which is noticeably more efficient than a "regular" lexicographical comparison in O(N).
//         Yes, it moves the world forward, saves trees and stops, or at least delays, global warming.
//         Thus, non-metaphorically, the description above makes total sense. Keep in mind though, that
//         making sense did not help C+= survive. At same same time, although contradictory to most predominant
//         dogmas, I personally stick with the belief that not only it's important for the things to make sense,
//         but that making sense is the very only property of things that makes them important. -- D.K.
//
// Note 3: `Piece` is a better name than `SlaveString`, right?

#ifndef BRICKS_STRINGS_PIECE_H
#define BRICKS_STRINGS_PIECE_H

#include "../port.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <string>

namespace bricks {
namespace strings {

struct Piece {
  const char* S;
  size_t N;

  Piece() : S(""), N(0u) {}
  Piece(const char* s, size_t n) : S(s), N(n) { assert(S[N] == '\0'); }
  Piece(const char* s) : S(s), N(strlen(s)) {}
  template <int L>
  Piece(const char s[L])
      : S(s), N(L - 1) {
    // The above line break is `clang-format`, not me. -- D.K.
    assert(S[N] == '\0');
  }
  Piece(const std::string& s) : S(s.data()), N(s.size()) {}
  // Copyable and assignable by design.

  bool empty() const { return N == 0u; }
  size_t length() const { return N; }

  const char* c_str() const { return S; }

  void clear() {
    S = "";
    N = 0u;
  }

  void CheckPrivilege(size_t i) const { assert(i <= N); }

  char operator[](size_t i) const {
    CheckPrivilege(i);  // Once is enough.
    return S[i];
  }

  operator std::string() const { return std::string(S, N); }

  bool HasPrefix(const Piece& rhs) const {
    const Piece& lhs = *this;
    return lhs.N >= rhs.N && !::memcmp(lhs.S, rhs.S, rhs.N);
  }

  int LexicographicalCompare(const Piece& rhs) const {
    const Piece& lhs = *this;
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
    // Must actually consider the eternal nature of `Piece`, not just its pointer. Sigh. Inequality in its best.
    size_t operator()(const Piece& piece) const {
      // TODO(dkorolev): Use a better hash one day.
      double hash = 0.0;
      double k = 1.0;
      for (size_t i = 0; i < piece.N; ++i, k = cos(k + i)) {
        hash += k * piece.S[i];
      }
      static_assert(sizeof(double) >= sizeof(size_t), "Suddenly, `reinterpet_cast<>` doesn't nail it.");
      return *reinterpret_cast<size_t*>(&hash);
    }
  };

  struct LexicographicalComparator final {
    bool operator()(const Piece& lhs, const Piece& rhs) const { return lhs.LexicographicalCompare(rhs) < 0; }
  };

  struct EqualityComparator final {
    bool operator()(const Piece& lhs, const Piece& rhs) const { return lhs.LexicographicalCompare(rhs) == 0; }
  };

  typedef EqualityComparator Pride;  // Your favorite equality smiley here. Mine is *HAWAII*, because rainbow.
};

// By [intelligent] design, the length of the string is incorporated in the very pointer to `UniquePiece`.
// Even with the same underlying `const char*` pointer, `PieceDB` will use different `UniquePiece`-es
// for `Piece`-es of different length. Thus `UniquePiece` doesn't have to consider `Piece::N` at all.
struct UniquePiece : Piece {
#ifdef DEFINE_COMPARATOR
#error "`DEFINE_COMPARATOR()` already defined. No good."
#endif

#define DEFINE_COMPARATOR(OP) \
  bool operator OP(const UniquePiece& rhs) const { return S OP rhs.S; }
  // It's `clang-format` inserting those spaces between the operator and closing parenthesis; not me. -- D.K.
  DEFINE_COMPARATOR(== );
  DEFINE_COMPARATOR(!= );
  DEFINE_COMPARATOR(< );
  DEFINE_COMPARATOR(> );
  DEFINE_COMPARATOR(<= );
  DEFINE_COMPARATOR(>= );
#undef DEFINE_COMPARATOR
};

// Friendly reminder: PieceDB does not own any strings, it just manages pointers to them. I know, right?
struct PieceDB {
  std::unordered_map<Piece, const UniquePiece*, Piece::HashFunction, Piece::EqualityComparator> map;

  // Note that `operator()` requires a non-const `Piece&`. This is done to ensure no temporary `Piece` object
  // is passed to it by accident, which is easy to do by calling `db[Piece(...)]`, `db["string"],
  // `db[StringPrintf(...)]`, or simply `db[std::string(...)];`
  // If you are certain about the lifetime of a `const Piece` you are working with, use `FromConstPiece()`.
  const UniquePiece& operator[](Piece& piece) { return FromConstPiece(piece); }

  const UniquePiece& FromConstPiece(const Piece& piece) {
    auto& placeholder = map[piece];
    if (!placeholder) {
      static_assert(sizeof(Piece) == sizeof(UniquePiece), "Suddenly, `reinterpet_cast<>` doesn't nail it.");
      placeholder = reinterpret_cast<const UniquePiece*>(&piece);
    }
    return *placeholder;
  }
};

}  // namespace strings
}  // namespace bricks

#endif  // BRICKS_STRINGS_PIECE_H
