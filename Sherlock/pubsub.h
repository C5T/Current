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

#ifndef SHERLOCK_PUBSUB_H
#define SHERLOCK_PUBSUB_H

#include "../port.h"

#include <utility>

#include "extract_timestamp.h"

#include "../Blocks/HTTP/api.h"
#include "../Bricks/time/chrono.h"

namespace sherlock {

template <typename E>
class PubSubHTTPEndpoint final {
 public:
  PubSubHTTPEndpoint(const std::string& value_name, Request r)
      : value_name_(value_name),
        http_request_(std::move(r)),
        http_response_(http_request_.SendChunkedResponse()) {
    if (http_request_.url.query.has("recent")) {
      serving_ = false;  // Start in 'non-serving' mode when `recent` is set.
      from_timestamp_ =
          r.timestamp - static_cast<bricks::time::MILLISECONDS_INTERVAL>(
                            bricks::strings::FromString<uint64_t>(http_request_.url.query["recent"]));
    }
    if (http_request_.url.query.has("n")) {
      serving_ = false;  // Start in 'non-serving' mode when `n` is set.
      bricks::strings::FromString(http_request_.url.query["n"], n_);
      cap_ = n_;  // If `?n=` parameter is set, it sets `cap_` too by default. Use `?n=...&cap=0` to override.
    }
    if (http_request_.url.query.has("n_min")) {
      // `n_min` is same as `n`, but it does not set the cap; just the lower bound for `recent`.
      serving_ = false;  // Start in 'non-serving' mode when `n_min` is set.
      bricks::strings::FromString(http_request_.url.query["n_min"], n_);
    }
    if (http_request_.url.query.has("cap")) {
      bricks::strings::FromString(http_request_.url.query["cap"], cap_);
    }
  }

  // The implementation of the listener in `PubSubHTTPEndpoint` is an example of using:
  // * `index` as the second parameter,
  // * `total` as the third parameter, and
  // * `bool` as the return value.
  // It does so to respect the URL parameters of the range of entries to listen to.
  bool operator()(const E& entry, size_t index, size_t total) {
    // TODO(dkorolev): Should we always extract the timestamp and throw an exception if there is a mismatch?
    try {
      if (!serving_) {
        const bricks::time::EPOCH_MILLISECONDS timestamp = ExtractTimestamp(entry);
        // Respect `n`.
        if (total - index <= n_) {
          serving_ = true;
        }
        // Respect `recent`.
        if (from_timestamp_ != static_cast<bricks::time::EPOCH_MILLISECONDS>(-1) &&
            timestamp >= from_timestamp_) {
          serving_ = true;
        }
      }
      if (serving_) {
        http_response_(entry, value_name_);
        if (cap_) {
          --cap_;
          if (!cap_) {
            return false;
          }
        }
      }
      return true;
    } catch (const bricks::net::NetworkException&) {
      return false;
    }
  }

  bool Terminate() {
    http_response_("{\"error\":\"The subscriber has terminated.\"}\n");
    return true;  // Confirm termination.
  }

 private:
  // Top-level JSON object name for Cereal.
  const std::string& value_name_;

  // `http_request_`:  need to keep the passed in request in scope for the lifetime of the chunked response.
  Request http_request_;

  // `http_response_`: the instance of the chunked response object to use.
  bricks::net::HTTPServerConnection::ChunkedResponseSender http_response_;

  // Conditions on which parts of the stream to serve.
  bool serving_ = true;
  // If set, the number of "last" entries to output.
  size_t n_ = 0;
  // If set, the hard limit on the maximum number of entries to output.
  size_t cap_ = 0;
  // If set, the timestamp from which the output should start.
  bricks::time::EPOCH_MILLISECONDS from_timestamp_ = static_cast<bricks::time::EPOCH_MILLISECONDS>(-1);

  PubSubHTTPEndpoint() = delete;
  PubSubHTTPEndpoint(const PubSubHTTPEndpoint&) = delete;
  void operator=(const PubSubHTTPEndpoint&) = delete;
  PubSubHTTPEndpoint(PubSubHTTPEndpoint&&) = delete;
  void operator=(PubSubHTTPEndpoint&&) = delete;
};

}  // namespace sherlock

#endif  // SHERLOCK_PUBSUB_H
