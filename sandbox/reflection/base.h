#ifndef BRICKS_REFLECTION_BASE_H
#define BRICKS_REFLECTION_BASE_H

namespace bricks {
namespace reflection {

struct CurrentBaseType {
  virtual ~CurrentBaseType() = default;
};

// Instantiation types.
struct DeclareFields {};
struct CountFields {};

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

// `CURRENT_REFLECTION()` for `Index<FieldType, n>` calls `f(TypeWrapper<real_field_type>)`.
template <typename T>
struct TypeWrapper {};

}  // namespace reflection
}  // namespace bricks

#endif  // BRICKS_REFLECTION_BASE_H
