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

DEFINE_string(input, "input.json", "The name of the input file.");
DEFINE_double(fraction, 0.33, "The fraction of examples to split into the second output.");
DEFINE_string(output1, "train.json", "The name of the first output file.");
DEFINE_string(output2, "test.json", "The name of the first output file.");

// TODO(dkorolev): Rand seed?

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  CURRENT_ASSERT(FLAGS_fraction > 0.0);
  CURRENT_ASSERT(FLAGS_fraction < 1.0);

  const auto input =
      ParseJSON<InputOfBinaryLabelsAndBinaryFeatures>(current::FileSystem::ReadFileAsString(FLAGS_input));

  const size_t N = input.labels.size();

  std::vector<std::vector<size_t>> examples(2);
  for (size_t i = 0; i < N; ++i) {
    examples[input.labels[i] ? 1 : 0].push_back(i);
  }

  std::vector<size_t> split(N, 1u);
  for (int label = 0; label < 2; ++label) {
    std::random_shuffle(examples[label].begin(), examples[label].end());
    const size_t c = examples[label].size() * FLAGS_fraction;
    CURRENT_ASSERT(c > 0);
    CURRENT_ASSERT(c < examples[label].size());
    for (size_t i = 0; i < c; ++i) {
      split[examples[label][i]] = 0u;
    }
  }

  InputOfBinaryLabelsAndBinaryFeatures output[2];
  output[0].features = output[1].features = input.features;

  if (Exists(input.point_names)) {
    output[0].point_names = std::vector<std::string>();
    output[1].point_names = std::vector<std::string>();
  }

  for (size_t i = 0; i < N; ++i) {
    auto& placeholder = output[split[i]];
    if (Exists(input.point_names)) {
      Value(placeholder.point_names).push_back(Value(input.point_names)[i]);
    }
    placeholder.labels.push_back(input.labels[i]);
    placeholder.matrix.push_back(input.matrix[i]);
  }

  current::FileSystem::WriteStringToFile(JSON(output[0]), FLAGS_output1.c_str());
  current::FileSystem::WriteStringToFile(JSON(output[1]), FLAGS_output2.c_str());
}
