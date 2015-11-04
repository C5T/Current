#ifndef CURRENT_REFLECTION_BASE_H
#define CURRENT_REFLECTION_BASE_H

namespace current {
namespace reflection {

struct CurrentBaseType {
  virtual ~CurrentBaseType() = default;
};

// Instantiation types.
struct DeclareFields {};
struct CountFields {};
typedef long long CountFieldsImplementationType;

// Helper structs for reflection.
class FieldType {};
class FieldName {};
class FieldTypeAndName {};
class FieldValue {};
class FieldNameAndImmutableValueReference {};
class FieldNameAndMutableValueReference {};

// Complex index: <HelperStruct, int Index>.
template <class T, int N>
struct Index {};

// `CURRENT_REFLECTION()` for `Index<FieldType, n>` calls `f(TypeSelector<real_field_type>)`.
template <typename T>
struct TypeSelector {};

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_REFLECTION_BASE_H
