/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2018 Maxim Zhurovich <zhurovich@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

// Current "NLP" module provides a convenient way to work with natural language queries, leveraging strong typing and
// compile-time validation. It allows user to define the strict set of types and rules ("schema") that can be applied
// to the query string to perform matching and computations, yielding the results of predetermined types.
//
// The query processing pipeline consists of the following steps:
//
// 1) Marking.
//
//    Stateless process that tokenizes the query into terms, normalizes each term (`ToLower()`), and performs basic
//    annotation via simple matching. N-grams and regular expression matches also belong to this phase.
//
// 2) Annotation.
//
//    On this stage various predicates are computed for the set of query terms.
//
//    For example, in case of numerical evaluations, parentheses are extracted as separate tokens during the marking
//    phase. Further matching them with one another and computing partial balances for further checks of whether
//    certain query range contains a parenthesis is done in the annotation phase.
//
// 3) Matching.
//
//    This is the stage where the schema is applied to the marked and annotated query.
//
//    The schema consists of blocks. Each block has its own `emitted_t` type, which is the type of object it can yield
//    while being applied to a [sub]query. I.e. any schema block can be applied to any [sub]query, yielding zero, one
//    or more objects of the `emitted_t` type.
//
//    Schema blocks can be combined using operators, such as `a >> b` ('a' followed by 'b') or `a | b` (logical or).
//    There's a special type, called `Unit`, which is used to indicate a block that simply performs the "yes/no" check,
//    without carrying any data over.
//
// On the implementation level, each schema is defined in its own C++ namespace.
// Current NLP uses preprocessor macros to keep schema definitions concise. Though it's possible to make macro
// names unique and not pollute "macro namespace", it would hurt schema readability. Thus, the Current
// implementation uses a set of two `.inl' files that should be included before and after schema definition -
// first of them defines all the required Current NLP macro keywords, and the second one "#undef"s them:
//
//   #include "nlp_schema_begin.inl"
//   NLPSchema(MySchema, CustomAnnotation) {
//     ...
//   }
//   #include "nlp_schema_end.inl"
//
//   See "test.cc" for the examples of usage.

#ifndef NLP_NLP_H
#define NLP_NLP_H

#include "../bricks/strings/split.h"
#include "../bricks/strings/util.h"
#include "../bricks/template/tuple.h"
#include "../typesystem/struct.h"

#include <functional>

namespace current {
namespace nlp {

// `AnnotatedQueryTerm` is the base class for term annotation with the minimally required functionality.
// All the user-defined annotators should derive from this class.
CURRENT_STRUCT(AnnotatedQueryTerm) {
  CURRENT_FIELD(normalized_term, std::string);
  CURRENT_FIELD_DESCRIPTION(normalized_term, "The normalized individual query term.");

  CURRENT_FIELD(original_term, std::string);
  CURRENT_FIELD_DESCRIPTION(original_term, "This query term before normalization.");
};

// Template class `AnnotatedQuery` is a central part for all schema evaluations.
// Template parameter `T` is defined in schema definition by specifying the second argument of the `NLPSchema`.
CURRENT_STRUCT_T(AnnotatedQuery) {
  CURRENT_FIELD(original_query, std::string);
  CURRENT_FIELD_DESCRIPTION(original_query, "The original query; UTF8 if not ASCII.");

  CURRENT_FIELD(annotated_terms, std::vector<T>);
  CURRENT_FIELD_DESCRIPTION(annotated_terms,
                            "The vector of annotated query terms. `T` defaults to `AnnotatedQueryTerm`.");
};

// `Unit` is a special type that represents simple "yes/no" check in the NLP schema.
// It is used for the blocks the result of which is not important, such as keywords.
// Also, `Void(...)` makes the block a `Unit` block.
CURRENT_STRUCT(Unit){};

namespace impl {

template <typename ANNOTATED_QUERY_TERM, class IMPL>
struct UnitImpl {
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = Unit;
  const IMPL& impl_;
  explicit UnitImpl(const IMPL& impl) : impl_(impl) {}
  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    struct SignalUnitEmitted {};
    try {
      impl_.EvalImpl(query, begin, end, [&emit](const typename IMPL::emitted_t&) {
        emit(Unit());
        throw SignalUnitEmitted();
      });
    } catch (const SignalUnitEmitted&) {
      // Only a single `Unit` block should be emitted.
    }
  }
};

template <typename ANNOTATED_QUERY_TERM,
          class LHS_IMPL,
          class RHS_IMPL,
          typename LHS_TYPE,
          typename RHS_TYPE,
          typename EMITTED_TYPE>
struct OrImplWithVariant {
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = EMITTED_TYPE;
  const LHS_IMPL& lhs_impl_;
  const RHS_IMPL& rhs_impl_;
  OrImplWithVariant(const LHS_IMPL& impl, const RHS_IMPL& rhs_impl) : lhs_impl_(impl), rhs_impl_(rhs_impl) {}
  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    lhs_impl_.EvalImpl(query, begin, end, [&emit](const LHS_TYPE& value) { emit(emitted_t(value)); });
    rhs_impl_.EvalImpl(query, begin, end, [&emit](const RHS_TYPE& value) { emit(emitted_t(value)); });
  }
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename TYPE>
struct OrImplSameType {
  static_assert(!std::is_same_v<TYPE, Unit>, "");
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = TYPE;
  const LHS_IMPL& lhs_impl_;
  const RHS_IMPL& rhs_impl_;
  OrImplSameType(const LHS_IMPL& impl, const RHS_IMPL& rhs_impl) : lhs_impl_(impl), rhs_impl_(rhs_impl) {}
  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    lhs_impl_.EvalImpl(query, begin, end, [&emit](const TYPE& value) { emit(emitted_t(value)); });
    rhs_impl_.EvalImpl(query, begin, end, [&emit](const TYPE& value) { emit(emitted_t(value)); });
  }
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL>
struct OrImplUnitUnit {
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = Unit;
  const LHS_IMPL& lhs_impl_;
  const RHS_IMPL& rhs_impl_;
  OrImplUnitUnit(const LHS_IMPL& impl, const RHS_IMPL& rhs_impl) : lhs_impl_(impl), rhs_impl_(rhs_impl) {}
  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    struct SignalUnitShouldBeEmittedFromOr {};
    try {
      lhs_impl_.EvalImpl(query, begin, end, [](const Unit&) { throw SignalUnitShouldBeEmittedFromOr(); });
      rhs_impl_.EvalImpl(query, begin, end, [](const Unit&) { throw SignalUnitShouldBeEmittedFromOr(); });
    } catch (const SignalUnitShouldBeEmittedFromOr&) {
      emit(Unit());
    }
  }
};

template <typename ANNOTATED_QUERY_TERM,
          class LHS_IMPL,
          class RHS_IMPL,
          typename LHS_TYPE,
          typename RHS_TYPE,
          bool LHS_IS_VARIANT,
          bool RHS_IS_VARIANT>
struct OrImplWithVariantSelector;

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename LHS_TYPE, typename RHS_TYPE>
struct OrImplWithVariantSelector<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, LHS_TYPE, RHS_TYPE, false, false> {
  using type =
      OrImplWithVariant<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, LHS_TYPE, RHS_TYPE, Variant<LHS_TYPE, RHS_TYPE>>;
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename LHS_TYPE, typename RHS_TYPE>
struct OrImplWithVariantSelector<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, LHS_TYPE, RHS_TYPE, true, false> {
  using type = OrImplWithVariant<
      ANNOTATED_QUERY_TERM,
      LHS_IMPL,
      RHS_IMPL,
      LHS_TYPE,
      RHS_TYPE,
      Variant<current::metaprogramming::TypeListUnion<typename LHS_TYPE::typelist_t, TypeListImpl<RHS_TYPE>>>>;
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename LHS_TYPE, typename RHS_TYPE>
struct OrImplWithVariantSelector<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, LHS_TYPE, RHS_TYPE, false, true> {
  using type = OrImplWithVariant<
      ANNOTATED_QUERY_TERM,
      LHS_IMPL,
      RHS_IMPL,
      LHS_TYPE,
      RHS_TYPE,
      Variant<current::metaprogramming::TypeListUnion<TypeListImpl<LHS_TYPE>, typename RHS_TYPE::typelist_t>>>;
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename LHS_TYPE, typename RHS_TYPE>
struct OrImplWithVariantSelector<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, LHS_TYPE, RHS_TYPE, true, true> {
  using type = OrImplWithVariant<
      ANNOTATED_QUERY_TERM,
      LHS_IMPL,
      RHS_IMPL,
      LHS_TYPE,
      RHS_TYPE,
      Variant<current::metaprogramming::TypeListUnion<typename LHS_TYPE::typelist_t, typename RHS_TYPE::typelist_t>>>;
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename LHS_TYPE, typename RHS_TYPE>
struct OrImplSelector {
  using type = typename OrImplWithVariantSelector<ANNOTATED_QUERY_TERM,
                                                  LHS_IMPL,
                                                  RHS_IMPL,
                                                  LHS_TYPE,
                                                  RHS_TYPE,
                                                  IS_CURRENT_VARIANT(LHS_TYPE),
                                                  IS_CURRENT_VARIANT(RHS_TYPE)>::type;
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, class TYPE>
struct OrImplSelector<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, TYPE, TYPE> {
  using type = OrImplSameType<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, TYPE>;
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL>
struct OrImplSelector<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, Unit, Unit> {
  using type = OrImplUnitUnit<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL>;
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL>
using OrImpl = typename OrImplSelector<ANNOTATED_QUERY_TERM,
                                       LHS_IMPL,
                                       RHS_IMPL,
                                       typename LHS_IMPL::emitted_t,
                                       typename RHS_IMPL::emitted_t>::type;

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL>
struct AndImpl {
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = current::metaprogramming::tuple_cat_t<typename LHS_IMPL::emitted_t, typename RHS_IMPL::emitted_t>;
  const LHS_IMPL& lhs_impl_;
  const RHS_IMPL& rhs_impl_;
  AndImpl(const LHS_IMPL& impl, const RHS_IMPL& rhs_impl) : lhs_impl_(impl), rhs_impl_(rhs_impl) {}
  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    lhs_impl_.EvalImpl(query, begin, end, [this, &query, begin, end, &emit](const typename LHS_IMPL::emitted_t& lhs) {
      rhs_impl_.EvalImpl(query, begin, end, [&lhs, &emit](const typename RHS_IMPL::emitted_t& rhs) {
        emit(std::tuple_cat(current::metaprogramming::wrapped_into_tuple_t<typename LHS_IMPL::emitted_t>(lhs),
                            current::metaprogramming::wrapped_into_tuple_t<typename RHS_IMPL::emitted_t>(rhs)));
      });
    });
  }
};

struct L2R {};
struct R2L {};
template <class DIRECTION,
          typename ANNOTATED_QUERY_TERM,
          class LHS_IMPL,
          class RHS_IMPL,
          typename LHS_TYPE,
          typename RHS_TYPE>
struct SeqImpl;

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename LHS_TYPE, typename RHS_TYPE>
struct SeqImpl<L2R, ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, LHS_TYPE, RHS_TYPE> {
  static_assert(!std::is_same_v<LHS_TYPE, Unit>, "");
  static_assert(!std::is_same_v<RHS_TYPE, Unit>, "");
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = current::metaprogramming::tuple_cat_t<LHS_TYPE, RHS_TYPE>;
  const LHS_IMPL& lhs_impl_;
  const RHS_IMPL& rhs_impl_;
  SeqImpl(const LHS_IMPL& impl, const RHS_IMPL& rhs_impl) : lhs_impl_(impl), rhs_impl_(rhs_impl) {}
  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    for (size_t i = begin; i <= end; ++i) {
      lhs_impl_.EvalImpl(query, begin, i, [this, &query, i, end, &emit](const LHS_TYPE& lhs) {
        rhs_impl_.EvalImpl(query, i, end, [&lhs, &emit](const RHS_TYPE& rhs) {
          emit(std::tuple_cat(current::metaprogramming::wrapped_into_tuple_t<LHS_TYPE>(lhs),
                              current::metaprogramming::wrapped_into_tuple_t<RHS_TYPE>(rhs)));
        });
      });
    }
  }
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename LHS_TYPE, typename RHS_TYPE>
struct SeqImpl<R2L, ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, LHS_TYPE, RHS_TYPE> {
  static_assert(!std::is_same_v<LHS_TYPE, Unit>, "");
  static_assert(!std::is_same_v<RHS_TYPE, Unit>, "");
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = current::metaprogramming::tuple_cat_t<LHS_TYPE, RHS_TYPE>;
  const LHS_IMPL& lhs_impl_;
  const RHS_IMPL& rhs_impl_;
  SeqImpl(const LHS_IMPL& impl, const RHS_IMPL& rhs_impl) : lhs_impl_(impl), rhs_impl_(rhs_impl) {}
  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    for (size_t i = begin; i <= end; ++i) {
      rhs_impl_.EvalImpl(query, i, end, [this, &query, begin, i, &emit](const RHS_TYPE& rhs) {
        lhs_impl_.EvalImpl(query, begin, i, [&rhs, &emit](const LHS_TYPE& lhs) {
          emit(std::tuple_cat(current::metaprogramming::wrapped_into_tuple_t<LHS_TYPE>(lhs),
                              current::metaprogramming::wrapped_into_tuple_t<RHS_TYPE>(rhs)));
        });
      });
    }
  }
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename LHS_TYPE>
struct SeqImplUnitInRHS {
  static_assert(!std::is_same_v<LHS_TYPE, Unit>, "");
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = LHS_TYPE;
  const LHS_IMPL& lhs_impl_;
  const RHS_IMPL& rhs_impl_;
  SeqImplUnitInRHS(const LHS_IMPL& impl, const RHS_IMPL& rhs_impl) : lhs_impl_(impl), rhs_impl_(rhs_impl) {}
  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    for (size_t i = begin; i <= end; ++i) {
      // Rely on the fact that only a single `Unit` will be emitted.
      // Start from testing the right hand side, as it's faster, and the order does not matter when a `Unit` is
      // present.
      rhs_impl_.EvalImpl(
          query, i, end, [this, &query, begin, i, &emit](const Unit&) { lhs_impl_.EvalImpl(query, begin, i, emit); });
    }
  }
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename RHS_TYPE>
struct SeqImplUnitInLHS {
  static_assert(!std::is_same_v<RHS_TYPE, Unit>, "");
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = RHS_TYPE;
  const LHS_IMPL& lhs_impl_;
  const RHS_IMPL& rhs_impl_;
  SeqImplUnitInLHS(const LHS_IMPL& impl, const RHS_IMPL& rhs_impl) : lhs_impl_(impl), rhs_impl_(rhs_impl) {}
  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    for (size_t i = begin; i <= end; ++i) {
      // Rely on the fact that only a single `Unit` will be emitted.
      lhs_impl_.EvalImpl(
          query, begin, i, [this, &query, i, end, &emit](const Unit&) { rhs_impl_.EvalImpl(query, i, end, emit); });
    }
  }
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL>
struct SeqImplUnitUnit {
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = Unit;
  const LHS_IMPL& lhs_impl_;
  const RHS_IMPL& rhs_impl_;
  SeqImplUnitUnit(const LHS_IMPL& impl, const RHS_IMPL& rhs_impl) : lhs_impl_(impl), rhs_impl_(rhs_impl) {}
  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    struct SignalUnitWasEmittedFromSeq {};
    try {
      for (size_t i = begin; i <= end; ++i) {
        lhs_impl_.EvalImpl(query, begin, i, [this, &query, i, end, &emit](const Unit&) {
          rhs_impl_.EvalImpl(query, i, end, [&emit](const Unit& unit) {
            emit(unit);
            throw SignalUnitWasEmittedFromSeq();
          });
        });
      }
    } catch (const SignalUnitWasEmittedFromSeq&) {
    }
  }
};

template <class DIRECTION,
          typename ANNOTATED_QUERY_TERM,
          class LHS_IMPL,
          class RHS_IMPL,
          typename LHS_TYPE,
          typename RHS_TYPE>
struct SeqImplSelector {
  using type = SeqImpl<DIRECTION, ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, LHS_TYPE, RHS_TYPE>;
};

template <class DIRECTION, typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename LHS_TYPE>
struct SeqImplSelector<DIRECTION, ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, LHS_TYPE, Unit> {
  using type = SeqImplUnitInRHS<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, LHS_TYPE>;
};

// NOTE: `DIRECTION` of evaluation is safe to ignore in `Unit`-containing sequences, but not the "for-loop" direction.
template <class DIRECTION, typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL, typename RHS_TYPE>
struct SeqImplSelector<DIRECTION, ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, Unit, RHS_TYPE> {
  using type = SeqImplUnitInLHS<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, RHS_TYPE>;
};

template <class DIRECTION, typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL>
struct SeqImplSelector<DIRECTION, ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL, Unit, Unit> {
  using type = SeqImplUnitUnit<ANNOTATED_QUERY_TERM, LHS_IMPL, RHS_IMPL>;
};

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL>
using L2RSeqImpl = typename SeqImplSelector<L2R,
                                            ANNOTATED_QUERY_TERM,
                                            LHS_IMPL,
                                            RHS_IMPL,
                                            typename LHS_IMPL::emitted_t,
                                            typename RHS_IMPL::emitted_t>::type;

template <typename ANNOTATED_QUERY_TERM, class LHS_IMPL, class RHS_IMPL>
using R2LSeqImpl = typename SeqImplSelector<R2L,
                                            ANNOTATED_QUERY_TERM,
                                            LHS_IMPL,
                                            RHS_IMPL,
                                            typename LHS_IMPL::emitted_t,
                                            typename RHS_IMPL::emitted_t>::type;

template <typename ANNOTATED_QUERY_TERM, class IMPL, typename OUTPUT_TYPE>
struct MapImpl {
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using input_t = typename IMPL::emitted_t;
  using emitted_t = OUTPUT_TYPE;
  using map_function_t = std::function<void(const input_t& input, emitted_t& output)>;
  const IMPL& impl_;
  map_function_t map_function_;
  MapImpl(const IMPL& impl, map_function_t map_function) : impl_(impl), map_function_(map_function) {}

  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    impl_.EvalImpl(query, begin, end, [this, &emit](const input_t& input) {
      emitted_t output;
      map_function_(input, output);
      emit(output);
    });
  }
};

template <typename ANNOTATED_QUERY_TERM, class IMPL>
struct FilterImpl {
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = typename IMPL::emitted_t;
  using filter_function_t = std::function<bool(const emitted_t& input)>;
  const IMPL& impl_;
  filter_function_t filter_function_;
  FilterImpl(const IMPL& impl, filter_function_t map_function) : impl_(impl), filter_function_(map_function) {}

  void EvalImpl(const ::current::nlp::AnnotatedQuery<annotated_query_term_t>& query,
                size_t begin,
                size_t end,
                std::function<void(const emitted_t&)> emit) const {
    impl_.EvalImpl(query, begin, end, [this, &emit](const emitted_t& input) {
      if (filter_function_(input)) {
        emit(input);
      }
    });
  }
};

}  // namespace current::nlp::impl

template <typename ANNOTATED_QUERY_TERM, class IMPL>
struct SchemaBlock {
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using emitted_t = typename IMPL::emitted_t;

  mutable std::string name_;  // `mutable` for `InjectName` to remain `const`.
  const std::string type_;
  const IMPL& impl_;  // Const reference as all `IMPL`-s are static.

  SchemaBlock(std::string name, const IMPL& impl)
      : name_(std::move(name)), type_(current::reflection::CurrentTypeName<emitted_t>()), impl_(impl) {}

  void InjectName(std::string new_name) const { name_ = std::move(new_name); }

  void Eval(const AnnotatedQuery<annotated_query_term_t>& query,
            size_t begin,
            size_t end,
            std::function<void(const emitted_t&)> emit) const {
    impl_.EvalImpl(query, begin, end, emit);
  }

  const std::string& DebugName() const { return name_; }
  const std::string& DebugType() const { return type_; }
  std::string DebugInfo() const { return DebugName() + " :: " + DebugType(); }

  template <class RHS_IMPL>
  SchemaBlock<annotated_query_term_t, impl::AndImpl<annotated_query_term_t, IMPL, RHS_IMPL>> operator&(
      const SchemaBlock<annotated_query_term_t, RHS_IMPL>& rhs) const {
    static impl::AndImpl<annotated_query_term_t, IMPL, RHS_IMPL> static_and_impl(impl_, rhs.impl_);
    return SchemaBlock<annotated_query_term_t, impl::AndImpl<annotated_query_term_t, IMPL, RHS_IMPL>>(
        '(' + name_ + " & " + rhs.name_ + ')', static_and_impl);
  }

  template <class RHS_IMPL>
  SchemaBlock<annotated_query_term_t, impl::OrImpl<annotated_query_term_t, IMPL, RHS_IMPL>> operator|(
      const SchemaBlock<annotated_query_term_t, RHS_IMPL>& rhs) const {
    static impl::OrImpl<annotated_query_term_t, IMPL, RHS_IMPL> static_or_impl(impl_, rhs.impl_);
    return SchemaBlock<annotated_query_term_t, impl::OrImpl<annotated_query_term_t, IMPL, RHS_IMPL>>(
        '(' + name_ + " | " + rhs.name_ + ')', static_or_impl);
  }

  template <class RHS_IMPL>
  SchemaBlock<annotated_query_term_t, impl::L2RSeqImpl<annotated_query_term_t, IMPL, RHS_IMPL>> operator>>(
      const SchemaBlock<annotated_query_term_t, RHS_IMPL>& rhs) const {
    static impl::L2RSeqImpl<annotated_query_term_t, IMPL, RHS_IMPL> static_l2r_impl(impl_, rhs.impl_);
    return SchemaBlock<annotated_query_term_t, impl::L2RSeqImpl<annotated_query_term_t, IMPL, RHS_IMPL>>(
        '(' + name_ + " >> " + rhs.name_ + ')', static_l2r_impl);
  }

  template <class RHS_IMPL>
  SchemaBlock<annotated_query_term_t, impl::R2LSeqImpl<annotated_query_term_t, IMPL, RHS_IMPL>> operator<<(
      const SchemaBlock<annotated_query_term_t, RHS_IMPL>& rhs) const {
    static impl::R2LSeqImpl<annotated_query_term_t, IMPL, RHS_IMPL> static_r2l_impl(impl_, rhs.impl_);
    return SchemaBlock<annotated_query_term_t, impl::R2LSeqImpl<annotated_query_term_t, IMPL, RHS_IMPL>>(
        '(' + name_ + " << " + rhs.name_ + ')', static_r2l_impl);
  }

  SchemaBlock<annotated_query_term_t, impl::UnitImpl<annotated_query_term_t, IMPL>> UnitHelper() const {
    static impl::UnitImpl<annotated_query_term_t, IMPL> static_unit_impl(impl_);
    return SchemaBlock<annotated_query_term_t, impl::UnitImpl<annotated_query_term_t, IMPL>>(
        std::string("Void(") + name_ + ')', static_unit_impl);
  }

  template <class OUTPUT_TYPE>
  SchemaBlock<annotated_query_term_t, impl::MapImpl<annotated_query_term_t, IMPL, OUTPUT_TYPE>> MapHelper(
      typename impl::MapImpl<annotated_query_term_t, IMPL, OUTPUT_TYPE>::map_function_t map_function) const {
    static impl::MapImpl<annotated_query_term_t, IMPL, OUTPUT_TYPE> static_map_impl(impl_, map_function);
    return SchemaBlock<annotated_query_term_t, impl::MapImpl<annotated_query_term_t, IMPL, OUTPUT_TYPE>>(
        std::string("Map(") + name_ + ", " + current::reflection::CurrentTypeName<OUTPUT_TYPE>() + ')',
        static_map_impl);
  }

  SchemaBlock<annotated_query_term_t, impl::FilterImpl<annotated_query_term_t, IMPL>> FilterHelper(
      typename impl::FilterImpl<annotated_query_term_t, IMPL>::filter_function_t filter_function) const {
    static impl::FilterImpl<annotated_query_term_t, IMPL> static_filter_impl(impl_, filter_function);
    return SchemaBlock<annotated_query_term_t, impl::FilterImpl<annotated_query_term_t, IMPL>>(
        std::string("Filter(") + name_ + ')', static_filter_impl);
  }
};

inline void DefaultAnnotateQueryTerm(const std::string& original_term, AnnotatedQueryTerm& output) {
  output.original_term = original_term;
  output.normalized_term = current::strings::ToLower(original_term);
}

template <typename ANNOTATED_QUERY_TERM>
struct StaticQueryTermAnnotators {
  using annotated_query_term_t = ANNOTATED_QUERY_TERM;
  using annotator_t = std::function<void(const std::string&, annotated_query_term_t&)>;
  using per_term_annotator_t = std::function<void(annotated_query_term_t&)>;

  annotator_t annotator_ = nullptr;
  std::unordered_map<std::string, std::vector<per_term_annotator_t>> annotators_;

  void SetAnnotator(annotator_t annotator) {
    // NOTE: Assert no `annotator_` is set.
    annotator_ = std::move(annotator);
  }

  void Add(const std::string& term, per_term_annotator_t annotator) {
    annotators_[term].push_back(std::move(annotator));
  }

  void StaticallyAnnotateQueryTerm(annotated_query_term_t& term) {
    const auto cit = annotators_.find(term.normalized_term);
    if (cit != annotators_.end()) {
      for (const auto& a : cit->second) {
        a(term);
      }
    }
  }
};

template <typename ANNOTATED_QUERY_TERM = AnnotatedQueryTerm, typename S>
AnnotatedQuery<ANNOTATED_QUERY_TERM> AnnotateQuery(S&& query_string) {
  AnnotatedQuery<ANNOTATED_QUERY_TERM> annotated_query;
  annotated_query.original_query = std::forward<S>(query_string);
  std::vector<std::string> query_terms =
      current::strings::Split<current::strings::ByWhitespace>(annotated_query.original_query);
  annotated_query.annotated_terms.resize(query_terms.size());
  StaticQueryTermAnnotators<ANNOTATED_QUERY_TERM>& static_annotators =
      Singleton<StaticQueryTermAnnotators<ANNOTATED_QUERY_TERM>>();
  for (size_t i = 0; i < query_terms.size(); ++i) {
    DefaultAnnotateQueryTerm(query_terms[i], annotated_query.annotated_terms[i]);
    if (static_annotators.annotator_) {
      static_annotators.annotator_(query_terms[i], annotated_query.annotated_terms[i]);
    }
    static_annotators.StaticallyAnnotateQueryTerm(annotated_query.annotated_terms[i]);
  }
  return annotated_query;
}

template <typename ANNOTATED_QUERY_TERM, typename IMPL, typename S>
std::vector<typename IMPL::emitted_t> MatchQueryIntoVector(const SchemaBlock<ANNOTATED_QUERY_TERM, IMPL>& schema_block,
                                                           S&& query_string) {
  using emitted_t = typename IMPL::emitted_t;
  const AnnotatedQuery<ANNOTATED_QUERY_TERM> query = AnnotateQuery<ANNOTATED_QUERY_TERM>(std::forward<S>(query_string));
  std::vector<emitted_t> results;
  schema_block.Eval(
      query, 0u, query.annotated_terms.size(), [&results](const emitted_t& result) { results.emplace_back(result); });
  return results;
}

template <typename ANNOTATED_QUERY_TERM, typename IMPL, typename S>
Optional<typename IMPL::emitted_t> JustMatchQuery(const SchemaBlock<ANNOTATED_QUERY_TERM, IMPL>& schema_block,
                                                  S&& query_string) {
  using emitted_t = typename IMPL::emitted_t;
  const AnnotatedQuery<ANNOTATED_QUERY_TERM> query = AnnotateQuery<ANNOTATED_QUERY_TERM>(std::forward<S>(query_string));
  Optional<emitted_t> return_value;
  struct SignalEvalSucceeded {};
  try {
    schema_block.Eval(query, 0u, query.annotated_terms.size(), [&return_value](const emitted_t& result) {
      return_value = result;
      throw SignalEvalSucceeded();
    });
    return nullptr;
  } catch (const SignalEvalSucceeded&) {
    // Unlike `MatchQueryIntoVector`, return a single successfully evaluated result.
    return return_value;
  }
}

}  // namespace current::nlp
}  // namespace current

// `UseNLPSchema` injects names defined in the Current NLP schema into user's namespace.
#define UseNLPSchema(schema_name) \
  using namespace ::current::nlp; \
  using namespace ::schema_name##_schema

#endif  // NLP_NLP_H
