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

#include "../../../blocks/http/api.h"
#include "../../../bricks/dflags/dflags.h"
#include "../../../stream/stream.h"

#include "generate_stream.h"

DEFINE_string(route, "/raw_log", "Route to spawn the stream on.");
DEFINE_uint16(port, 8383, "Port to spawn the stream on.");
DEFINE_string(stream_data_filename,
              "",
              "If set, path to load the stream data from. If not, the data is generated from scratch.");
DEFINE_uint32(entry_length, 1000, "The length of the string member values in the generated stream entries.");
DEFINE_uint32(entries_count, 10000, "The number of entries to replicate.");
DEFINE_bool(do_not_remove_autogen_data, false, "Set to not remove the data file.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  Optional<current::Owned<benchmark::replication::stream_t>> stream;
  std::unique_ptr<current::FileSystem::ScopedRmFile> temp_file_remover;
  if (FLAGS_stream_data_filename.empty()) {
    const auto filename = current::FileSystem::GenTmpFileName();
    std::cout << "Generating " << filename << " with " << FLAGS_entries_count << " entries of " << FLAGS_entry_length
              << " bytes each." << std::endl;
    if (!FLAGS_do_not_remove_autogen_data) {
      temp_file_remover = std::make_unique<current::FileSystem::ScopedRmFile>(filename);
    }
    benchmark::replication::GenerateStream(filename, FLAGS_entry_length, FLAGS_entries_count, stream);
  } else {
    const auto filename = current::FileSystem::GenTmpFileName();
    stream = benchmark::replication::stream_t::CreateStream(FLAGS_stream_data_filename);
  }
  std::cout << "Spawning the server on port " << FLAGS_port << std::endl;
  const auto scope =
      HTTP(FLAGS_port)
          .Register(FLAGS_route, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, *Value(stream));
  std::cout << "The server is up on http://localhost:" << FLAGS_port << FLAGS_route << std::endl;
  HTTP(FLAGS_port).Join();
  return 0;
}
