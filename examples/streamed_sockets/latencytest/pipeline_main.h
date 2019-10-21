/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2019 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_PIPELINE_MAIN_H
#define EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_PIPELINE_MAIN_H

#include "blob.h"
#include "dsl/dsl.h"

#include "../../../blocks/xterm/vt100.h"
#include "../../../bricks/dflags/dflags.h"

DECLARE_double(buffer_mb);

#ifndef NDEBUG
inline void DebugModeDisclaimerIfAppropriate() {
  using namespace current::vt100;
  std::cout << yellow << bold << "warning" << reset << ": unoptimized build";
#if defined(CURRENT_POSIX) || defined(CURRENT_APPLE)
  std::cout << ", run " << cyan << "NDEBUG=1 make clean all";
#endif
  std::cout << std::endl;
}
#else
inline void DebugModeDisclaimerIfAppropriate() {}
#endif

#define PIPELINE_MAIN(...)                                                                                \
  int main(int argc, char** argv) {                                                                       \
    DebugModeDisclaimerIfAppropriate();                                                                   \
    ParseDFlags(&argc, &argv);                                                                            \
    using namespace current::examples::streamed_sockets;                                                  \
    Pipeline(__VA_ARGS__)                                                                                 \
        .Run(PipelineRunParams<Blob>().SetCircularBufferSize(static_cast<size_t>(FLAGS_buffer_mb * 1e6))) \
        .Join();                                                                                          \
  }                                                                                                       \
  struct DummyPipelineMainEatSemicolon {}

#endif  // EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_PIPELINE_MAIN_H
