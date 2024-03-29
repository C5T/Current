/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// Just `make` will run the test.
// Invoke with `./.current/test --run=1` to spawn on `localhost:5000`.
//
// More info:
// * http://en.wikipedia.org/wiki/Iris_flower_data_set
// * http://support.sas.com/documentation/cdl/en/graphref/65389/HTML/default/images/gtdshapa.png
// * http://www.math.uah.edu/stat/data/Fisher.html

#include "../../typesystem/struct.h"
#include "../../storage/storage.h"
#include "../../storage/persister/stream.h"

#include "../../blocks/http/api.h"

#include "../../bricks/graph/gnuplot.h"
#include "../../bricks/strings/printf.h"
#include "../../bricks/file/file.h"
#include "../../bricks/dflags/dflags.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

#include "iris.h"

using namespace current::strings;
using namespace current::gnuplot;

// `using namespace yoda`, as well as `using yoda::Dictionary` conflicts with Storage. -- D.K.

DEFINE_bool(run, false, "Set to true to run indefinitely.");

// Flower ID, global and auto-increasing, for test purposes.
TEST(Iris, Demo) {
  using TestDB = LabeledFlowersDB<StreamInMemoryStreamPersister>;
  auto db = TestDB::CreateMasterStorage();

  size_t number_of_flowers = 0u;
  std::map<size_t, std::string> dimension_names;

  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  auto& http_server = HTTP(std::move(reserved_port));

  auto http_scope = http_server.Register("/import", [&db, &number_of_flowers, &dimension_names](Request request) {
    EXPECT_EQ("POST", request.method);
    const std::string body = request.body;
    db->ReadWriteTransaction(
          [body, &number_of_flowers, &dimension_names](MutableFields<TestDB> fields) {
            // Skip the first line with labels.
            bool first_line = true;
            for (auto flower_definition_line : Split<ByLines>(body)) {
              std::vector<std::string> flower_definition_fields = Split(flower_definition_line, '\t');
              CURRENT_ASSERT(flower_definition_fields.size() == 5u);
              if (first_line && flower_definition_fields.back() == "Label") {
                // For this example, just overwrite the labels on each `/import`.
                dimension_names.clear();
                for (size_t i = 0; i < flower_definition_fields.size() - 1; ++i) {
                  dimension_names[i] = flower_definition_fields[i];
                }
                continue;
              }
              first_line = false;
              fields.flowers.Add(LabeledFlower(++number_of_flowers,
                                               current::FromString<double>(flower_definition_fields[0]),
                                               current::FromString<double>(flower_definition_fields[1]),
                                               current::FromString<double>(flower_definition_fields[2]),
                                               current::FromString<double>(flower_definition_fields[3]),
                                               flower_definition_fields[4]));
            }
            return Printf("Successfully imported %d flowers.\n", static_cast<int>(number_of_flowers));
          },
          std::move(request))
        .Wait();
  });

  // The input file is in the `golden` directory for it to be successfully picked up by
  // `scripts/full-test.sh`.
  EXPECT_EQ("Successfully imported 150 flowers.\n",
            HTTP(POSTFromFile(Printf("http://localhost:%d/import", port),
                              current::FileSystem::JoinPath("golden", "dataset.tsv"),
                              "text/tsv"))
                .body);

#if 0
  // TODO(dkorolev): Re-enable soon.

  // Ref.: http://localhost:3000/stream
  HTTP(port).Register("/stream", db);

  // The very first flower.
  const auto flower1 = ParseJSON<LabeledFlower>(
      HTTP(GET(Printf("http://localhost:%d/stream?cap=1", port))).body);
  EXPECT_DOUBLE_EQ(5.1, flower1.SL);
  EXPECT_DOUBLE_EQ(3.5, flower1.SW);
  EXPECT_DOUBLE_EQ(1.4, flower1.PL);
  EXPECT_DOUBLE_EQ(0.2, flower1.PW);
  EXPECT_EQ("setosa", flower1.label);

  // The very last flower.
  const auto flower2 = ParseJSON<LabeledFlower>(
      HTTP(GET(Printf("http://localhost:%d/stream?n=1", port))).body);
  EXPECT_DOUBLE_EQ(5.9, flower2.SL);
  EXPECT_DOUBLE_EQ(3.0, flower2.SW);
  EXPECT_DOUBLE_EQ(5.1, flower2.PL);
  EXPECT_DOUBLE_EQ(1.8, flower2.PW);
  EXPECT_EQ("virginica", flower2.label);

  // LCOV_EXCL_START
  if (port) {
    // Ref.: http://localhost:3000/get?id=42
    HTTP(port)
        .Register("/get",
                  [&api](Request request) {
                    const auto id = current::FromString<size_t>(request.url.query["id"]);
                    api.Transaction([id](TestDB::data_t data) {
                      return yoda::Dictionary<LabeledFlower>::Accessor(data)[id];
                    }, std::move(request));
                  });

    // Ref.: [POST] http://localhost:3000/add?label=setosa&sl=5&sw=5&pl=5&pw=5
    HTTP(port)
        .Register("/add",
                  [&api](Request request) {
                    const std::string label = request.url.query["label"];
                    const auto sl = current::FromString<double>(request.url.query["sl"]);
                    const auto sw = current::FromString<double>(request.url.query["sw"]);
                    const auto pl = current::FromString<double>(request.url.query["pl"]);
                    const auto pw = current::FromString<double>(request.url.query["pw"]);
                    // In real life this should be a POST.
                    if (!label.empty()) {
                      const LabeledFlower flower(++number_of_flowers, sl, sw, pl, pw, label);
                      api.Transaction([flower](TestDB::data_t data) {
                        yoda::Dictionary<LabeledFlower>::Mutator(data).Add(flower);
                        return "OK\n";
                      }, std::move(request));
                    } else {
                      request("Need non-empty label, as well as sl/sw/pl/pw.\n");
                    }
                  });

    // Ref.: http://localhost:3000/viz
    // Ref.: http://localhost:3000/viz?x=1&y=2
    HTTP(port)
        .Register("/viz",
                  [&api](Request request) {
                    auto x_dim = std::min(size_t(3), current::FromString<size_t>(request.url.query.get("x", "0")));
                    auto y_dim = std::min(size_t(3), current::FromString<size_t>(request.url.query.get("y", "1")));
                    if (y_dim == x_dim) {
                      y_dim = (x_dim + 1) % 4;
                    }
                    struct PlotIrises {
                      struct Info {
                        std::string x_label;
                        std::string y_label;
                        std::map<std::string, std::vector<std::pair<double, double>>> labeled_flowers;
                      };
                      Request request;
                      explicit PlotIrises(Request request) : request(std::move(request)) {}
                      void operator()(Info info) {
                        GNUPlot graph;
                        graph.Title("Iris flower data set.")
                            .Grid("back")
                            .XLabel(info.x_label)
                            .YLabel(info.y_label)
                            .ImageSize(800)
                            .OutputFormat("pngcairo");
                        for (const auto& per_label : info.labeled_flowers) {
                          graph.Plot(WithMeta([&per_label](Plotter& p) {
                            for (const auto& xy : per_label.second) {
                              p(xy.first, xy.second);
                            }
                          }).AsPoints().Name(per_label.first));
                        }
                        request(graph);
                      }
                    };
                    api.Transaction([x_dim, y_dim](TestDB::data_t data) {
                      PlotIrises::Info info;
                      for (size_t i = 1; i <= number_of_flowers; ++i) {
                        const LabeledFlower& flower = yoda::Dictionary<LabeledFlower>::Accessor(data)[i];
                        info.labeled_flowers[flower.label].emplace_back(flower.x[x_dim], flower.x[y_dim]);
                      }
                      info.x_label = dimension_names[x_dim];
                      info.y_label = dimension_names[y_dim];
                      return info;
                    }, PlotIrises(std::move(request)));
                  });

    HTTP(port).Join();
  }
  // LCOV_EXCL_STOP
#endif
}
