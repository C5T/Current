#ifdef SMOKE_TEST_TEMPLATES_NAMESPACE

namespace SMOKE_TEST_TEMPLATES_NAMESPACE {

// clang-format off

CURRENT_STRUCT(Integer) {
  CURRENT_FIELD(i, int32_t, 0);
};

CURRENT_STRUCT_T(Templated) {
  CURRENT_FIELD(x, T);
};

namespace no_template {
CURRENT_STRUCT(FakeTemplatedInteger) {
  CURRENT_EXPORTED_TEMPLATED_STRUCT(Templated, Integer);
  CURRENT_FIELD(x, Integer);
};
}  // namespace SMOKE_TEST_TEMPLATES_NAMESPACE::no_template

}  // namespace SMOKE_TEST_TEMPLATES_NAMESPACE

// clang-format on

#endif
