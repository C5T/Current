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

namespace current {

template <bool STRIPPED, bool REQUIRED, typename TYPELIST, typename ORIGINAL_TYPELIST>
struct GenericPolymorphicImpl;

template <typename T>
struct IS_POLYMORPHIC {
  enum { value = false };
};

template <bool STRIPPED, bool REQUIRED, typename TYPELIST, typename ORIGINAL_TYPELIST>
struct IS_POLYMORPHIC<GenericPolymorphicImpl<STRIPPED, REQUIRED, TYPELIST, ORIGINAL_TYPELIST>> {
  enum { value = true };
};

struct ForceDefaultConstructionDespiteDeletedConstructor {};

namespace reflection {

// Instantiation types.
struct DeclareFields {};
struct DeclareStrippedFields {};
struct CountFields {};

}  // namespace reflection

// The superclass for all Current-defined types, to enable polymorphic serialization and deserialization.
struct CurrentSuper {
  virtual ~CurrentSuper() = default;
};

#define IS_CURRENT_STRUCT(T) (std::is_base_of<::current::CurrentSuper, T>::value)

namespace reflection {

// Dummy type for `CountFields` instantiation type.
struct CountFieldsImplementationType {
  template <typename... T>
  CountFieldsImplementationType(T&&...) {}
  long long x;
};

// Helper structs for reflection.
class FieldTypeAndName {};
class FieldNameAndImmutableValue {};
class FieldNameAndMutableValue {};

// Complex index: <HelperStruct, int Index>.
template <class T, int N>
struct Index {};

// `CURRENT_REFLECTION()` for `Index<FieldType, n>` calls `f(TypeSelector<real_field_type>)`.
template <typename T>
struct TypeSelector {};

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_BASE_H
