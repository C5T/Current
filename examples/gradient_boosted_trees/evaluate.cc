/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2017 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "schema.h"
#include "../../bricks/graph/gnuplot.h"

DEFINE_string(ensemble, "ensemble.json", "The name of the model ensemble file.");
DEFINE_string(input, "test.json", "The name of the file containing the dataset to test the model against.");
DEFINE_string(chart, ".current/pr.png", "The name of the output plot file.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  const auto input =
      ParseJSON<InputOfBinaryLabelsAndBinaryFeatures>(current::FileSystem::ReadFileAsString(FLAGS_input));
  const auto ensemble = ParseJSON<TreeEnsemble>(current::FileSystem::ReadFileAsString(FLAGS_ensemble));

  const size_t N = input.points.size();
  const size_t M = input.features.size();
  std::vector<std::vector<bool>> X(N, std::vector<bool>(M));
  for (size_t point = 0; point < N; ++point) {
    for (int32_t feature : input.matrix[point]) {
      X[point][feature] = true;
    }
  }

  std::vector<int64_t> score(N, 0ll);
  for (size_t point = 0; point < N; ++point) {
    for (TreeIndex index : ensemble.trees) {
      size_t i = static_cast<size_t>(index);
      CURRENT_ASSERT(i < ensemble.nodes.size());
      while (!ensemble.nodes[i].leaf) {
        i = X[point][ensemble.nodes[i].feature] ? ensemble.nodes[i].yes : ensemble.nodes[i].no;
        CURRENT_ASSERT(i != static_cast<uint32_t>(-1));
        CURRENT_ASSERT(i < ensemble.nodes.size());
      }
      score[point] += ensemble.nodes[i].value;
    }
  }

  size_t total_points = 0u;
  size_t total_positives = 0u;
  std::map<int64_t, std::pair<size_t, size_t>, std::greater<int64_t>> sorted;
  for (size_t point = 0; point < N; ++point) {
    const auto label = static_cast<bool>(input.labels[point]);
    auto& rhs = sorted[score[point]];
    ++total_points;
    ++rhs.second;
    if (label) {
      ++rhs.first;
      ++total_positives;
    }
  }

  {
    using namespace current::gnuplot;
    GNUPlot gnuplot;

    gnuplot.Title("Precision/recall")
        .Grid("back")
        .XLabel("Recall")
        .YLabel("Precision")
        .ImageSize(1000)
        .OutputFormat("png")
        .Plot(WithMeta([&](Plotter p) {
          double twice_area_under_pr_curve = 0.0;
          double previous_precision = 1.0;
          double previous_recall = 0.0;

          size_t scanned = 0u;
          double scanned_positives = 0u;

          p(0.0, 1.0);
          for (const auto& per_score : sorted) {
            const double save_scanned_positives = scanned_positives;
            for (size_t point = 0; point < per_score.second.second; ++point) {
              ++scanned;
              scanned_positives =
                  save_scanned_positives + 1.0 * (point + 1) * per_score.second.first / per_score.second.second;
              const double recall = scanned_positives / total_positives;
              const double precision = scanned_positives / scanned;
              p(recall, precision);
              twice_area_under_pr_curve += (precision + previous_precision) * (recall - previous_recall);
              previous_recall = recall;
              previous_precision = precision;
            }
          }
          p(1.0, 0.0);
          twice_area_under_pr_curve += previous_precision * (1.0 - previous_recall);

          std::cout << "Area under the P/R curve: " << 0.5 * twice_area_under_pr_curve << std::endl;
        })
                  .Name("Ensemble output")
                  .LineWidth(3));

    current::FileSystem::WriteStringToFile(gnuplot, FLAGS_chart.c_str());
  }
}
