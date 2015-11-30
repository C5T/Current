#ifdef SMOKE_TEST_STRUCT_NAMESPACE

namespace SMOKE_TEST_STRUCT_NAMESPACE {

CURRENT_STRUCT(Primitives) {
  CURRENT_FIELD(a, uint8_t);
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
  CURRENT_FIELD(n, std::chrono::microseconds);
  CURRENT_FIELD(o, std::chrono::milliseconds);
};

CURRENT_STRUCT(A) { CURRENT_FIELD(a, int32_t); };

CURRENT_STRUCT(B, A) { CURRENT_FIELD(b, int32_t); };

CURRENT_STRUCT(X) { CURRENT_FIELD(x, int32_t); };

CURRENT_STRUCT(Y) { CURRENT_FIELD(x, int32_t); };

CURRENT_STRUCT(C) {
  CURRENT_FIELD(c, (OptionalPolymorphic<X, Y>));
  CURRENT_DEFAULT_CONSTRUCTOR(C) {}
  CURRENT_CONSTRUCTOR(C)(Polymorphic<X, Y> && c) : c(std::move(c)) {}
};

CURRENT_STRUCT(FullTest) {
  CURRENT_FIELD(primitives, Primitives);
  CURRENT_FIELD(v1, std::vector<std::string>);
  CURRENT_FIELD(v2, std::vector<Primitives>);
  CURRENT_FIELD(p, (std::pair<std::string, Primitives>));
  CURRENT_FIELD(o, Optional<Primitives>);
  CURRENT_FIELD(q, (Polymorphic<A, B, C>));
  CURRENT_FIELD(r, (OptionalPolymorphic<A, B, C>));
  CURRENT_CONSTRUCTOR(FullTest)(Polymorphic<A, B, C> && q, OptionalPolymorphic<A, B, C> && r)
      : q(std::move(q)), r(std::move(r)) {}
};

}  // namespace

#endif
