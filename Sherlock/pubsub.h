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
//
//    `terminate`  : Terminate HTTP connection for the subscription id passed as the value of this parameter.

// TODO(dkorolev): Add timestamps to `sizeonly` and `HEAD` too?
// TODO(dkorolev): Mention head updates now as we're here?

namespace current {
namespace sherlock {

struct ParsedHTTPRequestParams {
  // If set, return current stream size.
  // Controlled by `sizeonly` URL parameter or using `HEAD` method.
  bool size_only = false;
  // If set, termination of the HTTP connection is requested.
  // Controlled by `terminate` URL parameter.
  bool terminate_requested = false;
  // Id of the subscription to terminate.
  std::string terminate_id;
  // If set, return the schema of stream.
  // Controlled by `schema` URL parameter or by the first URL path argument.
  bool schema_requested = false;
  // Schema format requested. If empty, top-level object with all supported languages is returned.
  std::string schema_format;
  // If set, return entries with timestamp >= (request_timestamp - recent).
  // Controlled by `recent` URL parameter.
  std::chrono::microseconds recent = std::chrono::microseconds(0);
  // If set, return entries with timestamp >= since.
  // Controlled by `since` URL parameter.
  std::chrono::microseconds since = std::chrono::microseconds(0);
  // If set, the index of the first record to return. Controlled by `i` URL parameter.
  uint64_t i = 0u;
  // If set, the number of "last" entries to output. Controlled by `tail` URL parameter.
  uint64_t tail = 0u;
  // If set, the total number of records to return. Controlled by `n` URL parameter.
  uint64_t n = 0u;
  // If set, the maximum difference between the first and the last record's timestamp.
  // Controlled by `period` URL parameter.
  std::chrono::microseconds period = std::chrono::microseconds(0);
  // If set, stop serving when current entry is the last entry. Controlled by `nowait` URL parameter.
  bool no_wait = false;
  // If set, stop serving after the response size reached/exceeded the value.
  // Controlled by `stop_after_bytes` URL parameter.
  uint64_t stop_after_bytes = 0u;
  // If set, do not prepend each entry with its index and timestamp followed by a '\t', return data JSONs only.
  bool entries_only = false;
  // If set, wrap the entries into a large JSON array. Mostly to please JSON-beautifying browser extensions.
  bool array = false;
};

inline ParsedHTTPRequestParams ParsePubSubHTTPRequest(const Request& r) {
  ParsedHTTPRequestParams result;

  if (r.url.query.has("terminate")) {
    result.terminate_requested = true;
    result.terminate_id = r.url.query["terminate"];
  }
  if (r.url.query.has("sizeonly") || r.method == "HEAD") {
    result.size_only = true;
  }

  if (r.url.query.has("schema")) {
    result.schema_requested = true;
    result.schema_format = r.url.query["schema"];
  } else if (r.url_path_args.size() == 1) {
    const std::string schema_prefix = "schema.";
    result.schema_requested = true;
    if (r.url_path_args[0].substr(0, schema_prefix.length()) == schema_prefix) {
      result.schema_format = r.url_path_args[0].substr(schema_prefix.length());
    } else {
      result.schema_format = r.url_path_args[0];
    }
  }

  if (r.url.query.has("recent")) {
    result.recent = std::chrono::microseconds(current::FromString<uint64_t>(r.url.query["recent"]));
  }
  if (r.url.query.has("since")) {
    result.since = std::chrono::microseconds(current::FromString<uint64_t>(r.url.query["since"]));
  }
  if (r.url.query.has("tail")) {
    result.tail = current::FromString<uint64_t>(r.url.query["tail"]);
  }
  if (r.url.query.has("i")) {
    result.i = current::FromString<uint64_t>(r.url.query["i"]);
  }
  if (r.url.query.has("n")) {
    result.n = current::FromString<uint64_t>(r.url.query["n"]);
  }
  if (r.url.query.has("period")) {
    result.period = std::chrono::microseconds(current::FromString<uint64_t>(r.url.query["period"]));
  }
  if (r.url.query.has("stop_after_bytes")) {
    result.stop_after_bytes = current::FromString<uint64_t>(r.url.query["stop_after_bytes"]);
  }
  if (r.url.query.has("nowait")) {
    result.no_wait = true;
  }
  if (r.url.query.has("entries_only")) {
    result.entries_only = true;
  }
  if (r.url.query.has("array")) {
    result.array = true;
    result.entries_only = true;  // Obviously, `array` implies `entries_only`.
  }

  return result;
}

template <typename E, template <typename> class PERSISTENCE_LAYER, JSONFormat J>
class PubSubHTTPEndpointImpl : public AbstractSubscriberObject {
 public:
  using stream_data_t = StreamData<E, PERSISTENCE_LAYER>;

  PubSubHTTPEndpointImpl(const std::string& subscription_id,
                         ScopeOwned<stream_data_t>& data,
                         Request r,
                         ParsedHTTPRequestParams params)
      : data_(data, [this]() { time_to_terminate_ = true; }),
        http_request_(std::move(r)),
        params_(std::move(params)),
        output_started_(false),
        http_response_(http_request_.SendChunkedResponse(
            HTTPResponseCode.OK,
            current::net::constants::kDefaultJSONContentType,
            current::net::http::Headers({
                {kSherlockHeaderCurrentSubscriptionId, subscription_id},
                {kSherlockHeaderCurrentStreamSize, current::ToString(data_->persistence.Size())},
            }))) {
    if (params_.recent.count() > 0) {
      serving_ = false;  // Start in 'non-serving' mode when `recent` is set.
      from_timestamp_ = r.timestamp - params_.recent;
    } else if (params_.since.count() > 0) {
      serving_ = false;  // Start in 'non-serving' mode when `since` is set.
      from_timestamp_ = params_.since;
    }
    if (params_.n > 0u) {
      n_ = params_.n;
    }
  }

  // The implementation of the subscriber in `PubSubHTTPEndpointImpl` is an example of using:
  // * `current` as the second parameter,
  // * `last` as the third parameter, and
  // * `EntryResponse` as the return value.
  // It does so to respect the URL parameters of the range of entries to subscribe to.
  ss::EntryResponse operator()(const E& entry, idxts_t current, idxts_t last) {
    const ss::EntryResponse result = [&, this]() {
    if (time_to_terminate_) {
      return ss::EntryResponse::Done;
    }
    // TODO(dkorolev): Should we always extract the timestamp and throw an exception if there is a mismatch?
    if (!serving_) {
      if (current.index >= params_.i &&                                           // Respect `i`.
          (params_.tail == 0u || (last.index - current.index) < params_.tail) &&  // Respect `tail`.
          (from_timestamp_.count() == 0u || current.us >= from_timestamp_)) {  // Respect `since` and `recent`.
        serving_ = true;
      }
      // Reached the end, didn't started serving and should not wait.
      if (!serving_ && current.index == last.index && params_.no_wait) {
        return ss::EntryResponse::Done;
      }
    }
    if (serving_) {
      // If `period` is set, set the maximum possible timestamp.
      if (params_.period.count() && to_timestamp_.count() == 0u) {
        to_timestamp_ = current.us + params_.period;
      }
      // Stop serving if the limit on timestamp is exceeded.
      if (to_timestamp_.count() && current.us > to_timestamp_) {
        return ss::EntryResponse::Done;
      }
      const std::string entry_json = [this, &current, &entry]() {
        if (params_.entries_only) {
          return JSON<J>(entry) + '\n';
        } else {
          return JSON<J>(current) + '\t' + JSON<J>(entry) + '\n';
        }
      }();
      current_response_size_ += entry_json.length();
      try {
        if (params_.array) {
          if (!output_started_) {
            http_response_("[\n");
            output_started_ = true;
          } else {
            http_response_(",\n");
          }
        }
        http_response_(std::move(entry_json));
      } catch (const current::net::NetworkException&) {  // LCOV_EXCL_LINE
        return ss::EntryResponse::Done;                  // LCOV_EXCL_LINE
      }
      // Respect `stop_after_bytes`.
      if (params_.stop_after_bytes && current_response_size_ >= params_.stop_after_bytes) {
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
      if (current.index == last.index && params_.no_wait) {
        return ss::EntryResponse::Done;
      }
    }
    return ss::EntryResponse::More;
    }();
    if (result == ss::EntryResponse::Done && params_.array) {
      if (!output_started_) {
        http_response_("[]\n");
      } else {
        http_response_("]\n");
      }
    }
    return result;
  }

  // TODO(dkorolev): This is a long shot, but looks right: For type-filtered HTTP subscriptions,
  // whether we should terminate or no depends on `nowait`.
  ss::EntryResponse EntryResponseIfNoMorePassTypeFilter() const {
    return (time_to_terminate_ || params_.no_wait) ? ss::EntryResponse::Done : ss::EntryResponse::More;
  }

  // LCOV_EXCL_START
  ss::TerminationResponse Terminate() {
    static const std::string message = "{\"error\":\"The subscriber has terminated.\"}\n";
    if (params_.array && output_started_) {
      http_response_(",\n" + message + "]\n");
    } else {
      http_response_(message);
    }
    return ss::TerminationResponse::Terminate;
  }
  // LCOV_EXCL_STOP

 private:
  // The HTTP listener must register itself as a user of stream data to ensure the lifetime of stream data.
  ScopeOwnedBySomeoneElse<stream_data_t> data_;
  std::atomic_bool time_to_terminate_{false};

  // `http_request_`:  need to keep the passed in request in scope for the lifetime of the chunked response.
  Request http_request_;
  const ParsedHTTPRequestParams params_;
  // `output_started_`: will change to `true` is `params_.array` is `true` as the first piece of data
  // has already been sent, thus triggering the need to close the array at the end.
  bool output_started_ = false;
  // `http_response_`: the instance of the chunked response object to use.
  current::net::HTTPServerConnection::ChunkedResponseSender http_response_;
  // Current response size in bytes.
  size_t current_response_size_ = 0u;

  // Conditions on which parts of the stream to serve.
  bool serving_ = true;
  // If set, the timestamp from which the output should start.
  std::chrono::microseconds from_timestamp_ = std::chrono::microseconds(0);
  // Calculated if `period_` is set.
  std::chrono::microseconds to_timestamp_ = std::chrono::microseconds(0);
  // Remaining number of records to return. Initialized if `n` URL parameter is set.
  uint64_t n_ = 0u;

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
