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
//   #define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type) ...
//   #include "primitive_types.dsl.h"
//   #undef CURRENT_DECLARE_PRIMITIVE_TYPE

#ifdef CURRENT_DECLARE_PRIMITIVE_TYPE  // To pass `make check`.

CURRENT_DECLARE_PRIMITIVE_TYPE(11, bool, Bool, "bool", "`true` or `false`")

CURRENT_DECLARE_PRIMITIVE_TYPE(21, uint8_t, UInt8, "byte", "Integer (8-bit unsigned)")
CURRENT_DECLARE_PRIMITIVE_TYPE(22, uint16_t, UInt16, "uint16", "Integer (16-bit unsigned)")
CURRENT_DECLARE_PRIMITIVE_TYPE(23, uint32_t, UInt32, "uint32", "Integer (32-bit unsigned)")
CURRENT_DECLARE_PRIMITIVE_TYPE(24, uint64_t, UInt64, "uint64", "Integer (64-bit unsigned)")

CURRENT_DECLARE_PRIMITIVE_TYPE(31, int8_t, Int8, "sbyte", "Integer (8-bit signed)")
CURRENT_DECLARE_PRIMITIVE_TYPE(32, int16_t, Int16, "int16", "Integer (16-bit signed)")
CURRENT_DECLARE_PRIMITIVE_TYPE(33, int32_t, Int32, "int32", "Integer (32-bit signed)")
CURRENT_DECLARE_PRIMITIVE_TYPE(34, int64_t, Int64, "int64", "Integer (64-bit signed)")

CURRENT_DECLARE_PRIMITIVE_TYPE(41, char, Char, "char", "Character")  // Although F# chars are Unicode.
CURRENT_DECLARE_PRIMITIVE_TYPE(42, std::string, String, "string", "String")

CURRENT_DECLARE_PRIMITIVE_TYPE(51, float, Float, "float", "Number (floating point, single precision)")
CURRENT_DECLARE_PRIMITIVE_TYPE(52, double, Double, "double", "Number (floating point, double precision)")

CURRENT_DECLARE_PRIMITIVE_TYPE(
    61, std::chrono::microseconds, Microseconds, "int64  // microseconds.", "Time (microseconds since epoch)")
CURRENT_DECLARE_PRIMITIVE_TYPE(
    62, std::chrono::milliseconds, Milliseconds, "int64  // milliseconds.", "Time (milliseconds since epoch)")

#endif
