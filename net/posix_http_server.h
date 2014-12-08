#ifndef BRICKS_NET_POSIX_HTTP_SERVER_H
#define BRICKS_NET_POSIX_HTTP_SERVER_H

// HTTP message: http://www.w3.org/Protocols/rfc2616/rfc2616.html

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "exceptions.h"
#include "posix_tcp_server.h"
#include "http_response_codes.h"

namespace bricks {
namespace net {

typedef std::vector<std::pair<std::string, std::string>> HTTPHeadersType;

class HTTPHeaderParser {
 public:
  inline HTTPHeaderParser(const int intial_buffer_size = 1600,
                          const double buffer_growth_k = 1.95,
                          const size_t buffer_max_growth_due_to_content_length = 1024 * 1024)
      : buffer_(intial_buffer_size),
        buffer_growth_k_(buffer_growth_k),
        buffer_max_growth_due_to_content_length_(buffer_max_growth_due_to_content_length) {
  }

  inline const std::string& Method() const {
    return method_;
  }

  inline const std::string& URL() const {
    return url_;
  }

  inline bool HasBody() const {
    return content_offset_ != static_cast<size_t>(-1) && content_length_ != static_cast<size_t>(-1);
  }

  inline const std::string Body() const {
    if (HasBody()) {
      return std::string(&buffer_[content_offset_], &buffer_[content_offset_] + content_length_);
    } else {
      throw HTTPNoBodyProvidedException();
    }
  }

  inline const char* const BodyAsNonCopiedBuffer() const {
    if (HasBody()) {
      return &buffer_[content_offset_];
    } else {
      throw HTTPNoBodyProvidedException();
    }
  }

  inline const size_t BodyLength() const {
    if (HasBody()) {
      return content_length_;
    } else {
      throw HTTPNoBodyProvidedException();
    }
  }

 protected:
  // Parses HTTP headers. Extracts method, URL, and, if provided, the body.
  // Can be statically overridden by providing a different template class as a parameter for
  // GenericHTTPConnection.
  //
  // Return value:
  // - False if the headers could not have been parsed due to connection interrupted by peer.
  // - True in any other case, since almost no header validation is performed by this implementation.
  inline bool ParseHTTPHeader(Connection& c) {
    // HTTP constants to parse the header and extract method, URL, headers and body.
    const char* const kCRLF = "\r\n";
    const size_t kCRLFLength = strlen(kCRLF);
    const char* const kHeaderKeyValueSeparator = ": ";
    const size_t kHeaderKeyValueSeparatorLength = strlen(kHeaderKeyValueSeparator);
    const char* const kContentLengthHeaderKey = "Content-Length";

    // `buffer_` stores all the stream of data read from the socket, headers followed by optional body.
    size_t current_line_offset = 0;

    // `first_line_parsed` denotes whether the line being parsed is the first one, with method and URL.
    bool first_line_parsed = false;

    // `offset` is the number of bytes read so far.
    // `length_cap` is infinity first (size_t is unsigned), and it changes/ to the absolute offset
    // of the end of HTTP body in the buffer_, once `Content-Length` and two consecutive CRLS have been seen.
    size_t offset = 0;
    size_t length_cap = static_cast<size_t>(-1);

    while (offset < length_cap) {
      size_t chunk;
      size_t read_count;
      // Use `- offset - 1` instead of just `- offset` to leave room for the '\0'.
      while (chunk = buffer_.size() - offset - 1,
             read_count = c.BlockingRead(&buffer_[offset], chunk),
             offset += read_count,
             read_count == chunk) {
        buffer_.resize(buffer_.size() * buffer_growth_k_);
      }
      if (!read_count) {
        // This is worth re-checking, but as for 2014/12/06 the concensus of reading through man
        // and StackOverflow is that a return value of zero from read() from a socket indicates
        // that the socket has been closed by the peer. Returning `false` will mark this HTTP session
        // as unhealthy, attempts to send responses via it will throw.
        return false;
      }
      buffer_[offset] = '\0';
      char* p = &buffer_[current_line_offset];
      char* current_line = p;
      while ((p = strstr(current_line, kCRLF))) {
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
              OnHeader(key, value);
              if (!strcmp(key, kContentLengthHeaderKey)) {
                content_length_ = static_cast<size_t>(atoi(value));
              }
            }
          } else {
            // HTTP body starts right after this last CRLF.
            content_offset_ = current_line + kCRLFLength - &buffer_[0];
            // Only accept HTTP body if Content-Length has been set; ignore it otherwise.
            if (content_length_ != static_cast<size_t>(-1)) {
              // Has HTTP body to parse.
              length_cap = content_offset_ + content_length_;
              // Resize the buffer to be able to get the contents of HTTP body without extra resizes,
              // while being careful to not be open to extra-large mistakenly or maliciously set Content-Length.
              // Keep in mind that `buffer_` should have the size of `length_cap + 1`, to include the `\0'.
              if (length_cap + 1 > buffer_.size()) {
                const size_t delta_size = length_cap + 1 - buffer_.size();
                buffer_.resize(buffer_.size() + std::min(delta_size, buffer_max_growth_due_to_content_length_));
              }
            } else {
              // Indicate we are done parsing the header.
              length_cap = content_offset_;
            }
          }
        }
        current_line = p + 2;
      }
      current_line_offset = current_line - &buffer_[0];
    }
    return true;
  }

  // Non-virtual, but can be statically overridden via template parameter to class GenericHTTPConnection.
  inline void OnHeader(const char* key, const char* value) {
    headers_[key] = value;
  }

 private:
  std::string method_;
  std::string url_;
  std::map<std::string, std::string> headers_;
  std::vector<char> buffer_;
  const double buffer_growth_k_;
  const size_t buffer_max_growth_due_to_content_length_;
  size_t content_offset_ = static_cast<size_t>(-1);
  size_t content_length_ = static_cast<size_t>(-1);
};

template <typename HEADER_PARSER = HTTPHeaderParser>
class GenericHTTPConnection final : public Connection, public HEADER_PARSER {
 public:
  typedef HEADER_PARSER T_HEADER_PARSER;

  inline GenericHTTPConnection(Connection&& c)
      : Connection(std::move(c)), T_HEADER_PARSER(), good_(T_HEADER_PARSER::ParseHTTPHeader(*this)) {
  }

  inline GenericHTTPConnection(GenericHTTPConnection&& c)
      : Connection(std::move(c)), T_HEADER_PARSER(), good_(T_HEADER_PARSER::ParseHTTPHeader(*this)) {
  }

  inline operator bool() const {
    return good_;
  }

  inline static const std::string DefaultContentType() {
    return "text/plain";
  }

  template <typename T>
  inline typename std::enable_if<sizeof(typename T::value_type) == 1>::type SendHTTPResponse(
      const T& begin,
      const T& end,
      HTTPResponseCode code = HTTPResponseCode::OK,
      const std::string& content_type = DefaultContentType(),
      const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    if (!good_) {
      throw HTTPConnectionClosedByPeerBeforeHeadersWereSentInException();
    }
    if (responded_) {
      throw HTTPAttemptedToRespondTwiceException();
    }
    responded_ = true;
    std::ostringstream os;
    os << "HTTP/1.1 " << static_cast<int>(code) << " " << HTTPResponseCodeAsStringGenerator::CodeAsString(code)
       << "\r\n"
       << "Content-Type: " << content_type << "\r\n"
       << "Content-Length: " << (end - begin) << "\r\n";
    for (const auto cit : extra_headers) {
      os << cit.first << ": " << cit.second << "\r\n";
    }
    os << "\r\n";
    BlockingWrite(os.str());
    BlockingWrite(begin, end);
    BlockingWrite("\r\n");
  }

  template <typename T>
  inline typename std::enable_if<sizeof(typename T::value_type) == 1>::type SendHTTPResponse(
      const T& container,
      HTTPResponseCode code = HTTPResponseCode::OK,
      const std::string& content_type = DefaultContentType(),
      const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    if (!good_) {
      throw HTTPConnectionClosedByPeerBeforeHeadersWereSentInException();
    }
    SendHTTPResponse(container.begin(), container.end(), code, content_type, extra_headers);
  }

  inline void SendHTTPResponse(const std::string& container,
                               HTTPResponseCode code = HTTPResponseCode::OK,
                               const std::string& content_type = DefaultContentType(),
                               const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    SendHTTPResponse(container.begin(), container.end(), code, content_type, extra_headers);
  }

 private:
  bool good_ = false;
  bool responded_ = false;

  GenericHTTPConnection(const GenericHTTPConnection&) = delete;
  void operator=(const GenericHTTPConnection&) = delete;
  void operator=(GenericHTTPConnection&&) = delete;
};

// Default HTTPConnection parses URL, method, and body for requests with Content-Length.
typedef GenericHTTPConnection<HTTPHeaderParser> HTTPConnection;

}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_POSIX_HTTP_SERVER_H
