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

// GraphViz's `dot` binding.

#ifndef BRICKS_DOT_GRAPHVIZ_H
#define BRICKS_DOT_GRAPHVIZ_H

#include "../port.h"

#include <vector>
#include <string>
#include <sstream>

#include "../util/singleton.h"
#include "../file/file.h"
#include "../strings/printf.h"

#include "../../TypeSystem/optional.h"
#include "../../TypeSystem/helpers.h"

namespace current {
namespace graphviz {

static inline std::string Escape(const std::string& s) {
  std::string result;
  for (char c : s) {
    if (c == '"') {
      result += '\\';
    }
    result += c;
  }
  return result;
}

struct Node {
  struct Impl {
    size_t order_key;
    std::string label;
    std::map<std::string, Optional<std::string>> params;
    bool html = false;
    Impl(const std::string& label) : label(label) {}
  };
  std::shared_ptr<Impl> impl;

  Node(const std::string& label = "") : impl(std::make_shared<Impl>(label)) {}

  operator std::shared_ptr<Impl>() const { return impl; }

  template <typename T>
  Node& Set(const std::string& key, T&& shape) {
    operator[](key) = std::forward<T>(shape);
    return *this;
  }

  template <typename T>
  Node& Shape(T&& shape) {
    return Set("shape", std::forward<T>(shape));
  }

  Node& HTML() {
    impl->html = true;
    return *this;
  }

  Optional<std::string>& operator[](const std::string& param) { return impl->params[param]; }
};

struct Edge {
  std::shared_ptr<Node::Impl> from;
  std::shared_ptr<Node::Impl> into;

  Edge() = default;
  Edge(const Node& from, const Node& into) : from(from), into(into) {}
};

struct Group {
  std::vector<std::shared_ptr<Node::Impl>> nodes;
  std::map<std::string, std::string> params;
  std::map<std::string, std::string> graph;  // ex. "style" = "dashed".
  Group() = default;
  Group& Add(const Node& node) {
    nodes.push_back(node);
    return *this;
  }
  Group& Label(const std::string& value) {
    params["label"] = value;
    return *this;
  }
  Group& LabelLoc(const std::string& value) {
    params["labelloc"] = value;
    return *this;
  }
  Group& FontName(const std::string& value) {
    params["fontname"] = value;
    return *this;
  }
  Group& FontSize(const std::string& value) {
    params["fontsize"] = value;
    return *this;
  }
  Group& GraphStyle(const std::string& new_style) {
    graph["style"] = new_style;
    return *this;
  }
  Group& Color(const std::string& value) {
    graph["color"] = value;
    return *this;
  }
  Group& FillColor(const std::string& value) {
    graph["fillcolor"] = value;
    return *this;
  }
  Group& operator+=(const Node& node) {
    nodes.push_back(node);
    return *this;
  }
};

enum class GraphDirected : bool { Undirected = false, Directed = true };

template <GraphDirected DIRECTED>
struct GenericGraph {
  Optional<std::string> title;
  std::map<std::string, Optional<std::string>> params;
  std::vector<std::shared_ptr<Node::Impl>> nodes;
  std::vector<Edge> edges;
  std::vector<Group> groups;

  Optional<std::string>& operator[](const std::string& param_name) { return params[param_name]; }

  // `rankdir LR` lays the graph left-to-right instead of top-to-bottom.
  Optional<std::string>& RankDir() { return operator[]("rankdir"); }
  void RankDirLR() { RankDir() = "LR"; }

  GenericGraph() = default;

  template <typename T>
  GenericGraph& Title(T&& value) {
    title = std::forward<T>(value);
    return *this;
  }

  GenericGraph& Add(const Node& node) {
    nodes.push_back(node.impl);
    return *this;
  }
  GenericGraph& Add(const Edge& edge) {
    edges.push_back(edge);
    return *this;
  }
  GenericGraph& Add(const Group& group) {
    groups.push_back(group);
    return *this;
  }

  template <typename T>
  GenericGraph& operator+=(T&& x) {
    return Add(std::forward<T>(x));
  };

  struct OrderNodesByOrderKey {
    bool operator()(const Node::Impl* lhs, const Node::Impl* rhs) const { return lhs->order_key < rhs->order_key; }
  };

  std::string AsDOT() const {
    std::map<const Node::Impl*, size_t, OrderNodesByOrderKey> node_index;

    size_t order_key = 0u;
    for (const auto& node : nodes) {
      node->order_key = ++order_key;
      node_index[node.get()];
    }
    size_t nodes_count = 0u;
    for (auto& idx : node_index) {
      idx.second = ++nodes_count;
    }

    std::ostringstream os;
    os << ((DIRECTED == GraphDirected::Directed) ? "digraph" : "graph") << ' ';
    if (Exists(title)) {
      os << '\"' << Escape(Value(title)) << "\" ";
    }
    os << "{\n";
    for (const auto& param : params) {
      if (Exists(param.second)) {
        os << "  " << param.first << "=\"" << Escape(Value(param.second)) << "\";\n";
      }
    }
    for (const auto& node : nodes) {
      os << "  node" << node_index[node.get()];
      if (!node->html) {
        os << " [ label=\"" << Escape(node->label) << "\" ";
      } else {
        os << " [ label=<" << node->label << "> ";
      }
      for (const auto& node_param : node->params) {
        if (Exists(node_param.second)) {
          os << node_param.first << "=\"" << Escape(Value(node_param.second)) << "\" ";
        }
      }
      os << "];\n";
    }
    // NOTE: This design will create a superficial "node0" if the graph contains an edge or a group involving
    // a node which was not added by itself. We could `assert` on it or throw an exception. Meh for now. -- D.K.
    for (size_t group_index = 0; group_index < groups.size(); ++group_index) {
      const auto& group = groups[group_index];
      os << "  subgraph cluster_" << (group_index + 1) << " {\n";
      for (const auto& node : group.nodes) {
        os << "    node" << node_index[node.get()] << ";\n";
      }
      for (const auto& param : group.params) {
        os << "    " << param.first << "=\"" << Escape(param.second) << "\";\n";
      }
      if (!group.graph.empty()) {
        os << "    graph [";
        for (const auto& kv : group.graph) {
          os << ' ' << kv.first << "=\"" + Escape(kv.second) << '\"';
        }
        os << " ];\n";
      }
      os << "  }\n";
    }
    for (const auto& edge : edges) {
      os << "  node" << node_index[edge.from.get()] << ' ' << ((DIRECTED == GraphDirected::Directed) ? "->" : "--")
         << " node" << node_index[edge.into.get()] << ";\n";
    }
    os << "}\n";

    return os.str();
  }

  std::string AsSVG() const {
    const std::string input_file_name = current::FileSystem::GenTmpFileName();
    const std::string output_file_name = current::FileSystem::GenTmpFileName();
    const auto input_deleter = current::FileSystem::ScopedRmFile(input_file_name);
    const auto output_file_deleter = current::FileSystem::ScopedRmFile(output_file_name);

    current::FileSystem::WriteStringToFile(AsDOT(), input_file_name.c_str());
    CURRENT_ASSERT(!::system(current::strings::Printf(
                                 "dot -T svg %s -o %s\n", input_file_name.c_str(), output_file_name.c_str()).c_str()));
    return current::FileSystem::ReadFileAsString(output_file_name.c_str());
  }
};

using Graph = GenericGraph<GraphDirected::Undirected>;
using DiGraph = GenericGraph<GraphDirected::Directed>;

}  // namespace graphviz
}  // namespace current

#endif  // BRICKS_DOT_GRAPHVIZ_H
