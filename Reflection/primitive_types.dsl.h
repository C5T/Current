// This file is used for mass registering of primitives type handlers in reflection and serialization routines.
// Typical usecase:
//    #define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type) \
//    ...
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
