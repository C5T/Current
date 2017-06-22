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

#include "../../../Blocks/HTTP/api.h"
#include "../../../Bricks/dflags/dflags.h"
#include "../../../Sherlock/sherlock.h"

#include "generate_stream.h"

DEFINE_string(route, "/raw_log", "Route to spawn the stream on.");
DEFINE_uint16(port, 8383, "Port to spawn the stream on.");
DEFINE_string(db, "data.json", "Path to load the stream data from.");
DEFINE_bool(regenerate_db, true, "Regenerate the stream data.");
DEFINE_uint32(entry_length, 1000, "The length of the string member values in the generated stream entries.");
DEFINE_uint32(entries_count, 10000, "The number of entries to replicate.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  std::unique_ptr<benchmark::replication::stream_t> stream;
  if (FLAGS_regenerate_db) {
    current::FileSystem::RmFile(FLAGS_db, current::FileSystem::RmFileParameters::Silent);
    stream = benchmark::replication::GenerateStream(FLAGS_db, FLAGS_entry_length, FLAGS_entries_count);
  } else {
    stream = std::make_unique<benchmark::replication::stream_t>(FLAGS_db);
  }
  const auto scope =
      HTTP(FLAGS_port).Register(FLAGS_route, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, *stream);
  std::cout << "Server is spawned on port " << FLAGS_port << std::endl;
  HTTP(FLAGS_port).Join();
  return 0;
}
