/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef KARL_TEST_SERVICE_HTTP_SUBSCRIBER_H
#define KARL_TEST_SERVICE_HTTP_SUBSCRIBER_H

#include "../../blocks/http/api.h"
#include "../../blocks/ss/idx_ts.h"

template <typename ENTRY>
class HTTPStreamSubscriber {
 public:
  using callback_t = std::function<void(idxts_t, ENTRY&&)>;

  HTTPStreamSubscriber(const std::string& remote_stream_url, callback_t callback)
      : remote_stream_url_(remote_stream_url),
        callback_(callback),
        destructing_(false),
        index_(0u),
        has_terminate_id_(false),
        thread_([this]() { Thread(); }) {
    while (!has_terminate_id_) {
      // Note: This will hang forever if the local Karl is not responsing.
      // OK and desired for the test, obviously not how it should be in production.
      std::this_thread::yield();
    }
  }

  virtual ~HTTPStreamSubscriber() {
    destructing_ = true;
    if (!terminate_id_.empty()) {
      const std::string terminate_url = remote_stream_url_ + "?terminate=" + terminate_id_;
      try {
        HTTP(GET(terminate_url));
      } catch (current::net::NetworkException&) {
      }
    }
    thread_.join();
  }

 private:
  void Thread() {
    const std::string url = remote_stream_url_ + "?i=" + current::ToString(index_);
    try {
      HTTP(ChunkedGET(
          url,
          [this](const std::string& header, const std::string& value) { OnHeader(header, value); },
          [this](const std::string& chunk_body) { OnChunk(chunk_body); },
          []() {}));
    } catch (current::net::NetworkException& e) {
      std::cerr << e.DetailedDescription() << std::endl;
      CURRENT_ASSERT(false);
    }
    // NOTE(dkorolev): The code above was all in the `while (!destructing_)` loop. But it's unnecessary for the test!
    while (!destructing_) {
      std::this_thread::yield();
    }
  }

  void OnHeader(const std::string& header, const std::string& value) {
    if (header == "X-Current-Stream-Subscription-Id") {
      terminate_id_ = value;
      has_terminate_id_ = true;
    }
  }
  void OnChunk(const std::string& chunk) {
    if (destructing_) {
      return;
    }
    const auto split = current::strings::Split(chunk, '\t');
    if (split.size() != 2u) {
      std::cerr << "HTTPStreamSubscriber got malformed chunk: '" << chunk << "'." << std::endl;
      CURRENT_ASSERT(false);
    }
    const idxts_t idxts = ParseJSON<idxts_t>(split[0]);
    if (idxts.index != index_) {
      std::cerr << "HTTPStreamSubscriber expected index " << index_ << ", got " << idxts.index << '.' << std::endl;
      CURRENT_ASSERT(false);
    }
    callback_(idxts, ParseJSON<ENTRY>(split[1]));
    ++index_;
  }

 private:
  const std::string remote_stream_url_;
  callback_t callback_;
  std::atomic_bool destructing_;
  uint64_t index_;
  std::atomic_bool has_terminate_id_;
  std::thread thread_;
  std::string terminate_id_;
};

#endif  // KARL_TEST_SERVICE_HTTP_SUBSCRIBER_H
