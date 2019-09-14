#include <deque>

#include "../../../blocks/xterm/progress.h"
#include "../../../blocks/xterm/vt100.h"
#include "../../../bricks/dflags/dflags.h"
#include "../../../bricks/net/tcp/tcp.h"
#include "../../../bricks/strings/printf.h"
#include "../../../bricks/time/chrono.h"

DEFINE_uint16(port, 9001, "The port to use.");
DEFINE_double(receive_buffer_gb, 0.5, "The size of the buffer the read the data into.");
DEFINE_double(window_size_seconds, 5.0, "The length of sliding window the throughput within which is reported.");
DEFINE_double(window_size_gb, 20.0, "The maximum amount of data per the sliding window to report the throughput.");
DEFINE_double(output_frequency, 0.1, "The minimim amount of time, in seconds, between terminal updates.");
DEFINE_double(max_seconds_of_silence, 1.5, "Terminate the connection if it sends nothing for this long, in seconds.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  std::chrono::microseconds const t_window_size(static_cast<int64_t>(FLAGS_window_size_seconds * 1e6));
  size_t const window_size_bytes = static_cast<size_t>(FLAGS_window_size_gb * 1e9);
  std::chrono::microseconds const t_output_frequency(static_cast<int64_t>(FLAGS_output_frequency * 1e6));

  std::vector<uint8_t> buffer(static_cast<size_t>(1e9 * FLAGS_receive_buffer_gb));

  using namespace current::net;
  using namespace current::vt100;

  current::ProgressLine progress;

  progress << "starting on " << cyan << bold << "localhost:" << FLAGS_port;

  while (true) {
    try {
      size_t total_bytes_received = 0ull;
      std::deque<std::pair<std::chrono::microseconds, size_t>> history;  // { unix epoch time, total bytes received }.

      Socket socket(FLAGS_port);
      progress << "listening on " << cyan << bold << "localhost:" << FLAGS_port;
      Connection connection(socket.Accept());
      progress << "connected, " << cyan << connection.LocalIPAndPort().ip << ':' << connection.LocalIPAndPort().port
               << reset << " <= " << magenta << connection.RemoteIPAndPort().ip << ':'
               << connection.RemoteIPAndPort().port << reset;

      std::chrono::microseconds t_next_output = current::time::Now() + t_output_frequency;
      std::chrono::microseconds t_last_successful_receive = current::time::Now();
      while (true) {
        size_t const size = connection.BlockingRead(&buffer[0], buffer.size());
        std::chrono::microseconds const t_now = current::time::Now();
        if (size) {
          total_bytes_received += size;
          t_last_successful_receive = t_now;
          history.emplace_back(t_now, total_bytes_received);
          std::chrono::microseconds const t_cutoff = t_now - t_window_size;
          size_t const size_cutoff =
              (total_bytes_received > window_size_bytes ? total_bytes_received - window_size_bytes : 0);
          if (t_now >= t_next_output) {
            if (history.size() >= 2) {
              while (history.size() > 2 &&
                     (history.front().first <= t_cutoff || history.front().second <= size_cutoff)) {
                history.pop_front();
              }
              double const gb = 1e-9 * (history.back().second - history.front().second);
              double const s = 1e-6 * (history.back().first - history.front().first).count();
              progress << bold << green << current::strings::Printf("%.2lfGB/s", gb / s) << reset << ", " << bold
                       << yellow << current::strings::Printf("%.2lfGB", gb) << reset << '/' << bold << blue
                       << current::strings::Printf("%.1lfs", s) << reset << ", " << cyan
                       << connection.LocalIPAndPort().ip << ':' << connection.LocalIPAndPort().port << reset
                       << " <= " << magenta << connection.RemoteIPAndPort().ip << ':'
                       << connection.RemoteIPAndPort().port << reset;
            }
            t_next_output = t_now + t_output_frequency;
          }
        } else {
          std::this_thread::yield();
          if (t_now >= t_next_output) {
            double const seconds_of_silence = 1e-6 * (t_now - t_last_successful_receive).count();
            double const seconds_timeout = std::max(0.0, FLAGS_max_seconds_of_silence - seconds_of_silence);
            progress << "dropping connection in " << bold << red << current::strings::Printf("%.1lfs", seconds_timeout)
                     << reset;
            t_next_output = t_now + t_output_frequency;
            if (!seconds_timeout) {
              progress << "dropping connection";
              break;
            }
          }
        }
      }
    } catch (current::net::SocketBindException const&) {
      progress << "can not bind to " << red << bold << "localhost:" << FLAGS_port << reset
               << ", check for other apps holding the post";
    } catch (current::Exception const& e) {
      progress << red << bold << "error" << reset << ": " << e.OriginalDescription() << reset;
    }
  }
}
