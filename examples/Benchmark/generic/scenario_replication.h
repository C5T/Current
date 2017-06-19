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

#ifndef BENCHMARK_SCENARIO_REPLICATION_H
#define BENCHMARK_SCENARIO_REPLICATION_H

#include "benchmark.h"

#include "../replication/generate_stream_data.h"
#include "../../../Blocks/HTTP/api.h"
#include "../../../Sherlock/replicator.h"

#include "../../../Bricks/dflags/dflags.h"

#ifndef CURRENT_MAKE_CHECK_MODE
DEFINE_string(remote_url, "", "Remote url to subscribe to.");
DEFINE_uint16(local_port, 8383, "Local port to subscribe to.");
DEFINE_string(remote_db, "db.json", "Path to load the stream data from.");
DEFINE_string(local_db, "db_copy.json", "Path to write the replicated stream to.");
DEFINE_bool(regenerate_db, true, "Regenerate the stream data to replicate.");
DEFINE_uint32(entry_length, 100, "The length of the string member values in the generated entries.");
DEFINE_uint32(entries_count, 1000, "The number of entries in the output data.");
#else
DECLARE_string(remote_url);
DECLARE_uint16(local_port);
DECLARE_string(remote_db);
DECLARE_string(local_db);
DECLARE_bool(regenerate_db);
DECLARE_uint32(entry_length);
DECLARE_uint32(entries_count);
#endif

SCENARIO(stream_replication, "Replicate the Current stream of simple string entries.") {
  std::unique_ptr<benchmark::stream_t> stream;
  std::string stream_url;
  HTTPRoutesScope scope;

  stream_replication() {
    if (FLAGS_remote_url.empty()) {
      if (FLAGS_regenerate_db) {
        current::FileSystem::RmFile(FLAGS_remote_db, current::FileSystem::RmFileParameters::Silent);
        benchmark::GenerateStreamData(FLAGS_remote_db, FLAGS_entry_length, FLAGS_entries_count);
      }
      stream = std::make_unique<benchmark::stream_t>(FLAGS_remote_db);
      stream_url = current::strings::Printf("127.0.0.1:%u/raw_log", FLAGS_local_port);
      scope += HTTP(FLAGS_local_port).Register(
        "/raw_log", URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, *stream);
    } else {
      stream_url = FLAGS_remote_url;
    }
  }

  template <typename STREAM, typename... ARGS>
  void Replicate(ARGS&&... args) {
    STREAM replicated_stream(std::forward<ARGS>(args)...);
    using RemoteStreamReplicator = current::sherlock::StreamReplicator<STREAM>;
    current::sherlock::SubscribableRemoteStream<benchmark::entry_t> remote_stream(stream_url);
    auto replicator = std::make_unique<RemoteStreamReplicator>(replicated_stream);
    {
      const auto subscriber_scope = remote_stream.Subscribe(*replicator);
      while (replicated_stream.Persister().Size() != FLAGS_entries_count) {
        std::this_thread::yield();
      }
    }
  }

  void RunOneQuery() override {
    if (FLAGS_local_db.empty()) {
      Replicate<current::sherlock::Stream<benchmark::entry_t, current::persistence::Memory>>();
    } else {
      const auto filename = current::FileSystem::GenTmpFileName();
      const auto replicated_stream_file_remover = current::FileSystem::ScopedRmFile(filename);
      Replicate<benchmark::stream_t>(filename);
    }
  }
};

REGISTER_SCENARIO(stream_replication);

#endif  // BENCHMARK_SCENARIO_REPLICATION_H
