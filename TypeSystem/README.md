# Current Framework Type System

**Current** framework uses its own type system to define data structures and schemas. The main building block is structure, which natively supports reflection and serialization into binary and JSON formats.

## Structures
 
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
Internally, Current structures are polymorphic C++ `struct`'s, derived from the special class `CurrentSuper`. Thus, they can contain methods implementing some user functionality:
```cpp
CURRENT_STRUCT(WithVector) {
  CURRENT_FIELD(v, std::vector<std::string>);
  size_t vector_size() const { return v.size(); }
};
```

Structure inheritance is strictly linear, no diamond-shape inheritance is allowed.

### Structure fields
Each field inside the structure **must** be declared via the following macro:
```cpp
CURRENT_FIELD(FieldName, FieldType[, DefaultValue]);
```
**Important notes**: 
* pure C++ declaration of member variables is **not allowed** for Current structures;
* if field type contains commas, it should be put into parentheses:
```cpp
CURRENT_FIELD(m, (std::map<uint32_t, std::string>));
```

#### Supported types
* `bool`
* fixed-size integral
  * `int8_t` / `uint8_t`
  * `int16_t` / `uint16_t`
  * `int32_t` / `uint32_t`
  * `int64_t` / `uint64_t`
* IEC-559 compliant floating point
  * `float` (32-bit)
  * `double` (64-bit)
* STL containers
  * `std::pair`
  * `std::map`
  * `std::vector`
* `std::string`
* `std::chrono::milliseconds/microseconds`
* `enum class`-es declared via `CURRENT_ENUM`:
```cpp
CURRENT_ENUM(Fruits, uint32_t) {APPLE = 1u, ORANGE = 2u};
``` 

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

### Using structures in event log
If the structure is supposed to be stored in the event log, it should contain a dedicated field for microsecond timestamp and notify Current via a `CURRENT_TIMESTAMP` macro:
```cpp
CURRENT_STRUCT(TimestampedEvent) {
  CURRENT_FIELD(EpochMicroseconds, std::chrono::microseconds);
  CURRENT_TIMESTAMP(EpochMicroseconds);
};
```

## `Optional` type

## `Polymorphic` and `OptionalPolymorphic` types
