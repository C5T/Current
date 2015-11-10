/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// This file is used for mass registering of primitives type handlers in reflection and serialization routines.
// Typical usecase:
//    #define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type) ...
//    #include "primitive_types.dsl.h"
//    #undef CURRENT_DECLARE_PRIMITIVE_TYPE

#ifdef CURRENT_DECLARE_PRIMITIVE_TYPE

// Integral types.
CURRENT_DECLARE_PRIMITIVE_TYPE(1, bool, Bool)
CURRENT_DECLARE_PRIMITIVE_TYPE(10, char, Char)
CURRENT_DECLARE_PRIMITIVE_TYPE(11, uint8_t, UInt8)
CURRENT_DECLARE_PRIMITIVE_TYPE(12, uint16_t, UInt16)
CURRENT_DECLARE_PRIMITIVE_TYPE(13, uint32_t, UInt32)
CURRENT_DECLARE_PRIMITIVE_TYPE(14, uint64_t, UInt64)
CURRENT_DECLARE_PRIMITIVE_TYPE(21, int8_t, Int8)
CURRENT_DECLARE_PRIMITIVE_TYPE(22, int16_t, Int16)
CURRENT_DECLARE_PRIMITIVE_TYPE(23, int32_t, Int32)
CURRENT_DECLARE_PRIMITIVE_TYPE(24, int64_t, Int64)

// Floating point types.
CURRENT_DECLARE_PRIMITIVE_TYPE(31, float, Float)
CURRENT_DECLARE_PRIMITIVE_TYPE(32, double, Double)

// Other primitive types.
CURRENT_DECLARE_PRIMITIVE_TYPE(101, std::string, String)

#endif
