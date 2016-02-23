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
 private:
  using EntryResponse = current::ss::EntryResponse;
  using TerminationResponse = current::ss::TerminationResponse;

 public:
  explicit PubSubHTTPEndpointImpl(Request r)
      : http_request_(std::move(r)), http_response_(http_request_.SendChunkedResponse()) {
    if (http_request_.url.query.has("recent")) {
      serving_ = false;  // Start in 'non-serving' mode when `recent` is set.
      from_timestamp_ =
          r.timestamp -
          std::chrono::microseconds(current::strings::FromString<uint64_t>(http_request_.url.query["recent"]));
    } else if (http_request_.url.query.has("since")) {
      serving_ = false;  // Start in 'non-serving' mode when `recent` is set.
      from_timestamp_ =
          std::chrono::microseconds(current::strings::FromString<uint64_t>(http_request_.url.query["since"]));
    }
    if (http_request_.url.query.has("n")) {
      serving_ = false;  // Start in 'non-serving' mode when `n` is set.
      current::strings::FromString(http_request_.url.query["n"], n_);
      cap_ = n_;  // If `?n=` parameter is set, it sets `cap_` too by default. Use `?n=...&cap=0` to override.
    }
    if (http_request_.url.query.has("n_min")) {
      // `n_min` is same as `n`, but it does not set the cap; just the lower bound for `recent`.
      serving_ = false;  // Start in 'non-serving' mode when `n_min` is set.
      current::strings::FromString(http_request_.url.query["n_min"], n_);
    }
    if (http_request_.url.query.has("cap")) {
      current::strings::FromString(http_request_.url.query["cap"], cap_);
    }
    if (http_request_.url.query.has("stop_after_bytes")) {
      current::strings::FromString(http_request_.url.query["stop_after_bytes"], stop_after_bytes_);
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
  EntryResponse operator()(const E& entry, idxts_t current, idxts_t last) {
    // TODO(dkorolev): Should we always extract the timestamp and throw an exception if there is a mismatch?
    try {
      if (!serving_) {
        // Respect `n`.
        if (last.index - current.index < n_) {
          serving_ = true;
        }
        // Respect `recent`.
        if (from_timestamp_.count() && current.us >= from_timestamp_) {
          serving_ = true;
        }
      }
      if (serving_) {
        const std::string entry_json(JSON<J>(current) + '\t' + JSON<J>(entry) + '\n');
        current_response_size_ += entry_json.length();
        http_response_(std::move(entry_json));
        // Respect `stop_after_bytes`.
        if (stop_after_bytes_ && current_response_size_ >= stop_after_bytes_) {
          return EntryResponse::Done;
        }
        // Respect `cap`.
        if (cap_) {
          --cap_;
          if (!cap_) {
            return EntryResponse::Done;
          }
        }
        // Respect `no_wait`.
        if (current.index == last.index && no_wait_) {
          return EntryResponse::Done;
        }
      }
      return EntryResponse::More;
    } catch (const current::net::NetworkException&) {
      return EntryResponse::Done;
    }
  }

  TerminationResponse Terminate() {
    http_response_("{\"error\":\"The subscriber has terminated.\"}\n");
    return TerminationResponse::Terminate;  // Confirm termination.
  }

 private:
  // `http_request_`:  need to keep the passed in request in scope for the lifetime of the chunked response.
  Request http_request_;
  // `http_response_`: the instance of the chunked response object to use.
  current::net::HTTPServerConnection::ChunkedResponseSender http_response_;
  // Current response size in bytes.
  size_t current_response_size_ = 0u;

  // Conditions on which parts of the stream to serve.
  bool serving_ = true;
  // If set, the number of "last" entries to output.
  size_t n_ = 0;
  // If set, the hard limit on the maximum number of entries to output.
  size_t cap_ = 0;
  // If set, stop serving after the response size reached/exceeded the value.
  size_t stop_after_bytes_ = 0;
  // If set, the timestamp from which the output should start.
  std::chrono::microseconds from_timestamp_ = std::chrono::microseconds(0);
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
