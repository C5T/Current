// Based on official example from http://rapidjson.org/md_doc_stream.html
#ifndef RAPIDJSON_STREAMWRAPPER_H_
#define RAPIDJSON_STREAMWRAPPER_H_

#include "rapidjson.h"

namespace rapidjson {

class IStreamWrapper {
 public:
  typedef char Ch;
  IStreamWrapper(std::istream& is) : is_(is) {}

  Ch Peek() const {
      int c = is_.peek();
      return c == std::char_traits<char>::eof() ? '\0' : (Ch)c;
  }
  Ch Take() {
      int c = is_.get();
      return c == std::char_traits<char>::eof() ? '\0' : (Ch)c;
  }
  size_t Tell() const { return (size_t)is_.tellg(); }

  // Not implemented.
  Ch* PutBegin() { RAPIDJSON_ASSERT(false); return 0; }
  void Put(Ch) { RAPIDJSON_ASSERT(false); }
  void Flush() { RAPIDJSON_ASSERT(false); }
  size_t PutEnd(Ch*) { RAPIDJSON_ASSERT(false); return 0; }

 private:
  IStreamWrapper(const IStreamWrapper&) = delete;
  IStreamWrapper& operator=(const IStreamWrapper&) = delete;

  std::istream& is_;
};

class OStreamWrapper {
 public:
  typedef char Ch;
  OStreamWrapper(std::ostream& os) : os_(os) {}

  void Put(Ch c) { os_.put(c); }
  void PutN(char c, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      Put(c);
    }
  }
  void Flush() { os_.flush(); }
  size_t Tell() const { return (int)os_.tellp(); }

  // Not implemented.
  Ch Peek() const { RAPIDJSON_ASSERT(false); return '\0'; }
  Ch Take() { RAPIDJSON_ASSERT(false); return '\0'; }
  Ch* PutBegin() { RAPIDJSON_ASSERT(false); return 0; }
  size_t PutEnd(Ch*) { RAPIDJSON_ASSERT(false); return 0; }

 private:
  OStreamWrapper(const OStreamWrapper&) = delete;
  OStreamWrapper& operator=(const OStreamWrapper&) = delete;

  std::ostream& os_;
};

}  // namespace rapidjson

#endif  // RAPIDJSON_STREAMWRAPPER_H_
