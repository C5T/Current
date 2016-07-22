/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_RAPIDJSON_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_RAPIDJSON_H

// Keep all RapidJSON includes here, to make sure the right macros are defined. -- D.K.

#include "exceptions_base.h"

inline void RapidJSONAssert(bool condition, const char* text, const char* file, int line) {
  if (!condition) {
    current::serialization::json::RapidJSONAssertionFailedException e(text);
    e.SetCaller(text);
    e.SetOrigin(file, line);
    throw e;
  }
}

#define RAPIDJSON_HAS_STDSTRING 1
#define RAPIDJSON_ASSERT(x) RapidJSONAssert(x, #x, __FILE__, __LINE__)

#include "../../3rdparty/rapidjson/document.h"
#include "../../3rdparty/rapidjson/prettywriter.h"
#include "../../3rdparty/rapidjson/streamwrapper.h"

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_RAPIDJSON_H
