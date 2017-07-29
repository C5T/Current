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

#include "../../../bricks/dflags/dflags.h"
#include "../../../stream/replicator.h"

#include "entry.h"

DEFINE_string(url, "127.0.0.1:8383/raw_log", "The URL to subscribe to.");
DEFINE_string(replicated_stream_persister, "file", "`file` or `memory`.");
DEFINE_string(replicated_stream_data_filename,
              "",
              "If set, in `--replicated_stream_persister=fail` mode use this file for the destination of replication.");
DEFINE_uint64(total_entries, 0, "If set, the maximum number of entries to replicate.");
DEFINE_double(seconds, 0, "If set, the maximum number of seconds to run the benchmark for.");
DEFINE_bool(do_not_remove_replicated_data, false, "Set to not remove the data file.");
DEFINE_bool(use_checked_subscription, false, "Set to use checked subscription for the replication");
DEFINE_bool(use_safe_replication, false, "Set to use \"safe\" (checked) replication");

inline std::chrono::microseconds FastNow() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
}

template <typename STREAM, typename... ARGS>
void Replicate(ARGS&&... args) {
  auto replicated_stream = STREAM::CreateStream(std::forward<ARGS>(args)...);
  std::cerr << "Connecting to the stream at '" << FLAGS_url << "' ..." << std::flush;
  current::stream::SubscribableRemoteStream<benchmark::replication::Entry> remote_stream(FLAGS_url);
  auto replicator = std::make_unique<current::stream::StreamReplicator<STREAM>>(replicated_stream);
  std::cerr << "\b\b\bOK" << std::endl;

  const auto size_response = HTTP(GET(FLAGS_url + "?sizeonly"));
  if (size_response.code != HTTPResponseCode.OK) {
    std::cerr << "Cannot obtain the remote stream size, got an error " << static_cast<uint32_t>(size_response.code)
              << " response: " << size_response.body << std::endl;
    return;
  }
  const auto stream_size = current::FromString<uint64_t>(size_response.body);
  const uint64_t records_to_replicate = FLAGS_total_entries ? std::min(FLAGS_total_entries, stream_size) : stream_size;

  const auto start_time = FastNow();
  const auto stop_time =
      std::chrono::microseconds(FLAGS_seconds ? start_time.count() + static_cast<int64_t>(1e6 * FLAGS_seconds)
                                              : std::numeric_limits<int64_t>::max());

  {
    std::cerr << "Subscribing to the stream ..." << std::flush;
    const std::chrono::milliseconds print_delay(500);
	  const auto subscriber_scope = FLAGS_use_safe_replication
		? (current::stream::SubscriberScope)remote_stream.Subscribe(*replicator, 0, FLAGS_use_checked_subscription)
		: (current::stream::SubscriberScope)remote_stream.SubscribeUnsafe(*replicator, 0, FLAGS_use_checked_subscription);
    std::cerr << "\b\b\bOK" << std::endl;
    auto next_print_time = start_time + print_delay;
    while (replicated_stream->Data()->Size() < records_to_replicate) {
      std::this_thread::yield();
      const auto now = FastNow();
      if (now >= next_print_time || replicated_stream->Data()->Size() >= records_to_replicate) {
        next_print_time = now + print_delay;
        std::cerr << "\rReplicated " << replicated_stream->Data()->Size() << " of " << records_to_replicate
                  << " entries." << std::flush;
      }
      if (now >= stop_time) {
        break;
      }
    }
  }
  if (replicated_stream->Data()->Size() > records_to_replicate) {
    std::cerr << "\nWarning: more (" << replicated_stream->Data()->Size() << ") entries then requested ("
              << records_to_replicate << ") were replicated" << std::endl;
  }
  std::cerr << "\nReplication finished, calculating the stats ..." << std::flush;
  // The length of the json-serialized empty entry, including the '\n' in the end.
  const uint64_t empty_entry_length = JSON(benchmark::replication::Entry()).length() + 1;
  uint64_t replicated_data_size = 0;
  for (const auto& e : replicated_stream->Data()->Iterate()) {
    replicated_data_size += empty_entry_length + e.entry.s.length();
  }

  const auto duration_in_seconds = (FastNow() - start_time).count() * 1e-6;
  std::cerr << "\b\b\bOK\nSeconds\tEPS\tMBps" << std::endl;
  std::cout << duration_in_seconds << '\t' << replicated_stream->Data()->Size() / duration_in_seconds << '\t'
            << replicated_data_size / duration_in_seconds / 1024 / 1024 << std::endl;
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  if (FLAGS_replicated_stream_persister == "file") {
    const std::string filename = !FLAGS_replicated_stream_data_filename.empty() ? FLAGS_replicated_stream_data_filename
                                                                                : current::FileSystem::GenTmpFileName();
    std::unique_ptr<current::FileSystem::ScopedRmFile> temp_file_remover;
    if (!FLAGS_do_not_remove_replicated_data) {
      temp_file_remover = std::make_unique<current::FileSystem::ScopedRmFile>(filename);
    }
    std::cerr << "Replicating to " << filename << std::endl;
    Replicate<current::stream::Stream<benchmark::replication::Entry, current::persistence::File>>(filename);
  } else if (FLAGS_replicated_stream_persister == "memory") {
    Replicate<current::stream::Stream<benchmark::replication::Entry, current::persistence::Memory>>();
  } else {
    std::cout << "--replicated_stream_persister should be `file` or `memory`." << std::endl;
    return -1;
  }
  return 0;
}
