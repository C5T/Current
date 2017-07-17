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

#include "../../bricks/strings/util.h"
#include "../../typesystem/struct.h"

namespace current {
namespace nginx {
namespace config {

// NOTE: Parameters for simple and complex directives, are defined as a single string for simplicity.
// It is user's responsibility to supply properly formatted parameters pack for the directive.

CURRENT_STRUCT(SimpleDirective) {
  CURRENT_FIELD(name, std::string);
  CURRENT_FIELD_DESCRIPTION(name, "Nginx simple directive name.");
  CURRENT_FIELD(parameters, std::string);
  CURRENT_FIELD_DESCRIPTION(parameters, "Nginx simple directive parameters.");
  CURRENT_DEFAULT_CONSTRUCTOR(SimpleDirective){};
  CURRENT_CONSTRUCTOR(SimpleDirective)(const std::string& name, const std::string& parameters)
      : name(name), parameters(parameters) {}
};

CURRENT_FORWARD_DECLARE_STRUCT(BlockDirective);
CURRENT_FORWARD_DECLARE_STRUCT(ServerDirective);
CURRENT_FORWARD_DECLARE_STRUCT(LocationDirective);

using directives_t = Variant<SimpleDirective, BlockDirective, ServerDirective, LocationDirective>;

// clang-format off
CURRENT_STRUCT(BlockDirective) {
  CURRENT_FIELD(name, std::string);
  CURRENT_FIELD_DESCRIPTION(name, "Nginx block directive name.");
  CURRENT_FIELD(parameters, std::string);
  CURRENT_FIELD_DESCRIPTION(parameters, "Nginx block directive parameters.");
  CURRENT_FIELD(directives, std::vector<directives_t>);
  CURRENT_FIELD_DESCRIPTION(directives, "The array of Nginx directives contained in this block directive.");
  CURRENT_DEFAULT_CONSTRUCTOR(BlockDirective) {}
  CURRENT_CONSTRUCTOR(BlockDirective)(const std::string& name,
                                      std::initializer_list<directives_t> directives = {})
      : name(name), directives(directives) {}
  CURRENT_CONSTRUCTOR(BlockDirective)(const std::string& name,
                                      const std::string& parameters,
                                      std::initializer_list<directives_t> directives = {})
      : name(name), parameters(parameters), directives(directives) {}

  template <typename T>
  BlockDirective& Add(T&& directive) {
    directives.emplace_back(std::forward<T>(directive));
    return *this;
  }

  template <typename T>
  T& Create(T&& directive) {
    directives.emplace_back(std::forward<T>(directive));
    return Value<T>(directives.back());
  }
};

CURRENT_STRUCT(LocationDirective, BlockDirective) {
  CURRENT_DEFAULT_CONSTRUCTOR(LocationDirective) : SUPER("location") {}
  CURRENT_CONSTRUCTOR(LocationDirective)(const std::string& location) : SUPER("location", location) {}
};
// clang-format on

inline LocationDirective ProxyPassLocationDirective(const std::string& location,
                                                    const std::string& proxy_pass_to) {
  LocationDirective result(location);
  result.Add(SimpleDirective("proxy_pass", proxy_pass_to))
      .Add(SimpleDirective("proxy_set_header", "X-Real-IP $remote_addr"))
      .Add(SimpleDirective("proxy_set_header", "X-Forwarded-For $proxy_add_x_forwarded_for"))
      .Add(SimpleDirective("proxy_pass_request_headers", "on"));
  return result;
}

// clang-format off
CURRENT_STRUCT(ServerDirective, BlockDirective) {
  CURRENT_FIELD(port, uint16_t, 0u);
  CURRENT_FIELD_DESCRIPTION(port, "The port for Nginx to listen to as part of this block directive.");
  CURRENT_DEFAULT_CONSTRUCTOR(ServerDirective) : SUPER("server") {}
  CURRENT_CONSTRUCTOR(ServerDirective)(uint16_t port) : SUPER("server"), port(port) {
    SUPER::Add(SimpleDirective("listen", current::ToString(port)));
  }

  LocationDirective& CreateLocation(const std::string& location) {
    return SUPER::Create(LocationDirective(location));
  }

  LocationDirective& CreateProxyPassLocation(const std::string& location, const std::string& proxy_pass_to) {
    return SUPER::Create(ProxyPassLocationDirective(location, proxy_pass_to));
  }
};

CURRENT_STRUCT(HttpDirective, BlockDirective) {
  CURRENT_DEFAULT_CONSTRUCTOR(HttpDirective) : SUPER("http") {}

  ServerDirective& CreateServer(uint16_t port) {
    for (const auto& d : SUPER::directives) {
      if (Exists<ServerDirective>(d) && Value<ServerDirective>(d).port == port) {
         CURRENT_THROW(PortAlreadyUsedException(port));
      }
    }
    return SUPER::Create(ServerDirective(port));
  }
};
// clang-format on

using full_config_directives_t = Variant<SimpleDirective, BlockDirective>;

// clang-format off
CURRENT_STRUCT(FullConfig) {
  CURRENT_FIELD(directives, std::vector<full_config_directives_t>);
  CURRENT_FIELD_DESCRIPTION(directives, "The array of Nginx directives.");
  CURRENT_FIELD(http, HttpDirective);
  CURRENT_FIELD_DESCRIPTION(http, "The Nginx directive describing HTTP configuration spanning zero, one, or more ports.");

  template <typename T>
  FullConfig& Add(T&& directive) {
    directives.emplace_back(std::forward<T>(directive));
    return *this;
  }
};
// clang-format on

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
        os_ << "  ";
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

}  // namespace current::nginx::config
}  // namespace current::nginx
}  // namespace current

#endif  // CURRENT_UTILS_NGINX_CONFIG_H
