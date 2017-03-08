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

// Takes a free-form JSON, and, if its schema is strict, describes it in a human-readable format.
// The output format is a TSV, but only in a sense it can be copy-pasted into a spreadsheet editor
// for other humans to understand. By no means it's a TSV view of the incoming JSON.

#include "infer.h"

#include "../../Bricks/dflags/dflags.h"

DEFINE_string(input, "input_data.json", "The name of the input file containing the JSON to parse.");

DEFINE_string(output, ".current/output_schema.tsv", "The name of the output file to dump the schema as TSV.");

DEFINE_string(ignore, "", "The colon-separated list of JSON paths to ignore during schema inference.");

DEFINE_int32(number_of_example_values,
             20,
             "Dump string values and their counters if the number of distinct ones is no greater than this one.");

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  try {
    current::FileSystem::WriteStringToFile(
        current::utils::DescribeSchema(FLAGS_input,
                                       current::utils::TrackPath(current::utils::TrackPathIgnoreList(FLAGS_ignore)),
                                       FLAGS_number_of_example_values),
        FLAGS_output.c_str());
    return 0;
  } catch (const current::utils::InferSchemaException& e) {
    std::cerr << e.DetailedDescription() << std::endl;
    return -1;
  }
}
