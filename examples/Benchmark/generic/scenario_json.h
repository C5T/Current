/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BENCHMARK_SCENARIO_JSON_H
#define BENCHMARK_SCENARIO_JSON_H

#include "../../../port.h"

#include "../../../TypeSystem/struct.h"
#include "../../../TypeSystem/variant.h"
#include "../../../TypeSystem/Serialization/json.h"

#include "benchmark.h"

#include "../../../Bricks/dflags/dflags.h"

#ifndef CURRENT_MAKE_CHECK_MODE
DEFINE_string(json, "gen", "JSON action to take in the performance test, gen/parse/both.");
#else
DECLARE_string(json);
#endif

CURRENT_STRUCT(InnerLevelA) {
  // for i in `seq 100` ; do echo "  CURRENT_FIELD(s$i, std::string, \"value$i\");" ; done
  CURRENT_FIELD(s1, std::string, "value1");
  CURRENT_FIELD(s2, std::string, "value2");
  CURRENT_FIELD(s3, std::string, "value3");
  CURRENT_FIELD(s4, std::string, "value4");
  CURRENT_FIELD(s5, std::string, "value5");
  CURRENT_FIELD(s6, std::string, "value6");
  CURRENT_FIELD(s7, std::string, "value7");
  CURRENT_FIELD(s8, std::string, "value8");
  CURRENT_FIELD(s9, std::string, "value9");
  CURRENT_FIELD(s10, std::string, "value10");
  CURRENT_FIELD(s11, std::string, "value11");
  CURRENT_FIELD(s12, std::string, "value12");
  CURRENT_FIELD(s13, std::string, "value13");
  CURRENT_FIELD(s14, std::string, "value14");
  CURRENT_FIELD(s15, std::string, "value15");
  CURRENT_FIELD(s16, std::string, "value16");
  CURRENT_FIELD(s17, std::string, "value17");
  CURRENT_FIELD(s18, std::string, "value18");
  CURRENT_FIELD(s19, std::string, "value19");
  CURRENT_FIELD(s20, std::string, "value20");
  CURRENT_FIELD(s21, std::string, "value21");
  CURRENT_FIELD(s22, std::string, "value22");
  CURRENT_FIELD(s23, std::string, "value23");
  CURRENT_FIELD(s24, std::string, "value24");
  CURRENT_FIELD(s25, std::string, "value25");
  CURRENT_FIELD(s26, std::string, "value26");
  CURRENT_FIELD(s27, std::string, "value27");
  CURRENT_FIELD(s28, std::string, "value28");
  CURRENT_FIELD(s29, std::string, "value29");
  CURRENT_FIELD(s30, std::string, "value30");
  CURRENT_FIELD(s31, std::string, "value31");
  CURRENT_FIELD(s32, std::string, "value32");
  CURRENT_FIELD(s33, std::string, "value33");
  CURRENT_FIELD(s34, std::string, "value34");
  CURRENT_FIELD(s35, std::string, "value35");
  CURRENT_FIELD(s36, std::string, "value36");
  CURRENT_FIELD(s37, std::string, "value37");
  CURRENT_FIELD(s38, std::string, "value38");
  CURRENT_FIELD(s39, std::string, "value39");
  CURRENT_FIELD(s40, std::string, "value40");
  CURRENT_FIELD(s41, std::string, "value41");
  CURRENT_FIELD(s42, std::string, "value42");
  CURRENT_FIELD(s43, std::string, "value43");
  CURRENT_FIELD(s44, std::string, "value44");
  CURRENT_FIELD(s45, std::string, "value45");
  CURRENT_FIELD(s46, std::string, "value46");
  CURRENT_FIELD(s47, std::string, "value47");
  CURRENT_FIELD(s48, std::string, "value48");
  CURRENT_FIELD(s49, std::string, "value49");
  CURRENT_FIELD(s50, std::string, "value50");
  CURRENT_FIELD(s51, std::string, "value51");
  CURRENT_FIELD(s52, std::string, "value52");
  CURRENT_FIELD(s53, std::string, "value53");
  CURRENT_FIELD(s54, std::string, "value54");
  CURRENT_FIELD(s55, std::string, "value55");
  CURRENT_FIELD(s56, std::string, "value56");
  CURRENT_FIELD(s57, std::string, "value57");
  CURRENT_FIELD(s58, std::string, "value58");
  CURRENT_FIELD(s59, std::string, "value59");
  CURRENT_FIELD(s60, std::string, "value60");
  CURRENT_FIELD(s61, std::string, "value61");
  CURRENT_FIELD(s62, std::string, "value62");
  CURRENT_FIELD(s63, std::string, "value63");
  CURRENT_FIELD(s64, std::string, "value64");
  CURRENT_FIELD(s65, std::string, "value65");
  CURRENT_FIELD(s66, std::string, "value66");
  CURRENT_FIELD(s67, std::string, "value67");
  CURRENT_FIELD(s68, std::string, "value68");
  CURRENT_FIELD(s69, std::string, "value69");
  CURRENT_FIELD(s70, std::string, "value70");
  CURRENT_FIELD(s71, std::string, "value71");
  CURRENT_FIELD(s72, std::string, "value72");
  CURRENT_FIELD(s73, std::string, "value73");
  CURRENT_FIELD(s74, std::string, "value74");
  CURRENT_FIELD(s75, std::string, "value75");
  CURRENT_FIELD(s76, std::string, "value76");
  CURRENT_FIELD(s77, std::string, "value77");
  CURRENT_FIELD(s78, std::string, "value78");
  CURRENT_FIELD(s79, std::string, "value79");
  CURRENT_FIELD(s80, std::string, "value80");
  CURRENT_FIELD(s81, std::string, "value81");
  CURRENT_FIELD(s82, std::string, "value82");
  CURRENT_FIELD(s83, std::string, "value83");
  CURRENT_FIELD(s84, std::string, "value84");
  CURRENT_FIELD(s85, std::string, "value85");
  CURRENT_FIELD(s86, std::string, "value86");
  CURRENT_FIELD(s87, std::string, "value87");
  CURRENT_FIELD(s88, std::string, "value88");
  CURRENT_FIELD(s89, std::string, "value89");
  CURRENT_FIELD(s90, std::string, "value90");
  CURRENT_FIELD(s91, std::string, "value91");
  CURRENT_FIELD(s92, std::string, "value92");
  CURRENT_FIELD(s93, std::string, "value93");
  CURRENT_FIELD(s94, std::string, "value94");
  CURRENT_FIELD(s95, std::string, "value95");
  CURRENT_FIELD(s96, std::string, "value96");
  CURRENT_FIELD(s97, std::string, "value97");
  CURRENT_FIELD(s98, std::string, "value98");
  CURRENT_FIELD(s99, std::string, "value99");
  CURRENT_FIELD(s100, std::string, "value100");
};

CURRENT_STRUCT(InnerLevelB) {
  CURRENT_FIELD(one, InnerLevelA);
  CURRENT_FIELD(two, InnerLevelA);
};

CURRENT_STRUCT(InnerLevelC) {
  CURRENT_FIELD(foo, InnerLevelB);
  CURRENT_FIELD(bar, InnerLevelB);
  CURRENT_FIELD(baz, InnerLevelB);
};

CURRENT_STRUCT(TopLevel) {
  CURRENT_FIELD(name, std::string);
  CURRENT_FIELD(value, (std::vector<Variant<InnerLevelA, InnerLevelB, InnerLevelC>>));
  CURRENT_DEFAULT_CONSTRUCTOR(TopLevel) : name("JSONPerfTest") {
    value.emplace_back(InnerLevelA());
    value.emplace_back(InnerLevelB());
    value.emplace_back(InnerLevelC());
  }
};

SCENARIO(json, "JSON performance test.") {
  const TopLevel test_object;
  const std::string test_object_json;
  constexpr static size_t test_object_json_golden_length = 14509;
  std::function<void()> f;

  json() : test_object(), test_object_json(JSON(test_object)) {
    if (test_object_json.length() != test_object_json_golden_length) {
      std::cerr << "Actual JSON length: " << test_object_json.length() << ", expected "
                << test_object_json_golden_length << std::endl;
      assert(false);
    }
    if (FLAGS_json == "gen") {
      f = [this]() { JSON(test_object); };
    } else if (FLAGS_json == "parse") {
      f = [this]() { ParseJSON<TopLevel>(test_object_json); };
    } else if (FLAGS_json == "both") {
      f = [this]() { ParseJSON<TopLevel>(JSON(test_object)); };
    } else {
      std::cerr << "The `--json` flag must be 'gen', 'parse', or 'both'." << std::endl;
    }
  }

  void RunOneQuery() override { f(); }
};

REGISTER_SCENARIO(json);

#endif  // BENCHMARK_SCENARIO_JSON_H
