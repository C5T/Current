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

#include "../replication/generate_stream.h"
#include "../../../Blocks/HTTP/api.h"
#include "../../../Sherlock/replicator.h"

#include "../../../Bricks/dflags/dflags.h"

#ifndef CURRENT_MAKE_CHECK_MODE
DEFINE_string(remote_url, "", "Remote url to subscribe to.");
DEFINE_uint16(local_port, 8383, "Local port to spawn the source stream on.");
DEFINE_string(db, "data.json", "Path to load the source stream data from.");
DEFINE_bool(regenerate_db, true, "Regenerate the stream data to replicate.");
DEFINE_string(persister, "disk", "The type of the replicator-side persister - one of 'disk' / 'memory'");
DEFINE_uint32(entry_length, 100, "The length of the string member values in the generated stream entries.");
DEFINE_uint32(entries_count, 1000, "The number of entries to replicate.");
#else
DECLARE_string(remote_url);
DECLARE_uint16(local_port);
DECLARE_string(db);
DECLARE_bool(regenerate_db);
DECLARE_string(persister);
DECLARE_uint32(entry_length);
DECLARE_uint32(entries_count);
#endif

SCENARIO(stream_replication, "Replicate the Current stream of simple string entries.") {
  std::unique_ptr<benchmark::replication::stream_t> stream;
  std::string stream_url;
  HTTPRoutesScope scope;
  enum class PERSISTER_TYPE : int { DISK, MEMORY };
  PERSISTER_TYPE persister_type;

  stream_replication() {
    if (FLAGS_remote_url.empty()) {
      if (FLAGS_regenerate_db) {
        current::FileSystem::RmFile(FLAGS_db, current::FileSystem::RmFileParameters::Silent);
        stream = benchmark::replication::GenerateStream(FLAGS_db, FLAGS_entry_length, FLAGS_entries_count);
      } else {
        stream = std::make_unique<benchmark::replication::stream_t>(FLAGS_db);
      }
      stream_url = current::strings::Printf("127.0.0.1:%u/raw_log", FLAGS_local_port);
      scope += HTTP(FLAGS_local_port)
                   .Register("/raw_log", URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, *stream);
    } else {
      stream_url = FLAGS_remote_url;
    }
    const std::map<std::string, PERSISTER_TYPE> supported_persisters = {{"disk", PERSISTER_TYPE::DISK},
                                                                        {"memory", PERSISTER_TYPE::MEMORY}};
    try {
      persister_type = supported_persisters.at(FLAGS_persister);
    } catch (const std::out_of_range&) {
      throw std::logic_error(current::strings::Printf("Unsupported persister type: %s", FLAGS_persister.c_str()));
    }
  }

  template <typename STREAM, typename... ARGS>
  void Replicate(ARGS && ... args) {
    STREAM replicated_stream(std::forward<ARGS>(args)...);
    using RemoteStreamReplicator = current::sherlock::StreamReplicator<STREAM>;
    current::sherlock::SubscribableRemoteStream<benchmark::replication::Entry> remote_stream(stream_url);
    auto replicator = std::make_unique<RemoteStreamReplicator>(replicated_stream);
    {
      const auto subscriber_scope = remote_stream.Subscribe(*replicator);
      while (replicated_stream.Persister().Size() != FLAGS_entries_count) {
        std::this_thread::yield();
      }
    }
  }

  void RunOneQuery() override {
    if (persister_type == PERSISTER_TYPE::DISK) {
      const auto filename = current::FileSystem::GenTmpFileName();
      const auto replicated_stream_file_remover = current::FileSystem::ScopedRmFile(filename);
      Replicate<benchmark::replication::stream_t>(filename);
    } else {
      Replicate<current::sherlock::Stream<benchmark::replication::Entry, current::persistence::Memory>>();
    }
  }
};

REGISTER_SCENARIO(stream_replication);

#endif  // BENCHMARK_SCENARIO_REPLICATION_H
