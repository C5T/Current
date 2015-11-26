/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_CEREALIZE_CONSTANTS_H
#define BRICKS_CEREALIZE_CONSTANTS_H

#include <string>

namespace current {
namespace cerealize {

struct Constants {
  // By default, when sealizing as JSON, use this top-level name instead of Cereal's default "value0".
  static const char* DefaultJSONEntryName() { return "data"; }
  // When using JSON serialization into (file) streams, use these shorter names instead.
  static const char* DefaultJSONSerializeNonPolymorphicEntryName() { return "e"; }
  static const char* DefaultJSONSerializePolymorphicEntryName() { return "p"; }
};

}  // namespace cerealize
}  // namespace current

#endif  // BRICKS_CEREALIZE_CONSTANTS_H
