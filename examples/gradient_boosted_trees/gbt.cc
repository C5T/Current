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

// Debugging macros first, before the headers. Uncomment the next line to run ~50x slower, but with paranoid checks.
// #define GBT_EXTRA_CHECKS
#ifndef GBT_EXTRA_CHECKS
#define GBT_EXTRA_CHECK(...)
#else
#define GBT_EXTRA_CHECK(x) x
#endif

#include "schema.h"
#include "iterable_subset.h"
#include "train.h"

DEFINE_string(input, "train.json", "The name of the input file.");
DEFINE_string(output, "ensemble.json", "The name of the output file.");
DEFINE_double(test_set_fraction, 0.33, "Fraction of examples to use as the test set; 0 to train on all data.");
DEFINE_uint32(trees, 2500, "The number of trees to train.");
DEFINE_uint32(tree_depth, 5, "If nonzero, the maximum depth of each tree.");
DEFINE_double(fraction_of_features_to_use_per_iteration, 0.4, "The fraction of features to use per tree.");
DEFINE_int32(debug_iterations_frequency, 25, "If nonzero, debug output each N-th iteration.");
DEFINE_bool(log_trees, false, "Set to true to log each tree in a pseudo-JSON (node.js-friendly) format.");

// TODO(dkorolev): Rand seed?

struct AllPoints {};

struct SimpleIterableRange {
  struct Iterator {
    size_t i;
    size_t operator*() const { return i; }
    void operator++() { ++i; }
    bool operator!=(const Iterator& rhs) const { return i != rhs.i; }
  };
  const size_t n_;
  explicit SimpleIterableRange(size_t n) : n_(n) {}
  Iterator begin() const { return Iterator{0u}; }
  Iterator end() const { return Iterator{n_}; }
};

SimpleIterableRange IterateByFilter(size_t n, AllPoints) { return SimpleIterableRange(n); }

const std::vector<size_t>& IterateByFilter(size_t, const std::vector<size_t>& filter) { return filter; }

template <typename SCORE, typename LABEL, typename FILTER = AllPoints>
double ComputeAreaUnderPrecisionRecallCurve(const std::vector<SCORE>& scores,
                                            const std::vector<LABEL>& labels,
                                            FILTER&& filter = FILTER()) {
  CURRENT_ASSERT(scores.size() == labels.size());
  size_t total_points = 0u;                                                // The total number of examples.
  size_t total_positives = 0u;                                             // The number of positive examples.
  std::map<SCORE, std::pair<size_t, size_t>, std::greater<SCORE>> sorted;  // Score -> [ # good, # total ].
  for (const size_t i : IterateByFilter(scores.size(), std::forward<FILTER>(filter))) {
    const auto label = static_cast<bool>(labels[i]);
    auto& rhs = sorted[scores[i]];
    ++total_points;
    ++rhs.second;
    if (label) {
      ++rhs.first;
      ++total_positives;
    }
  }

  double twice_area_under_pr_curve = 0.0;
  double previous_precision = 1.0;
  double previous_recall = 0.0;

  size_t scanned = 0u;
  double scanned_positives = 0u;

  for (const auto& per_score : sorted) {
    const double save_scanned_positives = scanned_positives;
    for (size_t i = 0; i < per_score.second.second; ++i) {
      ++scanned;
      scanned_positives = save_scanned_positives + 1.0 * (i + 1) * per_score.second.first / per_score.second.second;
      const double recall = scanned_positives / total_positives;
      const double precision = scanned_positives / scanned;
      twice_area_under_pr_curve += (precision + previous_precision) * (recall - previous_recall);
      previous_recall = recall;
      previous_precision = precision;
    }
  }
  twice_area_under_pr_curve += previous_precision * (1.0 - previous_recall);

  return 0.5 * twice_area_under_pr_curve;
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  const auto input =
      ParseJSON<InputOfBinaryLabelsAndBinaryFeatures>(current::FileSystem::ReadFileAsString(FLAGS_input));

  const size_t N = input.labels.size();
  const size_t M = input.GetNumberOfFeatures();

  std::vector<std::vector<size_t>> examples(2);
  for (size_t i = 0; i < N; ++i) {
    examples[input.labels[i] ? 1 : 0].push_back(i);
  }

  std::vector<size_t> features_list(M);
  for (size_t f = 0; f < M; ++f) {
    features_list[f] = f;
  }

  std::vector<std::vector<uint32_t>> transposed_adjacency_lists(M);
  std::vector<std::vector<bool>> dense_transposed_matrix(M, std::vector<bool>(N, false));
  for (size_t i = 0; i < N; ++i) {
    for (uint32_t f : input.matrix[i]) {
      transposed_adjacency_lists[f].push_back(i);
      dense_transposed_matrix[f][i] = true;
    }
  }

  std::vector<size_t> train_points;
  std::vector<size_t> test_points;
  if (FLAGS_test_set_fraction) {
    for (int label = 0; label < 2; ++label) {
      std::random_shuffle(examples[label].begin(), examples[label].end());
      CURRENT_ASSERT(FLAGS_test_set_fraction > 0.0);
      CURRENT_ASSERT(FLAGS_test_set_fraction < 1.0);
      const size_t c = examples[label].size() * FLAGS_test_set_fraction;
      CURRENT_ASSERT(c > 0);
      CURRENT_ASSERT(c < examples[label].size());
      for (size_t i = 0; i < c; ++i) {
        train_points.push_back(examples[label][i]);
      }
      for (size_t i = c; i < examples[label].size(); ++i) {
        test_points.push_back(examples[label][i]);
      }
    }
  } else {
    for (size_t i = 0; i < N; ++i) {
      train_points.push_back(i);
      test_points.push_back(i);
    }
  }

  std::ostream* logging_ostream = nullptr;
  if (FLAGS_log_trees) {
    logging_ostream = &std::cout;
  }

  // The data structured to keep building new trees.
  TreeBuilder builder(N, transposed_adjacency_lists, dense_transposed_matrix, input.feature_names, logging_ostream);
  const double param_ff = FLAGS_fraction_of_features_to_use_per_iteration;
  CURRENT_ASSERT(param_ff > 0);
  CURRENT_ASSERT(param_ff <= 1);
  const size_t param_td = FLAGS_tree_depth ? static_cast<size_t>(FLAGS_tree_depth) : static_cast<size_t>(-1);

  // Model output, integer, sum over all the trees.
  std::vector<int64_t> output(N, 0ll);
  std::vector<int64_t> goal(N, 0ll);

  for (size_t tree = 0; tree < static_cast<size_t>(FLAGS_trees); ++tree) {
    // The objective function for this tree: softmax penalty. TODO(dkorolev): These coefficients should be parameters.
    for (size_t i = 0; i < N; ++i) {
      double x = 0.001 * output[i] * (input.labels[i] ? -1 : +1);
      if (x < +20.0) {
        x = std::log(1.0 + std::exp(x));
      }
      if (!input.labels[i]) {
        x = -x;
      }
      goal[i] = static_cast<int64_t>(x * 100);
    }

    CURRENT_ASSERT(builder.TreesCount() == tree);
    std::random_shuffle(features_list.begin(), features_list.end());
    TreeIndex ti = builder.BuildTree(
        goal,
        train_points,
        std::vector<size_t>(features_list.begin(), features_list.begin() + features_list.size() * param_ff),
        param_td);
    CURRENT_ASSERT(builder.TreesCount() == tree + 1);

    for (size_t i = 0; i < N; ++i) {
      output[i] += builder.Apply(ti, [&](size_t feature_index) { return dense_transposed_matrix[feature_index][i]; });
    }

    if (FLAGS_debug_iterations_frequency && ((tree + 1) % FLAGS_debug_iterations_frequency) == 0) {
      const double area = ComputeAreaUnderPrecisionRecallCurve(output, input.labels, test_points);
      std::cerr << "Trees trained: " << (tree + 1) << ", area under the PR curve: " << area
                << ", nodes: " << builder.NodesCount() << std::endl;
    }
  }

  if (!FLAGS_output.empty()) {
    current::FileSystem::WriteStringToFile(JSON(builder.Ensemble()), FLAGS_output.c_str());
  }
}
