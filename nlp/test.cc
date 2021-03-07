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

#include "nlp.h"

#include "../bricks/strings/join.h"
#include "../bricks/strings/printf.h"

#include "../typesystem/serialization/json.h"

#include "../3rdparty/gtest/gtest-main.h"

/**********************************************************************************************************************

  NLP.DefaultTermAnnotation
  Smoke test.

**********************************************************************************************************************/

TEST(NLP, QueryAnnotation) {
  const current::nlp::AnnotatedQuery<current::nlp::AnnotatedQueryTerm> result =
      current::nlp::AnnotateQuery("Test PASSED");

  EXPECT_EQ("Test PASSED", result.original_query);

  ASSERT_EQ(2u, result.annotated_terms.size());

  EXPECT_EQ("test", result.annotated_terms[0].normalized_term);
  EXPECT_EQ("passed", result.annotated_terms[1].normalized_term);

  EXPECT_EQ("Test", result.annotated_terms[0].original_term);
  EXPECT_EQ("PASSED", result.annotated_terms[1].original_term);
}

/**********************************************************************************************************************

  NLP.CustomTermAnnotation
  Custom annotation type that extends the default `AnnotatedQueryTerm`.

**********************************************************************************************************************/

#include "nlp_schema_begin.inl"

NLPSchema(CustomTermAnnotation, CustomlyAnnotatedQueryTerm) {
  // clang-format off
  CURRENT_STRUCT(CustomlyAnnotatedQueryTerm, AnnotatedQueryTerm) {
    CURRENT_FIELD(term_length, uint32_t);
  };
  // clang-format on
  CustomAnnotation(output.term_length = static_cast<uint32_t>(output.normalized_term.length()));
}

TEST(NLP, CustomTermAnnotation) {
  UseNLPSchema(CustomTermAnnotation);

  const auto result1 = AnnotateQuery("a bc foo");
  const auto result2 = AnnotateQuery<CustomlyAnnotatedQueryTerm>("a bc foo");

  EXPECT_STREQ("AnnotatedQuery<AnnotatedQueryTerm>", current::reflection::CurrentTypeName<decltype(result1)>());
  EXPECT_STREQ("AnnotatedQuery<CustomlyAnnotatedQueryTerm>", current::reflection::CurrentTypeName<decltype(result2)>());

  ASSERT_EQ(3u, result1.annotated_terms.size());
  ASSERT_EQ(3u, result2.annotated_terms.size());

  EXPECT_EQ("a", result1.annotated_terms[0].normalized_term);
  EXPECT_EQ("bc", result1.annotated_terms[1].normalized_term);
  EXPECT_EQ("foo", result1.annotated_terms[2].normalized_term);

  EXPECT_EQ("a", result2.annotated_terms[0].normalized_term);
  EXPECT_EQ("bc", result2.annotated_terms[1].normalized_term);
  EXPECT_EQ("foo", result2.annotated_terms[2].normalized_term);

  EXPECT_EQ(1u, result2.annotated_terms[0].term_length);
  EXPECT_EQ(2u, result2.annotated_terms[1].term_length);
  EXPECT_EQ(3u, result2.annotated_terms[2].term_length);
}

#include "nlp_schema_end.inl"

/**********************************************************************************************************************

 NLP.DictionaryBasedAnnotation
 Two user-provided dictionary-based annotations.

**********************************************************************************************************************/

#include "nlp_schema_begin.inl"

NLPSchema(DictionaryBasedAnnotation, C5TAnnotatedQueryTerm) {
  CURRENT_STRUCT(CurrentMaintainer) {
    CURRENT_FIELD(is_dima, bool);
    CURRENT_FIELD(is_max, bool);
    CURRENT_FIELD(is_grixa, bool);
    // NOTE: Will need to implement default constructor generation as part of reflected schema output generation,
    //       both for `CURRENT_STRUCT`-based schema output and for the native C++ format.
    //       Also, `CURRENT_STRUCT`-s will need to support hashing and expose/export hash functions computation.
    CURRENT_CONSTRUCTOR(CurrentMaintainer)
    (bool is_dima = false, bool is_max = false, bool is_grixa = false)
        : is_dima(is_dima), is_max(is_max), is_grixa(is_grixa) {}
  };

  CURRENT_STRUCT(StateOfResidence) {
    CURRENT_FIELD(state, std::string);
    CURRENT_CONSTRUCTOR(StateOfResidence)(std::string state = "") : state(std::move(state)) {}
  };

  CURRENT_STRUCT(C5TAnnotatedQueryTerm, AnnotatedQueryTerm) {
    CURRENT_FIELD(c5t, Optional<CurrentMaintainer>);
    CURRENT_FIELD(state, Optional<StateOfResidence>);
  };

  DictionaryAnnotation(
      c5t, {"dima", {true, false, false}}, {"max", {false, true, false}}, {"grixa", {false, false, true}});
  DictionaryAnnotation(state, {"dima", {"WA"}}, {"max", {"CA"}}, {"serge", {"TX"}});
}

TEST(NLP, DictionaryBasedAnnotation) {
  UseNLPSchema(DictionaryBasedAnnotation);

  const AnnotatedQuery<C5TAnnotatedQueryTerm> result = AnnotateQuery<C5TAnnotatedQueryTerm>("dima max grixa serge");

  ASSERT_EQ(4u, result.annotated_terms.size());

  ASSERT_TRUE(Exists(result.annotated_terms[0].c5t));
  ASSERT_TRUE(Exists(result.annotated_terms[1].c5t));
  ASSERT_TRUE(Exists(result.annotated_terms[2].c5t));
  ASSERT_FALSE(Exists(result.annotated_terms[3].c5t));

  ASSERT_TRUE(Exists(result.annotated_terms[0].state));
  ASSERT_TRUE(Exists(result.annotated_terms[1].state));
  ASSERT_FALSE(Exists(result.annotated_terms[2].state));
  ASSERT_TRUE(Exists(result.annotated_terms[3].state));

  EXPECT_EQ("dima", result.annotated_terms[0].normalized_term);
  EXPECT_TRUE(Value(result.annotated_terms[0].c5t).is_dima);
  EXPECT_FALSE(Value(result.annotated_terms[0].c5t).is_max);
  EXPECT_FALSE(Value(result.annotated_terms[0].c5t).is_grixa);
  EXPECT_EQ("WA", Value(result.annotated_terms[0].state).state);

  EXPECT_EQ("max", result.annotated_terms[1].normalized_term);
  EXPECT_FALSE(Value(result.annotated_terms[1].c5t).is_dima);
  EXPECT_TRUE(Value(result.annotated_terms[1].c5t).is_max);
  EXPECT_FALSE(Value(result.annotated_terms[1].c5t).is_grixa);
  EXPECT_EQ("CA", Value(result.annotated_terms[1].state).state);

  EXPECT_EQ("grixa", result.annotated_terms[2].normalized_term);
  EXPECT_FALSE(Value(result.annotated_terms[2].c5t).is_dima);
  EXPECT_FALSE(Value(result.annotated_terms[2].c5t).is_max);
  EXPECT_TRUE(Value(result.annotated_terms[2].c5t).is_grixa);

  EXPECT_EQ("serge", result.annotated_terms[3].normalized_term);
  EXPECT_EQ("TX", Value(result.annotated_terms[3].state).state);

  // The dictionary-defined fields can be used as schema building blocks; more in the tests below.
  EXPECT_EQ("c5t :: CurrentMaintainer", c5t.DebugInfo());
  EXPECT_EQ("state :: StateOfResidence", state.DebugInfo());
}

#include "nlp_schema_end.inl"

/**********************************************************************************************************************

  NLP.TrivialSingleTermMatching
  Single-term schema matching.

**********************************************************************************************************************/

#include "nlp_schema_begin.inl"

NLPSchema(TrivialSingleTermMatching, PieceAnnotatedQueryTerm) {
  CURRENT_STRUCT(Piece) {
    CURRENT_FIELD(piece, std::string);
    CURRENT_CONSTRUCTOR(Piece)(std::string piece = "") : piece(std::move(piece)) {}
  };

  CURRENT_STRUCT(PieceAnnotatedQueryTerm, AnnotatedQueryTerm) { CURRENT_FIELD(piece, Optional<Piece>); };

  DictionaryAnnotation(piece, {"pawn", {"pawn"}}, {"rook", {"rook"}}, {"king", {"king"}});

  Term(just_piece, piece);
}

TEST(NLP, TrivialSingleTermMatching) {
  UseNLPSchema(TrivialSingleTermMatching);

  EXPECT_EQ("piece :: Piece", piece.DebugInfo());
  EXPECT_EQ("just_piece :: Piece", just_piece.DebugInfo());

  {
    const std::vector<Piece> result = MatchQueryIntoVector(just_piece, "king");
    ASSERT_EQ(1u, result.size());
    EXPECT_EQ("king", result[0].piece);
  }
  {
    const Optional<Piece> result = JustMatchQuery(just_piece, "pawn");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ("pawn", Value(result).piece);
  }

  {
    const std::vector<Piece> result = MatchQueryIntoVector(just_piece, "apple");
    ASSERT_EQ(0u, result.size());
  }
  {
    const Optional<Piece> result = JustMatchQuery(just_piece, "banana");
    ASSERT_FALSE(Exists(result));
  }
}

#include "nlp_schema_end.inl"

/**********************************************************************************************************************

 NLP.SchemaCompositionMechamisms
 Implementation and tests for `&`, `|`, `<<`, and `>>` operators.

**********************************************************************************************************************/

#include "nlp_schema_begin.inl"

NLPSchema(SchemaCompositionMechamisms, ColorsAndFruitsQueryTerm) {
  CURRENT_STRUCT(Color) {
    CURRENT_FIELD(rgb, std::string);
    CURRENT_CONSTRUCTOR(Color)(std::string rgb = "") : rgb(std::move(rgb)) {}
  };
  CURRENT_STRUCT(Fruit) {
    CURRENT_FIELD(shape, std::string);
    CURRENT_CONSTRUCTOR(Fruit)(std::string shape = "") : shape(std::move(shape)) {}
  };

  CURRENT_STRUCT(ColorsAndFruitsQueryTerm, AnnotatedQueryTerm) {
    CURRENT_FIELD(color, Optional<Color>);
    CURRENT_FIELD(fruit, Optional<Fruit>);
  };

  DictionaryAnnotation(color, {"red", {"ff0000"}}, {"black", {"000000"}}, {"orange", {"ffbf2f"}});
  DictionaryAnnotation(fruit, {"apple", {"bulky"}}, {"banana", {"long"}}, {"orange", {"sphere"}});

  Term(color_and_fruit, color & fruit);
  Term(color_or_fruit, color | fruit);

  Term(color_followed_by_fruit, color >> fruit);
  Term(fruit_followed_by_color, fruit >> color);
}

TEST(NLP, SchemaCompositionMechamisms) {
  UseNLPSchema(SchemaCompositionMechamisms);

  EXPECT_EQ("color :: Color", color.DebugInfo());
  EXPECT_EQ("fruit :: Fruit", fruit.DebugInfo());

  EXPECT_EQ("color_and_fruit :: std::tuple<Color, Fruit>", color_and_fruit.DebugInfo());
  EXPECT_EQ("(color & fruit) :: std::tuple<Color, Fruit>", (color & fruit).DebugInfo());

  EXPECT_EQ("color_or_fruit :: Variant<Color, Fruit>", color_or_fruit.DebugInfo());
  EXPECT_EQ("(color | fruit) :: Variant<Color, Fruit>", (color | fruit).DebugInfo());

  EXPECT_EQ("color_followed_by_fruit :: std::tuple<Color, Fruit>", color_followed_by_fruit.DebugInfo());
  EXPECT_EQ("(color >> fruit) :: std::tuple<Color, Fruit>", (color >> fruit).DebugInfo());

  EXPECT_EQ("fruit_followed_by_color :: std::tuple<Fruit, Color>", fruit_followed_by_color.DebugInfo());
  EXPECT_EQ("(fruit >> color) :: std::tuple<Fruit, Color>", (fruit >> color).DebugInfo());

  EXPECT_EQ("Void(color_and_fruit) :: Unit", Void(color_and_fruit).DebugInfo());
  EXPECT_EQ("Void((color >> fruit)) :: Unit", Void(color >> fruit).DebugInfo());

  ASSERT_FALSE(Exists(JustMatchQuery(color, "")));
  ASSERT_FALSE(Exists(JustMatchQuery(fruit, "")));

  ASSERT_FALSE(Exists(JustMatchQuery(color, "foo bar")));
  ASSERT_FALSE(Exists(JustMatchQuery(fruit, "foo bar")));

  ASSERT_TRUE(Exists(JustMatchQuery(color, "red")));
  ASSERT_TRUE(Exists(JustMatchQuery(color, "orange")));
  ASSERT_FALSE(Exists(JustMatchQuery(color, "apple")));
  ASSERT_FALSE(Exists(JustMatchQuery(color, "banana")));

  ASSERT_TRUE(Exists(JustMatchQuery(fruit, "apple")));
  ASSERT_TRUE(Exists(JustMatchQuery(fruit, "orange")));
  ASSERT_FALSE(Exists(JustMatchQuery(fruit, "red")));
  ASSERT_FALSE(Exists(JustMatchQuery(fruit, "black")));

  ASSERT_FALSE(Exists(JustMatchQuery(color_and_fruit, "black")));
  ASSERT_FALSE(Exists(JustMatchQuery(color_and_fruit, "apple")));
  ASSERT_TRUE(Exists(JustMatchQuery(color_or_fruit, "black")));
  ASSERT_TRUE(Exists(JustMatchQuery(color_or_fruit, "apple")));

  {
    const Optional<std::tuple<Color, Fruit>> result = JustMatchQuery(color_and_fruit, "orange");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ("ffbf2f", std::get<0>(Value(result)).rgb);
    EXPECT_EQ("sphere", std::get<1>(Value(result)).shape);
  }

  {
    const Optional<std::tuple<Color, Fruit>> result = JustMatchQuery(color & fruit, "orange");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ("ffbf2f", std::get<0>(Value(result)).rgb);
    EXPECT_EQ("sphere", std::get<1>(Value(result)).shape);
  }
  {
    const Optional<Variant<Color, Fruit>> result = JustMatchQuery(color_or_fruit, "orange");
    ASSERT_TRUE(Exists(result));
    EXPECT_TRUE(Exists<Color>(Value(result)));
  }

  EXPECT_TRUE(MatchQueryIntoVector(color_or_fruit, "foo").empty());
  {
    const std::vector<Variant<Color, Fruit>> result = MatchQueryIntoVector(color_or_fruit, "red");
    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(Exists<Color>(result[0]));
  }
  {
    const std::vector<Variant<Color, Fruit>> result = MatchQueryIntoVector(color_or_fruit, "apple");
    ASSERT_EQ(1u, result.size());
    EXPECT_TRUE(Exists<Fruit>(result[0]));
  }
  {
    const std::vector<Variant<Color, Fruit>> result = MatchQueryIntoVector(color_or_fruit, "orange");
    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(Exists<Color>(result[0]));
    EXPECT_TRUE(Exists<Fruit>(result[1]));
  }
  {
    // Confirm that `Void(...)`, which generates a `Unit` type, emits only one instance.
    const std::vector<Unit> result = MatchQueryIntoVector(Void(color_or_fruit), "orange");
    ASSERT_EQ(1u, result.size());
  }

  {
    const std::vector<Variant<Color, Fruit>> result = MatchQueryIntoVector(color | fruit, "orange");
    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(Exists<Color>(result[0]));
    EXPECT_TRUE(Exists<Fruit>(result[1]));
  }
  {
    const std::vector<Variant<Fruit, Color>> result = MatchQueryIntoVector(fruit | color, "orange");
    ASSERT_EQ(2u, result.size());
    EXPECT_TRUE(Exists<Fruit>(result[0]));
    EXPECT_TRUE(Exists<Color>(result[1]));
  }

  {
    ASSERT_FALSE(Exists(JustMatchQuery(color_followed_by_fruit, "")));
    ASSERT_FALSE(Exists(JustMatchQuery(color_followed_by_fruit, "foo")));
    ASSERT_FALSE(Exists(JustMatchQuery(color_followed_by_fruit, "foo bar")));
    ASSERT_FALSE(Exists(JustMatchQuery(color_followed_by_fruit, "red")));
    ASSERT_FALSE(Exists(JustMatchQuery(color_followed_by_fruit, "apple")));
    ASSERT_FALSE(Exists(JustMatchQuery(color_followed_by_fruit, "apple red")));
    ASSERT_TRUE(Exists(JustMatchQuery(color_followed_by_fruit, "red apple")));
    EXPECT_EQ("[[{\"rgb\":\"ff0000\"},{\"shape\":\"bulky\"}]]",
              JSON(MatchQueryIntoVector(color_followed_by_fruit, "red apple")));
    ASSERT_FALSE(Exists(JustMatchQuery(color >> fruit, "apple red")));
    ASSERT_TRUE(Exists(JustMatchQuery(color >> fruit, "red apple")));
  }

  {
    ASSERT_FALSE(Exists(JustMatchQuery(fruit_followed_by_color, "")));
    ASSERT_FALSE(Exists(JustMatchQuery(fruit_followed_by_color, "foo")));
    ASSERT_FALSE(Exists(JustMatchQuery(fruit_followed_by_color, "foo bar")));
    ASSERT_FALSE(Exists(JustMatchQuery(fruit_followed_by_color, "red")));
    ASSERT_FALSE(Exists(JustMatchQuery(fruit_followed_by_color, "apple")));
    ASSERT_FALSE(Exists(JustMatchQuery(fruit_followed_by_color, "red apple")));
    ASSERT_TRUE(Exists(JustMatchQuery(fruit_followed_by_color, "apple red")));
    EXPECT_EQ("[[{\"shape\":\"bulky\"},{\"rgb\":\"ff0000\"}]]",
              JSON(MatchQueryIntoVector(fruit_followed_by_color, "apple red")));
    ASSERT_FALSE(Exists(JustMatchQuery(fruit >> color, "red apple")));
    ASSERT_TRUE(Exists(JustMatchQuery(fruit >> color, "apple red")));
  }
}

#include "nlp_schema_end.inl"

/**********************************************************************************************************************

 NLP.EvaluationOrder
 Tests the operator evaluation order.

**********************************************************************************************************************/

#include "nlp_schema_begin.inl"

NLPSchema(EvaluationOrder, ThreeWayAnnotation) {
  CURRENT_STRUCT(X){};
  CURRENT_STRUCT(Y){};
  CURRENT_STRUCT(Z) {
    CURRENT_FIELD(z, std::string);
    CURRENT_CONSTRUCTOR(Z)(std::string z = "") : z(std::move(z)) {}
  };

  CURRENT_STRUCT(ThreeWayAnnotation, AnnotatedQueryTerm) {
    CURRENT_FIELD(one, Optional<X>);
    CURRENT_FIELD(two, Optional<Y>);
    CURRENT_FIELD(three, Optional<Z>);
  };

  DictionaryAnnotation(one, {"a", {}});
  DictionaryAnnotation(two, {"a", {}});

  Term(ambiguous, one | two);

  Term(sequence1, ambiguous >> ambiguous);
  Term(sequence2, ambiguous << ambiguous);

  DictionaryAnnotation(three, {"p", {"p"}}, {"q", {"q"}});

  // Explanation:
  // * `three` accepts "p" or "q", or "p|q".
  // * `repeated1` accepts one "p|q" or two "p|q"-s.
  // * `three >> three` has the type `std::tuple<Z, Z>`.
  // * The `Map` transforms this `std::tuple<Z, Z>` into a single `Z`.
  // * The type of `repeated1` then is just `Z`, as `Variant<Z, Z>` is collapsed.
  Term(repeated1, three | Map(three >> three, Z, output.z = '(' + Input(0).z + ' ' + Input(1).z + ')'));

  // Explanation:
  // * As `repeated1` accepts one or two "p|q"-s, `repeated1 >> repeated1` accepts one, two, three, or four "p|q"-s.
  // * The type of `repeated2` is `Z` (see explanation above for `repeated1`).
  Term(repeated2, Map(repeated1 >> repeated1, Z, output.z = '{' + Input(0).z + ' ' + Input(1).z + '}'));
}

TEST(NLP, EvaluationOrder) {
  UseNLPSchema(EvaluationOrder);

  EXPECT_EQ("one :: X", one.DebugInfo());
  EXPECT_EQ("two :: Y", two.DebugInfo());

  EXPECT_EQ("ambiguous :: Variant<X, Y>", ambiguous.DebugInfo());

  EXPECT_EQ("sequence1 :: std::tuple<Variant<X, Y>, Variant<X, Y>>", sequence1.DebugInfo());
  EXPECT_EQ("sequence2 :: std::tuple<Variant<X, Y>, Variant<X, Y>>", sequence2.DebugInfo());

  EXPECT_EQ("(ambiguous >> ambiguous) :: std::tuple<Variant<X, Y>, Variant<X, Y>>",
            (ambiguous >> ambiguous).DebugInfo());
  EXPECT_EQ("(ambiguous << ambiguous) :: std::tuple<Variant<X, Y>, Variant<X, Y>>",
            (ambiguous << ambiguous).DebugInfo());

  EXPECT_EQ("repeated1 :: Z", repeated1.DebugInfo());
  EXPECT_EQ("repeated2 :: Z", repeated2.DebugInfo());

  {
    // Order: { XX, XY, YX, YY }, as the left one is iterated over first.
    const std::vector<std::tuple<Variant<X, Y>, Variant<X, Y>>> result = MatchQueryIntoVector(sequence1, "a a");
    EXPECT_EQ("[[{\"X\":{}},{\"X\":{}}],[{\"X\":{}},{\"Y\":{}}],[{\"Y\":{}},{\"X\":{}}],[{\"Y\":{}},{\"Y\":{}}]]",
              JSON<JSONFormat::Minimalistic>(result));
  }

  {
    // Order: { XX, YX, XY, YY }, as the right one is iterated over first.
    const std::vector<std::tuple<Variant<X, Y>, Variant<X, Y>>> result = MatchQueryIntoVector(sequence2, "a a");
    EXPECT_EQ("[[{\"X\":{}},{\"X\":{}}],[{\"Y\":{}},{\"X\":{}}],[{\"X\":{}},{\"Y\":{}}],[{\"Y\":{}},{\"Y\":{}}]]",
              JSON<JSONFormat::Minimalistic>(result));
  }

  {
    const auto schema = Map(three, Z, output.z = "Ha: " + input.z);
    EXPECT_EQ("Map(three, Z) :: Z", schema.DebugInfo());
    const std::vector<Z> result = MatchQueryIntoVector(schema, "p");
    ASSERT_EQ(1u, result.size());
    EXPECT_EQ("Ha: p", result[0].z);
  }

  {
    ASSERT_FALSE(Exists(JustMatchQuery(repeated1, "")));

    ASSERT_TRUE(Exists(JustMatchQuery(repeated1, "p")));
    ASSERT_TRUE(Exists(JustMatchQuery(repeated1, "q")));

    ASSERT_TRUE(Exists(JustMatchQuery(repeated1, "p p")));
    ASSERT_TRUE(Exists(JustMatchQuery(repeated1, "p q")));
    ASSERT_TRUE(Exists(JustMatchQuery(repeated1, "q p")));
    ASSERT_TRUE(Exists(JustMatchQuery(repeated1, "q q")));

    ASSERT_FALSE(Exists(JustMatchQuery(repeated1, "p p p")));
  }

  {
    EXPECT_EQ("[{\"z\":\"p\"}]", JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(repeated1, "p")));
    EXPECT_EQ("[{\"z\":\"(p q)\"}]", JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(repeated1, "p q")));
  }

  {
    ASSERT_FALSE(Exists(JustMatchQuery(repeated2, "p")));

    ASSERT_TRUE(Exists(JustMatchQuery(repeated2, "p p")));
    ASSERT_TRUE(Exists(JustMatchQuery(repeated2, "p q p")));
    ASSERT_TRUE(Exists(JustMatchQuery(repeated2, "q p p q")));

    ASSERT_FALSE(Exists(JustMatchQuery(repeated2, "q q q q q")));
  }

  {
    // One way to group.
    EXPECT_EQ("[{\"z\":\"{p q}\"}]", JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(repeated2, "p q")));
    // Two ways to group.
    EXPECT_EQ("[{\"z\":\"{p (q p)}\"},{\"z\":\"{(p q) p}\"}]",
              JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(repeated2, "p q p")));
    // One way to group.
    EXPECT_EQ("[{\"z\":\"{(p q) (p q)}\"}]",
              JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(repeated2, "p q p q")));
  }
}

#include "nlp_schema_end.inl"

/**********************************************************************************************************************

 NLP.VariantTypes
 Tests that the type lists of produced `Variant<>` types are correct.

**********************************************************************************************************************/

#include "nlp_schema_begin.inl"

NLPSchema(VariantTypes, MultidimensionalAnnotation) {
  CURRENT_STRUCT(A){};
  CURRENT_STRUCT(B){};
  CURRENT_STRUCT(C){};

  CURRENT_STRUCT(MultidimensionalAnnotation, AnnotatedQueryTerm) {
    CURRENT_FIELD(a, Optional<A>);
    CURRENT_FIELD(b, Optional<B>);
    CURRENT_FIELD(c, Optional<C>);
  };

  DictionaryAnnotation(a);
  DictionaryAnnotation(b);
  DictionaryAnnotation(c);
}

TEST(NLP, VariantTypes) {
  UseNLPSchema(VariantTypes);

  EXPECT_EQ("a :: A", a.DebugInfo());
  EXPECT_EQ("b :: B", b.DebugInfo());
  EXPECT_EQ("c :: C", c.DebugInfo());

  // Check that `JustMatchQuery` return types are correct.
  {
    auto r = JustMatchQuery(a | b, "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B>>>, "");
  }

  {
    auto r = JustMatchQuery((a | b) | c, "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B, C>>>, "");
  }

  {
    auto r = JustMatchQuery(a | (b | c), "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B, C>>>, "");
  }

  {
    auto r = JustMatchQuery(a | b | c, "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B, C>>>, "");
  }

  {
    auto r = JustMatchQuery((a | b) | a, "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B>>>, "");
  }

  {
    auto r = JustMatchQuery((a | b) | b, "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B>>>, "");
  }

  {
    auto r = JustMatchQuery(a | (a | b), "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B>>>, "");
  }

  {
    auto r = JustMatchQuery(b | (a | b), "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<B, A>>>, "");
  }

  {
    auto r = JustMatchQuery(c | (a | b), "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<C, A, B>>>, "");
  }

  {
    auto r = JustMatchQuery((a | b) | c, "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B, C>>>, "");
  }

  {
    auto r = JustMatchQuery((a | b) | (b | a), "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B>>>, "");
  }

  {
    auto r = JustMatchQuery((a | b) | (a | c), "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B, C>>>, "");
  }

  {
    auto r = JustMatchQuery((a | b) | (b | c), "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B, C>>>, "");
  }

  {
    auto r = JustMatchQuery((a | b) | (c | b), "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<Variant<A, B, C>>>, "");
  }

  {
    auto r = JustMatchQuery(a & b, "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<std::tuple<A, B>>>, "");
  }

  {
    auto r = JustMatchQuery(a & b & c, "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<std::tuple<A, B, C>>>, "");
  }
  {
    auto r = JustMatchQuery(a & (b & c), "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<std::tuple<A, B, C>>>, "");
  }
  {
    auto r = JustMatchQuery((a & b) & (b & c), "");
    EXPECT_FALSE(Exists(r));
    static_assert(std::is_same_v<decltype(r), Optional<std::tuple<A, B, B, C>>>, "");
  }
}

#include "nlp_schema_end.inl"

/**********************************************************************************************************************

 NLP.CalculatorExample
 A primitive, though close to real world, example.

**********************************************************************************************************************/

#include "nlp_schema_begin.inl"

NLPSchema(Calculator, CalculatorAnnotation) {
  CURRENT_STRUCT(Digit) {
    CURRENT_FIELD(d, uint32_t);
    CURRENT_CONSTRUCTOR(Digit)(uint32_t d = 0u) : d(d) {}
  };

  // clang-format off
  CURRENT_STRUCT(CalculatorAnnotation, AnnotatedQueryTerm) {
    CURRENT_FIELD(digit, Optional<Digit>);
  };
  // clang-format on

  DictionaryAnnotation(digit, {"zero", {0}}, {"one", {1}}, {"two", {2}}, {"three", {3}});

  Keyword(what);
  Keyword(how);
  Keyword(many);
  Keyword(much);
  Keyword(is);
  Keyword(are);
  Keyword(would);
  Keyword(be);
  Keyword(plus);
  Keyword(minus);
  Keyword(times);

  CURRENT_STRUCT(Number) {
    CURRENT_FIELD(x, int32_t);
    CURRENT_CONSTRUCTOR(Number)(int32_t x = 0u) : x(x) {}
    CURRENT_CONSTRUCTOR(Number)(const Digit& digit) : x(digit.d) {}  // For `As` below.
  };

  // clang-format off
  Term(formula,
       Maybe((what | (how >> (much | many))) >> (is | are))
       >> (As(digit, Number)
           | Map(digit >> plus >> digit, Number, output.x = Input(0).d + Input(1).d)
           | Map(digit >> minus >> digit, Number, output.x = Input(0).d - Input(1).d)
           | Map(digit >> times >> digit, Number, output.x = Input(0).d * Input(1).d)));
  // clang-format on

  Term(odd_digit, Filter(digit, ((input.d % 2) == 1)));
}

TEST(NLP, CalculatorExample) {
  UseNLPSchema(Calculator);

  EXPECT_EQ("None :: Unit", None.DebugInfo());
  EXPECT_EQ("is :: Unit", is.DebugInfo());
  EXPECT_EQ("(None | None) :: Unit", (None | None).DebugInfo());
  EXPECT_EQ("(is | None) :: Unit", (is | None).DebugInfo());
  EXPECT_EQ("(None | is) :: Unit", (None | is).DebugInfo());

  EXPECT_EQ("digit :: Digit", digit.DebugInfo());
  EXPECT_EQ("formula :: Number", formula.DebugInfo());

  ASSERT_FALSE(Exists(JustMatchQuery(digit, "test")));
  {
    const Optional<Digit> result = JustMatchQuery(digit, "two");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(2u, Value(result).d);
  }

  ASSERT_TRUE(Exists(JustMatchQuery(None, "")));
  ASSERT_TRUE(Exists(JustMatchQuery(is, "is")));
  ASSERT_TRUE(Exists(JustMatchQuery(None | None, "")));
  ASSERT_TRUE(Exists(JustMatchQuery(Maybe(None), "")));
  ASSERT_TRUE(Exists(JustMatchQuery(GreedyMaybe(None), "")));
  ASSERT_TRUE(Exists(JustMatchQuery(None | is, "")));
  ASSERT_TRUE(Exists(JustMatchQuery(None | is, "is")));
  ASSERT_TRUE(Exists(JustMatchQuery(Maybe(is), "is")));
  ASSERT_TRUE(Exists(JustMatchQuery(GreedyMaybe(is), "is")));

  ASSERT_TRUE(Exists(JustMatchQuery(how, "how")));
  ASSERT_TRUE(Exists(JustMatchQuery(many, "many")));
  ASSERT_TRUE(Exists(JustMatchQuery(is, "is")));
  ASSERT_TRUE(Exists(JustMatchQuery(how >> many, "how many")));
  ASSERT_TRUE(Exists(JustMatchQuery(how >> many >> is, "how many is")));
  ASSERT_TRUE(Exists(JustMatchQuery(how >> many >> (None | is), "how many")));
  ASSERT_TRUE(Exists(JustMatchQuery(how >> many >> (None | is), "how many is")));
  ASSERT_TRUE(Exists(JustMatchQuery(how >> many >> is >> digit, "how many is zero")));

  ASSERT_TRUE(Exists(JustMatchQuery(digit, "one")));
  ASSERT_TRUE(Exists(JustMatchQuery(digit, "two")));
  ASSERT_TRUE(Exists(JustMatchQuery(digit, "three")));
  ASSERT_TRUE(Exists(JustMatchQuery(odd_digit, "one")));
  ASSERT_FALSE(Exists(JustMatchQuery(odd_digit, "two")));
  ASSERT_TRUE(Exists(JustMatchQuery(odd_digit, "three")));

  {
    const Optional<Number> result = JustMatchQuery(formula, "what is two");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(2, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "how many is two");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(2, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "how much is two");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(2, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "how many are two");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(2, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "how much are two");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(2, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "two plus three");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(5, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "what is two plus three");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(5, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "how many is two plus three");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(5, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "one minus three");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(-2, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "what is one minus three");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(-2, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "two times three");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(6, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "what is two times three");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(6, Value(result).x);
  }

  {
    const Optional<Number> result = JustMatchQuery(formula, "how many is two times three");
    ASSERT_TRUE(Exists(result));
    EXPECT_EQ(6, Value(result).x);
  }
}

#include "nlp_schema_end.inl"

/**********************************************************************************************************************

  NLP.MultiparametricTuples
  Make sure the `std::tuple<>` type after a bunch of `operator>>` and `operator<<` is correct.

**********************************************************************************************************************/

#include "nlp_schema_begin.inl"

NLPSchema(MultiparametricTuples, ABC_PQR_XYZ) {
  CURRENT_STRUCT(ABC) {
    CURRENT_FIELD(abc, std::string);
    CURRENT_CONSTRUCTOR(ABC)(std::string abc = "") : abc(std::move(abc)) {}
  };

  CURRENT_STRUCT(PQR) {
    CURRENT_FIELD(pqr, std::string);
    CURRENT_CONSTRUCTOR(PQR)(std::string pqr = "") : pqr(std::move(pqr)) {}
  };

  CURRENT_STRUCT(XYZ) {
    CURRENT_FIELD(xyz, std::string);
    CURRENT_CONSTRUCTOR(XYZ)(std::string xyz = "") : xyz(std::move(xyz)) {}
  };

  CURRENT_STRUCT(ABC_PQR_XYZ, AnnotatedQueryTerm) {
    CURRENT_FIELD(abc, Optional<ABC>);
    CURRENT_FIELD(pqr, Optional<PQR>);
    CURRENT_FIELD(xyz, Optional<XYZ>);
  };

  DictionaryAnnotation(abc, {"a", {"a"}}, {"b", {"b"}}, {"c", {"c"}});
  DictionaryAnnotation(pqr, {"p", {"p"}}, {"q", {"q"}}, {"r", {"r"}});
  DictionaryAnnotation(xyz, {"x", {"x"}}, {"y", {"y"}}, {"z", {"z"}});

  CURRENT_STRUCT(Magic) {
    CURRENT_FIELD(magic, std::string);
    CURRENT_DEFAULT_CONSTRUCTOR(Magic) {}
    CURRENT_CONSTRUCTOR(Magic)(const ABC& _1, const PQR& _2, const XYZ& _3) : magic(_1.abc + _2.pqr + _3.xyz) {}
  };
}

TEST(NLP, MultiparametricTuples) {
  UseNLPSchema(MultiparametricTuples);

  EXPECT_EQ("abc :: ABC", abc.DebugInfo());
  EXPECT_EQ("pqr :: PQR", pqr.DebugInfo());
  EXPECT_EQ("xyz :: XYZ", xyz.DebugInfo());

  {
    const std::string golden = "[[{\"abc\":\"a\"},{\"pqr\":\"q\"},{\"xyz\":\"z\"}]]";
    EXPECT_EQ(golden, JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc >> pqr >> xyz, "a q z")));
    EXPECT_EQ(golden, JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc >> (pqr >> xyz), "a q z")));
    EXPECT_EQ(golden, JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc << pqr >> xyz, "a q z")));
    EXPECT_EQ(golden, JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc << (pqr >> xyz), "a q z")));
    EXPECT_EQ(golden, JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc >> pqr << xyz, "a q z")));
    EXPECT_EQ(golden, JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc >> (pqr << xyz), "a q z")));
    EXPECT_EQ(golden, JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc << pqr << xyz, "a q z")));
    EXPECT_EQ(golden, JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc << (pqr << xyz), "a q z")));
  }

  {
    const std::string golden = "[[{\"abc\":\"a\"},{\"pqr\":\"p\"},{\"xyz\":\"y\"},{\"pqr\":\"q\"},{\"abc\":\"c\"}]]";
    EXPECT_EQ(golden,
              JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc >> pqr >> xyz >> pqr >> abc, "a p y q c")));
    EXPECT_EQ(golden,
              JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc >> (pqr >> xyz >> pqr >> abc), "a p y q c")));
    EXPECT_EQ(golden,
              JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc >> (pqr >> xyz) >> (pqr >> abc), "a p y q c")));
    EXPECT_EQ(golden,
              JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector(abc >> ((pqr >> xyz) >> (pqr >> abc)), "a p y q c")));
    EXPECT_EQ(golden,
              JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector((abc >> pqr >> xyz) >> (pqr >> abc), "a p y q c")));
    EXPECT_EQ(golden,
              JSON<JSONFormat::Minimalistic>(MatchQueryIntoVector((abc >> pqr) >> (xyz >> pqr >> abc), "a p y q c")));
  }

  {
    const auto magic = Map(abc >> pqr >> xyz, Magic, output = Magic(Input(0), Input(1), Input(2)));
    EXPECT_EQ("[{\"magic\":\"cqz\"}]", JSON(MatchQueryIntoVector(magic, "c q z")));
    EXPECT_EQ("[{\"magic\":\"bpx\"}]", JSON(MatchQueryIntoVector(magic, "b p x")));
  }
}

#include "nlp_schema_end.inl"
