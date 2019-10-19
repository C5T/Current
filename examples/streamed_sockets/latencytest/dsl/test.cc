/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2019 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include <array>

#include "dsl.h"

#include "../../../../3rdparty/gtest/gtest-main.h"

namespace current::examples::streamed_sockets {
struct SourceA {};
struct SourceB {};
struct WorkerX {};
struct WorkerY {};
struct WorkerZ {};
}  // namespace current::examples::streamed_sockets

TEST(NextRipcurrent, SequentialProcessingTypes) {
  using namespace current::examples::streamed_sockets;

  const auto source_a = BlockSource<SourceA>();
  const auto source_b = BlockSource<SourceB>();
  const auto worker_x = BlockWorker<WorkerX>();
  const auto worker_y = BlockWorker<WorkerY>();
  const auto worker_z = BlockWorker<WorkerZ>();

  static_assert(sizeof(is_same_or_compile_error<PipelineImpl<SourceA, TypeListImpl<PipelineWorker<1, 1, 1, WorkerX>>>,
                                                decltype(Pipeline(source_a, worker_x))>));

  static_assert(
      sizeof(is_same_or_compile_error<
             PipelineImpl<SourceB, TypeListImpl<PipelineWorker<1, 1, 1, WorkerY>, PipelineWorker<2, 2, 2, WorkerZ>>>,
             decltype(Pipeline(source_b, worker_y, worker_z))>));

  {
    auto const pipeline = Pipeline(source_a, worker_x);
    EXPECT_EQ(2, decltype(pipeline)::N);  // This one is `constexpr static`, the number of "steps" in the pipeline.

    PipelineState<std::decay_t<decltype(pipeline)>> state;
    EXPECT_EQ(2, decltype(state)::N);  // This one is `constexpr static`, the number of "steps" in the pipeline.

    EXPECT_EQ(0u, state.template ReadyCountFor<1>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<0>(2u);
    EXPECT_EQ(2u, state.template ReadyCountFor<1>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<1>(1u);
    EXPECT_EQ(2u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<1>(2u);
    EXPECT_EQ(2u, state.template ReadyCountFor<0>());
    EXPECT_EQ(2u, state.template ReadyCountFor<1>());
    EXPECT_EQ(2u, state.GrandTotalProcessedCount());
  }

  {
    const auto pipeline = Pipeline(source_b, worker_y, worker_z);
    EXPECT_EQ(3, decltype(pipeline)::N);

    PipelineState<std::decay_t<decltype(pipeline)>> state;
    EXPECT_EQ(3, decltype(state)::N);

    EXPECT_EQ(0u, state.template ReadyCountFor<1>());
    EXPECT_EQ(0u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<0>(3u);
    EXPECT_EQ(3u, state.template ReadyCountFor<1>());
    EXPECT_EQ(0u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<1>(2u);
    EXPECT_EQ(3u, state.template ReadyCountFor<1>());
    EXPECT_EQ(2u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<2>(1u);
    EXPECT_EQ(3u, state.template ReadyCountFor<1>());
    EXPECT_EQ(2u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<1>(3u);
    EXPECT_EQ(3u, state.template ReadyCountFor<1>());
    EXPECT_EQ(3u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<2>(3u);
    EXPECT_EQ(3u, state.template ReadyCountFor<0>());
    EXPECT_EQ(3u, state.template ReadyCountFor<1>());
    EXPECT_EQ(3u, state.template ReadyCountFor<2>());
    EXPECT_EQ(3u, state.GrandTotalProcessedCount());
  }
}

TEST(NextRipcurrent, ParallelProcessingTypes) {
  using namespace current::examples::streamed_sockets;

  const auto source_a = BlockSource<SourceA>();
  const auto worker_x = BlockWorker<WorkerX>();
  const auto worker_y = BlockWorker<WorkerY>();
  const auto worker_z = BlockWorker<WorkerZ>();

  // Some inner-level template expansion specs.
  static_assert(
      sizeof(is_same_or_compile_error<ParallelPipelineWrapper<WorkerX>, decltype(ParallelPipeline(worker_x))>));

  static_assert(sizeof(is_same_or_compile_error<ParallelPipelineWrapper<WorkerX, WorkerY>,
                                                decltype(ParallelPipeline(worker_x, worker_y))>));

  static_assert(sizeof(is_same_or_compile_error<ParallelPipelineWrapper<WorkerX, WorkerY, WorkerZ>,
                                                decltype(ParallelPipeline(worker_x, worker_y, worker_z))>));

  // Wrapping a single worker into `ParallelPipeline` does nothing.
  static_assert(sizeof(is_same_or_compile_error<decltype(Pipeline(source_a, worker_x)),
                                                decltype(Pipeline(source_a, ParallelPipeline(worker_x)))>));
  static_assert(sizeof(is_same_or_compile_error<decltype(Pipeline(source_a, worker_x, worker_y)),
                                                decltype(Pipeline(source_a, ParallelPipeline(worker_x), worker_y))>));
  static_assert(sizeof(is_same_or_compile_error<decltype(Pipeline(source_a, worker_x, worker_y)),
                                                decltype(Pipeline(source_a, worker_x, ParallelPipeline(worker_y)))>));
  static_assert(sizeof(
      is_same_or_compile_error<decltype(Pipeline(source_a, worker_x, worker_y)),
                               decltype(Pipeline(source_a, ParallelPipeline(worker_x), ParallelPipeline(worker_y)))>));

  // Now the test cases of actual parallellism.
  static_assert(
      sizeof(is_same_or_compile_error<
             PipelineImpl<SourceA, TypeListImpl<PipelineWorker<1, 1, 2, WorkerX>, PipelineWorker<2, 1, 2, WorkerY>>>,
             decltype(Pipeline(source_a, ParallelPipeline(worker_x, worker_y)))>));

  static_assert(
      sizeof(is_same_or_compile_error<PipelineImpl<SourceA,
                                                   TypeListImpl<PipelineWorker<1, 1, 2, WorkerX>,
                                                                PipelineWorker<2, 1, 2, WorkerY>,
                                                                PipelineWorker<3, 3, 3, WorkerZ>>>,
                                      decltype(Pipeline(source_a, ParallelPipeline(worker_x, worker_y), worker_z))>));

  static_assert(
      sizeof(is_same_or_compile_error<PipelineImpl<SourceA,
                                                   TypeListImpl<PipelineWorker<1, 1, 1, WorkerX>,
                                                                PipelineWorker<2, 2, 3, WorkerY>,
                                                                PipelineWorker<3, 2, 3, WorkerZ>>>,
                                      decltype(Pipeline(source_a, worker_x, ParallelPipeline(worker_y, worker_z)))>));

  static_assert(
      sizeof(is_same_or_compile_error<PipelineImpl<SourceA,
                                                   TypeListImpl<PipelineWorker<1, 1, 3, WorkerX>,
                                                                PipelineWorker<2, 1, 3, WorkerY>,
                                                                PipelineWorker<3, 1, 3, WorkerZ>>>,
                                      decltype(Pipeline(source_a, ParallelPipeline(worker_x, worker_y, worker_z)))>));

  static_assert(sizeof(is_same_or_compile_error<
                       PipelineImpl<SourceA,
                                    TypeListImpl<PipelineWorker<1, 1, 1, WorkerX>,
                                                 PipelineWorker<2, 2, 3, WorkerY>,
                                                 PipelineWorker<3, 2, 3, WorkerZ>,
                                                 PipelineWorker<4, 4, 4, WorkerX>>>,
                       decltype(Pipeline(source_a, worker_x, ParallelPipeline(worker_y, worker_z), worker_x))>));

  // First, the "simple" case, where the parallel part is in the middle.
  // It is simple, because the parallelism is neither immediately before nor immediately after the source.
  {
    const auto pipeline = Pipeline(source_a, worker_x, ParallelPipeline(worker_y, worker_z), worker_x);
    EXPECT_EQ(5, decltype(pipeline)::N);

    PipelineState<std::decay_t<decltype(pipeline)>> state;
    EXPECT_EQ(5, decltype(state)::N);

    // Clean slate.
    EXPECT_EQ(0u, state.template ReadyCountFor<1>());
    EXPECT_EQ(0u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.template ReadyCountFor<4>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    // The source producing one record enables the first worker.
    state.template UpdateOutput<0>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(0u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.template ReadyCountFor<4>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    // The first worker processing this record enables the second and the third ones, which are parallelized.
    state.template UpdateOutput<1>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.template ReadyCountFor<4>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    // The second worker alone doesn't make a difference, as we need to wait for the third.
    state.template UpdateOutput<2>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.template ReadyCountFor<4>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    // The third worker cacthing up with the second one enables the fourth.
    state.template UpdateOutput<3>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.template ReadyCountFor<3>());
    EXPECT_EQ(1u, state.template ReadyCountFor<4>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    // Finally, the firth worker processing this record allows overwriting it.
    state.template UpdateOutput<4>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.template ReadyCountFor<3>());
    EXPECT_EQ(1u, state.template ReadyCountFor<4>());
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());

    // And now the same process but with flipped order in which the second and third worker process the record.
    state.template UpdateOutput<0>(2u);
    EXPECT_EQ(2u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.template ReadyCountFor<3>());
    EXPECT_EQ(1u, state.template ReadyCountFor<4>());
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());
    state.template UpdateOutput<1>(2u);
    EXPECT_EQ(2u, state.template ReadyCountFor<1>());
    EXPECT_EQ(2u, state.template ReadyCountFor<2>());
    EXPECT_EQ(2u, state.template ReadyCountFor<3>());
    EXPECT_EQ(1u, state.template ReadyCountFor<4>());
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());
    state.template UpdateOutput<3>(2u);
    EXPECT_EQ(2u, state.template ReadyCountFor<1>());
    EXPECT_EQ(2u, state.template ReadyCountFor<2>());
    EXPECT_EQ(2u, state.template ReadyCountFor<3>());
    EXPECT_EQ(1u, state.template ReadyCountFor<4>());
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());
    state.template UpdateOutput<2>(2u);
    EXPECT_EQ(2u, state.template ReadyCountFor<1>());
    EXPECT_EQ(2u, state.template ReadyCountFor<2>());
    EXPECT_EQ(2u, state.template ReadyCountFor<3>());
    EXPECT_EQ(2u, state.template ReadyCountFor<4>());
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());
    state.template UpdateOutput<4>(2u);
    EXPECT_EQ(2u, state.template ReadyCountFor<1>());
    EXPECT_EQ(2u, state.template ReadyCountFor<2>());
    EXPECT_EQ(2u, state.template ReadyCountFor<3>());
    EXPECT_EQ(2u, state.template ReadyCountFor<4>());
    EXPECT_EQ(2u, state.GrandTotalProcessedCount());

    EXPECT_EQ(2u, state.template ReadyCountFor<0>());
  }

  // The "simplification" of the above, where the full circle requires the source waiting for both parallel workers.
  {
    const auto pipeline = Pipeline(source_a, worker_x, ParallelPipeline(worker_y, worker_z));
    EXPECT_EQ(4, decltype(pipeline)::N);

    PipelineState<std::decay_t<decltype(pipeline)>> state;
    EXPECT_EQ(4, decltype(state)::N);

    EXPECT_EQ(0u, state.template ReadyCountFor<1>());
    EXPECT_EQ(0u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<0>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(0u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<1>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<2>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<3>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.template ReadyCountFor<3>());
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());

    EXPECT_EQ(1u, state.template ReadyCountFor<0>());
  }

  // And another "simplification" of the first test, where the parallelization starts right at the source.
  {
    const auto pipeline = Pipeline(source_a, ParallelPipeline(worker_x, worker_y), worker_z);
    EXPECT_EQ(4, decltype(pipeline)::N);

    PipelineState<std::decay_t<decltype(pipeline)>> state;
    EXPECT_EQ(4, decltype(state)::N);

    EXPECT_EQ(0u, state.template ReadyCountFor<1>());
    EXPECT_EQ(0u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<0>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<1>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<2>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<3>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.template ReadyCountFor<3>());
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());

    EXPECT_EQ(1u, state.template ReadyCountFor<0>());
  }

  // And the "trivial" case of just two workers in parallel.
  {
    const auto pipeline = Pipeline(source_a, ParallelPipeline(worker_x, worker_y));
    EXPECT_EQ(3, decltype(pipeline)::N);

    PipelineState<std::decay_t<decltype(pipeline)>> state;
    EXPECT_EQ(3, decltype(state)::N);

    EXPECT_EQ(0u, state.template ReadyCountFor<1>());
    EXPECT_EQ(0u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<0>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<1>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<2>(1u);
    EXPECT_EQ(1u, state.template ReadyCountFor<1>());
    EXPECT_EQ(1u, state.template ReadyCountFor<2>());
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());

    EXPECT_EQ(1u, state.template ReadyCountFor<0>());
  }

  // And the case of three workers, all running in parallel.
  {
    const auto pipeline = Pipeline(source_a, ParallelPipeline(worker_x, worker_y, worker_z));
    EXPECT_EQ(4, decltype(pipeline)::N);

    PipelineState<std::decay_t<decltype(pipeline)>> state;
    EXPECT_EQ(4, decltype(state)::N);

    EXPECT_EQ(0u, state.template ReadyCountFor<1>());
    EXPECT_EQ(0u, state.template ReadyCountFor<2>());
    EXPECT_EQ(0u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<0>(5u);
    EXPECT_EQ(5u, state.template ReadyCountFor<1>());
    EXPECT_EQ(5u, state.template ReadyCountFor<2>());
    EXPECT_EQ(5u, state.template ReadyCountFor<3>());
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<1>(1u);
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<2>(2u);
    EXPECT_EQ(0u, state.GrandTotalProcessedCount());

    state.template UpdateOutput<3>(3u);
    EXPECT_EQ(1u, state.GrandTotalProcessedCount());  // min(1, 2, 3) == 1.

    state.template UpdateOutput<1>(5u);
    EXPECT_EQ(2u, state.GrandTotalProcessedCount());  // min(5, 2, 3) == 2.

    state.template UpdateOutput<2>(5u);
    EXPECT_EQ(3u, state.GrandTotalProcessedCount());  // min(5, 5, 3) == 3.

    state.template UpdateOutput<3>(5u);
    EXPECT_EQ(5u, state.GrandTotalProcessedCount());  // min(5, 5, 5) == 5.

    EXPECT_EQ(5u, state.template ReadyCountFor<0>());
  }
}

namespace current::examples::streamed_sockets {

struct TestBlob {
  uint64_t x[4];
};

struct TestBlobSource {
  int foo = 100;  // For the test to make sure just one instance of `TestBlobSource` was created per pipeline.
  std::array<uint64_t, 4> value;
  const std::array<uint64_t, 4> delta;
  TestBlob blob;
  TestBlobSource(std::array<uint64_t, 4> init, std::array<uint64_t, 4> delta) : value(init), delta(delta) {}
  size_t DoGetInput(uint8_t* begin, uint8_t* end) {
    size_t count = (end - begin) / sizeof(TestBlob);
    TestBlob* ptr = reinterpret_cast<TestBlob*>(begin);
    for (size_t i = 0; i < count; ++i) {
      for (size_t i = 0; i < 4; ++i) {
        ptr->x[i] = value[i];
        value[i] += delta[i];
      }
      ++ptr;
    }
    return count * sizeof(TestBlob);
  }
};

template <int I>
struct TestBlobCollector {
  int bar = 200;  // For the test to make sure just one instance of `TestBlobCollector` is created per pipeline block.
  std::vector<uint64_t>& output;
  explicit TestBlobCollector(std::vector<uint64_t>& output) : output(output) {}
  TestBlob* DoWork(TestBlob* begin, TestBlob* end) {
    while (begin != end) {
      output.push_back(begin->x[I]);
      ++begin;
    }
    return begin;
  }
};

struct TestBlobModifier {
  const size_t i;
  const uint64_t k;
  const uint64_t b;
  TestBlobModifier(size_t i, uint64_t k, uint64_t b) : i(i), k(k), b(b) {}
  TestBlob* DoWork(TestBlob* begin, TestBlob* end) {
    while (begin != end) {
      begin->x[i] = begin->x[i] * k + b;
      ++begin;
    }
    return begin;
  }
};

template <class PROCESSING_PIPELINE>
void RunProcessingTest(std::array<uint64_t, 4>& init,
                       std::array<uint64_t, 4>& step,
                       std::array<std::vector<uint64_t>, 4>& output,
                       PROCESSING_PIPELINE pipeline) {
  {
    // The smoke test.
    init = {100, 100, 100, 100};
    step = {1, 2, 3, 4};
    for (std::vector<uint64_t>& output_vector : output) {
      output_vector.clear();
    }

    auto context = pipeline.Run(PipelineRunParams<TestBlob>());

    EXPECT_EQ(5u, context.N);

    static_assert(sizeof(is_same_or_compile_error<TestBlobSource&, decltype(context.Source())>));
    static_assert(sizeof(is_same_or_compile_error<TestBlobCollector<0>&, decltype(context.template Worker<1>())>));

    const auto immutable_context = context;

    static_assert(sizeof(is_same_or_compile_error<const TestBlobSource&, decltype(immutable_context.Source())>));
    static_assert(sizeof(
        is_same_or_compile_error<const TestBlobCollector<1>&, decltype(immutable_context.template Worker<2>())>));
    static_assert(sizeof(
        is_same_or_compile_error<const TestBlobCollector<2>&, decltype(immutable_context.template Worker<3>())>));
    static_assert(sizeof(
        is_same_or_compile_error<const TestBlobCollector<3>&, decltype(immutable_context.template Worker<4>())>));

    EXPECT_EQ(100, context.Source().foo);
    EXPECT_EQ(200, context.template Worker<1>().bar);
    EXPECT_EQ(200, context.template Worker<2>().bar);
    EXPECT_EQ(200, context.template Worker<3>().bar);
    EXPECT_EQ(200, context.template Worker<4>().bar);
    EXPECT_EQ(100, immutable_context.Source().foo);
    EXPECT_EQ(200, immutable_context.template Worker<1>().bar);
    ++context.Source().foo;
    context.template Worker<1>().bar += 2;
    context.template Worker<2>().bar += 3;
    context.template Worker<3>().bar += 4;
    context.template Worker<4>().bar += 5;
    EXPECT_EQ(101, context.Source().foo);
    EXPECT_EQ(202, context.template Worker<1>().bar);
    EXPECT_EQ(101, immutable_context.Source().foo);
    EXPECT_EQ(202, immutable_context.template Worker<1>().bar);
    EXPECT_EQ(203, immutable_context.template Worker<2>().bar);
    EXPECT_EQ(204, immutable_context.template Worker<3>().bar);
    EXPECT_EQ(205, immutable_context.template Worker<4>().bar);

    while (context.GrandTotalProcessedCount() < 100u) {
      std::this_thread::yield();
    }

    context.ForceStop();

    ASSERT_GE(context.GrandTotalProcessedCount(), 100u);
    ASSERT_GE(output[0].size(), 100u);
    ASSERT_GE(output[1].size(), 100u);
    ASSERT_GE(output[2].size(), 100u);
    ASSERT_GE(output[3].size(), 100u);

    for (size_t t = 0; t < 4; ++t) {
      for (size_t i = 0; i < output[t].size(); ++i) {
        EXPECT_EQ(output[t][i], init[t] + i * step[t]);
      }
    }
  }

  {
    // The performance test.
    init = {1'000'000'000, 1'000'000'000, 1'000'000'000, 1'000'000'000};
    step = {1, 2, 3, 4};
    for (std::vector<uint64_t>& output_vector : output) {
      output_vector.clear();
    }

    auto context = pipeline.Run(PipelineRunParams<TestBlob>().SetCircularBufferSize(1ull << 29));  // 512MB.

#ifdef NDEBUG
    constexpr static size_t N = 50'000'000u;
#else
    constexpr static size_t N = 10'000'000u;
#endif

    while (context.GrandTotalProcessedCount() < N) {
      std::this_thread::yield();
    }

    context.ForceStop();

    ASSERT_GE(context.GrandTotalProcessedCount(), N);
    ASSERT_GE(output[0].size(), N);
    ASSERT_GE(output[1].size(), N);
    ASSERT_GE(output[2].size(), N);
    ASSERT_GE(output[3].size(), N);

    for (size_t t = 0; t < 4; ++t) {
      for (size_t i = 0; i < output[t].size(); i += 1'000) {
        EXPECT_EQ(output[t][i], init[t] + i * step[t]);
      }
    }
  }
}

}  // namespace current::examples::streamed_sockets

TEST(NextRipcurrent, SequentialProcessing) {
  using namespace current::examples::streamed_sockets;

  std::array<uint64_t, 4> init;
  std::array<uint64_t, 4> step;
  std::array<std::vector<uint64_t>, 4> output;
  RunProcessingTest(init,
                    step,
                    output,
                    Pipeline(BlockSource<TestBlobSource>(std::cref(init), std::cref(step)),
                             BlockWorker<TestBlobCollector<0>>(std::ref(output[0])),
                             BlockWorker<TestBlobCollector<1>>(std::ref(output[1])),
                             BlockWorker<TestBlobCollector<2>>(std::ref(output[2])),
                             BlockWorker<TestBlobCollector<3>>(std::ref(output[3]))));
}

TEST(NextRipcurrent, ParallelProcessing) {
  using namespace current::examples::streamed_sockets;

  std::array<uint64_t, 4> init;
  std::array<uint64_t, 4> step;
  std::array<std::vector<uint64_t>, 4> output;
  RunProcessingTest(init,
                    step,
                    output,
                    Pipeline(BlockSource<TestBlobSource>(std::cref(init), std::cref(step)),
                             ParallelPipeline(BlockWorker<TestBlobCollector<0>>(std::ref(output[0])),
                                              BlockWorker<TestBlobCollector<1>>(std::ref(output[1])),
                                              BlockWorker<TestBlobCollector<2>>(std::ref(output[2])),
                                              BlockWorker<TestBlobCollector<3>>(std::ref(output[3])))));
}

TEST(NextRipcurrent, ProcessingLogicSmokeTest) {
  using namespace current::examples::streamed_sockets;

  {
    std::vector<uint64_t> output;
    auto context = Pipeline(BlockSource<TestBlobSource>(std::array<uint64_t, 4>({0, 0, 0, 0}),
                                                        std::array<uint64_t, 4>({1, 1, 1, 1})),
                            BlockWorker<TestBlobModifier>(0, 2, 100),
                            BlockWorker<TestBlobCollector<0>>(std::ref(output)))
                       .Run(PipelineRunParams<TestBlob>());
    while (context.GrandTotalProcessedCount() < 3u) {
      std::this_thread::yield();
    }
    context.ForceStop();

    ASSERT_GE(output.size(), 3u);
    EXPECT_EQ(100u, output[0]);
    EXPECT_EQ(102u, output[1]);
    EXPECT_EQ(104u, output[2]);
  }
  {
    std::vector<uint64_t> output;
    auto context = Pipeline(BlockSource<TestBlobSource>(std::array<uint64_t, 4>({0, 0, 0, 0}),
                                                        std::array<uint64_t, 4>({1, 1, 1, 1})),
                            BlockWorker<TestBlobModifier>(1, 2, 100),
                            BlockWorker<TestBlobModifier>(1, 2, 100),
                            BlockWorker<TestBlobCollector<1>>(std::ref(output)))
                       .Run(PipelineRunParams<TestBlob>());
    while (context.GrandTotalProcessedCount() < 3u) {
      std::this_thread::yield();
    }
    context.ForceStop();

    ASSERT_GE(output.size(), 3u);
    EXPECT_EQ(300u, output[0]);
    EXPECT_EQ(304u, output[1]);
    EXPECT_EQ(308u, output[2]);
  }
}
