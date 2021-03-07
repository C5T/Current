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

// NOTE(dkorolev): This should most certainly not live in `examples/streamed_sockets/latencytest/dsl` some time soon.

// NOTE(dkorolev): This is not really a DSL. It may not be a DSL "yet", or we may choose to not go there at all.
// As of now, it's more of a template-metaprogrammed "clean" solution to defining and running fast crunching pipelines.

// NOTE(dkorolev): This is not really the implementation of the full sequential/parallel worker setup, but rather
// a sequential way to run individual workers or parallel groups of workers. I may want to extend it later.
// In other words, TODO(dkorolev): Allow `SequentialPipeline`-s within `ParallelPipeline`-s, and vice versa.

// TODO(dkorolev): Have `ParallelPipeline` within `ParallelPipeline` fail with a nice static assert message.

// TODO(dkorolev): The HTTP/HTML/gnuplot/graphviz status page (source/workers topology + processing rate + history).

#ifndef EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_DSL_DSL_H
#define EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_DSL_DSL_H

#include <iostream>
#include <thread>
#include <vector>

#include "../../../../bricks/sync/waitable_atomic.h"
#include "../../../../bricks/template/typelist.h"
#include "../../../../bricks/util/lazy_instantiation.h"

namespace current::examples::streamed_sockets {

enum class SourceOrWorker : bool { Source = true, Worker = false };

template <SourceOrWorker, class T>
class BlockSourceOrWorker final {
 private:
  struct Impl final {
    const current::LazilyInstantiated<T> instantiator;
    template <typename... ARGS>
    explicit Impl(ARGS&&... args) : instantiator(current::DelayedInstantiate<T>(std::forward<ARGS>(args)...)) {}
  };
  std::shared_ptr<Impl> impl_;

 public:
  using block_t = T;

  BlockSourceOrWorker(const BlockSourceOrWorker& rhs) : impl_(rhs.impl_) {}
  BlockSourceOrWorker(BlockSourceOrWorker&& rhs) : impl_(std::move(rhs.impl_)) {}
  BlockSourceOrWorker& operator=(const BlockSourceOrWorker& rhs) {
    impl_ = rhs.impl_;
    return *this;
  }
  BlockSourceOrWorker& operator=(BlockSourceOrWorker&& rhs) {
    impl_ = std::move(rhs.impl_);
    return *this;
  }

  BlockSourceOrWorker() { impl_ = std::make_shared<Impl>(); }

  // NOTE(dkorolev): This ugliness is essential, otherwise this constructor is chosed instead of the copy/move ones. :-(
  template <typename X, typename... XS, class = std::enable_if_t<!std::is_same_v<std::decay_t<X>, BlockSourceOrWorker>>>
  BlockSourceOrWorker(X&& arg, XS&&... args) {
    impl_ = std::make_shared<Impl>(std::forward<X>(arg), std::forward<XS>(args)...);
  }

  std::unique_ptr<T> Instantiate() const { return impl_->instantiator.InstantiateAsUniquePtr(); }
};

template <class T>
using BlockSource = BlockSourceOrWorker<SourceOrWorker::Source, T>;

template <class T>
using BlockWorker = BlockSourceOrWorker<SourceOrWorker::Worker, T>;

template <class T>
struct IsBlockSource final {
  inline constexpr static bool value = false;
};

template <class T>
struct IsBlockWorker final {
  inline constexpr static bool value = false;
};

template <class T>
struct IsBlockSource<BlockSource<T>> final {
  inline constexpr static bool value = true;
  using source_impl_t = T;
};

template <class T>
struct IsBlockWorker<BlockWorker<T>> final {
  inline constexpr static bool value = true;
  using worker_impl_t = T;
};

template <class T>
inline constexpr bool is_block_source_v = IsBlockSource<T>::value;

template <class T>
inline constexpr bool is_block_worker_v = IsBlockWorker<T>::value;

template <class T>
using extract_source_impl_t = typename IsBlockSource<T>::source_impl_t;

template <class T>
using extract_worker_impl_t = typename IsBlockWorker<T>::worker_impl_t;

// `PipelineWorker` is the worker wrapped into the assembled pipeline.
//
// `I` is the 1-based index of this worker, with 0 is reserved for the source. The `I`-s of all workers are distinct,
// and there is one `volatile size_t` counter kept per worked in pipeline state. This counter keeps track of the
// number of entries processed so far by this worker.
//
// `[U, V]` is the range of (1-based) indexes of workers that should complete in order for it to be acceptable
// to use the result of this worker.
//
// For sequential processing, both `U` and `V` are equal to `I`. For parallel processing, by M workers simultaneously,
// each of them will have `(V - U) == (M + 1)`, and their respective `I`-s will be between `U and `V`, inclusive.
//
// NOTE(dkorolev): Yes, this is not the complete solution, as it does not support interleaving sequential work
// with parallel work, except in the simplest setting. In reality, instead of the `[U, V]` range there should be
// a list of indexes. I am not going to do it any time soon though.
template <int I, int U, int V, class WORKER>
struct PipelineWorker final {
  static_assert(I >= U && I <= V);
};

template <class WORKER>
struct BlockWorkerFromPipelineWorkerImpl;

template <int I, int U, int V, class WORKER>
struct BlockWorkerFromPipelineWorkerImpl<PipelineWorker<I, U, V, WORKER>> final {
  using block_worker_t = BlockWorker<WORKER>;
};

template <class WORKER>
using block_worker_from_pipeline_worker_t = typename BlockWorkerFromPipelineWorkerImpl<WORKER>::block_worker_t;

template <class SOURCE>
struct SourceInstance {
  std::unique_ptr<SOURCE> source_instance;
};

template <int I, int N, class WORKERS_TYPELIST>
struct WorkerInstance : WorkerInstance<I + 1, N, WORKERS_TYPELIST> {
  static_assert(N == TypeListSize<WORKERS_TYPELIST> + 1);
  static_assert(I > 0 && I < N);
  using worker_t = extract_worker_impl_t<TypeListElement<I - 1, WORKERS_TYPELIST>>;
  std::unique_ptr<worker_t> worker_instance;
};

template <int N, class WORKERS_TYPELIST>
struct WorkerInstance<N, N, WORKERS_TYPELIST> {};

template <class SOURCE, class WORKERS_AS_TUPLE>
struct PipelineImpl;

template <class>
struct PipelineInstances;

template <class SOURCE, class... WORKERS>
struct PipelineInstances<PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>> final
    : WorkerInstance<1, sizeof...(WORKERS) + 1, TypeListImpl<block_worker_from_pipeline_worker_t<WORKERS>...>>,
      SourceInstance<SOURCE> {};

template <class SOURCE>
struct SourceInstantiator {
  const BlockSource<SOURCE> source;
  explicit SourceInstantiator(BlockSource<SOURCE> source) : source(std::move(source)) {}
  template <class PIPELINE>
  void DoInstantiate(PipelineInstances<PIPELINE>& instances) const {
    instances.SourceInstance<SOURCE>::source_instance = source.Instantiate();
  }
};

template <class... TS>
struct ParallelPipelineWrapper final {
  using block_workers_tuple_t = std::tuple<BlockWorker<TS>...>;
  block_workers_tuple_t workers;
  explicit ParallelPipelineWrapper(block_workers_tuple_t workers) : workers(std::move(workers)) {}
};

struct WorkersInstantiatorImplConstructSequential {};

template <int J, int M>
struct WorkersInstantiatorImplConstructParallel {};

template <int I, class WORKER>
struct WorkersInstantiatorInitializerListWorkerCapturer {
  std::unique_ptr<WORKER> captured_worker;
  WorkersInstantiatorInitializerListWorkerCapturer() = default;
  explicit WorkersInstantiatorInitializerListWorkerCapturer(WORKER worker)
      : captured_worker(std::make_unique<WORKER>(std::move(worker))) {}
};

template <int I, int N, class WORKERS_TYPELIST>
struct WorkersInstantiatorImpl
    : WorkersInstantiatorInitializerListWorkerCapturer<I, TypeListElement<I - 1, WORKERS_TYPELIST>>,
      WorkersInstantiatorImpl<I + 1, N, WORKERS_TYPELIST> {
  static_assert(I >= 1 && I < N);
  static_assert(N == TypeListSize<WORKERS_TYPELIST> + 1);

  using worker_t = TypeListElement<I - 1, WORKERS_TYPELIST>;
  static_assert(is_block_worker_v<worker_t>);

  const worker_t worker;

 protected:
  // The very next argument is a pure `BlockWorker`, use it.
  template <class... MIXED_ARGS>
  WorkersInstantiatorImpl(WorkersInstantiatorImplConstructSequential, worker_t worker, MIXED_ARGS&&... args)
      : WorkersInstantiatorImpl<I + 1, N, WORKERS_TYPELIST>(std::forward<MIXED_ARGS>(args)...),
        worker(std::move(worker)) {}

  // The next `(M - J)` workers to be instantiated are pre-packaged into `ParallelPipeline(...)` by the caller, and can
  // be extracted from the `ParallelPipelineWrapper`'s tuple. They can safely be `std::move()`-d away from there.
  template <int J, int M, class... PARALLEL_ARGS, class... MIXED_ARGS>
  WorkersInstantiatorImpl(WorkersInstantiatorImplConstructParallel<J, M>,
                          ParallelPipelineWrapper<PARALLEL_ARGS...>&& parallel_args,
                          MIXED_ARGS&&... args)
      : WorkersInstantiatorInitializerListWorkerCapturer<I, worker_t>(std::move(std::get<J>(parallel_args.workers))),
        WorkersInstantiatorImpl<I + 1, N, WORKERS_TYPELIST>(WorkersInstantiatorImplConstructParallel<J + 1, M>(),
                                                            std::move(parallel_args),
                                                            std::forward<MIXED_ARGS>(args)...),
        worker(std::move(*WorkersInstantiatorInitializerListWorkerCapturer<I, worker_t>::captured_worker)) {}

  // All parallel workers have been extracted from `ParallelPipelineWrapper`'s tuple, resume "default" initialization.
  template <int M, class... PARALLEL_ARGS, class... MIXED_ARGS>
  WorkersInstantiatorImpl(WorkersInstantiatorImplConstructParallel<M, M>,
                          ParallelPipelineWrapper<PARALLEL_ARGS...>&&,
                          MIXED_ARGS&&... args)
      : WorkersInstantiatorImpl<I, N, WORKERS_TYPELIST>(std::forward<MIXED_ARGS>(args)...) {}

 public:
  template <class... MIXED_ARGS>
  WorkersInstantiatorImpl(worker_t worker, MIXED_ARGS&&... args)
      : WorkersInstantiatorImpl(
            WorkersInstantiatorImplConstructSequential(), std::move(worker), std::forward<MIXED_ARGS>(args)...) {}

  template <class... PARALLEL_ARGS, class... MIXED_ARGS>
  WorkersInstantiatorImpl(ParallelPipelineWrapper<PARALLEL_ARGS...>&& parallel_args, MIXED_ARGS&&... args)
      : WorkersInstantiatorImpl(WorkersInstantiatorImplConstructParallel<0, sizeof...(PARALLEL_ARGS)>(),
                                std::move(parallel_args),
                                std::forward<MIXED_ARGS>(args)...) {}

  template <class PIPELINE>
  void DoInstantiate(PipelineInstances<PIPELINE>& instances) const {
    WorkersInstantiatorImpl<I + 1, N, WORKERS_TYPELIST>::DoInstantiate(instances);
    instances.WorkerInstance<I, N, WORKERS_TYPELIST>::worker_instance = worker.Instantiate();
  }
};

template <int N, class WORKERS_TYPELIST>
struct WorkersInstantiatorImpl<N, N, WORKERS_TYPELIST> {
 protected:
  WorkersInstantiatorImpl() {}
  WorkersInstantiatorImpl(WorkersInstantiatorImplConstructSequential) {}
  template <int M, class UNUSED_PARALLEL_WORKERS>
  WorkersInstantiatorImpl(WorkersInstantiatorImplConstructParallel<M, M>, UNUSED_PARALLEL_WORKERS&&) {}

 public:
  template <class PIPELINE>
  void DoInstantiate(PipelineInstances<PIPELINE>&) const {}
};

template <class WORKERS_TYPELIST>
using WorkersInstantiator = WorkersInstantiatorImpl<1, TypeListSize<WORKERS_TYPELIST> + 1, WORKERS_TYPELIST>;

template <class IMPL>
struct PipelineRunner;

template <class IMPL, typename BLOB>
struct PipelineRunContext;

template <typename BLOB>
struct PipelineRunParams {
  size_t circular_buffer_size = (1ull << 20);  // 1MB sounds like a reasonable default. -- D.K.
  PipelineRunParams& SetCircularBufferSize(size_t value) {
    circular_buffer_size = value;
    return *this;
  }
};

template <class SOURCE, class... WORKERS>
struct PipelineImpl<SOURCE, TypeListImpl<WORKERS...>> final
    : SourceInstantiator<SOURCE>,
      WorkersInstantiator<TypeListImpl<block_worker_from_pipeline_worker_t<WORKERS>...>> {
  using this_t = PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>;
  constexpr static int N = sizeof...(WORKERS) + 1;

  // Can't just use `block_worker_from_pipeline_worker_t<WORKERS>... args`, because `ParallelPipeline`-s can be there.
  template <class... MIXED_ARGS>
  PipelineImpl(BlockSource<SOURCE> source, MIXED_ARGS&&... args)
      : SourceInstantiator<SOURCE>(std::move(source)),
        WorkersInstantiator<TypeListImpl<block_worker_from_pipeline_worker_t<WORKERS>...>>(
            std::forward<MIXED_ARGS>(args)...) {}

  void InstantiatePipeline(PipelineInstances<this_t>& unique_instances) const {
    // Respect the historical design of instantiating right-to-left.
    WorkersInstantiator<TypeListImpl<block_worker_from_pipeline_worker_t<WORKERS>...>>::DoInstantiate(unique_instances);
    SourceInstantiator<SOURCE>::DoInstantiate(unique_instances);
  }

  template <typename BLOB>
  PipelineRunContext<this_t, BLOB> Run(const PipelineRunParams<BLOB>& params) const {
    return PipelineRunner<this_t>::template DoRun<BLOB>(*this, params);
  }
};

template <int I, class... WORKERS>
struct PipelineTypeListBuilder;

template <int I>
struct PipelineTypeListBuilder<I> {
  using typelist_t = TypeListImpl<>;
};

template <int I, class WORKER, class... WORKERS>
struct PipelineTypeListBuilder<I, BlockWorker<WORKER>, WORKERS...> {
  using typelist_t = metaprogramming::TypeListCat<TypeListImpl<PipelineWorker<I, I, I, WORKER>>,
                                                  typename PipelineTypeListBuilder<I + 1, WORKERS...>::typelist_t>;
};

template <int I, class WORKER>
struct PipelineTypeListBuilder<I, BlockWorker<WORKER>> {
  using typelist_t = TypeListImpl<PipelineWorker<I, I, I, WORKER>>;
};

template <int, int, int, class...>
struct ExpandParallelWorkers;

template <int I, int U, int V, class WORKER>
struct ExpandParallelWorkers<I, U, V, WORKER> {
  static_assert(I >= U && I <= V);
  using typelist_t = TypeListImpl<PipelineWorker<I, U, V, WORKER>>;
};

template <int I, int U, int V, class WORKER, class... WORKERS>
struct ExpandParallelWorkers<I, U, V, WORKER, WORKERS...> {
  static_assert(I >= U && I <= V);
  using typelist_t = metaprogramming::TypeListCat<TypeListImpl<PipelineWorker<I, U, V, WORKER>>,
                                                  typename ExpandParallelWorkers<I + 1, U, V, WORKERS...>::typelist_t>;
};

template <class... UNDECAYED_PARALLEL_WORKERS>
ParallelPipelineWrapper<extract_worker_impl_t<std::decay_t<UNDECAYED_PARALLEL_WORKERS>>...> ParallelPipeline(
    UNDECAYED_PARALLEL_WORKERS&&... undecayed_parallel_workers) {
  return ParallelPipelineWrapper<extract_worker_impl_t<std::decay_t<UNDECAYED_PARALLEL_WORKERS>>...>(
      std::forward_as_tuple(undecayed_parallel_workers...));
}

template <int I, class... PARALLEL, class... NEXT>
struct PipelineTypeListBuilder<I, ParallelPipelineWrapper<PARALLEL...>, NEXT...> {
  using typelist_t = metaprogramming::TypeListCat<
      typename ExpandParallelWorkers<I, I, I + sizeof...(PARALLEL) - 1, PARALLEL...>::typelist_t,
      typename PipelineTypeListBuilder<I + sizeof...(PARALLEL), NEXT...>::typelist_t>;
};

template <class SOURCE, class... WORKERS>
struct PipelineBuilder final {
  static_assert(sizeof...(WORKERS) >= 1u, "`Pipeline(...)` takes at least two parameters, one source and one worker.");

  static_assert(is_block_source_v<SOURCE>);

  using source_impl_t = extract_source_impl_t<SOURCE>;
  using typelist_t = typename PipelineTypeListBuilder<1, WORKERS...>::typelist_t;
  using type_t = PipelineImpl<source_impl_t, typelist_t>;

  static type_t Build(SOURCE source, WORKERS... workers) { return type_t(std::move(source), std::move(workers)...); }
};

template <class UNDECAYED_SOURCE, class... UNDECAYED_WORKERS>
typename PipelineBuilder<std::decay_t<UNDECAYED_SOURCE>, std::decay_t<UNDECAYED_WORKERS>...>::type_t Pipeline(
    UNDECAYED_SOURCE&& source, UNDECAYED_WORKERS&&... workers) {
  return PipelineBuilder<std::decay_t<UNDECAYED_SOURCE>, std::decay_t<UNDECAYED_WORKERS>...>::Build(
      std::forward<UNDECAYED_SOURCE>(source), std::forward<UNDECAYED_WORKERS>(workers)...);
}

template <int I>
struct CompileTimeFixedSizeFixedAccessorArrayElement : CompileTimeFixedSizeFixedAccessorArrayElement<I - 1> {
  volatile size_t element = 0u;
};

template <>
struct CompileTimeFixedSizeFixedAccessorArrayElement<-1> {};

template <int I>
using CompileTimeFixedSizeFixedAccessorArray = CompileTimeFixedSizeFixedAccessorArrayElement<I - 1>;

template <int U, int V, class A>
struct CompileTimeMinimumArrayElementFromRange final {
  static size_t Compute(const volatile A& a) {
    const size_t lhs = a.CompileTimeFixedSizeFixedAccessorArrayElement<U>::element;
    const size_t rhs = CompileTimeMinimumArrayElementFromRange<U + 1, V, A>::Compute(a);
    return std::min(lhs, rhs);
  }
};

template <int I, class A>
struct CompileTimeMinimumArrayElementFromRange<I, I, A> final {
  static size_t Compute(const volatile A& a) { return a.CompileTimeFixedSizeFixedAccessorArrayElement<I>::element; }
};

template <int, class...>
struct UVPerIImpl;

template <int Z, class X, class... XS>
struct UVPerIImpl<Z, X, XS...> : UVPerIImpl<Z, X>, UVPerIImpl<Z, XS...> {};

template <int Z, int I, int U, int V, class WORKER>
struct UVPerIImpl<Z, PipelineWorker<I, U, V, WORKER>> {};

template <int I, int _U, int _V, class WORKER>
struct UVPerIImpl<I, PipelineWorker<I, _U, _V, WORKER>> {
  constexpr static int U = _U;
  constexpr static int V = _V;
};

template <int I, class... WORKERS>
struct UVPerI : UVPerIImpl<I, WORKERS...> {};

// Special case for the source. It has the assumed index of 0, and it is not "run" in parallel with anything. :) -- D.K.
template <class... WORKERS>
struct UVPerI<0, WORKERS...> {
  constexpr static int U = 0;
  constexpr static int V = 0;
};

template <class T>
class PipelineState;

template <class SOURCE, class... WORKERS>
class PipelineState<PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>> final {
 public:
  constexpr static int N = sizeof...(WORKERS) + 1;

  bool joined = false;
  volatile bool terminate_requested = false;

 private:
  using total_ready_t = CompileTimeFixedSizeFixedAccessorArray<N>;
  total_ready_t total_ready;

  template <int I>
  size_t InternalReadyCountFor() const volatile {
    // NOTE(dkorolev): Per the comment above, the "real" implementation should be minimum over a set, not over a range,
    // of indexes of what should be "ready" before worker `I` (or source `0`) can run further along the circular buffer.
    using UV = UVPerI<I, WORKERS...>;
    return CompileTimeMinimumArrayElementFromRange<UV::U, UV::V, total_ready_t>::Compute(total_ready);
  }

 public:
  size_t GrandTotalProcessedCount() const { return InternalReadyCountFor<N - 1>(); }

  template <int I>
  size_t ReadyCountFor() const volatile {
    // NOTE(dkorolev): Per the comment above, the "real" implementation would be different from "`U - 1` for this `I`".
    return InternalReadyCountFor<(N + UVPerI<I, WORKERS...>::U - 1) % N>();  // `ReadyCountFor<0>()` == grand total.
  }

  // NOTE(dkorolev): Only one thread runs each particular worker/source, so this call can be thread-unsafe by design.
  template <int I>
  void UpdateOutput(size_t value) {
#ifndef NDEBUG
    if (!(value >= total_ready.CompileTimeFixedSizeFixedAccessorArrayElement<I>::element)) {
      std::cerr << "Internal error: The " << (I ? "worker" : "source")
                << " attempted to decrease its `done_count` from "
                << total_ready.CompileTimeFixedSizeFixedAccessorArrayElement<I>::element << " to " << value << ".\n";
      std::exit(-1);
    }
#endif
    total_ready.CompileTimeFixedSizeFixedAccessorArrayElement<I>::element = value;
  }
};

template <int I, int N, class PIPELINE_IMPL, typename BLOB>
struct PipelineWorkerThread;

template <int I, int N, class SOURCE, class... WORKERS, typename BLOB>
struct PipelineWorkerThread<I, N, PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>, BLOB>
    : PipelineWorkerThread<I + 1, N, PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>, BLOB> {
  static_assert(N == sizeof...(WORKERS) + 1);

  using workers_typelist_t = TypeListImpl<WORKERS...>;
  static_assert(N == TypeListSize<workers_typelist_t> + 1);

  using worker_t =
      extract_worker_impl_t<block_worker_from_pipeline_worker_t<TypeListElement<I - 1, workers_typelist_t>>>;
  using pipeline_t = PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>;
  using state_t = PipelineState<pipeline_t>;
  using instances_t = PipelineInstances<pipeline_t>;
  using waitable_state_t = WaitableAtomic<state_t>;

  std::vector<BLOB>& buffer;
  WaitableAtomic<state_t>& state;
  worker_t& worker_instance;
  bool joined = false;
  std::thread worker_thread;

  PipelineWorkerThread(std::vector<BLOB>& buffer, waitable_state_t& state, const instances_t& instances)
      : PipelineWorkerThread<I + 1, N, PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>, BLOB>(buffer, state, instances),
        buffer(buffer),
        state(state),
        worker_instance(
            *instances
                 .WorkerInstance<I, N, TypeListImpl<block_worker_from_pipeline_worker_t<WORKERS>...>>::worker_instance),
        worker_thread(std::thread([this]() { WorkerThread(); })) {}

  ~PipelineWorkerThread() {
    if (!joined) {
      worker_thread.join();
    }
  }

  void JoinAllWorkerThreads() {
    PipelineWorkerThread<I + 1, N, PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>, BLOB>::JoinAllWorkerThreads();
    if (!joined) {
      joined = true;
      worker_thread.join();
    }
  }

 private:
  void WorkerThread() {
    const volatile state_t& volatile_state_reference =
        state.ImmutableUse([](const state_t& value) -> const state_t& { return value; });

    const size_t total_buffer_size = buffer.size();
    const size_t total_buffer_size_minus_one = total_buffer_size - 1u;
    if ((total_buffer_size & total_buffer_size_minus_one) != 0u) {
      std::cerr << "Internal error, must be a power of two." << std::endl;
      std::exit(-1);
    }

    BLOB* mutable_buffer_ptr = &buffer[0];

    size_t updating_total_blobs_done = 0u;
    size_t trailing_total_blobs_read = 0u;
    bool terminate_requested = false;
    const auto Update = [&trailing_total_blobs_read, &terminate_requested](const volatile state_t& state) {
      trailing_total_blobs_read = std::max(trailing_total_blobs_read, state.template ReadyCountFor<I>());
      terminate_requested |= state.terminate_requested;
    };
    const auto IsReady = [&trailing_total_blobs_read, &updating_total_blobs_done, &terminate_requested]() {
      return updating_total_blobs_done < trailing_total_blobs_read || terminate_requested;
    };

    while (true) {
      Update(volatile_state_reference);
      if (!IsReady()) {
        state.Wait([&IsReady, &Update](const state_t& value) {
          Update(value);
          return IsReady();
        });
      }

      if (terminate_requested) {
        break;
      }

      const auto DoWorkOverCircularBuffer = [&](size_t begin, size_t end) {
        BLOB* ptr_begin = mutable_buffer_ptr + begin;
        BLOB* ptr_end = mutable_buffer_ptr + end;
        const size_t processed = (worker_instance.DoWork(ptr_begin, ptr_end) - ptr_begin);
        updating_total_blobs_done += processed;
        state.MutableUse(
            [updating_total_blobs_done](state_t& state) { state.template UpdateOutput<I>(updating_total_blobs_done); });
      };

      const size_t bgn = (updating_total_blobs_done & total_buffer_size_minus_one);
      const size_t end = (trailing_total_blobs_read & total_buffer_size_minus_one);
      if (bgn < end) {
        DoWorkOverCircularBuffer(bgn, end);
      } else {
        DoWorkOverCircularBuffer(bgn, total_buffer_size);
      }
    }
  }
};

template <int N, class SOURCE, class... WORKERS, typename BLOB>
struct PipelineWorkerThread<N, N, PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>, BLOB> {
  using pipeline_t = PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>;
  using instances_t = PipelineInstances<pipeline_t>;
  using state_t = PipelineState<pipeline_t>;
  using waitable_state_t = WaitableAtomic<state_t>;
  PipelineWorkerThread(std::vector<BLOB>&, waitable_state_t&, const instances_t&) {}
  void JoinAllWorkerThreads() {}
};

template <class PIPELINE_IMPL, typename BLOB>
struct PipelineSourceThread;

template <typename BLOB, class SOURCE, class... WORKERS>
struct PipelineSourceThread<PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>, BLOB> {
  using source_t = SOURCE;
  using pipeline_t = PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>;
  using state_t = PipelineState<pipeline_t>;
  using instances_t = PipelineInstances<pipeline_t>;
  using waitable_state_t = WaitableAtomic<state_t>;

  std::vector<BLOB>& buffer;
  WaitableAtomic<PipelineState<PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>>>& state;
  source_t& source_instance;
  bool joined = false;
  std::thread source_thread;

  PipelineSourceThread(std::vector<BLOB>& buffer, waitable_state_t& state, const instances_t& instances)
      : buffer(buffer),
        state(state),
        source_instance(*instances.SourceInstance<source_t>::source_instance),
        source_thread(std::thread([this]() { SourceThread(); })) {}

  ~PipelineSourceThread() {
    if (!joined) {
      source_thread.join();
    }
  }

  void JoinSourceThread() {
    if (!joined) {
      joined = true;
      source_thread.join();
    }
  }

 private:
  void SourceThread() {
    const volatile state_t& volatile_state_reference =
        state.ImmutableUse([](const state_t& value) -> const state_t& { return value; });

    const size_t total_buffer_size = buffer.size();
    if ((total_buffer_size & (total_buffer_size - 1u)) != 0u) {
      std::cerr << "Internal error, must be a power of two." << std::endl;
      std::exit(-1);
    }

    const size_t total_buffer_size_in_bytes = total_buffer_size * sizeof(BLOB);
    const size_t total_buffer_size_in_bytes_minus_one = total_buffer_size_in_bytes - 1u;
    if ((total_buffer_size_in_bytes & total_buffer_size_in_bytes_minus_one) != 0u) {
      std::cerr << "Internal error, must be a power of two." << std::endl;
      std::exit(-1);
    }

    uint8_t* buffer_in_bytes = reinterpret_cast<uint8_t*>(&buffer[0]);

    // The number of bytes "available" is effectively the total bytes read plus the size of part
    // of the buffer that is not presently used by the blobs already read but not yet processed.
    size_t trailing_total_blobs_done = 0u;
    size_t trailing_total_bytes_aval = 0u;

    size_t updating_total_bytes_read = 0u;
    size_t updating_total_blobs_read = 0u;
    bool terminate_requested = false;

    // Updates the value of `trailing_total_bytes_aval`.
    // The resulting value may well result in the queue being full, in which case
    // the outer loop will wait on the condition_variable until some room frees up.
    const auto Update = [&trailing_total_bytes_aval,
                         total_buffer_size_in_bytes,
                         &trailing_total_blobs_done,
                         &terminate_requested](const volatile state_t& state) {
      trailing_total_blobs_done = std::max(trailing_total_blobs_done, state.template ReadyCountFor<0>());
      trailing_total_bytes_aval = (trailing_total_blobs_done * sizeof(BLOB) + total_buffer_size_in_bytes);
      terminate_requested |= state.terminate_requested;
    };

    // Checks whether waiting on the condition variable is required, i.e. checks if the buffer is full.
    const auto IsReady = [&updating_total_bytes_read, &trailing_total_bytes_aval, &terminate_requested]() {
      return trailing_total_bytes_aval != updating_total_bytes_read || terminate_requested;
    };

    while (true) {
      Update(volatile_state_reference);
      if (!IsReady()) {
        state.Wait([&IsReady, &Update](const state_t& value) {
          Update(value);
          return IsReady();
        });
      }

      if (terminate_requested) {
        break;
      }

      const auto DoWorkOverCircularBufferInBytes = [&](size_t bgn, size_t end) {
        const size_t bytes_read = source_instance.DoGetInput(buffer_in_bytes + bgn, buffer_in_bytes + end);
        if (bytes_read) {
          updating_total_bytes_read += bytes_read;
          const size_t candidate_total_blobs_read_value = updating_total_bytes_read / sizeof(BLOB);
          if (candidate_total_blobs_read_value != updating_total_blobs_read) {
            updating_total_blobs_read = candidate_total_blobs_read_value;
            state.MutableUse([updating_total_blobs_read](state_t& state) {
              state.template UpdateOutput<0>(updating_total_blobs_read);
            });
          }
        }
      };

      const size_t bgn = (updating_total_bytes_read & total_buffer_size_in_bytes_minus_one);
      const size_t end = (trailing_total_bytes_aval & total_buffer_size_in_bytes_minus_one);
      if (bgn < end) {
        DoWorkOverCircularBufferInBytes(bgn, end);
      } else {
        DoWorkOverCircularBufferInBytes(bgn, total_buffer_size_in_bytes);
      }
    }
  }
};

template <class PIPELINE, typename BLOB>
struct PipelineThreads;

template <class SOURCE, class... WORKERS, typename BLOB>
struct PipelineThreads<PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>, BLOB> final
    : PipelineWorkerThread<1, sizeof...(WORKERS) + 1, PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>, BLOB>,
      PipelineSourceThread<PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>, BLOB> {
  using pipeline_t = PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>;
  using state_t = PipelineState<pipeline_t>;
  using instances_t = PipelineInstances<pipeline_t>;
  using waitable_state_t = WaitableAtomic<state_t>;

  PipelineThreads(std::vector<BLOB>& buffer, waitable_state_t& state, const instances_t& instances)
      : PipelineWorkerThread<1, sizeof...(WORKERS) + 1, pipeline_t, BLOB>(buffer, state, instances),
        PipelineSourceThread<pipeline_t, BLOB>(buffer, state, instances) {}

  void JoinAllThreads() {
    PipelineWorkerThread<1, sizeof...(WORKERS) + 1, pipeline_t, BLOB>::JoinAllWorkerThreads();
    PipelineSourceThread<pipeline_t, BLOB>::JoinSourceThread();
  }
};

template <class SOURCE, class... WORKERS, typename BLOB>
struct PipelineRunContext<PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>, BLOB> final {
  using self_t = PipelineImpl<SOURCE, TypeListImpl<WORKERS...>>;
  static_assert((sizeof(BLOB) & (sizeof(BLOB) - 1)) == 0, "`sizeof(BLOB)` should be a power of two.");

  using state_t = PipelineState<self_t>;

  constexpr static size_t N = static_cast<size_t>(state_t::N);
  static_assert(N == sizeof...(WORKERS) + 1);
  static_assert(N == TypeListSize<TypeListImpl<block_worker_from_pipeline_worker_t<WORKERS>...>> + 1);

  struct PipelineRunContextInstancesCreator {
    PipelineInstances<self_t> instances;
    explicit PipelineRunContextInstancesCreator(const self_t& self) { self.InstantiatePipeline(instances); }
  };

  struct PipelineRunContextImpl final : PipelineRunContextInstancesCreator {
    std::vector<BLOB> circular_buffer;
    WaitableAtomic<state_t> state;
    PipelineThreads<self_t, BLOB> threads;

    static size_t RoundUpToPowerOfTwo(size_t x) {
      size_t n = 1u;
      while (n < x) {
        n *= 2;
      }
      return n;
    }

    PipelineRunContextImpl(const self_t& self, const PipelineRunParams<BLOB>& params)
        : PipelineRunContextInstancesCreator(self),
          circular_buffer(RoundUpToPowerOfTwo(params.circular_buffer_size / sizeof(BLOB))),
          threads(circular_buffer, state, PipelineRunContextInstancesCreator::instances) {}

    bool DoJoinAllThreadsCalled() {
      bool already_joined;
      state.MutableUse([&already_joined](state_t& state) {
        already_joined = state.joined;
        state.joined = true;
      });
      return already_joined;
    }
    void DoJoinAllThreads() {
      if (!DoJoinAllThreadsCalled()) {
        threads.JoinAllThreads();
      }
    }
    ~PipelineRunContextImpl() {
      if (!DoJoinAllThreadsCalled()) {
        std::cerr << "Error: The pipeline context is out of scope with no `.Join()` or `.ForceStop()` called.\n";
        std::exit(-1);
      }
    }
  };

  std::shared_ptr<PipelineRunContextImpl> impl;  // NOTE(dkorolev): Perhaps it should be `Owned`/`Borrowed` instead?

  size_t GrandTotalProcessedCount() const {
    return impl->state.ImmutableUse([](const state_t& state) -> size_t { return state.GrandTotalProcessedCount(); });
  }

  SOURCE& Source() { return *impl->instances.SourceInstance<SOURCE>::source_instance; }
  const SOURCE& Source() const { return *impl->instances.SourceInstance<SOURCE>::source_instance; }

  template <int I>
  typename PipelineInstances<
      self_t>::template WorkerInstance<I, N, TypeListImpl<block_worker_from_pipeline_worker_t<WORKERS>...>>::worker_t&
  Worker() {
    return *impl->instances
                .WorkerInstance<I, N, TypeListImpl<block_worker_from_pipeline_worker_t<WORKERS>...>>::worker_instance;
  }

  template <int I>
  const typename PipelineInstances<
      self_t>::template WorkerInstance<I, N, TypeListImpl<block_worker_from_pipeline_worker_t<WORKERS>...>>::worker_t&
  Worker() const {
    return *impl->instances
                .WorkerInstance<I, N, TypeListImpl<block_worker_from_pipeline_worker_t<WORKERS>...>>::worker_instance;
  }

  void Join() { impl->DoJoinAllThreads(); }
  void ForceStop() {
    impl->state.MutableUse([](state_t& state) { state.terminate_requested = true; });
    impl->DoJoinAllThreads();
  }

  PipelineRunContext() = default;

  PipelineRunContext(const self_t& self, const PipelineRunParams<BLOB>& params)
      : impl(std::make_shared<PipelineRunContextImpl>(self, params)) {}
};

template <class SOURCE, class WORKERS_AS_TUPLE>
struct PipelineRunner<PipelineImpl<SOURCE, WORKERS_AS_TUPLE>> final {
  using self_t = PipelineImpl<SOURCE, WORKERS_AS_TUPLE>;
  template <typename BLOB>
  static PipelineRunContext<self_t, BLOB> DoRun(const self_t& self, const PipelineRunParams<BLOB>& params) {
    return PipelineRunContext<self_t, BLOB>(self, params);
  }
};

}  // namespace current::examples::streamed_sockets

#endif  // EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_DSL_DSL_H
