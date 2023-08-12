#ifdef SMOKE_TEST_STRUCT_NAMESPACE

namespace SMOKE_TEST_STRUCT_NAMESPACE {

// clang-format off

CURRENT_STRUCT(Primitives) {
  CURRENT_FIELD(a, uint8_t, (static_cast<uint8_t>(1) << 7));
  CURRENT_FIELD_DESCRIPTION(a, "It's the \"order\" of fields that matters.");
  CURRENT_FIELD(b, uint16_t, (static_cast<uint16_t>(1) << 15));
  CURRENT_FIELD(c, uint32_t, (static_cast<uint32_t>(1) << 31));
  // uint64_t default value less than 53 bits to fit into JavaScript double: http://2ality.com/2012/04/number-encoding.html
  CURRENT_FIELD(d, uint64_t, (static_cast<uint64_t>(1) << 52));
  CURRENT_FIELD(e, int8_t, (static_cast<int8_t>(1) << 6));
  CURRENT_FIELD(f, int16_t, (static_cast<int16_t>(1) << 14));
  CURRENT_FIELD(g, int32_t, (static_cast<int32_t>(1) << 30));
  // int64_t default value less than 53 bits to fit into JavaScript double: http://2ality.com/2012/04/number-encoding.html
  CURRENT_FIELD(h, int64_t, (static_cast<int64_t>(1) << 52));
  CURRENT_FIELD(i, char, 'A');
  CURRENT_FIELD(j, std::string, "ABC");
  CURRENT_FIELD(k, float, (static_cast<float>(3.141590)));
  CURRENT_FIELD(l, double, (static_cast<double>(0.314159e-10)));
  CURRENT_FIELD(m, bool, true);
  CURRENT_FIELD_DESCRIPTION(m, "Multiline\ndescriptions\ncan be used.");
  CURRENT_FIELD(n, std::chrono::microseconds, std::chrono::microseconds(1499684174969000LL));
  CURRENT_FIELD(o, std::chrono::milliseconds, std::chrono::milliseconds(1499684174969LL));
  CURRENT_FIELD_DESCRIPTION(b, "Field descriptions can be set in any order.");
};

CURRENT_ENUM(E, uint16_t) { SOME, DUMMY, VALUES };

CURRENT_STRUCT(A) {
  CURRENT_FIELD(a, int32_t, (static_cast<int32_t>(1) << 30));
};

CURRENT_STRUCT(B, A) {
  CURRENT_FIELD(b, int32_t, (static_cast<int32_t>(1) << 30));
};

CURRENT_STRUCT(B2, A) {};  // Should still have field `a` from the base class `A` in it.

CURRENT_STRUCT(X) {
  CURRENT_FIELD(x, int32_t, (static_cast<int32_t>(1) << 30));
};

CURRENT_STRUCT(Y) {
  CURRENT_FIELD(e, E);
};

CURRENT_STRUCT(Empty) {
};

CURRENT_VARIANT(MyFreakingVariant, A, X, Y);

CURRENT_STRUCT(C) {
  CURRENT_FIELD(e, Empty);
  CURRENT_FIELD(c, MyFreakingVariant);
  CURRENT_FIELD(d, (Variant<A, X, Y>));  // Same type, test generating code and parsing it respects type ID.
  CURRENT_DEFAULT_CONSTRUCTOR(C) {}  // LCOV_EXCL_LINE
  CURRENT_CONSTRUCTOR(C)(Variant<A, X, Y> c) : c(c) {}  // Fully specified `Variant` name on purpose. -- D.K.
};

CURRENT_STRUCT_T(Templated) {
  CURRENT_FIELD(foo, int32_t, (static_cast<int32_t>(1) << 30));
  CURRENT_FIELD(bar, T);
};

CURRENT_STRUCT_T(TemplatedInheriting, A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, T);
};

CURRENT_STRUCT(TrickyEvolutionCases) {
  CURRENT_FIELD(o1, Optional<std::string>);
  CURRENT_FIELD(o2, Optional<int32_t>);
  CURRENT_FIELD(o3, Optional<std::vector<std::string>>);
  CURRENT_FIELD(o4, Optional<std::vector<int32_t>>);
  CURRENT_FIELD(o5, Optional<std::vector<A>>);
  CURRENT_FIELD(o6, (std::pair<std::string, Optional<A>>));
  CURRENT_FIELD(o7, (std::map<std::string, Optional<A>>));
};

CURRENT_STRUCT(FullTest) {
  CURRENT_FIELD(primitives, Primitives);
  CURRENT_FIELD_DESCRIPTION(primitives, "A structure with a lot of primitive types.");
  CURRENT_FIELD(v1, std::vector<std::string>);
  CURRENT_FIELD(v2, std::vector<Primitives>);
#if 0
  CURRENT_FIELD(a1, (std::array<int64_t, 2>));
  CURRENT_FIELD(a2, (std::array<std::pair<std::string, std::string>, 4>));
  CURRENT_FIELD(a3, (std::array<Primitives, 8>));
#endif
  CURRENT_FIELD(p, (std::pair<std::string, Primitives>));
  CURRENT_FIELD(o, Optional<Primitives>);
  CURRENT_FIELD(q, (Variant<A, B, B2, C, Empty>));
  CURRENT_FIELD_DESCRIPTION(q, "Field | descriptions | FTW !");
  CURRENT_FIELD(w1, Templated<X>);
  CURRENT_FIELD(w2, Templated<MyFreakingVariant>);
  CURRENT_FIELD(w3, Templated<TemplatedInheriting<Empty>>);
  CURRENT_FIELD(w4, TemplatedInheriting<X>);
  CURRENT_FIELD(w5, TemplatedInheriting<MyFreakingVariant>);
  CURRENT_FIELD(w6, TemplatedInheriting<Templated<Empty>>);
  CURRENT_FIELD(tsc, TrickyEvolutionCases);
  CURRENT_CONSTRUCTOR(FullTest)(Variant<A, B, B2, C, Empty> q) : q(q) {}
};

}  // namespace SMOKE_TEST_STRUCT_NAMESPACE

// clang-format on

#endif
