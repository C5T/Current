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

// TODO(dkorolev): RipCurrent style:
// 1) Clean syntax (macros and metaprogramming) to define sources and workers.
// 2) Clean syntax (macros and metaprogramming) to define pipelines.
// 3) Lazy instantiation with of sources and workers, with pre-provided constructor parameters.
// 4) The HTTP/HTML/gnuplot/graphviz status page.

#ifndef EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_NEXT_RIPCURRENT_H
#define EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_NEXT_RIPCURRENT_H

#include <iostream>
#include <thread>
#include <vector>

#include "../../../bricks/sync/waitable_atomic.h"

namespace current::examples::streamed_sockets {

template <class T_STATE, class T_SOURCE_OR_WORKER>
struct InputOf;

template <class T_STATE, class T_SOURCE_OR_WORKER>
struct OutputOf;

template <class T_STATE>
struct SinkOf;

template <class T_SOURCE, class T_BLOB, class T_WAITABLE_ATOMIC_STATE, typename... ARGS>
std::thread SpawnThreadSource(std::vector<T_BLOB>& buffer, T_WAITABLE_ATOMIC_STATE& mutable_state, ARGS&&... args) {
  using state_t = typename T_WAITABLE_ATOMIC_STATE::data_t;
  return std::thread([&buffer, &mutable_state, worker = std::make_unique<T_SOURCE>(std::forward<ARGS>(args)...) ]() {
    const state_t& volatile_immutable_state = mutable_state.ImmutableUse([](const state_t& value) { return value; });

    const size_t total_buffer_size = buffer.size();
    if ((total_buffer_size & (total_buffer_size - 1u)) != 0u) {
      std::cerr << "Internal error, must be a power of two." << std::endl;
      std::exit(-1);
    }

    const size_t total_buffer_size_in_bytes = total_buffer_size * sizeof(T_BLOB);
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

    // Updates the value of `trailing_total_bytes_aval`.
    // The resulting value may well result in the queue being full, in which case
    // the outer loop will wait on the condition_variable until some room frees up.
    const auto Update =
        [&trailing_total_bytes_aval, total_buffer_size_in_bytes, &trailing_total_blobs_done](const state_t& state) {
          trailing_total_blobs_done = std::max(trailing_total_blobs_done, SinkOf<state_t>::Get(state));
          trailing_total_bytes_aval = (trailing_total_blobs_done * sizeof(T_BLOB) + total_buffer_size_in_bytes);
        };

    // Checks whether waiting on the condition variable is required, i.e. checks if the buffer is full.
    const auto IsReady = [&updating_total_bytes_read, &trailing_total_bytes_aval]() {
      return trailing_total_bytes_aval != updating_total_bytes_read;
    };

    while (true) {
      Update(volatile_immutable_state);
      if (!IsReady()) {
        mutable_state.Wait([&IsReady, &Update](const state_t& value) {
          Update(value);
          return IsReady();
        });
      }

      const auto DoWorkOverCircularBufferInBytes = [&](size_t bgn, size_t end) {
        const size_t bytes_read = worker->DoGetInput(buffer_in_bytes + bgn, buffer_in_bytes + end);
        if (bytes_read) {
          updating_total_bytes_read += bytes_read;
          const size_t candidate_total_blobs_read_value = updating_total_bytes_read / sizeof(T_BLOB);
          if (candidate_total_blobs_read_value != updating_total_blobs_read) {
            updating_total_blobs_read = candidate_total_blobs_read_value;
            mutable_state.MutableUse([updating_total_blobs_read](state_t& state) {
              OutputOf<state_t, T_SOURCE>::Set(state, updating_total_blobs_read);
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
  });
}

template <class T_WORKER, class T_BLOB, class T_WAITABLE_ATOMIC_STATE, typename... ARGS>
std::thread SpawnThreadWorker(std::vector<T_BLOB>& buffer, T_WAITABLE_ATOMIC_STATE& mutable_state, ARGS&&... args) {
  using state_t = typename T_WAITABLE_ATOMIC_STATE::data_t;
  return std::thread([&buffer, &mutable_state, worker = std::make_unique<T_WORKER>(std::forward<ARGS>(args)...) ]() {
    const state_t& volatile_immutable_state = mutable_state.ImmutableUse([](const state_t& value) { return value; });

    const size_t total_buffer_size = buffer.size();
    const size_t total_buffer_size_minus_one = total_buffer_size - 1u;
    if ((total_buffer_size & total_buffer_size_minus_one) != 0u) {
      std::cerr << "Internal error, must be a power of two." << std::endl;
      std::exit(-1);
    }

    T_BLOB* const immutable_buffer_ptr = &buffer[0];
    // NOTE(dkorolev): And/or: `T_BLOB* mutable_buffer_ptr = &buffer[0];`

    size_t updating_total_blobs_done = 0u;
    size_t trailing_total_blobs_read = 0u;
    const auto Update = [&trailing_total_blobs_read](const state_t& state) {
      trailing_total_blobs_read = std::max(trailing_total_blobs_read, InputOf<state_t, T_WORKER>::Get(state));
    };
    const auto IsReady = [&trailing_total_blobs_read, &updating_total_blobs_done]() {
      return updating_total_blobs_done < trailing_total_blobs_read;
    };

    while (true) {
      Update(volatile_immutable_state);
      if (!IsReady()) {
        mutable_state.Wait([&IsReady, &Update](const state_t& value) {
          Update(value);
          return IsReady();
        });
      }

      const auto DoWorkOverCircularBuffer = [&](size_t begin, size_t end) {
        T_BLOB* const ptr_begin = immutable_buffer_ptr + begin;
        T_BLOB* const ptr_end = immutable_buffer_ptr + end;
        const size_t processed = (worker->DoWork(ptr_begin, ptr_end) - ptr_begin);
        updating_total_blobs_done += processed;
        mutable_state.MutableUse([updating_total_blobs_done](state_t& state) {
          OutputOf<state_t, T_WORKER>::Set(state, updating_total_blobs_done);
        });
      };

      const size_t bgn = (updating_total_blobs_done & total_buffer_size_minus_one);
      const size_t end = (trailing_total_blobs_read & total_buffer_size_minus_one);
      if (bgn < end) {
        DoWorkOverCircularBuffer(bgn, end);
      } else {
        DoWorkOverCircularBuffer(bgn, total_buffer_size);
      }
    }
  });
}

}  // namespace current::examples::streamed_sockets

#endif  // EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_NEXT_RIPCURRENT_H
