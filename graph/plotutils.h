/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// TODO(dkorolev): Platform-specific implementation and guards.

#ifndef BRICKS_GRAPH_PLOTUTILS_H
#define BRICKS_GRAPH_PLOTUTILS_H

#include <cassert>
#include <vector>
#include <string>
#include <map>

#include "../file/file.h"
#include "../strings/util.h"

namespace bricks {
namespace plotutils {

// https://www.gnu.org/software/plotutils/manual/en/html_node/Plot-Options.html#Plot-Options
// 0: no axes, tick marks or labels.
// 1: a pair of axes, with tick marks and labels.
// 2: box around plot, with tick marks and labels.
// 3: box around plot, with tick marks and labels; also grid lines.
// 4: axes intersect at the origin, with tick marks and labels.
enum class GridStyle : int { None = 0, Axes = 1, Box = 2, Full = 3, AxesAtOrigin = 4 };

// https://www.gnu.org/software/plotutils/manual/en/html_node/Dataset-Options.html#Dataset-Options
// By convention, linemode #0 means no line at all (data points are disconnected).
// 1: red, solid
// 2: green, solid
// 3: blue, solid
// 4: magenta, solid
// 5: cyan, solid
// Linemodes #6 through #10 use the same five colors, but are dotted; linemodes #11 through #15 are dotdashed;
// linemodes #16 through #20 are shortdashed; and linemodes #21 through #25 are longdashed.
// So besides linemode #0, there are a total of 25 distinct colored linemodes
enum class LineMode : int {
  None = 0,
};
enum class LineColor : int { Red = 1, Green = 2, Blue = 3, Magenta = 4, Cyan = 5 };
enum class LineStyle : int { Solid = 0, Dotted = 5, DotDashed = 10, ShortDashed = 15, LongDashed = 20 };
inline LineMode CustomLineMode(LineColor color, LineStyle style) {
  return static_cast<LineMode>(static_cast<int>(color) + static_cast<int>(style));
}

// https://www.gnu.org/software/plotutils/manual/en/html_node/Dataset-Options.html#Dataset-Options
// The following table lists the first few symbols (by convention, symbol #0 means no symbol at all).
// 1: dot
// 2: plus (+)
// 3: asterisk (*)
// 4: circle
// 5: cross
enum class Symbol : int { No = 0, Dot = 1, Plus = 2, Asterisk = 3, Circle = 4, Cross = 5 };

class Plotutils {
 public:
  Plotutils(const std::vector<std::pair<double, double>>& data) : data_(new SinglePlot(data)) {}
  Plotutils(const std::vector<std::vector<std::pair<double, double>>>& data) : data_(new MultiPlot(data)) {}

  Plotutils& X(const std::string& x_label) {
    parameters_["-X"] = SomewhatEscape(x_label);
    return *this;
  }

  Plotutils& Y(const std::string& y_label) {
    parameters_["-Y"] = SomewhatEscape(y_label);
    return *this;
  }

  Plotutils& Label(const std::string& label) {
    parameters_["-L"] = SomewhatEscape(label);
    return *this;
  }

  Plotutils& LineWidth(double w) {
    parameters_["-W"] = bricks::strings::to_string(w);
    return *this;
  }

  Plotutils& BitmapSize(int x, int y = 0) {
    assert(x > 0);
    assert(y >= 0);
    parameters_["--bitmap-size"] = bricks::strings::Printf("%dx%d", x, y ? y : x);
    return *this;
  }

  Plotutils& GridStyle(GridStyle grid_style) {
    parameters_["-g"] = bricks::strings::to_string(static_cast<int>(grid_style));
    return *this;
  }

  Plotutils& LineMode(LineMode line_mode) {
    parameters_["-m"] = bricks::strings::to_string(static_cast<int>(line_mode));
    return *this;
  }

  Plotutils& Symbol(Symbol symbol, double size = 0) {
    parameters_["-S"] = bricks::strings::to_string(static_cast<int>(symbol));
    if (size) {
      parameters_["-S"] += ' ' + bricks::strings::to_string(size);
    }
    return *this;
  }

  Plotutils& OutputFormat(const std::string& output_format) {
    parameters_["-T"] = output_format;
    return *this;
  }

  operator std::string() const {
    const std::string input_file_name = bricks::FileSystem::GenTmpFileName();
    const std::string output_file_name = bricks::FileSystem::GenTmpFileName();
    const auto input_deleter = bricks::FileSystem::ScopedRemoveFile(input_file_name);
    const auto output_file_deleter = bricks::FileSystem::ScopedRemoveFile(output_file_name);
    {
      bricks::FileSystem::OutputFile f(input_file_name);
      data_->WriteToFile(f);
    }
    std::string cmdline = "graph -C";
    if (parameters_.find("-T") == parameters_.end()) {
      cmdline += " -T png";  // LCOV_EXCL_LINE
    }
    for (const auto& cit : parameters_) {
      cmdline += ' ' + cit.first;
      if (!cit.second.empty()) {
        cmdline += ' ' + cit.second;
      }
    }
    cmdline += " < " + input_file_name;
    cmdline += " > " + output_file_name;
    ::system(cmdline.c_str());
    return bricks::FileSystem::ReadFileAsString(output_file_name.c_str());
  }

  static inline std::string SomewhatEscape(const std::string& s) {
    std::string result;
    for (char c : s) {
      if (c == '"') {
        result += '\\';
      }
      result += c;
    }
    return '"' + result + '"';
  }

  struct Data {
    virtual void WriteToFile(bricks::FileSystem::OutputFile&) const = 0;
  };

  struct SinglePlot : Data {
    SinglePlot(const std::vector<std::pair<double, double>>& data) : data_(data) {}
    virtual void WriteToFile(bricks::FileSystem::OutputFile& f) const {
      for (const auto xy : data_) {
        f << xy.first << ' ' << xy.second << "\n";
      }
    }
    const std::vector<std::pair<double, double>>& data_;
  };

  struct MultiPlot : Data {
    MultiPlot(const std::vector<std::vector<std::pair<double, double>>>& data) : data_(data) {}
    virtual void WriteToFile(bricks::FileSystem::OutputFile& f) const {
      for (size_t i = 0; i < data_.size(); ++i) {
        if (i > 0) {
          f << "\n";
        }
        for (const auto xy : data_[i]) {
          f << xy.first << ' ' << xy.second << "\n";
        }
      }
    }
    const std::vector<std::vector<std::pair<double, double>>>& data_;
  };

  std::unique_ptr<Data> data_;
  std::map<std::string, std::string> parameters_;
};

}  // namespace plotutils
}  // namespace bricks

#endif  // BRICKS_GRAPH_PLOTUTILS_H
