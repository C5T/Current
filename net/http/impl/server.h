/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_NET_HTTP_IMPL_SERVER_H
#define BRICKS_NET_HTTP_IMPL_SERVER_H

#include <iostream>  // TODO(dkorolev): Remove it.

#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

#include "../codes.h"

#include "../../exceptions.h"

#include "../../tcp/tcp.h"

#include "../../../strings/util.h"
#include "../../../cerealize/cerealize.h"

namespace bricks {
namespace net {

typedef std::vector<std::pair<std::string, std::string>> HTTPHeadersType;

// HTTP constants to parse the header and extract method, URL, headers and body.
namespace {

const char kCRLF[] = "\r\n";
const size_t kCRLFLength = strings::CompileTimeStringLength(kCRLF);
const char kHeaderKeyValueSeparator[] = ": ";
const size_t kHeaderKeyValueSeparatorLength = strings::CompileTimeStringLength(kHeaderKeyValueSeparator);
const char* const kContentLengthHeaderKey = "Content-Length";
const char* const kTransferEncodingHeaderKey = "Transfer-Encoding";
const char* const kTransferEncodingChunkedValue = "chunked";

}  // namespace constants

// HTTPDefaultHelper handles headers and chunked transfers.
// One can inject a custom implementaion of it to avoid keeping all HTTP body in memory.
// TODO(dkorolev): This is not yet the case, but will be soon once I fix HTTP parse code.
class HTTPDefaultHelper {
 public:
  typedef std::map<std::string, std::string> HeadersType;
  const HeadersType& headers() const { return headers_; }

 protected:
  inline void OnHeader(const char* key, const char* value) { headers_[key] = value; }

  inline void OnChunk(const char* chunk, size_t length) { body_.append(chunk, length); }

  inline void OnChunkedBodyDone(const char*& begin, const char*& end) {
    begin = body_.data();
    end = begin + body_.length();
  }

 private:
  HeadersType headers_;
  std::string body_;
};

// In constructor, TemplatedHTTPReceivedMessage parses HTTP response from `Connection&` is was provided with.
// Extracts method, path (URL + parameters), and, if provided, the body.
//
// Getters:
// * std::string Path().
// * std::string Method().
// * bool HasBody(), std::string Body(), size_t BodyLength(), const char* Body{Begin,End}().
//
// Exceptions:
// * HTTPNoBodyProvidedException : When attempting to access body when HasBody() is false.
// * ConnectionResetByPeer       : When the server is using chunked transfer and doesn't fully send one.
//
// HTTP message: http://www.w3.org/Protocols/rfc2616/rfc2616.html
template <class HELPER>
class TemplatedHTTPReceivedMessage : public HELPER {
 public:
  inline TemplatedHTTPReceivedMessage(Connection& c,
                                      const int intial_buffer_size = 1600,
                                      const double buffer_growth_k = 1.95,
                                      const size_t buffer_max_growth_due_to_content_length = 1024 * 1024)
      : buffer_(intial_buffer_size) {
    // `offset` is the number of bytes read into `buffer_` so far.
    // `length_cap` is infinity first (size_t is unsigned), and it changes/ to the absolute offset
    // of the end of HTTP body in the buffer_, once `Content-Length` and two consecutive CRLS have been seen.
    size_t offset = 0;
    size_t length_cap = static_cast<size_t>(-1);

    // `current_line_offset` is the index of the first character after CRLF in `buffer_`.
    size_t current_line_offset = 0;

    // `body_offset` and `body_length` describe the position of HTTP body, if it's not chunk-encoded.
    size_t body_offset = static_cast<size_t>(-1);
    size_t body_length = static_cast<size_t>(-1);

    // `first_line_parsed` denotes whether the line being parsed is the first one, with method and URL.
    bool first_line_parsed = false;

    // `chunked_transfer_encoding` is set when body should be received in chunks insted of a single read.
    bool chunked_transfer_encoding = false;

    // `receiving_body_in_chunks` is set to true when the parsing is already in the "receive body" mode.
    bool receiving_body_in_chunks = false;

    while (offset < length_cap) {
      size_t chunk;
      size_t read_count;
      // Use `- offset - 1` instead of just `- offset` to leave room for the '\0'.
      while (chunk = buffer_.size() - offset - 1,
             read_count = c.BlockingRead(&buffer_[offset], chunk),
             offset += read_count,
             read_count == chunk && offset < length_cap) {
        buffer_.resize(buffer_.size() * buffer_growth_k);
      }
      if (!read_count) {
        // This is worth re-checking, but as for 2014/12/06 the concensus of reading through man
        // and StackOverflow is that a return value of zero from read() from a socket indicates
        // that the socket has been closed by the peer.
        BRICKS_THROW(ConnectionResetByPeer());
      }
      buffer_[offset] = '\0';
      char* next_crlf_ptr;
      while ((body_offset == static_cast<size_t>(-1) || offset < body_offset) &&
             (next_crlf_ptr = strstr(&buffer_[current_line_offset], kCRLF))) {
        const bool line_is_blank = (next_crlf_ptr == &buffer_[current_line_offset]);
        *next_crlf_ptr = '\0';
        // `next_line_offset` is mutable since reading chunked body will change it.
        size_t next_line_offset = next_crlf_ptr + kCRLFLength - &buffer_[0];
        if (!first_line_parsed) {
          if (!line_is_blank) {
            // It's recommended by W3 to wait for the first line ignoring prior CRLF-s.
            const std::vector<std::string> pieces =
                strings::Split<strings::ByWhitespace>(&buffer_[current_line_offset]);
            if (pieces.size() >= 1) {
              method_ = pieces[0];
            }
            if (pieces.size() >= 2) {
              path_ = pieces[1];
            }
            first_line_parsed = true;
          }
        } else if (receiving_body_in_chunks) {
          // Ignore blank lines.
          if (!line_is_blank) {
            const size_t chunk_length =
                static_cast<size_t>(std::stoi(&buffer_[current_line_offset], nullptr, 16));
            if (chunk_length == 0) {
              // Done with the body.
              HELPER::OnChunkedBodyDone(body_buffer_begin_, body_buffer_end_);
              return;
            } else {
              // A chunk of length `chunk_length` bytes starts right at next_line_offset.
              const size_t chunk_offset = next_line_offset;
              // First, make sure it has been read.
              const size_t next_offset = chunk_offset + chunk_length;
              if (offset < next_offset) {
                const size_t bytes_to_read = next_offset - offset;
                // The `+1` is required for the '\0'.
                if (buffer_.size() < next_offset + 1) {
                  buffer_.resize(std::max<size_t>(buffer_.size() * buffer_growth_k, next_offset + 1));
                }
                if (bytes_to_read != c.BlockingRead(&buffer_[offset], bytes_to_read)) {
                  BRICKS_THROW(ConnectionResetByPeer());
                }
                offset = next_offset;
              }
              // Then, append this newly parsed or received chunk to the body.
              HELPER::OnChunk(&buffer_[chunk_offset], chunk_length);
              // Finally, change `next_line_offset` to force skipping the, possibly binary, body.
              // There will be an extra CRLF after the chunk, but we don't require it.
              next_line_offset = next_offset;
              // TODO(dkorolev): The above code works, but keeps growing memory usage. Shrink it.
            }
          }
        } else if (!line_is_blank) {
          char* p = strstr(&buffer_[current_line_offset], kHeaderKeyValueSeparator);
          if (p) {
            *p = '\0';
            const char* const key = &buffer_[current_line_offset];
            const char* const value = p + kHeaderKeyValueSeparatorLength;
            HELPER::OnHeader(key, value);
            if (!strcmp(key, kContentLengthHeaderKey)) {
              body_length = static_cast<size_t>(atoi(value));
            } else if (!strcmp(key, kTransferEncodingHeaderKey)) {
              if (!strcmp(value, kTransferEncodingChunkedValue)) {
                chunked_transfer_encoding = true;
              }
            }
          }
        } else {
          if (!chunked_transfer_encoding) {
            // HTTP body starts right after this last CRLF.
            body_offset = next_line_offset;
            // Non-chunked encoding. Assume BODY follows as raw data.
            // Only accept HTTP body if Content-Length has been set; ignore it otherwise.
            if (body_length != static_cast<size_t>(-1)) {
              // Has HTTP body to parse.
              length_cap = body_offset + body_length;
              // Resize the buffer to be able to get the contents of HTTP body without extra resizes,
              // while being careful to not be open to extra-large mistakenly or maliciously set
              // Content-Length.
              // Keep in mind that `buffer_` should have the size of `length_cap + 1`, to include the `\0'.
              if (length_cap + 1 > buffer_.size()) {
                const size_t delta_size = length_cap + 1 - buffer_.size();
                buffer_.resize(buffer_.size() + std::min(delta_size, buffer_max_growth_due_to_content_length));
              }
            } else {
              // Indicate we are done parsing the header.
              length_cap = body_offset;
            }
          } else {
            receiving_body_in_chunks = true;
          }
        }
        current_line_offset = next_line_offset;
      }
    }
    if (body_length != static_cast<size_t>(-1)) {
      // Initialize pointers pair to point to the BODY to be read.
      body_buffer_begin_ = &buffer_[body_offset];
      body_buffer_end_ = body_buffer_begin_ + body_length;
    }
  }

  inline const std::string& Method() const { return method_; }

  inline const std::string& Path() const { return path_; }

  // Note that `Body*()` methods assume that the body was fully read into memory.
  // If other means of reading the body, for example, event-based chunk parsing, is used,
  // then `HasBody()` will be false and all other `Body*()` methods will throw.
  inline bool HasBody() const { return body_buffer_begin_ != nullptr; }

  inline const std::string Body() const {
    if (body_buffer_begin_) {
      return std::string(body_buffer_begin_, body_buffer_end_);
    } else {
      BRICKS_THROW(HTTPNoBodyProvidedException());
    }
  }

  inline const char* BodyBegin() const {
    if (body_buffer_begin_) {
      return body_buffer_begin_;
    } else {
      BRICKS_THROW(HTTPNoBodyProvidedException());
    }
  }

  inline const char* BodyEnd() const {
    if (body_buffer_begin_) {
      assert(body_buffer_end_);
      return body_buffer_end_;
    } else {
      BRICKS_THROW(HTTPNoBodyProvidedException());
    }
  }

  inline size_t BodyLength() const {
    if (body_buffer_begin_) {
      assert(body_buffer_end_);
      return body_buffer_end_ - body_buffer_begin_;
    } else {
      BRICKS_THROW(HTTPNoBodyProvidedException());
    }
  }

 private:
  // Fields available to the user via getters.
  std::string method_;
  std::string path_;

  // HTTP parsing fields that have to be caried out of the parsing routine.
  std::vector<char> buffer_;  // The buffer into which data has been read, except for chunked case.
  const char* body_buffer_begin_ = nullptr;  // If BODY has been provided, pointer pair to it.
  const char* body_buffer_end_ = nullptr;    // Will not be nullptr if body_buffer_begin_ is not nullptr.
};

// The default implementation is exposed as HTTPReceivedMessage.
typedef TemplatedHTTPReceivedMessage<HTTPDefaultHelper> HTTPReceivedMessage;

class HTTPServerConnection {
 public:
  HTTPServerConnection(Connection&& c) : connection_(std::move(c)), message_(connection_) {}

  inline static const std::string DefaultContentType() { return "text/plain"; }

  inline static void PrepareHTTPResponseHeader(std::ostream& os,
                                               HTTPResponseCode code = HTTPResponseCode::OK,
                                               const std::string& content_type = DefaultContentType(),
                                               const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    os << "HTTP/1.1 " << static_cast<int>(code);
    os << " " << HTTPResponseCodeAsStringGenerator::CodeAsString(code) << kCRLF;
    os << "Content-Type: " << content_type << kCRLF;
    for (const auto cit : extra_headers) {
      os << cit.first << ": " << cit.second << kCRLF;
    }
  }

  // The actual implementation of sending the HTTP response.
  template <typename T>
  inline void SendHTTPResponseImpl(const T& begin,
                                   const T& end,
                                   HTTPResponseCode code,
                                   const std::string& content_type,
                                   const HTTPHeadersType& extra_headers) {
    std::ostringstream os;
    PrepareHTTPResponseHeader(os, code, content_type, extra_headers);
    os << "Content-Length: " << (end - begin) << kCRLF << kCRLF;
    connection_.BlockingWrite(os.str());
    connection_.BlockingWrite(begin, end);
  }

  // Only support STL containers of chars and bytes, this does not yet cover std::string.
  template <typename T>
  inline typename std::enable_if<sizeof(typename T::value_type) == 1>::type SendHTTPResponse(
      const T& begin,
      const T& end,
      HTTPResponseCode code = HTTPResponseCode::OK,
      const std::string& content_type = DefaultContentType(),
      const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    SendHTTPResponseImpl(begin, end, code, content_type, extra_headers);
  }
  template <typename T>
  inline typename std::enable_if<sizeof(typename std::remove_reference<T>::type::value_type) == 1>::type
  SendHTTPResponse(T&& container,
                   HTTPResponseCode code = HTTPResponseCode::OK,
                   const std::string& content_type = DefaultContentType(),
                   const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    SendHTTPResponseImpl(container.begin(), container.end(), code, content_type, extra_headers);
  }

  // Special case to handle std::string.
  inline void SendHTTPResponse(const std::string& container,
                               HTTPResponseCode code = HTTPResponseCode::OK,
                               const std::string& content_type = DefaultContentType(),
                               const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    SendHTTPResponseImpl(container.begin(), container.end(), code, content_type, extra_headers);
  }
  // Support objects that can be serialized as JSON-s via Cereal.
  template <class T>
  inline typename std::enable_if<
      (cerealize::is_cerealizeable<typename std::remove_reference<T>::type>::value)>::type
  SendHTTPResponse(T&& object,
                   HTTPResponseCode code = HTTPResponseCode::OK,
                   const std::string& content_type = DefaultContentType(),
                   const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    // TODO(dkorolev): We should probably make this not only correct but also efficient.
    const std::string s = cerealize::JSON(object) + kCRLF;
    SendHTTPResponseImpl(s.begin(), s.end(), code, content_type, extra_headers);
  }

  // The wrapper to send HTTP response in chunks.
  struct ChunkedResponseSender final {
    // `struct Impl` is the logic wrapped into an `std::unique_ptr<>` to call the destructor only once.
    struct Impl final {
      explicit Impl(Connection& connection) : connection_(connection) {}

      ~Impl() {
        try {
          connection_.BlockingWrite("0");
          connection_.BlockingWrite(kCRLF);
        } catch (std::exception& e) {  // LCOV_EXCL_LINE
          // TODO(dkorolev): More reliable logging.
          std::cerr << "Chunked response closure failed: " << e.what() << std::endl;  // LCOV_EXCL_LINE
        }
      }

      // The actual implementation of sending HTTP chunk data.
      template <typename T>
      void SendImpl(T&& data) {
        connection_.BlockingWrite(strings::Printf("%X", data.size()));
        connection_.BlockingWrite(kCRLF);
        connection_.BlockingWrite(data);
        connection_.BlockingWrite(kCRLF);
      }

      // Only support STL containers of chars and bytes, this does not yet cover std::string.
      template <typename T>
      inline typename std::enable_if<sizeof(typename std::remove_reference<T>::type::value_type) == 1>::type
      Send(T&& data) {
        SendImpl(data);
      }

      // Special case to handle std::string.
      inline void Send(const std::string& data) { SendImpl(data); }

      // Support objects that can be serialized as JSON-s via Cereal.
      template <class T>
      inline typename std::enable_if<
          (cerealize::is_cerealizeable<typename std::remove_reference<T>::type>::value)>::type
      Send(T&& object) {
        SendImpl(cerealize::JSON(object) + kCRLF);
      }

      Connection& connection_;

      Impl() = delete;
      Impl(const Impl&) = delete;
      Impl(Impl&&) = delete;
      void operator=(const Impl&) = delete;
      void operator=(Impl&&) = delete;
    };

    explicit ChunkedResponseSender(Connection& connection) : impl_(new Impl(connection)) {}

    template <typename T>
    inline ChunkedResponseSender& Send(T&& data) {
      impl_->Send(data);
      return *this;
    }

    std::unique_ptr<Impl> impl_;
  };

  inline ChunkedResponseSender SendChunkedHTTPResponse(
      HTTPResponseCode code = HTTPResponseCode::OK,
      const std::string& content_type = DefaultContentType(),
      const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    std::ostringstream os;
    PrepareHTTPResponseHeader(os, code, content_type, extra_headers);
    os << "Transfer-Encoding: chunked" << kCRLF << kCRLF;
    connection_.BlockingWrite(os.str());
    return std::move(ChunkedResponseSender(connection_));
  }

  const HTTPReceivedMessage& Message() const { return message_; }

  Connection& RawConnection() { return connection_; }

 private:
  Connection connection_;
  HTTPReceivedMessage message_;

  HTTPServerConnection(const HTTPServerConnection&) = delete;
  void operator=(const HTTPServerConnection&) = delete;
  HTTPServerConnection(HTTPServerConnection&&) = delete;
  void operator=(HTTPServerConnection&&) = delete;
};

}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_HTTP_IMPL_SERVER_H
