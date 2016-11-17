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

#ifndef BRICKS_UTIL_BASE64_H
#define BRICKS_UTIL_BASE64_H

#include <cctype>
#include <string>

#include "../exception.h"

namespace current {

struct Base64DecodeException : Exception {
  using Exception::Exception;
};

namespace base64 {

// Ref. https://tools.ietf.org/html/rfc3548#section-3
constexpr static char encode_map[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
// Ref. https://tools.ietf.org/html/rfc3548#section-4
constexpr static char url_encode_map[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
constexpr static char pad_char = '=';
// clang-format off
constexpr static uint8_t decode_map[128] = {
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62,  255, 62,  255, 63,
  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  255, 255, 0,   255, 255, 255, 
  255, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  255, 255, 255, 255, 63,
  255, 26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  255, 255, 255, 255, 255
};
// clang-format on
enum class EncodingType { Canonical, URL };

template <EncodingType TYPE>
struct Impl {
  static std::string Encode(const uint8_t* input, const size_t input_size) {
    const char* map = (TYPE == EncodingType::Canonical) ? encode_map : url_encode_map;
    const size_t result_size = 4 * (input_size / 3) + ((input_size % 3) ? 4 : 0);

    std::string result;
    result.reserve(result_size);
    uint16_t buf = 0u;
    uint8_t nbits = 0;
    for (size_t i = 0u; i < input_size; ++i) {
      buf = (buf << 8) + input[i];
      nbits += 8;
      while (nbits >= 6) {
        nbits -= 6;
        result.push_back(map[(buf >> nbits) & 0x3F]);
      }
    }
    if (nbits > 0) {
      result.push_back(map[((buf << 6) >> nbits) & 0x3F]);
    }
    const size_t num_pads = result_size - result.size();
    result.append(num_pads, pad_char);
    return result;
  }

  static bool IsValidChar(char c) {
    return (std::isalnum(c) || (TYPE == EncodingType::Canonical && (c == '+' || c == '/')) ||
            (TYPE == EncodingType::URL && (c == '-' || c == '_')));
  }

  static std::string Decode(const char* input, const size_t input_size) {
    std::string result;
    result.reserve(3 * input_size / 4);
    uint16_t buf = 0u;
    uint8_t nbits = 0u;
    for (size_t i = 0; i < input_size; ++i) {
      const char c = input[i];
      if (c == pad_char) {
        break;
      }
      if (!IsValidChar(c)) {
        CURRENT_THROW(Base64DecodeException());
      }
      buf = (buf << 6) + decode_map[static_cast<uint8_t>(c)];
      nbits += 6;
      if (nbits >= 8) {
        nbits -= 8;
        result.push_back(char((buf >> nbits) & 0xFF));
      }
    }
    return result;
  }
};

}  // namespace base64

inline std::string Base64Encode(const uint8_t* input, const size_t input_size) {
  return base64::Impl<base64::EncodingType::Canonical>::Encode(input, input_size);
}

inline std::string Base64Encode(const char* input, const size_t input_size) {
  return base64::Impl<base64::EncodingType::Canonical>::Encode(reinterpret_cast<const uint8_t*>(input), input_size);
}

inline std::string Base64Encode(const std::string& input) {
  return base64::Impl<base64::EncodingType::Canonical>::Encode(reinterpret_cast<const uint8_t*>(input.c_str()),
                                                               input.size());
}

inline std::string Base64URLEncode(const uint8_t* input, const size_t input_size) {
  return base64::Impl<base64::EncodingType::URL>::Encode(input, input_size);
}

inline std::string Base64URLEncode(const char* input, const size_t input_size) {
  return base64::Impl<base64::EncodingType::URL>::Encode(reinterpret_cast<const uint8_t*>(input), input_size);
}

inline std::string Base64URLEncode(const std::string& input) {
  return base64::Impl<base64::EncodingType::URL>::Encode(reinterpret_cast<const uint8_t*>(input.c_str()), input.size());
}

inline std::string Base64Decode(const char* input, const size_t input_size) {
  return base64::Impl<base64::EncodingType::Canonical>::Decode(input, input_size);
}

inline std::string Base64Decode(const std::string& input) {
  return base64::Impl<base64::EncodingType::Canonical>::Decode(input.c_str(), input.size());
}

inline std::string Base64URLDecode(const char* input, const size_t input_size) {
  return base64::Impl<base64::EncodingType::URL>::Decode(input, input_size);
}

inline std::string Base64URLDecode(const std::string& input) {
  return base64::Impl<base64::EncodingType::URL>::Decode(input.c_str(), input.size());
}

}  // namespace current

#endif  // BRICKS_UTIL_BASE64_H
