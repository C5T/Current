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

#ifndef BRICKS_STRINGS_ESCAPE_H
#define BRICKS_STRINGS_ESCAPE_H

#include <string>

namespace current {
namespace strings {

// A poor man's version, does not have to be complete.
inline std::string EscapeForCPlusPlus(const std::string& input) {
  std::string output;
  for (char c : input) {
    if (c == '\n') {
      output += "\\n";
    } else if (c == '\t') {
      output += "\\t";
    } else if (c == '\'') {
      output += "\\\'";
    } else if (c == '\"') {
      output += "\\\"";
    } else {
      output += c;
    }
  }
  return output;
}

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_ESCAPE_H
