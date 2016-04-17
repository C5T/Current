/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "stream_data.h"

#include "../TypeSystem/timestamp.h"

#include "../Blocks/SS/ss.h"
#include "../Blocks/HTTP/api.h"

#include "../Bricks/sync/scope_owned.h"
#include "../Bricks/time/chrono.h"

// HTTP publish-subscribe configuration.
//
// Accepted HTTP methods: GET, HEAD.
//
// 1. Range selection logic, defined by corresponding URL query parameters.
//
// 1.1. The beginning of the range, microsecond-based.
//
//      By default, the records are returned from the very beginning of the stream.
//      Extra constraints can be specified by the user.
//      For the record to be returned, all constraints should pass (i.e, the filters are logically AND-ed).
//
//      `since` :  The absolute epoch microseconds timestamp of the first record to be returned.
//                 Records with the `timestamp >= since` pass this filter.
//
//      `recent` : Return the records with timestamps at most `recent` microseconds of age,
//                 with respect to the time the request has been made.
//
//                 Conceptually equivalent to `&since=$(date -d '$((recent / 1000000)) sec ago' +"%s000000")`,
//                 except the time difference is more precise and computed on the server side.
//
// 1.2. The beginning of the range, index-based.
//
//      `i`    : The index of the first record to return. Indexes of records in Sherlock are 0-based.
//
//      `tail` : Return the records beginning from the index, which is exactly `tail` records until the end.
//
//               Data-wise, `curl &tail=$COUNT` is equivalent to `tail -n $COUNT -f`.
//               Use `&nowait` to remove the `-f` part of the logic.
//
//               The `tail` condition is tricky, as new data records may be added between the request was sent
//               and this request being fulfilled. Sherlock provides the guarantee that if the stream contains
//               at least `tail` records at the time the request is being fulfilled, the first `tail` records
//               returned would be the ones already available, so no waiting would happen.
//
//               Conceptually, the above means that `curl &tail=$(curl &sizeonly)&nowait` would always return
//               exactly `tail` records.
//
//      Combining `tail` and `i` is possible, in which case the tightest of the constraint would be used.
//      For instance, if the stream contains 100 records, both `?i=50&tail=10 and `?i=90&tail=50` would return
//      the entries starting from the 90-th 0-based one.
//
// 2. The end of the range.
//
//    `n`      : The total number of records to return.
//               Can be combined with `i`, with an obvious pattern being `&i=$INDEX&n=1`.
//
//    `period` : The length, in microseconds, of the band between the first and the last record
//               timestamp-wise.
//               The subscription channel will close itself as soon as the next data record,
//               or the HEAD of the stream, is `period` or more microseconds from the minimum between
//               the first record returned and the earliest timestamp requested via `since` or `recent`.
//
// 3. Termination conditions.
//
//    By default, unless the end of the range has been specified, Sherlock's publish-subscribe
//    behaves as a `tail -f` call: it will return the records indefinitely, waiting for the new ones to
//    arrive.
//
//    There are several ways to request an early termination of the stream of data:
//
//    `nowait`            : If set, never wait for new entries, return immediately upon reaching the end.
//                          Effectively, `nowait` to Sherlock is what the absence of `-f` does to `tail`.
//
//    `stop_after_bytes`  : If set, stop streaming as soon as total JTML response size exceeds certain size.
//                          This flag is used for backup purposes.
// 4. Special parameters.
//
//    `sizeonly`   : Instead of the actual data, return the total number of records in the stream.
//
//    HEAD request : Same as `sizeonly`, but return the total number of records in HTTP header, not body.

// TODO(dkorolev): Add timestamps to `sizeonly` and `HEAD` too?
// TODO(dkorolev): Mention head updates now as we're here?

namespace current {
namespace sherlock {

template <typename E, template <typename> class PERSISTENCE_LAYER, JSONFormat J>
class PubSubHTTPEndpointImpl : public AbstractSubscriberObject {
 public:
  using stream_data_t = StreamData<E, PERSISTENCE_LAYER>;

  PubSubHTTPEndpointImpl(const std::string& subscription_id, ScopeOwned<stream_data_t>& data, Request r)
      : data_(data, [this]() { time_to_terminate_ = true; }),
        http_request_(std::move(r)),
        http_response_(http_request_.SendChunkedResponse(
            HTTPResponseCode.OK,
            current::net::constants::kDefaultJSONContentType,
            current::net::http::Headers({
                {kSherlockHeaderCurrentSubscriptionId, subscription_id},
                {kSherlockHeaderCurrentStreamSize, current::ToString(data_->persistence.Size())},
            }))) {
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
    if (time_to_terminate_) {
      return ss::EntryResponse::Done;
    }
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

  // TODO(dkorolev): This is a long shot, but looks right: For type-filtered HTTP subscriptions,
  // whether we should terminate or no depends on `nowait`.
  ss::EntryResponse EntryResponseIfNoMorePassTypeFilter() const {
    return (time_to_terminate_ || no_wait_) ? ss::EntryResponse::Done : ss::EntryResponse::More;
  }

  // LCOV_EXCL_START
  ss::TerminationResponse Terminate() {
    http_response_("{\"error\":\"The subscriber has terminated.\"}\n");
    return ss::TerminationResponse::Terminate;
  }
  // LCOV_EXCL_STOP

 private:
  // The HTTP listener must register itself as a user of stream data to ensure the lifetime of stream data.
  ScopeOwnedBySomeoneElse<stream_data_t> data_;
  std::atomic_bool time_to_terminate_{false};

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

template <typename E, template <typename> class PERSISTENCE_LAYER, JSONFormat J = JSONFormat::Current>
using PubSubHTTPEndpoint = current::ss::StreamSubscriber<PubSubHTTPEndpointImpl<E, PERSISTENCE_LAYER, J>, E>;

}  // namespace sherlock
}  // namespace current

#endif  // CURRENT_SHERLOCK_PUBSUB_H
