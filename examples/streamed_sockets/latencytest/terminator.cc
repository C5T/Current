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

// NOTE(dkorolev): The copy-paste of `../speedtest/receiver.cc`, with port changed, to keep everything in a single dir.

#include <deque>

#include "../../../blocks/xterm/progress.h"
#include "../../../blocks/xterm/vt100.h"
#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/net/tcp/tcp.h"
#include "../../../bricks/strings/printf.h"
#include "../../../bricks/time/chrono.h"

DEFINE_uint16(port, 8003, "The port to use.");
DEFINE_double(receive_buffer_gb, 0.5, "The size of the buffer the read the data into.");
DEFINE_double(window_size_seconds, 5.0, "The length of sliding window the throughput within which is reported.");
DEFINE_double(window_size_gb, 20.0, "The maximum amount of data per the sliding window to report the throughput.");
DEFINE_double(output_frequency, 0.1, "The minimim amount of time, in seconds, between terminal updates.");
DEFINE_double(max_seconds_of_silence, 1.5, "Terminate the connection if it sends nothing for this long, in seconds.");
DEFINE_bool(silent, false, "Set to suppress GB/s output, for the `./run_all_locally.sh` script.");

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

  const std::chrono::microseconds t_window_size(static_cast<int64_t>(FLAGS_window_size_seconds * 1e6));
  const size_t window_size_bytes = static_cast<size_t>(FLAGS_window_size_gb * 1e9);
  const std::chrono::microseconds t_output_frequency(static_cast<int64_t>(FLAGS_output_frequency * 1e6));

  std::vector<uint8_t> buffer(static_cast<size_t>(1e9 * FLAGS_receive_buffer_gb));

  current::ProgressLine progress;

  if (!FLAGS_silent) {
    progress << "starting on " << cyan << bold << "localhost:" << FLAGS_port;
  }

  while (true) {
    try {
      size_t total_bytes_received = 0ull;
      std::deque<std::pair<std::chrono::microseconds, size_t>> history;  // { unix epoch time, total bytes received }.

      Socket socket(FLAGS_port);
      if (!FLAGS_silent) {
        progress << "listening on " << cyan << bold << "localhost:" << FLAGS_port;
      }

      Connection connection(socket.Accept());
      if (!FLAGS_silent) {
        progress << "connected, " << cyan << connection.LocalIPAndPort().ip << ':' << connection.LocalIPAndPort().port
                 << reset << " <= " << magenta << connection.RemoteIPAndPort().ip << ':'
                 << connection.RemoteIPAndPort().port << reset;
      }

      std::chrono::microseconds t_next_output = current::time::Now() + t_output_frequency;
      std::chrono::microseconds t_last_successful_receive = current::time::Now();
      while (true) {
        const size_t size = connection.BlockingRead(&buffer[0], buffer.size());
        const std::chrono::microseconds t_now = current::time::Now();
        if (size) {
          total_bytes_received += size;
          t_last_successful_receive = t_now;
          history.emplace_back(t_now, total_bytes_received);
          const std::chrono::microseconds t_cutoff = t_now - t_window_size;
          const size_t size_cutoff =
              (total_bytes_received > window_size_bytes ? total_bytes_received - window_size_bytes : 0);
          if (!FLAGS_silent && t_now >= t_next_output) {
            if (history.size() >= 2) {
              while (history.size() > 2 &&
                     (history.front().first <= t_cutoff || history.front().second <= size_cutoff)) {
                history.pop_front();
              }
              const double gb = 1e-9 * (history.back().second - history.front().second);
              const double s = 1e-6 * (history.back().first - history.front().first).count();
              progress << "Terminator: ";
              progress << "Terminator: " << bold << green << current::strings::Printf("%.3lfGB/s", gb / s) << reset
                       << ", " << bold << yellow << current::strings::Printf("%.3lfGB", gb) << reset << '/' << bold
                       << blue << current::strings::Printf("%.2lfs", s) << reset << ", " << cyan
                       << connection.LocalIPAndPort().ip << ':' << connection.LocalIPAndPort().port << reset
                       << " <= " << magenta << connection.RemoteIPAndPort().ip << ':'
                       << connection.RemoteIPAndPort().port << reset;
            }
            t_next_output = t_now + t_output_frequency;
          }
        } else {
          std::this_thread::yield();
          if (t_now >= t_next_output) {
            const double seconds_of_silence = 1e-6 * (t_now - t_last_successful_receive).count();
            const double seconds_timeout = std::max(0.0, FLAGS_max_seconds_of_silence - seconds_of_silence);
            if (!FLAGS_silent) {
              progress << "dropping connection in " << bold << red
                       << current::strings::Printf("%.1lfs", seconds_timeout) << reset;
            }
            t_next_output = t_now + t_output_frequency;
            if (!seconds_timeout) {
              if (!FLAGS_silent) {
                progress << "dropping connection";
              }
              break;
            }
          }
        }
      }
    } catch (const current::net::SocketBindException&) {
      if (!FLAGS_silent) {
        progress << "can not bind to " << red << bold << "localhost:" << FLAGS_port << reset
                 << ", check for other apps holding the post";
      }
    } catch (const current::Exception& e) {
      if (!FLAGS_silent) {
        progress << red << bold << "error" << reset << ": " << e.OriginalDescription() << reset;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Don't eat up 100% CPU when unable to connect.
  }
}
