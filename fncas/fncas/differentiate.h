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

#ifndef FNCAS_FNCAS_DIFFERENTIATE_H
#define FNCAS_FNCAS_DIFFERENTIATE_H

#include <vector>

#include "base.h"
#include "node.h"

namespace fncas {
namespace impl {

static const double_t APPROXIMATE_DERIVATIVE_EPS = 1e-4;

template <typename F>
double_t approximate_derivative(F&& f,
                                const std::vector<double_t>& x,
                                node_index_t i,
                                const double_t EPS = APPROXIMATE_DERIVATIVE_EPS) {
  std::vector<double_t> x1(x);
  std::vector<double_t> x2(x);
  x1[i] -= EPS;
  x2[i] += EPS;
  return (f(x2) - f(x1)) / (EPS + EPS);
}

template <typename F>
std::vector<double_t> approximate_gradient(F&& f,
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

inline V d_add(const V& a, const V& b, const V& da, const V& db) {
  static_cast<void>(a);
  static_cast<void>(b);
  if (da.is_value() && db.is_value()) {
    return da.value() + db.value();
  } else if (db.equals_to(0)) {
    return da;
  } else if (da.equals_to(0)) {
    return db;
  } else {
    return da + db;
  }
}

inline V d_sub(const V& a, const V& b, const V& da, const V& db) {
  static_cast<void>(a);
  static_cast<void>(b);
  if (da.is_value() && db.is_value()) {
    return da.value() - db.value();
  } else if (db.equals_to(0)) {
    return da;
  } else if (da.equals_to(0)) {
    return -db;
  } else {
    return da - db;
  }
}

inline V simplified_mul(const V& x, const V& y) {
  if (x.equals_to(0) || y.equals_to(0)) {
    return 0;
  } else if (x.is_value() && y.is_value()) {
    return x.value() * y.value();
  } else if (y.equals_to(1)) {
    return x;
  } else if (x.equals_to(1)) {
    return y;
  } else {
    return x * y;
  }
}

inline V d_mul(const V& a, const V& b, const V& da, const V& db) {
  bool const lhs_zero = a.equals_to(0) || db.equals_to(0);
  bool const rhs_zero = b.equals_to(0) || da.equals_to(0);
  if (lhs_zero && rhs_zero) {
    return 0;
  } else if (lhs_zero) {
    return simplified_mul(b, da);
  } else if (rhs_zero) {
    return simplified_mul(a, db);
  } else {
    return simplified_mul(a, db) + simplified_mul(b, da);
  }
}

inline node_index_t d_op(MathOperation operation, const V& a, const V& b, const V& da, const V& db) {
  static const size_t n = static_cast<size_t>(MathOperation::end);
  static const std::function<V(const V&, const V&, const V&, const V&)> differentiator[n] = {
      d_add, d_sub, d_mul, [](const V& a, const V& b, const V& da, const V& db) {
        // The unary minus doesn't exist, so a "0" node would still appear there, and thus no harm.
        return (simplified_mul(b, da) - simplified_mul(a, db)) / (b * b);
      }};
  return operation < MathOperation::end ? differentiator[static_cast<size_t>(operation)](a, b, da, db).index() : 0;
}

inline node_index_t d_f(MathFunction function, const V& original, const V& x, const V& dx) {
  static const size_t n = static_cast<size_t>(MathFunction::end);
  static const std::function<V(const V&, const V&, const V&)> differentiator[n] = {
      // sqr().
      [](const V&, const V& x, const V& dx) { return simplified_mul(V(2.0), simplified_mul(x, dx)); },
      // sqrt().
      [](const V& original, const V&, const V& dx) {
        if (original.is_value()) {
          return simplified_mul(V(0.5 / original.value()), dx);
        } else {
          return dx / (original + original);
        }
      },
      // exp().
      [](const V& original, const V&, const V& dx) { return simplified_mul(original, dx); },
      // log().
      [](const V&, const V& x, const V& dx) {
        if (x.is_value()) {
          return simplified_mul(V(1.0) / x.value(), dx);
        } else {
          return dx / x;
        }
      },
      // sin().
      [](const V&, const V& x, const V& dx) { return simplified_mul(dx, ::fncas::cos(x)); },
      // cos().
      [](const V&, const V& x, const V& dx) { return simplified_mul(V(-1.0), simplified_mul(dx, ::fncas::sin(x))); },
      // tan().
      [](const V&, const V& x, const V& dx) {
        V a = simplified_mul(V(4.0), simplified_mul(dx, ::fncas::cos(x) * ::fncas::cos(x)));
        V b = ::fncas::cos(x + x) + 1;
        if (b.is_value()) {
          return simplified_mul(V(1.0 / b.value()), a);
        } else {
          return a / (b * b);
        }
      },
      // asin().
      [](const V&, const V& x, const V& dx) {
        if (x.is_value()) {
          return simplified_mul(V(1.0) / (::fncas::sqrt(V(1.0) - x * x)), dx);
        } else {
          return dx / ::fncas::sqrt(V(1.0) - x * x);
        }
      },
      // acos().
      [](const V&, const V& x, const V& dx) {
        if (x.is_value()) {
          return simplified_mul(V(-1.0) / ::fncas::sqrt(V(1.0) - x * x), dx);
        } else {
          return V(-1.0) * dx / ::fncas::sqrt(V(1.0) - x * x);
        }
      },
      // atan().
      [](const V&, const V& x, const V& dx) {
        if (x.is_value()) {
          return simplified_mul(V(1.0) / (x * x + V(1.0)), dx);
        } else {
          return dx / (x * x + V(1.0));
        }
      },
      // unit_step().
      [](const V& o, const V&, const V&) {
        CURRENT_THROW(exceptions::FnCASZeroOrOneIsNonDifferentiable());
        return o;
      },
      // ramp().
      [](const V&, const V& x, const V& dx) { return ::fncas::unit_step(x) * dx; }};
  return function < MathFunction::end ? differentiator[static_cast<size_t>(function)](original, x, dx).index() : 0;
}

// differentiate_node() should use manual stack implementation to avoid SEGFAULT. Using plain recursion
// will overflow the stack for every formula containing repeated operation on the top level.
inline node_index_t differentiate_node(node_index_t index, size_t var_index, size_t dim) {
  CURRENT_ASSERT(var_index < dim);
  std::vector<std::vector<node_index_t>>& df_container = internals_singleton().df_;
  if (df_container.empty()) {
    df_container.resize(dim);
  }
  CURRENT_ASSERT(df_container.size() == dim);
  std::vector<node_index_t>& df = df_container[var_index];
  if (growing_vector_access(df, index, static_cast<node_index_t>(-1)) == -1) {
    const node_index_t zero_index = V(0.0).index();
    const node_index_t one_index = V(1.0).index();
    std::stack<node_index_t> stack;
    stack.push(index);
    while (!stack.empty()) {
      const node_index_t i = stack.top();
      stack.pop();
      const node_index_t dependent_i = ~i;
      if (i > dependent_i) {
        node_impl& f = node_vector_singleton()[static_cast<size_t>(i)];
        if (f.type() == NodeType::variable && static_cast<size_t>(f.variable()) == var_index) {
          growing_vector_access(df, i, static_cast<node_index_t>(-1)) = one_index;
        } else if (f.type() == NodeType::variable || f.type() == NodeType::value) {
          growing_vector_access(df, i, static_cast<node_index_t>(-1)) = zero_index;
        } else if (f.type() == NodeType::operation) {
          stack.push(~i);
          stack.push(f.lhs_index());
          stack.push(f.rhs_index());
        } else if (f.type() == NodeType::function) {
          stack.push(~i);
          stack.push(f.argument_index());
        } else {
          CURRENT_ASSERT(false);
          return 0;
        }
      } else {
        node_impl& f = node_vector_singleton()[static_cast<size_t>(dependent_i)];
        if (f.type() == NodeType::operation) {
          const node_index_t a = f.lhs_index();
          const node_index_t b = f.rhs_index();
          const node_index_t da = growing_vector_access(df, a, static_cast<node_index_t>(-1));
          const node_index_t db = growing_vector_access(df, b, static_cast<node_index_t>(-1));
          CURRENT_ASSERT(da != -1);
          CURRENT_ASSERT(db != -1);
          growing_vector_access(df, dependent_i, static_cast<node_index_t>(-1)) =
              d_op(f.operation(), from_index(a), from_index(b), from_index(da), from_index(db));
        } else if (f.type() == NodeType::function) {
          const node_index_t x = f.argument_index();
          const node_index_t dx = growing_vector_access(df, x, static_cast<node_index_t>(-1));
          CURRENT_ASSERT(dx != -1);
          growing_vector_access(df, dependent_i, static_cast<node_index_t>(-1)) =
              d_f(f.function(), from_index(dependent_i), from_index(x), from_index(dx));
        } else {
          CURRENT_ASSERT(false);
          return 0;
        }
      }
    }
  }
  const node_index_t result = growing_vector_access(df, index, static_cast<node_index_t>(-1));
  CURRENT_ASSERT(result != -1);
  return result;
}

template <JIT>
struct g_impl;

struct g_super : noncopyable {
  virtual ~g_super() {}
  virtual std::vector<double_t> operator()(const std::vector<double_t>& x) const = 0;
  // The dimensionality of the parameters vector for the function.
  virtual size_t dim() const = 0;
  // The number of external `double_t` "registers" required to compute it, for compiled versions.
  virtual size_t heap_size() const { return 0; }
};

template <>
struct g_impl<JIT::NativeWrapper> : g_super {
  std::function<double_t(const std::vector<double_t>&)> f_;
  size_t dim_;
  g_impl(std::function<double_t(const std::vector<double_t>&)> f, size_t dim) : f_(f), dim_(dim) {}
  g_impl(g_impl&& rhs) : f_(rhs.f_) {}
  g_impl() = default;
  g_impl(const g_impl&) = delete;
  void operator=(const g_impl& rhs) {
    f_ = rhs.f_;
    dim_ = rhs.dim_;
  }
  std::vector<double_t> operator()(const std::vector<double_t>& x) const override {
    return approximate_gradient(f_, x);
  }
  size_t dim() const override { return dim_; }
};

template <>
struct g_impl<JIT::Blueprint> : g_super {
  V f_;               // `f_` holds the node index for the value of the preprocessed expression.
  std::vector<V> g_;  // `g_[i]` holds the node index for the value of the derivative by variable `i`.
  g_impl(const X& x_ref, const V& f) : f_(f) {
    CURRENT_ASSERT(&x_ref == internals_singleton().x_ptr_);
    const size_t dim = internals_singleton().dim_;
    g_.resize(dim);
    for (size_t i = 0; i < dim; ++i) {
      g_[i] = f_.template differentiate<X>(x_ref, i);
    }
    internals_singleton().node_vector_.shrink_to_fit();
  }
  explicit g_impl(const V& f) : g_impl(*internals_singleton().x_ptr_, f) {}
  g_impl(const X& x_ref, const f_impl<JIT::Blueprint>& fi) : g_impl(x_ref, fi.f_) {}
  explicit g_impl(const f_impl<JIT::Blueprint>& fi) : g_impl(*internals_singleton().x_ptr_, fi.f_) {}
  g_impl() = delete;
  void operator=(const g_impl& rhs) {
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
  std::string debug_gradient_as_string(size_t i) const { return g_[i].debug_as_string(); }
  size_t dim() const override { return g_.size(); }
};

template <>
struct node_differentiate_impl<X> {
  static V differentiate(const X& x_ref, node_index_t node_index, size_t variable_index) {
    CURRENT_ASSERT(&x_ref == internals_singleton().x_ptr_);
    CURRENT_ASSERT(static_cast<size_t>(variable_index) < internals_singleton().dim_);
    return from_index(differentiate_node(node_index, variable_index, internals_singleton().dim_));
  }
};

// Helper code to enable JIT-based `g_impl`-s to be exposed as `fncas::gradient_t`.
template <JIT JIT_IMPLEMENTATION>
struct g_impl_selector {
  using type = g_impl<JIT_IMPLEMENTATION>;
};

template <>
struct g_impl_selector<JIT::Super> {
  using type = g_super;
};

}  // namespace impl

template <JIT JIT_IMPLEMENTATION = JIT::Super>
using gradient_t = typename impl::g_impl_selector<JIT_IMPLEMENTATION>::type;

}  // namespace fncas

#endif  // #ifndef FNCAS_FNCAS_DIFFERENTIATE_H
