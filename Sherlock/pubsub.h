/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_SHERLOCK_PUBSUB_H
#define CURRENT_SHERLOCK_PUBSUB_H

#include "../port.h"

#include <utility>

#include "../TypeSystem/timestamp.h"

#include "../Blocks/SS/ss.h"
#include "../Blocks/HTTP/api.h"
#include "../Bricks/time/chrono.h"

namespace current {
namespace sherlock {

template <typename E, JSONFormat J = JSONFormat::Current>
class PubSubHTTPEndpointImpl {
 public:
  explicit PubSubHTTPEndpointImpl(Request r)
      : http_request_(std::move(r)), http_response_(http_request_.SendChunkedResponse()) {
    if (http_request_.url.query.has("recent")) {
      serving_ = false;  // Start in 'non-serving' mode when `recent` is set.
      from_timestamp_ = r.timestamp - std::chrono::microseconds(
                                          current::FromString<uint64_t>(http_request_.url.query["recent"]));
    } else if (http_request_.url.query.has("since")) {
      serving_ = false;  // Start in 'non-serving' mode when `since` is set.
      from_timestamp_ =
          std::chrono::microseconds(current::FromString<uint64_t>(http_request_.url.query["since"]));
    } else if (http_request_.url.query.has("tail")) {
      serving_ = false;  // Start in 'non-serving' mode when `tail` is set.
      current::FromString(http_request_.url.query["tail"], tail_);
    }
    if (http_request_.url.query.has("i")) {
      serving_ = false;  // Start in 'non-serving' mode when `i` is set.
      current::FromString(http_request_.url.query["i"], i_);
    }
    if (http_request_.url.query.has("n")) {
      current::FromString(http_request_.url.query["n"], n_);
    }
    if (http_request_.url.query.has("period")) {
      period_ = std::chrono::microseconds(current::FromString<uint64_t>(http_request_.url.query["period"]));
    }
    if (http_request_.url.query.has("stop_after_bytes")) {
      current::FromString(http_request_.url.query["stop_after_bytes"], stop_after_bytes_);
    }
    if (http_request_.url.query.has("nowait")) {
      no_wait_ = true;
    }
  }

  // The implementation of the subscriber in `PubSubHTTPEndpointImpl` is an example of using:
  // * `curent` as the second parameter,
  // * `last` as the third parameter, and
  // * `EntryResponse` as the return value.
  // It does so to respect the URL parameters of the range of entries to subscribe to.
  ss::EntryResponse operator()(const E& entry, idxts_t current, idxts_t last) {
    // TODO(dkorolev): Should we always extract the timestamp and throw an exception if there is a mismatch?
    if (!serving_) {
      if (current.index >= i_ &&                                               // Respect `i`.
          (tail_ == 0u || (last.index - current.index) < tail_) &&             // Respect `tail`.
          (from_timestamp_.count() == 0u || current.us >= from_timestamp_)) {  // Respect `since` and `recent`.
        serving_ = true;
      }
      // Reached the end, didn't started serving and should not wait.
      if (!serving_ && current.index == last.index && no_wait_) {
        return ss::EntryResponse::Done;
      }
    }
    if (serving_) {
      // If `period` is set, set the maximum possible timestamp.
      if (period_.count() && to_timestamp_.count() == 0u) {
        to_timestamp_ = current.us + period_;
      }
      // Stop serving if the limit on timestamp is exceeded.
      if (to_timestamp_.count() && current.us > to_timestamp_) {
        return ss::EntryResponse::Done;
      }
      const std::string entry_json(JSON<J>(current) + '\t' + JSON<J>(entry) + '\n');
      current_response_size_ += entry_json.length();
      try {
        http_response_(std::move(entry_json));
      } catch (const current::net::NetworkException&) {  // LCOV_EXCL_LINE
        return ss::EntryResponse::Done;                  // LCOV_EXCL_LINE
      }
      // Respect `stop_after_bytes`.
      if (stop_after_bytes_ && current_response_size_ >= stop_after_bytes_) {
        return ss::EntryResponse::Done;
      }
      // Respect `n`.
      if (n_) {
        --n_;
        if (!n_) {
          return ss::EntryResponse::Done;
        }
      }
      // Respect `no_wait`.
      if (current.index == last.index && no_wait_) {
        return ss::EntryResponse::Done;
      }
    }
    return ss::EntryResponse::More;
  }

  // LCOV_EXCL_START
  ss::TerminationResponse Terminate() {
    http_response_("{\"error\":\"The subscriber has terminated.\"}\n");
    return ss::TerminationResponse::Terminate;
  }
  // LCOV_EXCL_STOP

 private:
  // `http_request_`:  need to keep the passed in request in scope for the lifetime of the chunked response.
  Request http_request_;
  // `http_response_`: the instance of the chunked response object to use.
  current::net::HTTPServerConnection::ChunkedResponseSender http_response_;
  // Current response size in bytes.
  size_t current_response_size_ = 0u;

  // Conditions on which parts of the stream to serve.
  bool serving_ = true;
  // If set, the timestamp from which the output should start.
  std::chrono::microseconds from_timestamp_ = std::chrono::microseconds(0);
  // If set, the number of "last" entries to output.
  size_t tail_ = 0u;
  // If set, the index of the first record to return.
  uint64_t i_ = 0u;
  // If set, the total number of records to return.
  size_t n_ = 0u;
  // If set, the maximum difference between the first and the last record's timestamp.
  std::chrono::microseconds period_ = std::chrono::microseconds(0);
  // Calculated if `period_` is set.
  std::chrono::microseconds to_timestamp_ = std::chrono::microseconds(0);
  // If set, stop serving after the response size reached/exceeded the value.
  size_t stop_after_bytes_ = 0u;
  // If set, stop serving when current entry is the last entry.
  bool no_wait_ = false;

  PubSubHTTPEndpointImpl() = delete;
  PubSubHTTPEndpointImpl(const PubSubHTTPEndpointImpl&) = delete;
  void operator=(const PubSubHTTPEndpointImpl&) = delete;
  PubSubHTTPEndpointImpl(PubSubHTTPEndpointImpl&&) = delete;
  void operator=(PubSubHTTPEndpointImpl&&) = delete;
};

template <typename E, JSONFormat J = JSONFormat::Current>
using PubSubHTTPEndpoint = current::ss::StreamSubscriber<PubSubHTTPEndpointImpl<E, J>, E>;

}  // namespace sherlock
}  // namespace current

#endif  // CURRENT_SHERLOCK_PUBSUB_H
