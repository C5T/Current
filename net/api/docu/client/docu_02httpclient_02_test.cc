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

#ifndef BRICKS_NET_API_DOCU_CLIENT_02_TEST_CC
#define BRICKS_NET_API_DOCU_CLIENT_02_TEST_CC

#include "../../../../cerealize/docu/docu_01cerealize_01_test.cc"  // SimpleType.

#include "../../api.h"
#include "../../../../strings/printf.h"
#include "../../../../dflags/dflags.h"
#include "../../../../3party/gtest/gtest-main-with-dflags.h"

DEFINE_int32(docu_net_client_port_02, 8082, "Okay to keep the same as in net/api/test.cc");

using bricks::strings::Printf;

using docu::SimpleType;

TEST(Docu, HTTPClient02) {
HTTP(FLAGS_docu_net_client_port_02).ResetAllHandlers();
HTTP(FLAGS_docu_net_client_port_02).Register("/ok", [](Request r) { r("OK"); });
#if 1
EXPECT_EQ("OK", HTTP(POST(Printf("localhost:%d/ok", FLAGS_docu_net_client_port_02), "BODY", "text/plain")).body);
EXPECT_EQ("OK", HTTP(POST(Printf("localhost:%d/ok", FLAGS_docu_net_client_port_02), SimpleType())).body);
#else
  // POST is supported as well.
  EXPECT_EQ("OK", HTTP(POST("test.tailproduce.org/ok"), "BODY", "text/plain").body);
  
  // Beyond plain strings, cerealizable objects can be passed in.
  // JSON will be sent, as "application/json" content type.
  EXPECT_EQ("OK", HTTP(POST("test.tailproduce.org/ok"), SimpleType()).body);
#endif
}
  
#endif  // BRICKS_NET_API_DOCU_CLIENT_02_TEST_CC
