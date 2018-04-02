#define NLPSchema(schema_name, annotated_query_term_type)                                    \
  namespace schema_name##_schema {                                                           \
    CURRENT_FORWARD_DECLARE_STRUCT(annotated_query_term_type);                               \
    using annotated_query_term_t = annotated_query_term_type;                                \
    using ::current::nlp::AnnotatedQueryTerm;                                                \
    using ::current::nlp::Unit;                                                              \
    struct none_schema_block_impl final {                                                    \
      using emitted_t = Unit;                                                                \
      void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>&,           \
                    size_t begin,                                                            \
                    size_t end,                                                              \
                    std::function<void(const Unit&)> emit) const {                           \
        if (end == begin) {                                                                  \
          emit(Unit());                                                                      \
        }                                                                                    \
      }                                                                                      \
    };                                                                                       \
    static none_schema_block_impl none_schema_block_impl_instance;                           \
    static ::current::nlp::SchemaBlock<annotated_query_term_t, none_schema_block_impl> None( \
        "None", none_schema_block_impl_instance);                                            \
  }                                                                                          \
  namespace schema_name##_schema

#define CustomAnnotation(...)                                                                                 \
  struct custom_annotator_initializer final {                                                                 \
    custom_annotator_initializer() {                                                                          \
      ::current::Singleton<::current::nlp::StaticQueryTermAnnotators<annotated_query_term_t>>().SetAnnotator( \
          [](const std::string& original_term, annotated_query_term_t& output) {                              \
            static_cast<void>(original_term);                                                                 \
            static_cast<void>(output);                                                                        \
            { __VA_ARGS__; }                                                                                  \
          });                                                                                                 \
    }                                                                                                         \
  };                                                                                                          \
  static custom_annotator_initializer custom_annotator_initializer_instance

#define DictionaryAnnotation(field_name, ...)                                                                 \
  struct field_name##_values_initializer final {                                                              \
    using optional_field_t = decltype(std::declval<annotated_query_term_t>().field_name);                     \
    using field_t = typename optional_field_t::optional_underlying_t;                                         \
    const std::unordered_map<std::string, field_t> field_name##_values{__VA_ARGS__};                          \
    field_name##_values_initializer() {                                                                       \
      for (const auto& e : field_name##_values) {                                                             \
        const auto& key = e.first;                                                                            \
        const auto& value = e.second;                                                                         \
        ::current::Singleton<::current::nlp::StaticQueryTermAnnotators<annotated_query_term_t>>().Add(        \
            key, [&value](annotated_query_term_t& annotated) { annotated.field_name = value; });              \
      }                                                                                                       \
    }                                                                                                         \
  };                                                                                                          \
  struct field_name##_schema_block_impl final {                                                               \
    using emitted_t = typename field_name##_values_initializer::field_t;                                      \
    void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,                        \
                  size_t begin,                                                                               \
                  size_t end,                                                                                 \
                  std::function<void(const typename field_name##_values_initializer::field_t&)> emit) const { \
      if (end == begin + 1u && Exists(query.annotated_terms[begin].field_name)) {                             \
        emit(Value(query.annotated_terms[begin].field_name));                                                 \
      }                                                                                                       \
    }                                                                                                         \
  };                                                                                                          \
  static field_name##_schema_block_impl field_name##_schema_block_impl_instance;                              \
  static field_name##_values_initializer field_name##_values_initializer_instance;                            \
  static ::current::nlp::SchemaBlock<annotated_query_term_t, field_name##_schema_block_impl> field_name(      \
      #field_name, field_name##_schema_block_impl_instance)

#define Keyword(keyword)                                                                           \
  struct keyword##_schema_block_impl final {                                                       \
    using emitted_t = Unit;                                                                        \
    void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,             \
                  size_t begin,                                                                    \
                  size_t end,                                                                      \
                  std::function<void(const Unit&)> emit) const {                                   \
      if (end == begin + 1u && query.annotated_terms[begin].normalized_term == #keyword) {         \
        emit(Unit());                                                                              \
      }                                                                                            \
    }                                                                                              \
  };                                                                                               \
  static keyword##_schema_block_impl keyword##_schema_block_impl_instance;                         \
  static ::current::nlp::SchemaBlock<annotated_query_term_t, keyword##_schema_block_impl> keyword( \
      #keyword, keyword##_schema_block_impl_instance)

#define Term(name, value)                              \
  static const auto name = (value);                    \
  struct name##_name_injector final {                  \
    name##_name_injector() { name.InjectName(#name); } \
  };                                                   \
  static const name##_name_injector name##_injector_impl

#define Void(term) ((term).UnitHelper())

#define As(term, output_type)              \
  ((term).template MapHelper<output_type>( \
      [](const typename decltype(term)::emitted_t& input, output_type& output) { output = input; }))

#define Map(term, output_type, ...)                                                                                  \
  ((term).template MapHelper<output_type>([](const typename decltype(term)::emitted_t& input, output_type& output) { \
    static_cast<void>(input);                                                                                        \
    __VA_ARGS__;                                                                                                     \
  }))

#define Filter(term, ...) \
  ((term).FilterHelper([](const typename decltype(term)::emitted_t& input) -> bool { return __VA_ARGS__; }))

#define Maybe(term) (None | (term))
#define GreedyMaybe(term) ((term) | None)

#define Input(i) std::get<i>(input)
