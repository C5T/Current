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

#include "ot.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/file/file.h"

DEFINE_string(input, "input.ot", "The name of the input file containing the Operational Transformation of the pad.");
DEFINE_string(output, "output.txt", "The name of the output file to dump the contents of the text into.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  const std::string utf8_output = current::utils::ot::OT(current::FileSystem::ReadFileAsString(FLAGS_input));
  if (FLAGS_output.empty()) {
    std::cout << utf8_output;
  } else {
    current::FileSystem::WriteStringToFile(utf8_output, FLAGS_output.c_str());
  }
}
