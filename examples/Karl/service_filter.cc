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

#include "current_build.h"

#include "../../Blocks/HTTP/api.h"
#include "../../bricks/dflags/dflags.h"
#include "../../Karl/test_service/filter.h"

DEFINE_uint16(port, 42004, "The port to spawn ServiceAnnotator on.");
DEFINE_string(annotator, "http://localhost:42003", "The route to `ServiceAnnotator`.");

int main(int argc, char **argv) {
  ParseDFlags(&argc, &argv);
  const karl_unittest::ServiceFilter service(FLAGS_port, FLAGS_annotator, current::karl::LocalKarl());
  std::cout << "ServiceFilter up, http://localhost:" << FLAGS_port << "/annotated" << std::endl;
  HTTP(FLAGS_port).Join();
}
