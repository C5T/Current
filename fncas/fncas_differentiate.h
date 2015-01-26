// https://github.com/dkorolev/fncas

// Defines the means to differentiate a node.

#ifndef FNCAS_DIFFERENTIATE_H
#define FNCAS_DIFFERENTIATE_H

#include "fncas_base.h"
#include "fncas_node.h"

namespace fncas {

static const double APPROXIMATE_DERIVATIVE_EPS = 1e-4;

template <typename F>
fncas_value_type approximate_derivative(F f,
                                        const std::vector<fncas_value_type>& x,
                                        node_index_type i,
                                        const fncas_value_type EPS = APPROXIMATE_DERIVATIVE_EPS) {
  std::vector<fncas_value_type> x1(x);
  std::vector<fncas_value_type> x2(x);
  x1[i] -= EPS;
  x2[i] += EPS;
  return (f(x2) - f(x1)) / (EPS + EPS);
}

template <typename F>
std::vector<fncas_value_type> approximate_gradient(F f,
                                                   const std::vector<fncas_value_type>& x,
                                                   const fncas_value_type EPS = APPROXIMATE_DERIVATIVE_EPS) {
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

node_index_type d_op(operation_t operation, const node& a, const node& b, const node& da, const node& db) {
  static const size_t n = static_cast<size_t>(operation_t::end);
  static const std::function<node(const node&, const node&, const node&, const node&)> differentiator[n] = {
      [](const node& a, const node& b, const node& da, const node& db) { return da + db; },
      [](const node& a, const node& b, const node& da, const node& db) { return da - db; },
      [](const node& a, const node& b, const node& da, const node& db) { return a * db + b * da; },
      [](const node& a, const node& b, const node& da, const node& db) { return (b * da - a * db) / (b * b); }};
  return operation < operation_t::end ? differentiator[static_cast<size_t>(operation)](a, b, da, db).index()
                                      : 0;
}

node_index_type d_f(function_t function, const node& original, const node& x, const node& dx) {
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
      [](const node& original, const node& x, const node& dx) {
        return node(-1.0) * dx / sqrt(node(1.0) - x * x);
      },
      // atan().
      [](const node& original, const node& x, const node& dx) { return dx / (x * x + 1); },
  };
  return function < function_t::end ? differentiator[static_cast<size_t>(function)](original, x, dx).index()
                                    : 0;
}

// differentiate_node() should use manual stack implementation to avoid SEGFAULT. Using plain recursion
// will overflow the stack for every formula containing repeated operation on the top level.
node_index_type differentiate_node(node_index_type index, int32_t var_index, int32_t number_of_variables) {
  assert(var_index < number_of_variables);
  std::vector<std::vector<node_index_type>>& df_container = internals_singleton().df_;
  if (df_container.empty()) {
    df_container.resize(number_of_variables);
  }
  assert(static_cast<int32_t>(df_container.size()) == number_of_variables);
  std::vector<node_index_type>& df = df_container[var_index];
  if (growing_vector_access(df, index, static_cast<node_index_type>(-1)) == -1) {
    const node_index_type zero_index = node(0.0).index();
    const node_index_type one_index = node(1.0).index();
    std::stack<node_index_type> stack;
    stack.push(index);
    while (!stack.empty()) {
      const node_index_type i = stack.top();
      stack.pop();
      const node_index_type dependent_i = ~i;
      if (i > dependent_i) {
        node_impl& f = node_vector_singleton()[i];
        if (f.type() == type_t::variable && f.variable() == var_index) {
          growing_vector_access(df, i, static_cast<node_index_type>(-1)) = one_index;
        } else if (f.type() == type_t::variable || f.type() == type_t::value) {
          growing_vector_access(df, i, static_cast<node_index_type>(-1)) = zero_index;
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
          const node_index_type a = f.lhs_index();
          const node_index_type b = f.rhs_index();
          const node_index_type da = growing_vector_access(df, a, static_cast<node_index_type>(-1));
          const node_index_type db = growing_vector_access(df, b, static_cast<node_index_type>(-1));
          assert(da != -1);
          assert(db != -1);
          growing_vector_access(df, dependent_i, static_cast<node_index_type>(-1)) =
              d_op(f.operation(), from_index(a), from_index(b), from_index(da), from_index(db));
        } else if (f.type() == type_t::function) {
          const node_index_type x = f.argument_index();
          const node_index_type dx = growing_vector_access(df, x, static_cast<node_index_type>(-1));
          assert(dx != -1);
          growing_vector_access(df, dependent_i, static_cast<node_index_type>(-1)) =
              d_f(f.function(), from_index(dependent_i), from_index(x), from_index(dx));
        } else {
          assert(false);
          return 0;
        }
      }
    }
  }
  const node_index_type result = growing_vector_access(df, index, static_cast<node_index_type>(-1));
  assert(result != -1);
  return result;
}

struct g : noncopyable {
  struct result {
    fncas_value_type value;
    std::vector<fncas_value_type> gradient;
  };
  virtual ~g() {}
  virtual result operator()(const std::vector<fncas_value_type>& x) const = 0;
  virtual int32_t dim() const = 0;
};

struct g_approximate : g {
  std::function<fncas_value_type(const std::vector<fncas_value_type>&)> f_;
  int32_t d_;
  g_approximate(std::function<fncas_value_type(const std::vector<fncas_value_type>&)> f, int32_t d)
      : f_(f), d_(d) {}
  g_approximate(g_approximate&& rhs) : f_(rhs.f_) {}
  g_approximate() = default;
  g_approximate(const g_approximate&) = default;
  void operator=(const g_approximate& rhs) {
    f_ = rhs.f_;
    d_ = rhs.d_;
  }
  virtual result operator()(const std::vector<fncas_value_type>& x) const {
    result r;
    r.value = f_(x);
    r.gradient = approximate_gradient(f_, x);
    return r;
  }
  virtual int32_t dim() const { return d_; }
};

struct g_intermediate : g {
  node f_;
  std::vector<node> g_;
  g_intermediate(const x& x_ref, const node& f) : f_(f) {
    assert(&x_ref == internals_singleton().x_ptr_);
    const int32_t dim = internals_singleton().dim_;
    g_.resize(dim);
    for (int32_t i = 0; i < dim; ++i) {
      g_[i] = f_.differentiate(x_ref, i);
    }
  }
  explicit g_intermediate(const x& x_ref, const f_intermediate& fi) : g_intermediate(x_ref, fi.f_) {}
  g_intermediate(g_intermediate&& rhs) {}
  g_intermediate() = default;
  g_intermediate(const g_intermediate&) = default;
  void operator=(const g_intermediate& rhs) {
    f_ = rhs.f_;
    g_ = rhs.g_;
  }
  virtual result operator()(const std::vector<fncas_value_type>& x) const {
    result r;
    r.value = f_(x);
    r.gradient.resize(g_.size());
    for (size_t i = 0; i < g_.size(); ++i) {
      r.gradient[i] = g_[i](x, reuse_cache::reuse);
    }
    return r;
  }
  virtual int32_t dim() const { return g_.size(); }
};

}  // namespace fncas

#endif  // #ifndef FNCAS_DIFFERENTIATE_H
