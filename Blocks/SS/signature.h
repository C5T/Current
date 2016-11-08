/*******************************************************************************
 The MIT License (MIT)

 Copyright (c) 2016 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>

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

#ifndef BLOCKS_SS_SIGNATURE_H
#define BLOCKS_SS_SIGNATURE_H

#include "../../TypeSystem/struct.h"
#include "../../TypeSystem/Schema/schema.h"

namespace current {
namespace ss {

CURRENT_STRUCT(StreamNamespaceName) {
  CURRENT_FIELD(namespace_name, std::string);
  CURRENT_FIELD(entry_name, std::string);
  CURRENT_DEFAULT_CONSTRUCTOR(StreamNamespaceName) {}
  CURRENT_CONSTRUCTOR(StreamNamespaceName)(const std::string& namespace_name, const std::string& entry_name)
      : namespace_name(namespace_name), entry_name(entry_name) {}
};

CURRENT_STRUCT(StreamSignature, StreamNamespaceName) {
  CURRENT_FIELD(schema, reflection::SchemaInfo);
  CURRENT_DEFAULT_CONSTRUCTOR(StreamSignature) {}
  CURRENT_CONSTRUCTOR(StreamSignature)(
      const std::string& namespace_name, const std::string& entry_name, const reflection::SchemaInfo& schema)
      : SUPER(namespace_name, entry_name), schema(schema) {}
  CURRENT_CONSTRUCTOR(StreamSignature)(const StreamNamespaceName& namespace_name, const reflection::SchemaInfo& schema)
      : SUPER(namespace_name), schema(std::move(schema)) {}
};

}  // namespace ss
}  // namespace current

#endif  // BLOCKS_SS_SIGNATURE_H
