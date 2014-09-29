// https://github.com/dkorolev/fncas

// Defines the means to differentiate a node.

#ifndef FNCAS_DIFFERENTIATE_H
#define FNCAS_DIFFERENTIATE_H

#include "fncas_base.h"
#include "fncas_node.h"

namespace fncas {

template <typename F>
fncas_value_type approximate_derivative(F f,
                                        const std::vector<fncas_value_type>& x,
                                        size_t i,
                                        const fncas_value_type EPS = 1e-6) {
  std::vector<fncas_value_type> x1(x);
  std::vector<fncas_value_type> x2(x);
  x1[i] -= EPS;
  x2[i] += EPS;
  return (f(x2) - f(x1)) / (EPS + EPS);
}

template <typename F>
std::vector<fncas_value_type> approximate_gradient(F f,
                                                   const std::vector<fncas_value_type>& x,
                                                   const fncas_value_type EPS = 1e-6) {
  std::vector<fncas_value_type> g(x.size());
  std::vector<fncas_value_type> xx(x);
  for (size_t i = 0; i < xx.size(); ++i) {
    const fncas_value_type v0 = xx[i];
    xx[i] = v0 - EPS;
    const fncas_value_type f1 = f(xx);
    xx[i] = v0 + EPS;
    const fncas_value_type f2 = f(xx);
    xx[i] = v0;
    g[i] = (f2 - f1) / (EPS + EPS);
  }
  return g;
}

uint32_t d_op(operation_t operation, const node& a, const node& b, const node& da, const node& db) {
  static const size_t n = static_cast<size_t>(operation_t::end);
  static const std::function<node(const node&, const node&, const node&, const node&)> differentiator[n] = {
      [](const node& a, const node& b, const node& da, const node& db) { return da + db; },
      [](const node& a, const node& b, const node& da, const node& db) { return da - db; },
      [](const node& a, const node& b, const node& da, const node& db) { return a * db + b * da; },
      [](const node& a, const node& b, const node& da, const node& db) { return (b * da - a * db) / (b * b); }};
  return operation < operation_t::end ? differentiator[static_cast<size_t>(operation)](a, b, da, db).index() : 0;
}

uint32_t d_f(function_t function, const node& original, const node& x, const node& dx) {
  static const size_t n = static_cast<size_t>(function_t::end);
  static const std::function<node(const node&, const node&, const node&)> differentiator[n] = {
      // sqrt().
      [](const node& original, const node& x, const node& dx) { return dx / (original + original); },
      // exp().
      [](const node& original, const node& x, const node& dx) { return original * dx; },
      // log().
      [](const node& original, const node& x, const node& dx) { return dx / x; },
      // sin().
      [](const node& original, const node& x, const node& dx) { return dx * cos(x); },
      // cos().
      [](const node& original, const node& x, const node& dx) { return node(-1.0) * dx * sin(x); },
      // tan().
      [](const node& original, const node& x, const node& dx) {
        node a = node(4.0) * dx * cos(x) * cos(x);
        node b = cos(x + x) + 1;
        return a / (b * b);
      },
      // asin().
      [](const node& original, const node& x, const node& dx) { return dx / sqrt(node(1.0) - x * x); },
      // acos().
      [](const node& original, const node& x, const node& dx) { return node(-1.0) * dx / sqrt(node(1.0) - x * x); },
      // atan().
      [](const node& original, const node& x, const node& dx) { return dx / (x * x + 1); },
  };
  return function < function_t::end ? differentiator[static_cast<size_t>(function)](original, x, dx).index() : 0;
}

// differentiate_node() should use manual stack implementation to avoid SEGFAULT. Using plain recursion
// will overflow the stack for every formula containing repeated operation on the top level.
uint32_t differentiate_node(uint32_t index, uint32_t var_index) {
  std::map<uint32_t, uint32_t>& df_cache = internals_singleton().df_by_var_[var_index];
  if (!df_cache.count(index)) {
    const uint32_t zero_index = node(0.0).index();
    const uint32_t one_index = node(1.0).index();
    std::stack<uint32_t> stack;
    stack.push(index);
    while (!stack.empty()) {
      const uint32_t i = stack.top();
      stack.pop();
      const uint32_t dependent_i = ~i;
      if (i < dependent_i) {
        node_impl& f = node_vector_singleton()[i];
        if (f.type() == type_t::variable && f.variable() == var_index) {
          df_cache[i] = one_index;
        } else if (f.type() == type_t::variable || f.type() == type_t::value) {
          df_cache[i] = zero_index;
        } else if (f.type() == type_t::operation) {
          stack.push(~i);
          stack.push(f.lhs_index());
          stack.push(f.rhs_index());
        } else if (f.type() == type_t::function) {
          stack.push(~i);
          stack.push(f.argument_index());
        } else {
          assert(false);
          return 0;
        }
      } else {
        node_impl& f = node_vector_singleton()[dependent_i];
        if (f.type() == type_t::operation) {
          const uint32_t a = f.lhs_index();
          const uint32_t b = f.rhs_index();
          assert(df_cache.count(a));
          assert(df_cache.count(b));
          const uint32_t da = df_cache[a];
          const uint32_t db = df_cache[b];
          df_cache[dependent_i] = d_op(f.operation(), from_index(a), from_index(b), from_index(da), from_index(db));
        } else if (f.type() == type_t::function) {
          const uint32_t x = f.lhs_index();
          assert(df_cache.count(x));
          const uint32_t dx = df_cache[x];
          df_cache[dependent_i] = d_f(f.function(), dependent_i, from_index(x), from_index(dx));
        } else {
          assert(false);
          return 0;
        }
      }
    }
  }
  assert(df_cache.count(index));
  return df_cache[index];
}

struct g : noncopyable {
  struct result {
    fncas_value_type value;
    std::vector<fncas_value_type> gradient;
  };
  virtual ~g() {
  }
  virtual result operator()(const std::vector<fncas_value_type>& x) const = 0;
  virtual size_t dim() const = 0;
};

struct g_approximate : g {
  std::function<fncas_value_type(const std::vector<fncas_value_type>&)> f_;
  size_t d_;
  g_approximate(std::function<fncas_value_type(const std::vector<fncas_value_type>&)> f, size_t d) : f_(f), d_(d) {
  }
  g_approximate(g_approximate&& rhs) : f_(rhs.f_) {
  }
  g_approximate(const g_approximate&) = delete;
  void operator=(const g_approximate&) = delete;
  virtual result operator()(const std::vector<fncas_value_type>& x) const {
    result r;
    r.value = f_(x);
    r.gradient = approximate_gradient(f_, x);
    return r;
  }
  virtual size_t dim() const {
    return d_;
  }
};

struct g_intermediate : g {
  node f_;
  std::vector<node> g_;
  g_intermediate(const node& f, const size_t dim) : f_(f), g_(dim) {
    for (size_t i = 0; i < dim; ++i) {
      g_[i] = f_.differentiate(i);
    }
  }
  explicit g_intermediate(const node_with_dim& fd) : g_intermediate(fd.f, fd.d) {
  }
  explicit g_intermediate(const f_intermediate& fi) : g_intermediate(fi.fd_) {
  }
  g_intermediate(g_intermediate&& rhs) {
  }
  g_intermediate(const g_intermediate&) = delete;
  void operator=(const g_intermediate&) = delete;
  virtual result operator()(const std::vector<fncas_value_type>& x) const {
    result r;
    r.value = f_(x);
    r.gradient.resize(g_.size());
    for (size_t i = 0; i < g_.size(); ++i) {
      r.gradient[i] = g_[i](x);
    }
    return r;
  }
  virtual size_t dim() const {
    return g_.size();
  }
};

}  // namespace fncas

#endif  // #ifndef FNCAS_DIFFERENTIATE_H
