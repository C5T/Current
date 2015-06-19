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

#ifndef BLOCKS_HTTP_DOCU_SERVER_05_TEST_CC
#define BLOCKS_HTTP_DOCU_SERVER_05_TEST_CC

#include <algorithm>
#include <functional>
#include <thread>
#include <chrono>

#include "../../api.h"
#include "../../../../Bricks/dflags/dflags.h"
#include "../../../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_int32(docu_net_server_port_05, 8082, "Okay to keep the same as in net/api/test.cc");

TEST(Docu, HTTPServer05) {
const auto port = FLAGS_docu_net_server_port_05;
HTTP(port).ResetAllHandlers();
  // Returning a potentially unlimited response chunk by chunk.
  HTTP(port).Register("/chunked", [](Request r) {
    const size_t n = atoi(r.url.query["n"].c_str());
    const size_t delay_ms = atoi(r.url.query["delay_ms"].c_str());
      
    const auto sleep = [&delay_ms]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    };
      
    auto response = r.SendChunkedResponse();
  
    sleep();
    for (size_t i = 0; n && i < n; ++i) {
      response(".");  // Use double quotes for a string, not single quotes for a char.
      sleep();
    }
    response("\n");
  });
#if 0
  
  EXPECT_EQ(".....\n", HTTP(GET("http://test.tailproduce.org/chunked?n=5&delay_ms=2")).body);
  
  // NOTE: For most legitimate practical usecases of returning unlimited
  // amounts of data, consider Sherlock's stream data replication mechanisms.
#endif

#ifndef BRICKS_APPLE
// Temporary disabled chunked-transfer test for Apple -- M.Z.
EXPECT_EQ(".....\n", HTTP(GET(Printf("http://localhost:%d/chunked?n=5&delay_ms=2", port))).body);
#endif
}

#endif  // BLOCKS_HTTP_DOCU_SERVER_05_TEST_CC
