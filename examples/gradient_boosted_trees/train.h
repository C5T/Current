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

#ifndef EXAMPLES_GRADIENT_BOOSTED_TREES_GBT_TRAIN_H
#define EXAMPLES_GRADIENT_BOOSTED_TREES_GBT_TRAIN_H

#include "schema.h"

// Debugging macros first, before the headers. Uncomment the next line to run ~50x slower, but with paranoid checks.
// #define GBT_EXTRA_CHECKS
#ifndef GBT_EXTRA_CHECKS
#define GBT_EXTRA_CHECK(...)
#else
#define GBT_EXTRA_CHECK(x) x
#endif

#include "schema.h"
#include "iterable_subset.h"

#define GBT_LARGE_EPSILON 0.001  // For relative improvements in standard deviation, which is computed in doubles.

class TreeBuilder {
 private:
  // Immutable members: The data to operate upon.
  const size_t n_;                                 // The total number of training examples (not all may be in use).
  const size_t m_;                                 // The total number of features (not all may be in use).
  const std::vector<std::vector<uint32_t>>& g_;    // The features-to-points adjacency lists, `.size()` == m_.
  const std::vector<std::vector<bool>>& matrix_;   // The dense represeneation of the [feature][points] -> 0/1 matrix.
  std::ostream* dump_ostream_ = nullptr;           // If set, the node.js-compliant JSON with the tree will be dumped.

  const Optional<std::vector<std::string>>& feature_names_;  // Feature names, for debug output purposes.

  // Mutable members: The goal to train towards, and the training parameters.
  const std::vector<int64_t>* py_ = nullptr;  // The pointer to the vector of objective functions per training example.
  size_t max_depth_;                          // How deeply to train the tree.
  IterableSubset points_to_consider_;         // The points to be considered.
  std::vector<size_t> features_to_consider_;  // The features to be considired (rearrangable).
  size_t* features_to_consider_begin_;
  size_t* features_to_consider_end_;

  // Output members: The tree(s) being built. The trees are stored in a single array of nodes or leaves.
  TreeEnsemble ensemble_;

  static std::vector<size_t> ParseFeaturesToConsider(std::vector<size_t> features_to_consider, size_t m) {
    if (!features_to_consider.empty()) {
      CURRENT_ASSERT(std::unordered_set<size_t>(features_to_consider.begin(), features_to_consider.end()).size() ==
                     features_to_consider.size());
      for (size_t f : features_to_consider) {
        CURRENT_ASSERT(f < m);
      }
      return features_to_consider;
    } else {
      std::vector<size_t> result(m);
      for (size_t i = 0; i < m; ++i) {
        result[i] = i;
      }
      return result;
    }
  }

 public:
  TreeBuilder(size_t n,
              const std::vector<std::vector<uint32_t>>& transposed_matrix_adjacency_lists,
              const std::vector<std::vector<bool>>& transposed_matrix,
              const Optional<std::vector<std::string>>& feature_names,
              std::ostream* dump_ostream = nullptr)
      : n_(n),
        m_(transposed_matrix_adjacency_lists.size()),
        g_(transposed_matrix_adjacency_lists),
        matrix_(transposed_matrix),
        dump_ostream_(dump_ostream),
        feature_names_(feature_names),
        points_to_consider_(n_) {
    for (const auto& col : g_) {
      for (uint32_t point_index : col) {
        CURRENT_ASSERT(point_index < static_cast<uint32_t>(n_));
      }
      for (size_t i = 1; i < col.size(); ++i) {
        CURRENT_ASSERT(col[i] > col[i - 1]);  // The transposed matrix's adjacency lists should be sorted.
      }
    }
  }

  TreeIndex BuildTree(const std::vector<int64_t>& goal,
                      const std::vector<size_t>& points_to_use,
                      std::vector<size_t> features_to_use = std::vector<size_t>(),
                      size_t max_depth = static_cast<size_t>(-1)) {
    CURRENT_ASSERT(goal.size() == n_);
    py_ = &goal;

    max_depth_ = max_depth;

    features_to_consider_ = ParseFeaturesToConsider(std::move(features_to_use), m_);
    features_to_consider_begin_ = &features_to_consider_[0];
    features_to_consider_end_ = &features_to_consider_[0] + features_to_consider_.size();

    std::vector<bool> point_used(n_, false);
    for (size_t i : points_to_use) {
      CURRENT_ASSERT(i < n_);
      CURRENT_ASSERT(!point_used[i]);
      point_used[i] = true;
    }

    TreeIndex result = TreeIndex::Invalid;
    points_to_consider_.Partition(
        [&](size_t point) { return point_used[point]; }, [&]() { result = DoBuildTree(); }, []() {});
    return result;
  }

  TreeIndex DoBuildTree() {
    if (dump_ostream_) {
      *dump_ostream_ << "console.log(JSON.stringify({\n";
    }
    const TreeIndex result = BuildTreeRecursively(0u);
    ensemble_.trees.push_back(result);
    if (dump_ostream_) {
      *dump_ostream_ << "}, null, 1));" << std::endl;
    }
    return result;
  }

  const TreeEnsemble& Ensemble() const { return ensemble_; }
  size_t TreesCount() const { return ensemble_.trees.size(); }
  size_t NodesCount() const { return ensemble_.nodes.size(); }

  template <typename F>
  int64_t Apply(TreeIndex index, F&& point) {
    size_t i = static_cast<size_t>(index);
    CURRENT_ASSERT(i < ensemble_.nodes.size());
    while (!ensemble_.nodes[i].leaf) {
      i = point(ensemble_.nodes[i].feature) ? ensemble_.nodes[i].yes : ensemble_.nodes[i].no;
      CURRENT_ASSERT(i != static_cast<uint32_t>(-1));
      CURRENT_ASSERT(i < ensemble_.nodes.size());
    }
    return ensemble_.nodes[i].value;
  }

 private:
  TreeIndex BuildTreeRecursively(size_t depth) {
    static const size_t indent_max_spaces = 1000u * 100u;
    static const std::string indent_placeholder(indent_max_spaces, ' ');
    const size_t indent_size = ((depth + 1) * 2);
    CURRENT_ASSERT(indent_size <= indent_placeholder.length());
    const char* indent = indent_placeholder.c_str() + indent_placeholder.length() - indent_size;

    const size_t n_points = points_to_consider_.size();
    CURRENT_ASSERT(n_points > 0);

    // Allocate a new node to build the tree from.
    const size_t node_index = ensemble_.nodes.size();
    ensemble_.nodes.resize(ensemble_.nodes.size() + 1u);

    // Sum and sum of squares of the values of the objective function per points considered.
    int64_t sum_p1 = 0;
    int64_t sum_p2 = 0;
    GBT_EXTRA_CHECK(size_t safe_n = 0u);
    for (size_t point : points_to_consider_) {
      const int64_t y = (*py_)[point];
      sum_p1 += y;
      sum_p2 += y * y;
      GBT_EXTRA_CHECK(++safe_n);
    }
    GBT_EXTRA_CHECK(CURRENT_ASSERT(safe_n == n_points));

    const double mean_y = sum_p1 * (1.0 / n_points);
    const double baseline_penalty = sum_p2 - sum_p1 * sum_p1 * (1.0 / n_points);

    // The penalty function is the sum of squares of the differences between the objective function for each
    // considered point and the average value of the objective function across all considered points.
    if (sum_p2 * static_cast<int64_t>(n_points) == sum_p1 * sum_p1) {
      // Compared `baseline_penalty` to zero in integers. -- D.K.
      if (dump_ostream_) {
        *dump_ostream_ << indent << "count: " << n_points << ", y: " << mean_y << "\n";
      }
      ensemble_.nodes[node_index].leaf = true;
      ensemble_.nodes[node_index].value = mean_y;
    } else {
      if (dump_ostream_) {
        *dump_ostream_ << indent << "count: " << n_points << ", y: " << sum_p1 * (1.0 / n_points)
                       << ", y_stddev: " << std::sqrt(baseline_penalty / n_points);
      }

      GBT_EXTRA_CHECK({
        int64_t p1 = 0;
        int64_t p2 = 0;
        for (size_t point = 0; point < n_; ++point) {
          if (points_to_consider_[point]) {
            const int64_t y = y_[point];
            p1 += y;
            p2 += y * y;
          }
        }
        CURRENT_ASSERT(p1 == sum_p1);
        CURRENT_ASSERT(p2 == sum_p2);

        double slowly_computed_penalty = 0.0;
        double mean = 1.0 * sum_p1 / n_points;
        for (size_t point = 0; point < n_; ++point) {
          if (points_to_consider_[point]) {
            const double d = y_[point] - mean;
            slowly_computed_penalty += d * d;
          }
        }
        const double ratio = slowly_computed_penalty / baseline_penalty;
        CURRENT_ASSERT(ratio >= 1.0 - GBT_LARGE_EPSILON && ratio <= 1.0 + GBT_LARGE_EPSILON);
      });

      // Find the best features to split the tree by.
      // The best feature is the one that minimizes the sum of penalties in the left and right subtrees.
      std::pair<double, size_t*> best_candidate(1.0 - GBT_LARGE_EPSILON, nullptr);
      for (size_t* feature_it = features_to_consider_begin_; feature_it != features_to_consider_end_; ++feature_it) {
        const size_t feature = *feature_it;
        int64_t candidate_lhs_sum_p1 = 0;
        int64_t candidate_lhs_sum_p2 = 0;
        uint64_t candidate_lhs_n = 0;
        for (const int32_t point : g_[feature]) {
          if (points_to_consider_[point]) {
            const int64_t y = (*py_)[point];
            candidate_lhs_sum_p1 += y;
            candidate_lhs_sum_p2 += y * y;
            ++candidate_lhs_n;
          }
        }
        GBT_EXTRA_CHECK(CURRENT_ASSERT(candidate_lhs_n <= n_points));
        if (candidate_lhs_n > 0 && candidate_lhs_n < n_points) {
          const int64_t candidate_rhs_sum_p1 = sum_p1 - candidate_lhs_sum_p1;
          const int64_t candidate_rhs_sum_p2 = sum_p2 - candidate_lhs_sum_p2;
          const uint64_t candidate_rhs_n = n_points - candidate_lhs_n;

          const double candidate_penalty =
              (candidate_lhs_sum_p2 - candidate_lhs_sum_p1 * candidate_lhs_sum_p1 * (1.0 / candidate_lhs_n)) +
              (candidate_rhs_sum_p2 - candidate_rhs_sum_p1 * candidate_rhs_sum_p1 * (1.0 / candidate_rhs_n));

          const double ratio = candidate_penalty / baseline_penalty;
          GBT_EXTRA_CHECK({
            int64_t lhs_p1 = 0;
            int64_t lhs_p2 = 0;
            int64_t rhs_p1 = 0;
            int64_t rhs_p2 = 0;
            for (size_t point = 0; point < n_; ++point) {
              if (points_to_consider_[point]) {
                CURRENT_ASSERT(std::binary_search(g_[feature].begin(), g_[feature].end(), point) ==
                               matrix_[feature][point]);
                const int64_t y = y_[point];
                if (matrix_[feature][point]) {
                  lhs_p1 += y;
                  lhs_p2 += y * y;
                } else {
                  rhs_p1 += y;
                  rhs_p2 += y * y;
                }
              }
            }
            CURRENT_ASSERT(lhs_p1 + rhs_p1 == sum_p1);
            CURRENT_ASSERT(lhs_p2 + rhs_p2 == sum_p2);

            CURRENT_ASSERT(lhs_p1 == candidate_lhs_sum_p1);
            CURRENT_ASSERT(lhs_p2 == candidate_lhs_sum_p2);

            CURRENT_ASSERT(rhs_p1 == candidate_rhs_sum_p1);
            CURRENT_ASSERT(rhs_p2 == candidate_rhs_sum_p2);
          });

#define GBT_DEBUG_DUMP(x) std::cerr << __LINE__ << ' ' << #x << " = " << x << std::endl  // Only used in debug mode.
          // Total penalty should be the same or better than the baseline, but keep machine precision in mind.
          GBT_EXTRA_CHECK(if (!(ratio < 1.0 + GBT_LARGE_EPSILON)) {
            std::cerr << ratio << std::endl;
            std::cerr << baseline_penalty << ' ' << candidate_penalty << std::endl;
            GBT_DEBUG_DUMP(candidate_lhs_sum_p2);
            GBT_DEBUG_DUMP(candidate_lhs_sum_p1 * candidate_lhs_sum_p1);
            GBT_DEBUG_DUMP(candidate_lhs_n);
            GBT_DEBUG_DUMP(candidate_rhs_sum_p2);
            GBT_DEBUG_DUMP(candidate_rhs_sum_p1 * candidate_rhs_sum_p1);
            GBT_DEBUG_DUMP(candidate_rhs_n);
          });
#undef GBT_DEBUG_DUMP
          CURRENT_ASSERT(ratio < 1.0 + GBT_LARGE_EPSILON);
          best_candidate = std::min(best_candidate, std::make_pair(ratio, feature_it));
        }
      }

      if (best_candidate.second && depth < max_depth_) {
        size_t pivot_feature = *best_candidate.second;
        if (dump_ostream_) {
          if (Exists(feature_names_)) {
            *dump_ostream_ << ", feature: " << JSON(Value(feature_names_)[pivot_feature]) << ", split: { yes: {\n";
          } else {
            *dump_ostream_ << ", feature #: " << pivot_feature << ", split: { yes: {\n";
          }
        }
        std::swap(*best_candidate.second, *features_to_consider_begin_);
        ++features_to_consider_begin_;
        const std::vector<bool>& matrix_row = matrix_[pivot_feature];
        ensemble_.nodes[node_index].leaf = false;
        ensemble_.nodes[node_index].feature = static_cast<uint64_t>(pivot_feature);
        points_to_consider_.Partition(
            [&](size_t point) {
              GBT_EXTRA_CHECK(CURRENT_ASSERT(std::binary_search(g_[pivot_feature].begin(),
                                                                g_[pivot_feature].end(),
                                                                point) == matrix_[pivot_feature][point]));
              // NOTE: This is only marginally faster than binary search for the cases I ran. -- D.K.
              //       Which means the code can support huge inputs as long as they are sparse. -- D.K.
              return matrix_row[point];
            },
            [&]() { ensemble_.nodes[node_index].yes = static_cast<size_t>(BuildTreeRecursively(depth + 1)); },
            [&]() {
              if (dump_ostream_) {
                *dump_ostream_ << indent << "}, no: {\n";
              }
              ensemble_.nodes[node_index].no = static_cast<size_t>(BuildTreeRecursively(depth + 1));
              if (dump_ostream_) {
                *dump_ostream_ << indent << "} },\n";
              }
            });
        --features_to_consider_begin_;
        std::swap(*best_candidate.second, *features_to_consider_begin_);
      } else {
        if (dump_ostream_) {
          *dump_ostream_ << '\n';
        }
        ensemble_.nodes[node_index].leaf = true;
        ensemble_.nodes[node_index].value = mean_y;
      }
    }

    return static_cast<TreeIndex>(node_index);
  }

  TreeBuilder(const TreeBuilder&) = delete;
  TreeBuilder(TreeBuilder&&) = delete;
  TreeBuilder& operator=(const TreeBuilder&) = delete;
  TreeBuilder& operator=(TreeBuilder&&) = delete;
};

#endif  // EXAMPLES_GRADIENT_BOOSTED_TREES_GBT_TRAIN_H
