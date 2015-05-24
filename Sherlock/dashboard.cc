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

#include "sherlock.h"

#include "../Bricks/time/chrono.h"
#include "../Bricks/cerealize/cerealize.h"
#include "../Bricks/net/api/api.h"
#include "../Bricks/dflags/dflags.h"

DEFINE_int32(port, 8191, "Local port to use.");

template <typename Y>
struct VizPoint {
  double x;
  Y y;
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(x), CEREAL_NVP(y));
  }
};

typedef VizPoint<double> DoublePoint;
typedef VizPoint<std::string> StringPoint;

// TODO(dkorolev): This class should be moved into `sherlock.h`.
template <typename T>
class ServeJSONOverHTTP {
 public:
  ServeJSONOverHTTP(Request r)
      : http_request_scope_(std::move(r)), http_response_(http_request_scope_.SendChunkedResponse()) {}

  inline bool Entry(const T& entry, size_t index, size_t total) {
    static_cast<void>(index);
    static_cast<void>(total);
    try {
      http_response_(entry, "point");
      return true;
    } catch (const bricks::net::NetworkException&) {
      return false;
    }
  }

  inline void Terminate() { http_response_("TERMINATED!\n"); }

 private:
  Request http_request_scope_;  // Need to keep `Request` in scope, for the lifetime of the chunked response.
  bricks::net::HTTPServerConnection::ChunkedResponseSender http_response_;

  ServeJSONOverHTTP() = delete;
  ServeJSONOverHTTP(const ServeJSONOverHTTP&) = delete;
  void operator=(const ServeJSONOverHTTP&) = delete;
  ServeJSONOverHTTP(ServeJSONOverHTTP&&) = delete;
  void operator=(ServeJSONOverHTTP&&) = delete;
};

struct ExampleConfig {
  std::string layout_url = "/layout";

  // For the sake of the demo we put an empty array of `data_hostnames`
  // that results in the option being ignored by the frontend.
  // In production, this array should be filled with a set of alternative
  // hostnames that all resolve to the same backend. This technique is used
  // to overcome the browser domain-based connection limit. The frontend selects
  // a domain from this array for every new connection via a simple round-robin.
  std::vector<std::string> data_hostnames;

  // The static template.
  std::string dashboard_template;

  ExampleConfig()
      : dashboard_template(bricks::FileSystem::ReadFileAsString(
            bricks::FileSystem::JoinPath("static", "knowsheet-demo.html"))) {}

  template <typename A>
  void save(A& ar) const {
    ar(CEREAL_NVP(layout_url), CEREAL_NVP(data_hostnames), CEREAL_NVP(dashboard_template));
  }
};

struct PlotMeta {
  struct Options {
    std::string header_text = "Real-time Data Made Easy";
    std::string color = "blue";
    double min = -1;
    double max = 1;
    double time_interval = 5000;
    template <typename A>
    void save(A& ar) const {
      // TODO(sompylasar): Make a meta that tells the frontend to use auto-min and max.
      ar(CEREAL_NVP(header_text),
         CEREAL_NVP(color),
         CEREAL_NVP(min),
         CEREAL_NVP(max),
         CEREAL_NVP(time_interval));
    }
  };

  // The `data_url` is relative to the `layout_url`.
  std::string data_url = "/plot_data";
  std::string visualizer_name = "plot-visualizer";
  Options visualizer_options;

  template <typename A>
  void save(A& ar) const {
    ar(CEREAL_NVP(data_url), CEREAL_NVP(visualizer_name), CEREAL_NVP(visualizer_options));
  }
};

struct PicMeta {
  struct Options {
    std::string header_text = "Header";
    std::string empty_text = "Empty";
    double time_interval = 10000;
    template <typename A>
    void save(A& ar) const {
      ar(CEREAL_NVP(header_text), CEREAL_NVP(empty_text), CEREAL_NVP(time_interval));
    }
  };

  // The `data_url` is relative to the `layout_url`.
  std::string data_url = "/pic_data";
  std::string visualizer_name = "image-visualizer";
  Options visualizer_options;

  template <typename A>
  void save(A& ar) const {
    ar(CEREAL_NVP(data_url), CEREAL_NVP(visualizer_name), CEREAL_NVP(visualizer_options));
  }
};

namespace layout {

// TODO(dkorolev): Think of making it impossible to create a row or rows or a col of cols.

struct Cell {
  // The `meta_url` is relative to the `layout_url`.
  std::string meta_url;

  Cell(const std::string& meta_url = "") : meta_url(meta_url) {}

  template <typename A>
  void save(A& ar) const {
    ar(CEREAL_NVP(meta_url));
  }
};

struct Layout;

struct Row {
  std::vector<Layout> data;
  Row(std::initializer_list<Layout> data) : data(data) {}
};

struct Col {
  std::vector<Layout> data;
  Col(std::initializer_list<Layout> data) : data(data) {}
};

struct Layout {
  std::vector<Layout> row;
  std::vector<Layout> col;
  Cell cell;

  Layout() {}
  Layout(const Row& row) : row(row.data) {}
  Layout(const Col& col) : col(col.data) {}
  Layout(const Cell& cell) : cell(cell) {}

  template <typename A>
  void save(A& ar) const {
    if (!row.empty()) {
      ar(CEREAL_NVP(row));
    } else if (!col.empty()) {
      ar(CEREAL_NVP(col));
    } else {
      ar(CEREAL_NVP(cell));
    }
  }
};

}  // namespace layout

using layout::Layout;

int main() {
  auto time_series = sherlock::Stream<DoublePoint>("time_series");
  auto pic_series = sherlock::Stream<StringPoint>("pic_series");

  std::thread points_populator([&time_series, &pic_series]() {
    while (true) {
      const double x = static_cast<double>(bricks::time::Now());
      time_series.Publish(DoublePoint{x, 0.5 * (1.0 + sin(0.003 * x))});
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  });

  std::thread pics_populator([&time_series, &pic_series]() {
    int index = 0;
    while (true) {
      const double x = static_cast<double>(bricks::time::Now());
      pic_series.Publish(
          StringPoint{x, bricks::strings::Printf("http://lorempixel.com/400/200/nature/%d/", index + 1)});
      index = (index + 1) % 10;
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
  });

  const int port = FLAGS_port;

  HTTP(port).Register("/config", [](Request r) {
    r(ExampleConfig(),
      "config",
      HTTPResponseCode.OK,
      "application/json; charset=utf-8",
      HTTPHeaders({{"Access-Control-Allow-Origin", "*"}}));
  });

  HTTP(port).Register("/layout/plot_data", [&time_series](Request r) {
    time_series.AsyncSubscribe(make_unique<ServeJSONOverHTTP<DoublePoint> >(std::move(r))).Detach();
  });

  HTTP(port).Register("/layout/pic_data", [&pic_series](Request r) {
    pic_series.AsyncSubscribe(make_unique<ServeJSONOverHTTP<StringPoint> >(std::move(r))).Detach();
  });

  HTTP(port).Register("/layout/plot_meta", [](Request r) {
    r(PlotMeta(),
      "meta",
      HTTPResponseCode.OK,
      "application/json; charset=utf-8",
      HTTPHeaders({{"Access-Control-Allow-Origin", "*"}}));
  });

  HTTP(port).Register("/layout/pic_meta", [](Request r) {
    r(PicMeta(),
      "meta",
      HTTPResponseCode.OK,
      "application/json; charset=utf-8",
      HTTPHeaders({{"Access-Control-Allow-Origin", "*"}}));
  });

  HTTP(port).Register("/layout", [](Request r) {
    using namespace layout;
    r(Layout(Col({Row({Cell("/plot_meta"), Cell("/pic_meta")}),
                  Row({Cell("/pic_meta"), Cell("/pic_meta"), Cell("/pic_meta")})})),
      "layout",
      HTTPResponseCode.OK,
      "application/json; charset=utf-8",
      HTTPHeaders({{"Access-Control-Allow-Origin", "*"}}));
  });

  const std::string dir = "Dashboard";  // Does not matter if it has a trailing slash or no here.
  HTTP(port).ServeStaticFilesFrom(dir, "/static/");

  // Need a dedicated handler for '/'.
  HTTP(port).Register(
      "/",
      new bricks::net::api::StaticFileServer(
          bricks::FileSystem::ReadFileAsString(bricks::FileSystem::JoinPath(dir, "index.html")), "text/html"));

  HTTP(port).Join();
}
