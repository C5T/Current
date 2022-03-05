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

#ifndef CURRENT_TYPE_SYSTEM_BASE_H
#define CURRENT_TYPE_SYSTEM_BASE_H

namespace crnt {
namespace r {

// Instantiation types.
struct DF {};
struct FC {};

// Special type used as `T` in templated Current structs instantiation along with `FC`.
struct DummyT {};

// Dummy type for `FC` instantiation type.
struct CountFieldsImplementationType {
  template <typename... T>
  CountFieldsImplementationType(T&&...) {}
  long long x[100];  // TODO(dkorolev): Fix JSON parse/serialize on Windows. Gotta go deeper. -- D.K.
};

// Helper structs for reflection.
class FieldType {};
class FieldTypeAndName {};
class FieldTypeAndNameAndIndex {};

template <typename SELF>
class FieldNameAndPtr {};

class FieldNameAndImmutableValue {};
class FieldNameAndMutableValue {};

// Simple index. TODO(dkorolev): Retire the complex `Index` below, it may speed up compilation.
template <int N>
struct SimpleIndex {};

// Complex index: <HelperStruct, int Index>.
template <class T, int N>
struct Index {};

// `CURRENT_REFLECTION()` for `Index<FieldType, n>` calls `f(TypeSelector<real_field_type>)`.
template <typename T>
struct TypeSelector {};

}  // namespace r
}  // namespace crnt

namespace current {
namespace reflection {
using DeclareFields = ::crnt::r::DF;
using CountFields = ::crnt::r::FC;
using DummyTemplateType = ::crnt::r::DummyT;
using ::crnt::r::CountFieldsImplementationType;
using ::crnt::r::FieldNameAndImmutableValue;
using ::crnt::r::FieldNameAndMutableValue;
using ::crnt::r::FieldNameAndPtr;
using ::crnt::r::FieldTypeAndName;
using ::crnt::r::FieldTypeAndNameAndIndex;
using ::crnt::r::Index;
using ::crnt::r::SimpleIndex;
using ::crnt::r::TypeSelector;
}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_BASE_H
