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

#include <cassert>
#include <iostream>
#include <string>

#include "compacttsv.h"

#include "../Bricks/dflags/dflags.h"
#include "../Bricks/file/file.h"
#include "../Bricks/strings/join.h"

DEFINE_string(input, "", "Input file to parse.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  assert(!FLAGS_input.empty());
  const auto contents = current::FileSystem::ReadFileAsString(FLAGS_input);

  CompactTSV::Unpack(
      [](const std::vector<std::string>& v) { std::cout << current::strings::Join(v, '\t') << std::endl; },
      contents);
}
