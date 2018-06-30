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

#ifndef BRICKS_GRAPH_GNUPLOT_H
#define BRICKS_GRAPH_GNUPLOT_H

#include <vector>
#include <string>
#include <map>

#include "../file/file.h"
#include "../strings/util.h"
#include "../system/syscalls.h"

namespace current {
namespace gnuplot {

static inline std::string Escape(const std::string& s) {
  std::string result;
  for (char c : s) {
    if (c == '"') {
      result += '\\';
    }
    result += c;
  }
  return '"' + result + '"';
}

struct Plotter {
  current::FileSystem::OutputFile& of_;
  explicit Plotter(current::FileSystem::OutputFile& of) : of_(of) {}
  template <typename T1, typename T2>
  void operator()(T1&& x, T2&& y) {
    of_ << x << ' ' << y << std::endl;
  }
  template <typename T1, typename T2, typename T3>
  void operator()(T1&& x, T2&& y, T3&& z) {
    // `z` is usually `label`. But it can go places.
    of_ << x << ' ' << y << ' ' << z << std::endl;
  }
};

struct PlotDataBase {
  virtual ~PlotDataBase() = default;
  virtual void AppendData(current::FileSystem::OutputFile&) const = 0;
  virtual void AppendMetadata(current::FileSystem::OutputFile&) const = 0;
};

struct PlotDataFromFunction : PlotDataBase {
  std::function<void(Plotter&)> f_;
  std::string meta_;
  explicit PlotDataFromFunction(std::function<void(Plotter&)> f, const std::string& meta = " t 'Graph' with lines lw 5")
      : f_(f), meta_(meta) {}
  virtual void AppendMetadata(current::FileSystem::OutputFile& of) const override { of << ' ' << meta_; }
  virtual void AppendData(current::FileSystem::OutputFile& of) const override {
    Plotter p(of);
    f_(p);
  }
};

class WithMeta {
 public:
  explicit WithMeta(std::function<void(Plotter&)> f) : f_(f) {}

  WithMeta& Name(const std::string& name) {
    proto_meta_ += " t " + Escape(name);
    return *this;
  }
  WithMeta& Color(const std::string& color) {
    proto_meta_ += " lc " + color;
    return *this;
  }
  WithMeta& LineWidth(double lw) {
    type_ = "lines";
    proto_meta_ += strings::Printf(" lw %lf ", lw);
    return *this;
  }
  WithMeta& AsPoints() {
    type_ = "points";
    return *this;
  }
  WithMeta& AsLabels() {
    type_ = "labels";
    return *this;
  }
  WithMeta& Extra(const std::string& extra) {
    proto_meta_ += ' ';
    proto_meta_ += extra;
    return *this;
  }

  std::function<void(Plotter&)> GetFunction() const { return f_; }
  std::string ComposeMeta() const { return "with " + type_ + proto_meta_; }

 private:
  std::function<void(Plotter&)> f_;
  std::string type_ = "lines";
  std::string proto_meta_ = "";
};

struct GNUPlot {
  std::string output_format_ = "png";
  size_t x_ = 800;
  size_t y_ = 800;
  std::map<std::string, std::string> parameters_;
  std::vector<std::unique_ptr<PlotDataBase>> plots_;

  GNUPlot() = default;

  GNUPlot& ImageSize(size_t x, size_t y = 0) {
    x_ = x;
    y_ = y ? y : x;  // Square by default.
    return *this;
  }

  GNUPlot& Title(const std::string& title) {
    parameters_["title"] = Escape(title);
    return *this;
  }

  GNUPlot& NoTitle() {
    parameters_["title"] = "";
    return *this;
  }

  GNUPlot& XLabel(const std::string& label) {
    parameters_["xlabel"] = Escape(label);
    return *this;
  }

  GNUPlot& YLabel(const std::string& label) {
    parameters_["ylabel"] = Escape(label);
    return *this;
  }

  GNUPlot& Grid(const std::string& grid) {
    parameters_["grid"] = grid;
    return *this;
  }

  GNUPlot& NoKey() {
    parameters_["key"] = "";
    return *this;
  }

  GNUPlot& KeyTitle(const std::string& key_title) {
    parameters_["key"] = "title " + Escape(key_title);
    return *this;
  }

  GNUPlot& NoTics() {
    parameters_["tics"] = "";
    return *this;
  }

  GNUPlot& NoBorder() {
    parameters_["border"] = "";
    return *this;
  }

  GNUPlot& XRange(double x, double y) {
    parameters_["xrange"] = strings::Printf("[%+lf:%+lf]", x, y);
    return *this;
  }

  GNUPlot& YRange(double x, double y) {
    parameters_["yrange"] = strings::Printf("[%+lf:%+lf]", x, y);
    return *this;
  }

  GNUPlot& OutputFormat(const std::string& output_format) {
    output_format_ = output_format;
    return *this;
  }

  // TODO(dkorolev): `template<typename T> GNUPlot& Plot(T&& plot)` to support pre-populated plots as well.
  GNUPlot& Plot(std::function<void(Plotter&)> plot) {
    plots_.emplace_back(new PlotDataFromFunction(plot));
    return *this;
  }

  GNUPlot& Plot(const WithMeta& plot) {
    plots_.emplace_back(new PlotDataFromFunction(plot.GetFunction(), plot.ComposeMeta()));
    return *this;
  }

  operator std::string() const {
    CURRENT_ASSERT(!plots_.empty());  // TODO(dkorolev): Exception?

    const std::string input_file_name = current::FileSystem::GenTmpFileName();
    const std::string output_file_name = current::FileSystem::GenTmpFileName();
    const auto input_deleter = current::FileSystem::ScopedRmFile(input_file_name);
    const auto output_file_deleter = current::FileSystem::ScopedRmFile(output_file_name);
    {
      current::FileSystem::OutputFile f(input_file_name);
      f << "set term " << output_format_ << " size " << x_ << ',' << y_ << std::endl;
      for (const auto& p : parameters_) {
        if (!p.second.empty()) {
          f << "set " << p.first << ' ' << p.second << std::endl;
        } else {
          f << "unset " << p.first << std::endl;  // LCOV_EXCL_LINE
        }
      }
      for (size_t i = 0; i < plots_.size(); ++i) {
        f << (i ? ", " : "plot ") << "'-'";
        plots_[i]->AppendMetadata(f);
      }
      f << std::endl;
      for (const auto& p : plots_) {
        p->AppendData(f);
        f << "end" << std::endl;
      }
    }
    if (output_format_ != "gnuplot") {
      CURRENT_ASSERT(!bricks::system::SystemCall(
                         strings::Printf("gnuplot <%s >%s", input_file_name.c_str(), output_file_name.c_str())));
      return current::FileSystem::ReadFileAsString(output_file_name.c_str());
    } else {
      // For unit tests, just compare the inputs.
      return current::FileSystem::ReadFileAsString(input_file_name.c_str());
    }
  }
};

}  // namespace gnuplot
}  // namespace current

#endif  // BRICKS_GRAPH_GNUPLOT_H
