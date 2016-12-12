/*******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * *******************************************************************************/

#ifndef FNCAS_DIFFERENTIATE_H
#define FNCAS_DIFFERENTIATE_H

#include <vector>

#include "base.h"
#include "node.h"

namespace fncas {
namespace impl {

static const double_t APPROXIMATE_DERIVATIVE_EPS = 1e-4;

template <typename F>
double_t approximate_derivative(F f,
                                const std::vector<double_t>& x,
                                node_index_type i,
                                const double_t EPS = APPROXIMATE_DERIVATIVE_EPS) {
  std::vector<double_t> x1(x);
  std::vector<double_t> x2(x);
  x1[i] -= EPS;
  x2[i] += EPS;
  return (f(x2) - f(x1)) / (EPS + EPS);
}

template <typename F>
std::vector<double_t> approximate_gradient(F f,
                                           const std::vector<double_t>& x,
                                           const double_t EPS = APPROXIMATE_DERIVATIVE_EPS) {
  std::vector<double_t> g(x.size());
  std::vector<double_t> xx(x);
  for (size_t i = 0; i < xx.size(); ++i) {
    const double_t v0 = xx[i];
    xx[i] = v0 - EPS;
    const double_t f1 = f(xx);
    xx[i] = v0 + EPS;
    const double_t f2 = f(xx);
    xx[i] = v0;
    g[i] = (f2 - f1) / (EPS + EPS);
  }
  return g;
}

inline node_index_type d_op(operation_t operation, const V& a, const V& b, const V& da, const V& db) {
  static const size_t n = static_cast<size_t>(operation_t::end);
  static const std::function<V(const V&, const V&, const V&, const V&)> differentiator[n] = {
      [](const V&, const V&, const V& da, const V& db) { return da + db; },
      [](const V&, const V&, const V& da, const V& db) { return da - db; },
      [](const V& a, const V& b, const V& da, const V& db) { return a * db + b * da; },
      [](const V& a, const V& b, const V& da, const V& db) { return (b * da - a * db) / (b * b); }};
  return operation < operation_t::end ? differentiator[static_cast<size_t>(operation)](a, b, da, db).index() : 0;
}

inline node_index_type d_f(function_t function, const V& original, const V& x, const V& dx) {
  static const size_t n = static_cast<size_t>(function_t::end);
  static const std::function<V(const V&, const V&, const V&)> differentiator[n] = {
      // sqr().
      [](const V&, const V& x, const V& dx) { return V(2.0) * x * dx; },
      // sqrt().
      [](const V& original, const V&, const V& dx) { return dx / (original + original); },
      // exp().
      [](const V& original, const V&, const V& dx) { return original * dx; },
      // log().
      [](const V&, const V& x, const V& dx) { return dx / x; },
      // sin().
      [](const V&, const V& x, const V& dx) { return dx * ::fncas_functions::cos(x); },
      // cos().
      [](const V&, const V& x, const V& dx) { return V(-1.0) * dx * ::fncas_functions::sin(x); },
      // tan().
      [](const V&, const V& x, const V& dx) {
        V a = V(4.0) * dx * ::fncas_functions::cos(x) * ::fncas_functions::cos(x);
        V b = fncas_functions::cos(x + x) + 1;
        return a / (b * b);
      },
      // asin().
      [](const V&, const V& x, const V& dx) { return dx / ::fncas_functions::sqrt(V(1.0) - x * x); },
      // acos().
      [](const V&, const V& x, const V& dx) { return V(-1.0) * dx / ::fncas_functions::sqrt(V(1.0) - x * x); },
      // atan().
      [](const V&, const V& x, const V& dx) { return dx / (x * x + 1); },
      // unit_step().
      [](const V& o, const V&, const V&) {
        CURRENT_THROW(FnCASZeroOrOneIsNonDifferentiable());
        return o;
      },
      // ramp().
      [](const V&, const V& x, const V& dx) { return ::fncas_functions::unit_step(x) * dx; }};
  return function < function_t::end ? differentiator[static_cast<size_t>(function)](original, x, dx).index() : 0;
}

// differentiate_node() should use manual stack implementation to avoid SEGFAULT. Using plain recursion
// will overflow the stack for every formula containing repeated operation on the top level.
inline node_index_type differentiate_node(node_index_type index, size_t var_index, size_t dim) {
  CURRENT_ASSERT(var_index < dim);
  std::vector<std::vector<node_index_type>>& df_container = internals_singleton().df_;
  if (df_container.empty()) {
    df_container.resize(dim);
  }
  CURRENT_ASSERT(df_container.size() == dim);
  std::vector<node_index_type>& df = df_container[var_index];
  if (growing_vector_access(df, index, static_cast<node_index_type>(-1)) == -1) {
    const node_index_type zero_index = V(0.0).index();
    const node_index_type one_index = V(1.0).index();
    std::stack<node_index_type> stack;
    stack.push(index);
    while (!stack.empty()) {
      const node_index_type i = stack.top();
      stack.pop();
      const node_index_type dependent_i = ~i;
      if (i > dependent_i) {
        node_impl& f = node_vector_singleton()[i];
        if (f.type() == type_t::variable && static_cast<size_t>(f.variable()) == var_index) {
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
          CURRENT_ASSERT(false);
          return 0;
        }
      } else {
        node_impl& f = node_vector_singleton()[dependent_i];
        if (f.type() == type_t::operation) {
          const node_index_type a = f.lhs_index();
          const node_index_type b = f.rhs_index();
          const node_index_type da = growing_vector_access(df, a, static_cast<node_index_type>(-1));
          const node_index_type db = growing_vector_access(df, b, static_cast<node_index_type>(-1));
          CURRENT_ASSERT(da != -1);
          CURRENT_ASSERT(db != -1);
          growing_vector_access(df, dependent_i, static_cast<node_index_type>(-1)) =
              d_op(f.operation(), from_index(a), from_index(b), from_index(da), from_index(db));
        } else if (f.type() == type_t::function) {
          const node_index_type x = f.argument_index();
          const node_index_type dx = growing_vector_access(df, x, static_cast<node_index_type>(-1));
          CURRENT_ASSERT(dx != -1);
          growing_vector_access(df, dependent_i, static_cast<node_index_type>(-1)) =
              d_f(f.function(), from_index(dependent_i), from_index(x), from_index(dx));
        } else {
          CURRENT_ASSERT(false);
          return 0;
        }
      }
    }
  }
  const node_index_type result = growing_vector_access(df, index, static_cast<node_index_type>(-1));
  CURRENT_ASSERT(result != -1);
  return result;
}

struct g : noncopyable {
  virtual ~g() {}
  virtual std::vector<double_t> operator()(const std::vector<double_t>& x) const = 0;
  // The dimensionality of the parameters vector for the function.
  virtual size_t dim() const = 0;
  // The number of external `double_t` "registers" required to compute it, for compiled versions.
  virtual size_t heap_size() const { return 0; }
};

struct g_approximate : g {
  std::function<double_t(const std::vector<double_t>&)> f_;
  size_t dim_;
  g_approximate(std::function<double_t(const std::vector<double_t>&)> f, size_t dim) : f_(f), dim_(dim) {}
  g_approximate(g_approximate&& rhs) : f_(rhs.f_) {}
  g_approximate() = default;
  g_approximate(const g_approximate&) = default;
  void operator=(const g_approximate& rhs) {
    f_ = rhs.f_;
    dim_ = rhs.dim_;
  }
  std::vector<double_t> operator()(const std::vector<double_t>& x) const override {
    return approximate_gradient(f_, x);
  }
  size_t dim() const override { return dim_; }
};

struct g_intermediate : g {
  V f_;               // `f_` holds the node index for the value of the preprocessed expression.
  std::vector<V> g_;  // `g_[i]` holds the node index for the value of the derivative by variable `i`.
  g_intermediate(const X& x_ref, const V& f) : f_(f) {
    CURRENT_ASSERT(&x_ref == internals_singleton().x_ptr_);
    const size_t dim = internals_singleton().dim_;
    g_.resize(dim);
    for (size_t i = 0; i < dim; ++i) {
      g_[i] = f_.template differentiate<X>(x_ref, i);
    }
  }
  explicit g_intermediate(const X& x_ref, const f_intermediate& fi) : g_intermediate(x_ref, fi.f_) {}
  g_intermediate() = delete;
  void operator=(const g_intermediate& rhs) {
    f_ = rhs.f_;
    g_ = rhs.g_;
  }
  std::vector<double_t> operator()(const std::vector<double_t>& x) const override {
    std::vector<double_t> r;
    r.resize(g_.size());
    for (size_t i = 0; i < g_.size(); ++i) {
      r[i] = g_[i](x, i ? reuse_cache::reuse : reuse_cache::invalidate);
    }
    return r;
  }
  size_t dim() const override { return g_.size(); }
};

template <>
struct node_differentiate_impl<X> {
  static V differentiate(const X& x_ref, node_index_type node_index, size_t variable_index) {
    CURRENT_ASSERT(&x_ref == internals_singleton().x_ptr_);
    CURRENT_ASSERT(static_cast<size_t>(variable_index) < internals_singleton().dim_);
    return from_index(differentiate_node(node_index, variable_index, internals_singleton().dim_));
  }
};

}  // namespace fncas::impl
}  // namespace fncas

#endif  // #ifndef FNCAS_DIFFERENTIATE_H
