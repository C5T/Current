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

#include <iostream>

#include "event_collector.h"

#include "../../Current/Bricks/dflags/dflags.h"

DEFINE_int32(port, 8686, "Port to spawn log collector on.");
DEFINE_string(route, "/log", "The route to listen to events on.");
DEFINE_int32(tick_interval_ms, 1000, "Maximum interval between entries.");

int main(int argc, char **argv) {
  ParseDFlags(&argc, &argv);
  EventCollectorHTTPServer(FLAGS_port,
                           std::cerr,
                           static_cast<bricks::time::MILLISECONDS_INTERVAL>(FLAGS_tick_interval_ms),
                           FLAGS_route).Join();
}
