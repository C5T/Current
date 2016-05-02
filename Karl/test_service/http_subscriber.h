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

#include "../../Blocks/HTTP/api.h"
#include "../../Blocks/SS/idx_ts.h"

template <typename ENTRY>
class HTTPStreamSubscriber {
 public:
  HTTPStreamSubscriber(const std::string& remote_stream_url, std::function<void(ENTRY&&)> callback)
      : remote_stream_url_(remote_stream_url),
        callback_(callback),
        destructing_(false),
        index_(0u),
        thread_([this]() { Thread(); }) {}

  ~HTTPStreamSubscriber() {
    destructing_ = true;
    const std::string terminate_url = remote_stream_url_ + "?terminate=" + terminate_id_;
    HTTP(GET(terminate_url));
    thread_.join();
  }

 private:
  void Thread() {
    while (!destructing_) {
      const std::string url = remote_stream_url_ + "?i=" + current::ToString(index_);
      try {
        HTTP(
            ChunkedGET(url,
                       [this](const std::string& header, const std::string& value) { OnHeader(header, value); },
                       [this](const std::string& chunk_body) { OnChunk(chunk_body); },
                       [this]() {}));
      } catch (current::net::NetworkException&) {
      }
    }
  }

  void OnHeader(const std::string& header, const std::string& value) {
    if (header == "X-Current-Stream-Subscription-Id") {
      terminate_id_ = value;
    }
  }
  void OnChunk(const std::string& chunk) {
    if (destructing_) {
      return;
    }
    const auto split = current::strings::Split(chunk, '\t');
    assert(split.size() == 2u);
    assert(ParseJSON<idxts_t>(split[0]).index == index_);
    callback_(ParseJSON<ENTRY>(split[1]));
    ++index_;
  }

 private:
  const std::string remote_stream_url_;
  std::function<void(ENTRY&&)> callback_;
  std::atomic_bool destructing_;
  uint64_t index_;
  std::thread thread_;
  std::string terminate_id_;
};

#endif  // KARL_TEST_SERVICE_HTTP_SUBSCRIBER_H
