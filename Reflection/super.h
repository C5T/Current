#ifndef CURRENT_REFLECTION_SUPER_H
#define CURRENT_REFLECTION_SUPER_H

namespace current {
namespace reflection {

// The superclass for all Current-defined types, to enable polymorphic serialization and deserialization.
struct CurrentSuper {
  virtual ~CurrentSuper() = default;
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

#endif  // CURRENT_REFLECTION_SUPER_H
