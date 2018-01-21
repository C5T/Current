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

#include "../../../bricks/dflags/dflags.h"

#include "../../../stream/replicator.h"
#include "../../../stream/stream.h"

DEFINE_uint32(n, 100, "The length of the desired chain of subscribers. 255 seems to be the limit on Linux. -- D.K.");
DEFINE_uint32(m, 1, "The number of \"events\" (really, single-integer JSONs) to work through per replication.");

DEFINE_uint16(base_port, 8500, "The port to start from, will use this one and up.");

DEFINE_uint32(iterations, 10, "The number of iterations to run.");

DEFINE_string(tmpdir, ".current", "The temporary directory to save file-persisted streams into.");

CURRENT_STRUCT(Event) {
  CURRENT_FIELD(x, int32_t, 0);
  CURRENT_CONSTRUCTOR(Event)(int32_t x = 0) : x(x) {}
};

void InitializeStream(Optional<current::Owned<current::stream::Stream<Event, current::persistence::Memory>>>& output,
                      int) {
  output = current::stream::Stream<Event, current::persistence::Memory>::CreateStreamWithCustomNamespaceName(
      current::ss::StreamNamespaceName("Stream", "Event"));
}

void InitializeStream(Optional<current::Owned<current::stream::Stream<Event, current::persistence::File>>>& output,
                      int index) {
  static int global_index = 0;  // Must have unique filenames otherwise Linux could crash. -- D.K.
  const std::string fn = current::FileSystem::JoinPath(
      FLAGS_tmpdir, "log-" + current::ToString(index) + '-' + current::ToString(++global_index));
  current::FileSystem::RmFile(fn, current::FileSystem::RmFileParameters::Silent);
  output = current::stream::Stream<Event, current::persistence::File>::CreateStreamWithCustomNamespaceName(
      current::ss::StreamNamespaceName("Stream", "Event"), fn);
}

template <template <typename> class PERSISTER>
double RunIteration() {
  using stream_t = current::stream::Stream<Event, PERSISTER>;

  // N streams.
  std::vector<Optional<current::Owned<stream_t>>> streams(FLAGS_n);
  for (uint32_t i = 0; i < FLAGS_n; ++i) {
    InitializeStream(streams[i], i);
  }

  // Their HTTP routes.
  HTTPRoutesScope http_routes_scope;
  for (uint32_t i = 0; i < FLAGS_n; ++i) {
    http_routes_scope += HTTP(FLAGS_base_port + static_cast<uint16_t>(i))
                             .Register("/stream", URLPathArgs::CountMask::Any, *Value(streams[i]));
  }

  // N remote subscribers, although the last one is unused.
  std::vector<std::unique_ptr<current::stream::SubscribableRemoteStream<Event>>> remote_subscribers(FLAGS_n);
  for (uint32_t i = 0; i < FLAGS_n; ++i) {
    remote_subscribers[i] = std::make_unique<current::stream::SubscribableRemoteStream<Event>>(
        Printf("http://localhost:%d/stream", static_cast<uint16_t>(FLAGS_base_port + i)), "Event", "Stream");
  }

  // (N - 1) replicators, where index zero, "replicate into the source", is left uninitialized.
  std::vector<std::unique_ptr<current::stream::StreamReplicator<stream_t>>> replicators(FLAGS_n);
  for (uint32_t i = 1; i < FLAGS_n; ++i) {
    replicators[i] = std::make_unique<current::stream::StreamReplicator<stream_t>>(Value(streams[i]));
  }

  // Start the chain of replications.
  std::vector<current::stream::SubscriberScope> subscriber_scopes(FLAGS_n);
  for (uint32_t i = 1; i < FLAGS_n; ++i) {
    subscriber_scopes[i] = remote_subscribers[i - 1]->Subscribe(*replicators[i]);
  }

  // Publish M events and watch them propagate.
  const auto begin = current::time::Now();
  for (uint32_t x = 1; x <= FLAGS_m; ++x) {
    Value(streams[0])->Publisher()->Publish(Event(x));
  }
  while (Value(streams.back())->Data()->Size() != FLAGS_m) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // Do the math and return "replications per second".
  const auto end = current::time::Now();
  const double seconds = (end - begin).count() * 1e-6;

  return (FLAGS_n - 1) / seconds;
}

template <template <typename> class PERSISTER>
void Run() {
  double sum = 0.0;
  double sum_squares = 0.0;
  for (uint32_t i = 0; i < FLAGS_iterations; ++i) {
    const double value = RunIteration<PERSISTER>();
    printf("Iteration %d/%d, %lf replications per second.\n", i + 1, FLAGS_iterations, value);
    sum += value;
    sum_squares += value * value;
  }
  const double mean = sum / FLAGS_iterations;
  const double stddev = std::sqrt((sum_squares / FLAGS_iterations) - (mean * mean));
  printf("Replications per second: %lf (stddev %lf).\n", mean, stddev);
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  printf("N = %d, M = %d\n", FLAGS_n, FLAGS_m);
#ifndef NDEBUG
  printf("DEBUG\n");
#endif
  printf("Memory\n");
  Run<current::persistence::Memory>();
  printf("File\n");
  Run<current::persistence::File>();
}
