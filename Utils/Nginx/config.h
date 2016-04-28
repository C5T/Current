/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_UTILS_NGINX_CONFIG_H
#define CURRENT_UTILS_NGINX_CONFIG_H

#include "../../port.h"

#include "exceptions.h"

#include "../../Bricks/strings/util.h"
#include "../../TypeSystem/struct.h"

namespace current {
namespace utils {
namespace nginx {
namespace config {

CURRENT_STRUCT(SimpleDirective) {
  CURRENT_FIELD(name, std::string);
  CURRENT_FIELD(parameters, std::string);
  CURRENT_DEFAULT_CONSTRUCTOR(SimpleDirective){};
  CURRENT_CONSTRUCTOR(SimpleDirective)(const std::string& name, const std::string& parameters)
      : name(name), parameters(parameters) {}
};

CURRENT_FORWARD_DECLARE_STRUCT(BlockDirective);
CURRENT_FORWARD_DECLARE_STRUCT(ServerDirective);
CURRENT_FORWARD_DECLARE_STRUCT(LocationDirective);

using directives_t = Variant<SimpleDirective, BlockDirective, ServerDirective, LocationDirective>;

CURRENT_STRUCT(BlockDirective) {
  CURRENT_FIELD(name, std::string);
  CURRENT_FIELD(parameters, std::string);
  CURRENT_FIELD(directives, std::vector<directives_t>);
  CURRENT_DEFAULT_CONSTRUCTOR(BlockDirective) {}
  CURRENT_CONSTRUCTOR(BlockDirective)(const std::string& name) : name(name) {}
  CURRENT_CONSTRUCTOR(BlockDirective)(const std::string& name, const std::string& parameters)
      : name(name), parameters(parameters) {}

  SimpleDirective& AddSimpleDirective(const std::string& name, const std::string& parameters) {
    directives.push_back(SimpleDirective(name, parameters));
    return Value<SimpleDirective>(directives.back());
  }

  template <typename T>
  T& AddDirective(T && directive) {
    directives.push_back(std::forward<T>(directive));
    return Value<T>(directives.back());
  }
};

// clang-format off
CURRENT_STRUCT(LocationDirective, BlockDirective) {
  CURRENT_DEFAULT_CONSTRUCTOR(LocationDirective) : SUPER("location") {}
  CURRENT_CONSTRUCTOR(LocationDirective)(const std::string& location) : SUPER("location", location) {}

  LocationDirective& AddNestedLocation(const std::string& location) {
    return SUPER::AddDirective(LocationDirective(location));
  }
};
// clang-format on

inline LocationDirective DefaultLocationDirective(const std::string& location) {
  LocationDirective result(location);
  result.AddSimpleDirective("proxy_set_header", "X-Real-IP $remote_addr");
  result.AddSimpleDirective("proxy_set_header", "X-Forwarded-For $proxy_add_x_forwarded_for");
  result.AddSimpleDirective("proxy_pass_request_headers", "on");
  return result;
}

// clang-format off
CURRENT_STRUCT(ServerDirective, BlockDirective) {
  CURRENT_FIELD(port, uint16_t, 0u);
  CURRENT_DEFAULT_CONSTRUCTOR(ServerDirective) : SUPER("server") {}
  CURRENT_CONSTRUCTOR(ServerDirective)(uint16_t port) : SUPER("server"), port(port) {
    SUPER::AddSimpleDirective("listen", current::ToString(port));
  }

  LocationDirective& AddLocation(const std::string& location) {
    return SUPER::AddDirective(LocationDirective(location));
  }

  LocationDirective& AddDefaultLocation(const std::string& location) {
    return SUPER::AddDirective(DefaultLocationDirective(location));
  }
};

CURRENT_STRUCT(HttpDirective, BlockDirective) {
  CURRENT_DEFAULT_CONSTRUCTOR(HttpDirective) : SUPER("http") {}

  ServerDirective& AddServer(uint16_t port) {
    for (const auto& d : SUPER::directives) {
      if (Exists<ServerDirective>(d) && Value<ServerDirective>(d).port == port) {
         CURRENT_THROW(PortAlreadyUsedException(port));
      }
    }
    return SUPER::AddDirective(ServerDirective(port));
  }
};
// clang-format on

using full_config_directives_t = Variant<SimpleDirective, BlockDirective>;

CURRENT_STRUCT(FullConfig) {
  CURRENT_FIELD(directives, std::vector<full_config_directives_t>);
  CURRENT_FIELD(http, HttpDirective);

  SimpleDirective& AddSimpleDirective(const std::string& name, const std::string& parameters) {
    directives.push_back(SimpleDirective(name, parameters));
    return Value<SimpleDirective>(directives.back());
  }

  BlockDirective& AddBlockDirective(const std::string& name, const std::string& parameters) {
    directives.push_back(BlockDirective(name, parameters));
    return Value<BlockDirective>(directives.back());
  }
};

struct ConfigPrinter {
  struct DirectivesPrinter {
    DirectivesPrinter(std::ostream& os, size_t indent) : os_(os), indent_(indent) {}

    void operator()(const SimpleDirective& simple) const {
      PrintIndent();
      os_ << simple.name;
      if (!simple.parameters.empty()) {
        os_ << ' ' << simple.parameters;
      }
      os_ << ";\n";
    }

    void operator()(const BlockDirective& block) {
      PrintIndent();
      os_ << block.name;
      if (!block.parameters.empty()) {
        os_ << ' ' << block.parameters;
      }
      os_ << " {\n";
      ++indent_;
      for (const auto& directive : block.directives) {
        directive.Call(*this);
      }
      --indent_;
      PrintIndent();
      os_ << "}\n";
    }

    void PrintIndent() const {
      for (size_t i = 0; i < indent_; ++i) {
        os_ << '\t';
      }
    }

    std::ostream& os_;
    size_t indent_;
  };

  static void Print(std::ostream& os, const BlockDirective& block, size_t indent = 0u) {
    DirectivesPrinter printer(os, indent);
    printer(block);
  }

  static void Print(std::ostream& os, const FullConfig& config, size_t indent = 0u) {
    DirectivesPrinter printer(os, indent);
    for (const auto& directive : config.directives) {
      directive.Call(printer);
    }
    printer(config.http);
  }

  template <typename T>
  static std::string AsString(const T& element, size_t indent = 0u) {
    std::ostringstream os;
    Print(os, element, indent);
    return os.str();
  }
};

}  // namespace current::utils::nginx::config
}  // namespace current::utils::nginx
}  // namespace current::utils
}  // namespace current

#endif  // CURRENT_UTILS_NGINX_CONFIG_H
