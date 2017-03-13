/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2017 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BLOCKS_URL_EXCEPTIONS_H
#define BLOCKS_URL_EXCEPTIONS_H

#include "../../port.h"
#include "../../Bricks/exception.h"
#include "../../TypeSystem/struct.h"
#include "../../TypeSystem/Reflection/reflection.h"
#include "../../TypeSystem/Schema/schema.h"

namespace current {
namespace url {

struct URLException : Exception {
  using Exception::Exception;
};

struct URLParseObjectAsURLParameterException : URLException {
  const std::string key;
  const std::string error;
  const std::string expected_schema;
  URLParseObjectAsURLParameterException(const std::string& key,
                                        const std::string& error,
                                        const std::string& expected_schema = "")
      : URLException("URLParseObjectAsURLParameterException: `" + key + "`, `" + error + "`" +
                     (expected_schema.empty() ? "" : ", expected: ```\n" + expected_schema + "\n```")),
        key(key),
        error(error),
        expected_schema(expected_schema) {}
};

template <typename T>
struct URLParseSpecificObjectAsURLParameterException : URLParseObjectAsURLParameterException {
  static std::string DescribeT() {
    return current::reflection::StructSchema()
        .template AddType<T>()
        .GetSchemaInfo()
        .template Describe<current::reflection::Language::FSharp>();
  }
  URLParseSpecificObjectAsURLParameterException(const std::string& key, const std::string& error)
      : URLParseObjectAsURLParameterException(key, error, DescribeT()) {}
};

}  // namespace current::url
}  // namespace current

#endif  // BLOCKS_URL_EXCEPTIONS_H
