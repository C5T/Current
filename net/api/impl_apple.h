// TODO(dkorolev): A huge TODO on cleaning up this file. Perhaps it could become Objective-C only?

#ifndef BRICKS_NET_API_IMPL_APPLE_H
#define BRICKS_NET_API_IMPL_APPLE_H

/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus

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

#include <string>
#include <cassert>

namespace aloha {

class HTTPClientApple {
 public:
  enum {
    kNotInitialized = -1,
  };

 private:
  std::string url_requested_;
  // Contains final content's url taking redirects (if any) into an account.
  std::string url_received_;
  int error_code_;
  std::string post_file_;
  // Used instead of server_reply_ if set.
  std::string received_file_;
  // Data we received from the server if output_file_ wasn't initialized.
  std::string server_response_;
  std::string content_type_;
  std::string user_agent_;
  std::string post_body_;

  HTTPClientApple(const HTTPClientApple&) = delete;
  HTTPClientApple(HTTPClientApple&&) = delete;
  HTTPClientApple& operator=(const HTTPClientApple&) = delete;

 public:
  HTTPClientApple() : error_code_(kNotInitialized) {
  }
  HTTPClientApple(const std::string& url) : url_requested_(url), error_code_(kNotInitialized) {
  }
  HTTPClientApple& set_url_requested(const std::string& url) {
    url_requested_ = url;
    return *this;
  }
  // This method is mutually exclusive with set_post_body().
  HTTPClientApple& set_post_file(const std::string& post_file, const std::string& content_type) {
    post_file_ = post_file;
    content_type_ = content_type;
    // TODO (dkorolev) replace with exceptions as discussed offline.
    assert(post_body_.empty());
    return *this;
  }
  // If set, stores server reply in file specified.
  HTTPClientApple& set_received_file(const std::string& received_file) {
    received_file_ = received_file;
    return *this;
  }
  HTTPClientApple& set_user_agent(const std::string& user_agent) {
    user_agent_ = user_agent;
    return *this;
  }
  // This method is mutually exclusive with set_post_file().
  HTTPClientApple& set_post_body(const std::string& post_body, const std::string& content_type) {
    post_body_ = post_body;
    content_type_ = content_type;
    // TODO (dkorolev) replace with exceptions as discussed offline.
    assert(post_file_.empty());
    return *this;
  }
  // Move version to avoid string copying.
  // This method is mutually exclusive with set_post_file().
  HTTPClientApple& set_post_body(std::string&& post_body, const std::string& content_type) {
    post_body_ = post_body;
    post_file_.clear();
    content_type_ = content_type;
    return *this;
  }

  // Synchronous (blocking) call, should be implemented for each platform
  // @returns true only if server answered with HTTP 200 OK
  // @note Implementations should transparently support all needed HTTP redirects
  bool Go();

  std::string const& url_requested() const {
    return url_requested_;
  }
  // @returns empty string in the case of error
  std::string const& url_received() const {
    return url_received_;
  }
  bool was_redirected() const {
    return url_requested_ != url_received_;
  }
  // Mix of HTTP errors (in case of successful connection) and system-dependent error codes,
  // in the simplest success case use 'if (200 == client.error_code())' // 200 means OK in HTTP
  int error_code() const {
    return error_code_;
  }
  std::string const& server_response() const {
    return server_response_;
  }

};  // class HTTPClientApple

}  // namespace aloha

#if defined(__APPLE__)
#include "impl_apple.nm"
#else
// TODO(dkorolev) + TODO(deathbaba): I suggest to only keep Apple implementation in this file.
// #elif defined(ANDROID)
// #include "http_client_android.cc"
#endif

#endif  // BRICKS_NET_API_IMPL_APPLE_H
