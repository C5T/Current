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
DEFINE_double(at_m, 0, "If nonzero, the minute at which the contents of the pad should be output.");
DEFINE_double(interval_s, 5, "If `--at_m` is not set, the iterval between snapshots to be output, in seconds.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  struct Processor {
    const std::chrono::microseconds interval = std::chrono::microseconds(static_cast<int64_t>(FLAGS_interval_s * 1e6));
    const std::chrono::microseconds at = std::chrono::microseconds(static_cast<int64_t>(FLAGS_at_m * 60 * 1e6));
    bool done_with_output = false;
    std::chrono::microseconds first_seen_timestamp = std::chrono::microseconds(0);
    std::chrono::microseconds last_seen_timestamp = std::chrono::microseconds(0);
    std::chrono::microseconds marker = std::chrono::microseconds(0);
    bool Intermediate(std::chrono::microseconds timestamp, const std::deque<wchar_t>& rope) {
      last_seen_timestamp = timestamp;
      if (first_seen_timestamp == std::chrono::microseconds(0)) {
        first_seen_timestamp = timestamp;
        std::cout << "### Pad\nStarted " << current::FormatDateTime<current::time::TimeRepresentation::Local>(timestamp)
                  << " local time, " << current::FormatDateTime<current::time::TimeRepresentation::UTC>(timestamp)
                  << " UTC." << std::endl;
      }
      if (at.count()) {
        // `--at_m` is set, dump the contents at the timestamp of this number of minutes into coding.
        if ((timestamp - first_seen_timestamp) >= at) {
          std::string contents = current::utils::ot::AsUTF8String(rope);
          if (contents.empty() || contents.back() != '\n') {
            contents += '\n';
          }
          std::cout << "### " << current::strings::TimeIntervalAsHumanReadableString(timestamp - first_seen_timestamp)
                    << " into the code\n```\n";
          std::cout << contents;
          std::cout << "```" << std::endl;
          done_with_output = true;
          return false;
        } else {
          return true;
        }
      } else {
        // `--at_m` is not set, dump the contents periodically.
        if (timestamp - marker >= interval) {
          marker = timestamp;
          std::string contents = current::utils::ot::AsUTF8String(rope);
          if (contents.empty() || contents.back() != '\n') {
            contents += '\n';
          }
          std::cout << "### " << current::strings::TimeIntervalAsHumanReadableString(timestamp - first_seen_timestamp)
                    << " into the code\n```\n";
          std::cout << contents;
          std::cout << "```" << std::endl;
        }
        return true;
      }
    }
    void Final(const std::deque<wchar_t>& rope) const {
      if (!done_with_output) {
        std::string contents = current::utils::ot::AsUTF8String(rope);
        if (contents.empty() || contents.back() != '\n') {
          contents += '\n';
        }
        std::cout << "### Result after the total of "
                  << current::strings::TimeIntervalAsHumanReadableString(last_seen_timestamp - first_seen_timestamp)
                  << " of coding\n```\n";
        std::cout << contents;
        std::cout << "```" << std::endl;
      }
    }
    void EmptyOutput() const {}
  };

  current::utils::ot::OT(current::FileSystem::ReadFileAsString(FLAGS_input), Processor());
}
