/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2018 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

// Here's the best description I found: `https://misc.flogisoft.com/bash/tip_colors_and_formatting` -- D.K.

#ifndef BLOCKS_VT100_COLORS_H
#define BLOCKS_VT100_COLORS_H

#ifdef DEFINE_VT100
#error "The `DEFINE_VT100` macro should not be defined prior to `#include \"vt100\"`."
#endif

#include "../../port.h"

#include <ostream>

namespace current {
namespace vt100 {

// `E` is short for Escape sequence.
struct E {
  const int code;
  E(int code) : code(code) {}
};

struct Color : E {
  using E::E;
};

const static E reset(0);

const static E bold(1);
const static E dim(2);
const static E italic(3);  // Non-standard, it seems. -- D.K.
const static E underlined(4);
// const static E blink(5); -- commented out as not supported. -- D.K.
const static E reverse(7);
const static E hidden(8);
const static E strikeout(9);  // Non-standard, it seems. -- D.K.

inline E off(E e) { return E(e.code + 20); }  // Turns off bold/dim/underlined/reverse/hidden.

#define DEFINE_VT100(id, color) const static Color color(id)
DEFINE_VT100(39, default_color);
DEFINE_VT100(30, black);
DEFINE_VT100(31, red);
DEFINE_VT100(32, green);
DEFINE_VT100(33, yellow);
DEFINE_VT100(34, blue);
DEFINE_VT100(35, magenta);
DEFINE_VT100(36, cyan);
DEFINE_VT100(37, bright_gray);
DEFINE_VT100(90, dark_gray);
DEFINE_VT100(91, bright_red);
DEFINE_VT100(92, bright_green);
DEFINE_VT100(93, bright_yellow);
DEFINE_VT100(94, bright_blue);
DEFINE_VT100(95, bright_magenta);
DEFINE_VT100(96, bright_cyan);
DEFINE_VT100(97, white);
#undef DEFINE_VT100

inline E background(Color c) { return E(c.code + 10); }

}  // namespace current::vt100
}  // namespace current

inline std::ostream& operator<<(std::ostream& os, const current::vt100::E& e) {
  os << "\e[" << e.code << 'm';
  return os;
}

#endif  // BLOCKS_VT100_COLORS_H
