# Current Type System

In Current, we extended the C++ type system to suit our needs.

## Structures

The main building block is the `struct` that natively supports reflection with strong type identification.

Current structures can be declared using a special set of macros.

```cpp
// Base event.
CURRENT_STRUCT(BaseEvent) {
  CURRENT_FIELD(event_id, uint32_t);
};
// `AddUser` event, derived from `BaseEvent`.
CURRENT_STRUCT(AddUser, BaseEvent) {
  CURRENT_FIELD(user, std::string);
};
```
Internally, Current structures are polymorphic C++ `struct`-s, derived from the special class `current::CurrentSuper`. Thus, they can contain methods implementing some user functionality:
```cpp
CURRENT_STRUCT(WithVector) {
  CURRENT_FIELD(v, std::vector<std::string>);
  size_t vector_size() const { return v.size(); }
};
```

The inheritance of `CURRENT_STRUCT`-s is strictly linear, no diamond shape is allowed.

### Fields
Each field inside the structure **must** be declared via the following macro:
```cpp
CURRENT_FIELD(FieldName, FieldType[, DefaultValue]);
```
**Important notes**: 
* pure C++ declaration of member variables is **not allowed** for Current structures;
* if the field type contains commas, it should be put into parentheses:
```cpp
CURRENT_FIELD(m, (std::map<uint32_t, std::string>));
```

#### Types

Fields can be of basic type:

* `bool`
* fixed-size integral
  * `int8_t` / `uint8_t`
  * `int16_t` / `uint16_t`
  * `int32_t` / `uint32_t`
  * `int64_t` / `uint64_t`
* `std::string`
* `std::chrono::milliseconds/microseconds`
* IEC-559 compliant floating point
  * `float` (32-bit)
  * `double` (64-bit)
* STL containers
  * `std::map`
  * `std::vector`
  * `std::pair`

Or of a more sophisticated type, which is one of the below.

#### Enums
To be used in `CURRENT_STRUCT`-s, enumerated types must be declared via the `CURRENT_ENUM(EnumName, UnderlyingType)` macro:
```cpp
CURRENT_ENUM(Fruits, uint32_t) {APPLE = 1u, ORANGE = 2u};
```
This macro de-facto declares a `enum class` type and registers it with the type system.

#### `Optional` and `Variant` types.

Along with `CURRENT_STRUCT`-s, optional (`Optional<T>`) and variant (`Variant<TS...>`) types are first-class citizens in Current type system. More on them below.

### Constructors
There are two special macros to define default and parametrized constructors:
```cpp
CURRENT_DEFAULT_CONSTRUCTOR(StructName) { ... }
CURRENT_CONSTRUCTOR(StructName)(ARGS... args) { ... }
```
Example:
```cpp
CURRENT_STRUCT(Foo) {
  CURRENT_FIELD(i, uint64_t);
  CURRENT_DEFAULT_CONSTRUCTOR() : i(42u) {}
  CURRENT_CONSTRUCTOR(Foo)(uint64_t i) : i(i) {}
};
```
 
### Templates

Along with `CURRENT_STRUCT`, Current type system supports `CURRENT_STRUCT_T`: effectively a `template<typename T> struct ...`. The `CURRENT_CONSTRUCTOR_T` and `CURRENT_DEFAULT_CONSTRUCTOR_T` macros should be used instead of the non-`_T` ones.

### Field Descriptions

When a `CURRENT_STRUCT` defined a `CURRENT_FIELD(foo, ...);`, it may also define `CURRENT_FIELD_DESCRIPTION(foo, "This is the story of foo.");` Like fields, field descriptions can be reflected upon. Field descriptions can also be exported in various formats, most notably C++ itself (as the definition of the C++ struct), and [Markdown](https://github.com/C5T/Current/blob/stable/typesystem/schema/golden/smoke_test_struct.md).

## `Optional`

A special type `Optional<T>` is used to define `T` or `null`. C++ usage of `Optional<T> x;` is:
* an `if (Exists(x))` condition, and
* a `T y = Value(x)` retrieval.

This syntax returns an xvalue, which can be captured into a reference, assigned to, etc.

## `Variant`

For polymorphic types, `Variant<TS...>` can be used. C++ usage of `Variant<A, B> x` is:
* an `if (Exists<A>(x))` condition,
* a `A a = Value<A>(x)` retrieval, or
* the `x.Call(visitor)` pattern, where `visitor` is an instance of a struct/class that can be `operator()`-called for any of the underlying types.
 
The former two syntaxes return an xvalue, which can be captured into a reference, assigned to, etc.

The latter syntax is recommended for implementing per-type dispatching, as it's a) more efficient, and b) ensures the compile-type guarantee no inner type is left out.

# Serialization

One objective behind extending the type system has been to enable JSON serialization of C++ objects. For the full story, we've been unhappy with the way [Cereal](http://uscilab.github.io/cereal/)'s deals with polymorphic objects, both on the C++ side (not friendly with header-only) and on the resulting JSON side (not well-suited for RESTful API formats).

All `CURRENT_STRUCT`-s, as well as STL containers containing them, and as well as top-level `Variant` types, are fully serializable to and from JSON.

(Binary serialization is also there, but it's incomplete and not battle-tested as of now. -- D.K. & M.Z.)

The default syntax for JSON serialization is `T object; json = JSON(object)` and `auto object = ParseJSON<T>(json)`.

### Minimalistic Format

Note that for default serialization of `Variant<TS...>` types the resulting JSON would be a JSON object contain the Type ID of the serialized case of a variant, under a key of an empty string. For default deserialization, per-type dispatching is done based on the Type ID which is expected to be found in the empty-string key.

This is to enable distinguishing between two different types declared under the same name in different namespaces, as well as to eliminate the possibility of deserializing certain `Variant` case into a modified version of this type, with different set of fields.

If this behavior is undesired, and the user assumes responsibility for unique type naming (and knows what they are doing with respect to modifying fields, such as adding/removing them or changing their types), `JSON<JSONFormat::Minimalistic>(object)` and `ParseJSON<T, JSONFormat::Minimalistic>` can be used.

Generally, for a `Variant<A, B>`, where `A` is `CURRENT_STRUCT(A) { CURRENT_FIELD(x, int32_t); }` and `B` is `CURRENT_STRUCT(B) { CURRENT_FIELD(y, int32_t); }`, the `JSONFormat::Minimalistic` serialization would be:

```
{
  "a": {
    "x": 42
  }
}
```

or:

```
{
  "b": {
    "y": 42
  }
}
```

The non-minimalistic version would be:
```
{
  "": "T243029837452345",
  "a": {
    "x": 42
  }
}
```

or:

```
{
  "": "T793029837452339",
  "b": {
    "y": 42
  }
}
```

(Type ID-s not actual. -- D.K.)

Footnotes:
* `ParseJSON<T, JSONFormat::Minimalistic>(json)` can accept the JSONs created by a "full" `json = JSON(object)`.
* Minimalistic format skips `null` `Optional` fields in the output. The full format explicitly dumps `null`-s. When parsing a JSON, both formats treat `null` and missing key equally for `Optional` fields.

# Code

For the above examples to work, run `git clone https://github.com/C5T/Current` and `#include "Current/current.h"`.

To save on compilation time, consider fine-graining your headers, ex. `#include "Current/typesystem/struct.h"` for just the `CURRENT_STRUCT`.

The master branch of code is [C5T/Current/TypeSystem](https://github.com/C5T/Current/tree/master/TypeSystem), and its [unit test](https://github.com/C5T/Current/blob/stable/typesystem/test.cc) contains plenty of usage examples.
