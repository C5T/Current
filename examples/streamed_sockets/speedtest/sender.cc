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

#include <deque>

#include "../../../blocks/xterm/progress.h"
#include "../../../blocks/xterm/vt100.h"
#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/net/tcp/tcp.h"
#include "../../../bricks/strings/printf.h"
#include "../../../bricks/time/chrono.h"

DEFINE_string(host, "127.0.0.1", "The destination address to send data to.");
DEFINE_uint16(port, 9001, "The destination port to send data to.");
DEFINE_double(send_buffer_mb, 2.0, "Write buffer size.");
DEFINE_double(window_size_seconds, 5.0, "The length of sliding window the throughput within which is reported.");
DEFINE_double(window_size_gb, 20.0, "The maximum amount of data per the sliding window to report the throughput.");
DEFINE_double(output_frequency, 0.1, "The minimim amount of time, in seconds, between terminal updates.");
DEFINE_double(max_seconds_of_no_sends, 2.5, "Terminate the connection if can't send anything for this long.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  using namespace current::net;
  using namespace current::vt100;

#ifndef NDEBUG
  {
    std::cout << yellow << bold << "warning" << reset << ": unoptimized build";
#if defined(CURRENT_POSIX) || defined(CURRENT_APPLE)
    std::cout << ", run " << cyan << "NDEBUG=1 make clean all";
#endif
    std::cout << std::endl;
  }
#endif

  std::chrono::microseconds const t_window_size(static_cast<int64_t>(FLAGS_window_size_seconds * 1e6));
  size_t const window_size_bytes = static_cast<size_t>(FLAGS_window_size_gb * 1e9);
  std::chrono::microseconds const t_output_frequency(static_cast<int64_t>(FLAGS_output_frequency * 1e6));

  current::ProgressLine progress;

  progress << current::strings::Printf("allocating %.1lfMB", FLAGS_send_buffer_mb);
  size_t const buffer_size = static_cast<size_t>(1e6 * FLAGS_send_buffer_mb);
  std::vector<char> data(buffer_size);

  progress << current::strings::Printf("initializing %.1lfMB", FLAGS_send_buffer_mb);
  for (char& c : data) {
    c = 'a' + (rand() % 256);
  }

  progress << "preparing to send";
  while (true) {
    try {
      Connection connection(ClientSocket(FLAGS_host, FLAGS_port));
      progress << "connected, " << magenta << connection.LocalIPAndPort().ip << ':' << connection.LocalIPAndPort().port
               << reset << " => " << cyan << connection.RemoteIPAndPort().ip << ':' << connection.RemoteIPAndPort().port
               << reset;
      size_t total_bytes_sent = 0ull;
      std::deque<std::pair<std::chrono::microseconds, size_t>> history;  // { unix epoch time, total bytes received }.
      std::chrono::microseconds t_next_output = current::time::Now() + t_output_frequency;
      std::chrono::microseconds t_last_successful_receive = current::time::Now();
      while (true) {
        connection.BlockingWrite(reinterpret_cast<void const*>(&data[0]), data.size(), true);
        std::chrono::microseconds const t_now = current::time::Now();
        total_bytes_sent += data.size();
        t_last_successful_receive = t_now;
        history.emplace_back(t_now, total_bytes_sent);
        std::chrono::microseconds const t_cutoff = t_now - t_window_size;
        size_t const size_cutoff = (total_bytes_sent > window_size_bytes ? total_bytes_sent - window_size_bytes : 0);
        if (t_now >= t_next_output) {
          if (history.size() >= 2) {
            while (history.size() > 2 && (history.front().first <= t_cutoff || history.front().second <= size_cutoff)) {
              history.pop_front();
            }
            double const gb = 1e-9 * (history.back().second - history.front().second);
            double const s = 1e-6 * (history.back().first - history.front().first).count();
            progress << bold << green << current::strings::Printf("%.3lfGB/s", gb / s) << reset << ", " << bold
                     << yellow << current::strings::Printf("%.3lfGB", gb) << reset << '/' << bold << blue
                     << current::strings::Printf("%.2lfs", s) << reset << ", " << magenta
                     << connection.LocalIPAndPort().ip << ':' << connection.LocalIPAndPort().port << reset << " => "
                     << cyan << connection.RemoteIPAndPort().ip << ':' << connection.RemoteIPAndPort().port << reset;
          }
          t_next_output = t_now + t_output_frequency;
        }
      }
    } catch (current::net::SocketConnectException const&) {
      progress << "can not connect to " << red << bold << FLAGS_host << ':' << FLAGS_port << reset;
    } catch (current::Exception const& e) {
      progress << red << bold << "error" << reset << ": " << e.OriginalDescription() << reset;
    }
  }
}
