#ifdef SMOKE_TEST_STRUCT_NAMESPACE

namespace SMOKE_TEST_STRUCT_NAMESPACE {

// clang-format off

CURRENT_STRUCT(Primitives) {
  CURRENT_FIELD(a, uint8_t);
  CURRENT_FIELD_DESCRIPTION(a, "It's the \"order\" of fields that matters.");
  CURRENT_FIELD(b, uint16_t);
  CURRENT_FIELD(c, uint32_t);
  CURRENT_FIELD(d, uint64_t);
  CURRENT_FIELD(e, int8_t);
  CURRENT_FIELD(f, int16_t);
  CURRENT_FIELD(g, int32_t);
  CURRENT_FIELD(h, int64_t);
  CURRENT_FIELD(i, char);
  CURRENT_FIELD(j, std::string);
  CURRENT_FIELD(k, float);
  CURRENT_FIELD(l, double);
  CURRENT_FIELD(m, bool);
  CURRENT_FIELD_DESCRIPTION(m, "Multiline\ndescriptions\ncan be used.");
  CURRENT_FIELD(n, std::chrono::microseconds);
  CURRENT_FIELD(o, std::chrono::milliseconds);
  CURRENT_FIELD_DESCRIPTION(b, "Field descriptions can be set in any order.");
};

CURRENT_ENUM(E, uint16_t) { SOME, DUMMY, VALUES };

CURRENT_STRUCT(A) {
  CURRENT_FIELD(a, int32_t);
};

CURRENT_STRUCT(B, A) {
  CURRENT_FIELD(b, int32_t);
};

CURRENT_STRUCT(B2, A) {};  // Should still have field `a` from the base class `A` in it.

CURRENT_STRUCT(X) {
  CURRENT_FIELD(x, int32_t);
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
  CURRENT_DEFAULT_CONSTRUCTOR(C) {}  // LCOV_EXCL_LINE
  CURRENT_CONSTRUCTOR(C)(Variant<A, X, Y> c) : c(c) {}  // Fully specified `Variant` name on purpose. -- D.K.
};

CURRENT_STRUCT_T(Templated) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, T);
};

CURRENT_STRUCT_T(TemplatedInheriting, A) {
  CURRENT_FIELD(baz, std::string);
  CURRENT_FIELD(meh, T);
};

CURRENT_STRUCT(FullTest) {
  CURRENT_FIELD(primitives, Primitives);
  CURRENT_FIELD_DESCRIPTION(primitives, "A structure with a lot of primitive types.");
  CURRENT_FIELD(v1, std::vector<std::string>);
  CURRENT_FIELD(v2, std::vector<Primitives>);
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
  CURRENT_CONSTRUCTOR(FullTest)(Variant<A, B, B2, C, Empty> q) : q(q) {}
};

}  // namespace SMOKE_TEST_STRUCT_NAMESPACE

// clang-format on

#endif
