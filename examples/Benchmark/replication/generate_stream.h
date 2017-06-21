/*******************************************************************************
 The MIT License (MIT)

 Copyright (c) 2017 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>

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

#include "../../../Sherlock/sherlock.h"

#include "entry.h"

#ifndef BENCHMARK_REPLICATION_GENERATE_STREAM_H
#define BENCHMARK_REPLICATION_GENERATE_STREAM_H

namespace benchmark {
namespace replication {

using stream_t = current::sherlock::Stream<Entry, current::persistence::File>;

inline std::unique_ptr<stream_t> GenerateStream(const std::string& output_file,
                                                uint32_t entry_length,
                                                uint32_t entries_count) {
  const char symbols[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const uint32_t symbols_count = sizeof(symbols) / sizeof(symbols[0]) - 1;
  std::vector<char> pattern(entry_length + 1);

  auto stream = std::make_unique<stream_t>(output_file);

  for (uint32_t i = 0; i < entries_count; ++i) {
    for (uint32_t j = 0; j < entry_length; ++j) {
      pattern[j] = symbols[(i / symbols_count + (i + 1) * j) % symbols_count];
    }
    stream->Publish(Entry(&pattern[0]));
  }
  return stream;
}

}  // namespace replication
}  // namespace benchmark

#endif  // BENCHMARK_REPLICATION_GENERATE_STREAM_H
