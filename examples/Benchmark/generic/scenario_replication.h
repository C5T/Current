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
#include "../../../blocks/HTTP/api.h"
#include "../../../stream/replicator.h"

#include "../../../bricks/dflags/dflags.h"

#ifndef CURRENT_MAKE_CHECK_MODE
DEFINE_string(remote_url, "", "Remote url to subscribe to.");
DEFINE_uint16(local_port, 8383, "Local port to spawn the source stream on.");
DEFINE_string(db, "", "If set, path to load the stream data from. If not, the data is generated from scratch.");
DEFINE_string(persister, "disk", "The type of the replicator-side persister - one of 'disk' / 'memory'");
DEFINE_uint32(entry_length, 100, "The length of the string member values in the generated stream entries.");
DEFINE_uint32(entries_count, 1000, "The number of entries to replicate.");
#else
DECLARE_string(remote_url);
DECLARE_uint16(local_port);
DECLARE_string(db);
DECLARE_string(persister);
DECLARE_uint32(entry_length);
DECLARE_uint32(entries_count);
#endif

SCENARIO(stream_replication, "Replicate the Current stream of simple string entries.") {
  Optional<current::Owned<benchmark::replication::stream_t>> stream;  // `Optional<>` as it's initialized dynamically.
  std::unique_ptr<current::FileSystem::ScopedRmFile> tmp_db_remover;
  std::string stream_url;
  HTTPRoutesScope scope;
  enum class PERSISTER_TYPE : int { DISK, MEMORY };
  PERSISTER_TYPE persister_type;
  uint64_t entries_count;

  struct InvalidPersisterTypeException : current::Exception {
    explicit InvalidPersisterTypeException(const std::string& persister)
        : current::Exception("Unsupported persister type: " + persister) {}
  };
  struct GetStreamSizeFailedException : current::Exception {
    explicit GetStreamSizeFailedException(const current::http::HTTPResponseWithBuffer& response)
        : current::Exception(current::strings::Printf("Cannot obtain the remote stream size, got an error %u: %s",
                                                      static_cast<uint32_t>(response.code),
                                                      response.body.c_str())) {}
  };

  stream_replication() {
    if (FLAGS_remote_url.empty()) {
      if (FLAGS_db.empty()) {
        const auto filename = current::FileSystem::GenTmpFileName();
        tmp_db_remover = std::make_unique<current::FileSystem::ScopedRmFile>(filename);
        stream = benchmark::replication::GenerateStream(filename, FLAGS_entry_length, FLAGS_entries_count);
        entries_count = FLAGS_entries_count;
      } else {
        stream = benchmark::replication::stream_t::CreateStream(FLAGS_db);
        entries_count = Value(stream)->Data()->Size();
      }
      stream_url = current::strings::Printf("127.0.0.1:%u/", FLAGS_local_port);
      scope += HTTP(FLAGS_local_port)
                   .Register("/", URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, *Value(stream));
    } else {
      stream_url = FLAGS_remote_url;
      const auto size_response = HTTP(GET(stream_url + "?sizeonly"));
      if (size_response.code != HTTPResponseCode.OK) {
        CURRENT_THROW(GetStreamSizeFailedException(size_response));
      }
      entries_count = current::FromString<uint64_t>(size_response.body);
    }
    const std::map<std::string, PERSISTER_TYPE> supported_persisters = {{"disk", PERSISTER_TYPE::DISK},
                                                                        {"memory", PERSISTER_TYPE::MEMORY}};
    try {
      persister_type = supported_persisters.at(FLAGS_persister);
    } catch (const std::out_of_range&) {
      CURRENT_THROW(InvalidPersisterTypeException(FLAGS_persister));
    }
  }

  template <typename STREAM, typename... ARGS>
  void Replicate(ARGS && ... args) {
    auto replicated_stream = STREAM::CreateStream(std::forward<ARGS>(args)...);
    current::sherlock::SubscribableRemoteStream<benchmark::replication::Entry> remote_stream(stream_url);
    current::sherlock::StreamReplicator<STREAM> replicator(replicated_stream);
    {
      const auto subscriber_scope = remote_stream.Subscribe(replicator);
      while (replicated_stream->Data()->Size() < entries_count) {
        std::this_thread::yield();
      }
      CURRENT_ASSERT(replicated_stream->Data()->Size() == entries_count);
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
