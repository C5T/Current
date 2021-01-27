/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

#ifndef BLOCKS_HTTP_CHUNKED_RESPONSE_PARSER_H
#define BLOCKS_HTTP_CHUNKED_RESPONSE_PARSER_H

#include "../../port.h"

#include "../../bricks/net/http/response_kind.h"
#include "../../bricks/net/http/headers/headers.h"

class ChunkByChunkHTTPResponseReceiver {
 public:
  struct ConstructionParams {
    std::function<void(const std::string&, const std::string&, current::net::HTTPResponseKind&)> header_callback;
    std::function<void(const std::string&)> chunk_callback;
    std::function<void()> done_callback;

    ConstructionParams() = delete;

    ConstructionParams(std::function<void(const std::string&,
                                          const std::string&,
                                          current::net::HTTPResponseKind&)> header_callback,
                       std::function<void(const std::string&)> chunk_callback,
                       std::function<void()> done_callback)
        : header_callback(header_callback), chunk_callback(chunk_callback), done_callback(done_callback) {}

    ConstructionParams(std::function<void(const std::string&, const std::string&)> header_callback_param,
                       std::function<void(const std::string&)> chunk_callback,
                       std::function<void()> done_callback)
        : header_callback([header_callback_param](const std::string& k,
                                                  const std::string& v,
                                                  current::net::HTTPResponseKind&) {
            header_callback_param(k, v);
          }), chunk_callback(chunk_callback), done_callback(done_callback) {}

    ConstructionParams(const ConstructionParams& rhs) = default;
  };

  ChunkByChunkHTTPResponseReceiver() = delete;
  ChunkByChunkHTTPResponseReceiver(const ConstructionParams& params) : params(params) {}

  const ConstructionParams params;

  const std::string location = "";  // To please `HTTPClientPOSIX`. -- D.K.
  const current::net::http::Headers& headers() const { return headers_; }

 protected:
  inline void OnHeader(const char* key,
                       const char* value,
                       current::net::HTTPResponseKind& response_kind) {
    params.header_callback(key, value, response_kind);
  }

  inline void OnChunk(const char* chunk, size_t length) { params.chunk_callback(std::string(chunk, length)); }

  inline void OnChunkedBodyDone(const char*& begin, const char*& end) {
    params.done_callback();
    begin = nullptr;
    end = nullptr;
  }

 private:
  current::net::http::Headers headers_;
};

#endif  // BLOCKS_HTTP_CHUNKED_RESPONSE_PARSER_H
