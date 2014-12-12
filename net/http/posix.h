// TODO(dkorolev): Add Mac support and find out the right name for this header file.

#ifndef BRICKS_NET_HTTP_POSIX_H
#define BRICKS_NET_HTTP_POSIX_H

// HTTP message: http://www.w3.org/Protocols/rfc2616/rfc2616.html

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "codes.h"

#include "../exceptions.h"
#include "../tcp/posix.h"

using std::string;
using std::vector;
using std::move;
using std::pair;
using std::min;
using std::map;
using std::ostringstream;
using std::enable_if;

namespace bricks {
namespace net {

typedef vector<pair<string, string>> HTTPHeadersType;

// HTTP constants to parse the header and extract method, URL, headers and body.
namespace {

const char* const kCRLF = "\r\n";
const size_t kCRLFLength = strlen(kCRLF);
const char* const kHeaderKeyValueSeparator = ": ";
const size_t kHeaderKeyValueSeparatorLength = strlen(kHeaderKeyValueSeparator);
const char* const kContentLengthHeaderKey = "Content-Length";
const char* const kTransferEncodingHeaderKey = "Transfer-Encoding";
const char* const kTransferEncodingChunkedValue = "chunked";

}  // namespace constants

// HTTPDefaultHelper handles headers and chunked transfers.
class HTTPDefaultHelper {
 protected:
  inline void OnHeader(const char* key, const char* value) {
    headers_[key] = value;
  }

  inline void OnChunk(size_t /*length*/) {
  }

  inline void OnChunkedBodyDone() {
  }

 private:
  map<string, string> headers_;
};

// In constructor, TemplatedHTTPReceivedMessage parses HTTP response from `Connection&` is was provided with.
// Extracts method, URL, and, if provided, the body.
//
// Getters:
// * string URL().
// * string Method().
// * bool HasBody(), string Body(), size_t BodyLength(), const char* Body{Begin,End}().
//
// Exceptions:
// * HTTPNoBodyProvidedException    : When attempting to access body when HasBody() is false.
// * HTTPPrematureChunkEndException : When the server is using chunked transfer and doesn't fully send one.
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

    while (offset < length_cap) {
      size_t chunk;
      size_t read_count;
      // Use `- offset - 1` instead of just `- offset` to leave room for the '\0'.
      while (chunk = buffer_.size() - offset - 1,
             read_count = c.BlockingRead(&buffer_[offset], chunk),
             offset += read_count,
             read_count == chunk) {
        buffer_.resize(buffer_.size() * buffer_growth_k);
      }
      if (!read_count) {
        // This is worth re-checking, but as for 2014/12/06 the concensus of reading through man
        // and StackOverflow is that a return value of zero from read() from a socket indicates
        // that the socket has been closed by the peer.
        throw HTTPConnectionClosedByPeerException();
      }
      buffer_[offset] = '\0';
      char* p = &buffer_[current_line_offset];
      char* current_line = p;
      while ((body_offset == static_cast<size_t>(-1) || offset < body_offset) &&
             (p = strstr(current_line, kCRLF))) {
        *p = '\0';
        if (!first_line_parsed) {
          if (*current_line) {
            // It's recommended by W3 to wait for the first line ignoring prior CRLF-s.
            char* p1 = current_line;
            char* p2 = strstr(p1, " ");
            if (p2) {
              *p2 = '\0';
              ++p2;
              method_ = p1;
              char* p3 = strstr(p2, " ");
              if (p3) {
                *p3 = '\0';
              }
              url_ = p2;
            }
            first_line_parsed = true;
          }
        } else {
          if (*current_line) {
            char* p = strstr(current_line, kHeaderKeyValueSeparator);
            if (p) {
              *p = '\0';
              const char* const key = current_line;
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
            // HTTP body starts right after this last CRLF.
            body_offset = current_line + kCRLFLength - &buffer_[0];
            if (!chunked_transfer_encoding) {
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
                  buffer_.resize(buffer_.size() + min(delta_size, buffer_max_growth_due_to_content_length));
                }
              } else {
                // Indicate we are done parsing the header.
                length_cap = body_offset;
              }
            } else {
              // TODO(dkorolev): CODE THIS THING UP.
              return;
            }
          }
        }
        current_line = p + kCRLFLength;
      }
      current_line_offset = current_line - &buffer_[0];
    }
    if (body_length != static_cast<size_t>(-1)) {
      // Initialize pointers pair to point to the BODY to be read.
      body_buffer_begin_ = &buffer_[body_offset];
      body_buffer_end_ = body_buffer_begin_ + body_length;
    }
  }

  inline const string& Method() const {
    return method_;
  }

  inline const string& URL() const {
    return url_;
  }

  // Note that `Body*()` methods assume that the body was fully read into memory.
  // If other means of reading the body, for example, event-based chunk parsing, is used,
  // then `HasBody()` will be false and all other `Body*()` methods wil throw.
  inline bool HasBody() const {
    return body_buffer_begin_ != nullptr;
  }

  inline const string Body() const {
    if (body_buffer_begin_) {
      return string(body_buffer_begin_, body_buffer_end_);
    } else {
      throw HTTPNoBodyProvidedException();
    }
  }

  inline const char* BodyBegin() const {
    if (body_buffer_begin_) {
      return body_buffer_begin_;
    } else {
      throw HTTPNoBodyProvidedException();
    }
  }

  inline const char* BodyEnd() const {
    if (body_buffer_begin_) {
      assert(body_buffer_end_);
      return body_buffer_end_;
    } else {
      throw HTTPNoBodyProvidedException();
    }
  }

  inline size_t BodyLength() const {
    if (body_buffer_begin_) {
      assert(body_buffer_end_);
      return body_buffer_end_ - body_buffer_begin_;
    } else {
      throw HTTPNoBodyProvidedException();
    }
  }

 private:
  // Fields available to the user via getters.
  string method_;
  string url_;

  // HTTP parsing fields that have to be caried out of the parsing routine.
  vector<char> buffer_;  // The buffer into which data has been read, except for chunked case.
  const char* body_buffer_begin_ = nullptr;  // If BODY has been provided, pointer pair to it.
  const char* body_buffer_end_ = nullptr;    // Will not be nullptr if body_buffer_begin_ is not nullptr.
};

// The default implementation is exposed under the name HTTPReceivedMessage.
typedef TemplatedHTTPReceivedMessage<HTTPDefaultHelper> HTTPReceivedMessage;

class HTTPServerConnection {
 public:
  HTTPServerConnection(Connection&& c) : connection_(move(c)), message_(connection_) {
  }

  inline static const string DefaultContentType() {
    return "text/plain";
  }

  template <typename T>
  inline typename enable_if<sizeof(typename T::value_type) == 1>::type SendHTTPResponse(
      const T& begin,
      const T& end,
      HTTPResponseCode code = HTTPResponseCode::OK,
      const string& content_type = DefaultContentType(),
      const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    ostringstream os;
    os << "HTTP/1.1 " << static_cast<int>(code) << " " << HTTPResponseCodeAsStringGenerator::CodeAsString(code)
       << "\r\n"
       << "Content-Type: " << content_type << "\r\n"
       << "Content-Length: " << (end - begin) << "\r\n";
    for (const auto cit : extra_headers) {
      os << cit.first << ": " << cit.second << "\r\n";
    }
    os << "\r\n";
    connection_.BlockingWrite(os.str());
    connection_.BlockingWrite(begin, end);
    connection_.BlockingWrite("\r\n");
  }

  template <typename T>
  inline typename enable_if<sizeof(typename T::value_type) == 1>::type SendHTTPResponse(
      const T& container,
      HTTPResponseCode code = HTTPResponseCode::OK,
      const string& content_type = DefaultContentType(),
      const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    SendHTTPResponse(container.begin(), container.end(), code, content_type, extra_headers);
  }

  inline void SendHTTPResponse(const string& container,
                               HTTPResponseCode code = HTTPResponseCode::OK,
                               const string& content_type = DefaultContentType(),
                               const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    SendHTTPResponse(container.begin(), container.end(), code, content_type, extra_headers);
  }

  const HTTPReceivedMessage& Message() const {
    return message_;
  }

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

#endif  // BRICKS_NET_HTTP_POSIX_H
