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

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "../../../port.h"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include "../../../bricks/util/singleton.h"

struct Scenario {
  virtual ~Scenario() = default;

  virtual void RunOneQuery() = 0;
};

#define SCENARIO(name, description)                          \
  struct name##_helper : Scenario {                          \
    static const char* Description() { return description; } \
  };                                                         \
  struct name : name##_helper

struct ScenariosRegisterer {
  using factory_t = std::function<std::unique_ptr<Scenario>()>;

  void Register(const std::string& name, const std::string& description, factory_t factory) {
    map[name] = std::make_pair(description, factory);
  }
  void Synopsis() const {
    std::cout << "./.current/run --scenario={scenario} [--threads={threads_to_query_from}] "
                 "[--secons={seconds_to_run_benchmark_for}." << std::endl;
    for (const auto& scenario : map) {
      std::cout << "\t--scenario=" << scenario.first << " : " << scenario.second.first << std::endl;
    }
  }

  std::unordered_map<std::string, std::pair<std::string, factory_t>> map;
};

#define REGISTER_SCENARIO(name)                                                                                \
  struct name##_registerer {                                                                                   \
    name##_registerer() {                                                                                      \
      current::Singleton<ScenariosRegisterer>().Register(                                                      \
          #name, name::Description(), []() -> std::unique_ptr<Scenario> { return std::make_unique<name>(); }); \
    }                                                                                                          \
  };                                                                                                           \
  static name##_registerer name##_registerer_static_instance;

#endif  // BENCHMARK_H
