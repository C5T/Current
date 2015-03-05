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

#ifndef FNCAS_NODE_H
#define FNCAS_NODE_H

#include <cassert>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <stack>
#include <string>
#include <vector>

#include "fncas_base.h"

namespace fncas {

// Parsed expressions are stored in an array of node_impl objects.
// Instances of node_impl take 10 bytes each and are packed.
// Each node_impl refers to a value, an input variable, an operation or math function invocation.
// Singleton vector<node_impl> is the allocator, therefore the code is single-threaded.

inline const char* operation_as_string(operation_t operation) {
  static const char* representation[static_cast<size_t>(operation_t::end)] = {"+", "-", "*", "/"};
  return operation < operation_t::end ? representation[static_cast<size_t>(operation)] : "?";
}

inline const char* function_as_string(function_t function) {
  static const char* representation[static_cast<size_t>(function_t::end)] = {
      "sqrt", "exp", "log", "sin", "cos", "tan", "asin", "acos", "atan"};
  return function < function_t::end ? representation[static_cast<size_t>(function)] : "?";
}

template <typename T>
inline T apply_operation(operation_t operation, T lhs, T rhs) {
  static std::function<T(T, T)> evaluator[static_cast<size_t>(operation_t::end)] = {
      std::plus<T>(), std::minus<T>(), std::multiplies<T>(), std::divides<T>(),
  };
  return operation < operation_t::end ? evaluator[static_cast<size_t>(operation)](lhs, rhs)
                                      : std::numeric_limits<T>::quiet_NaN();
}

template <typename T>
inline T apply_function(function_t function, T argument) {
  static std::function<T(T)> evaluator[static_cast<size_t>(function_t::end)] = {
      sqrt, exp, log, sin, cos, tan, asin, acos, atan};
  return function < function_t::end ? evaluator[static_cast<size_t>(function)](argument)
                                    : std::numeric_limits<T>::quiet_NaN();
}

struct node_impl;
struct x;
struct internals_impl {
  // The dimensionality of the function that is currently being worked with.
  int32_t dim_;
  x* x_ptr_;

  // All expression nodes created so far, with fixed indexes.
  std::vector<node_impl> node_vector_;

  // Values per node computed so far.
  std::vector<fncas_value_type> node_value_;
  std::vector<int8_t> node_computed_;

  // df_[var_index][node_index] => node index for d (node[node_index]) / d (x[variable_index]), -1 if unknown.
  std::vector<std::vector<node_index_type>> df_;

  // A block of RAM to be used as the buffer for externally compiled functions.
  std::vector<fncas_value_type> ram_for_compiled_evaluations_;

  void reset() {
    dim_ = 0;
    x_ptr_ = nullptr;
    node_vector_.clear();
    df_.clear();
    ram_for_compiled_evaluations_.clear();
  }
};

inline internals_impl& internals_singleton() {
  static internals_impl storage;
  return storage;
}

// Invalidates cached functions, resets temp nodes enumeration from zero and frees cache memory.
inline void reset_internals_singleton() { internals_singleton().reset(); }

inline std::vector<node_impl>& node_vector_singleton() { return internals_singleton().node_vector_; }

struct node_impl {
  uint8_t data_[18];
  type_t& type() { return *reinterpret_cast<type_t*>(&data_[0]); }
  int32_t& variable() {
    assert(type() == type_t::variable);
    return *reinterpret_cast<int32_t*>(&data_[2]);
  }
  fncas_value_type& value() {
    assert(type() == type_t::value);
    return *reinterpret_cast<fncas_value_type*>(&data_[2]);
  }
  operation_t& operation() {
    assert(type() == type_t::operation);
    return *reinterpret_cast<operation_t*>(&data_[1]);
  }
  node_index_type& lhs_index() {
    assert(type() == type_t::operation);
    return *reinterpret_cast<node_index_type*>(&data_[2]);
  }
  node_index_type& rhs_index() {
    assert(type() == type_t::operation);
    return *reinterpret_cast<node_index_type*>(&data_[10]);
  }
  function_t& function() {
    assert(type() == type_t::function);
    return *reinterpret_cast<function_t*>(&data_[1]);
  }
  node_index_type& argument_index() {
    assert(type() == type_t::function);
    return *reinterpret_cast<node_index_type*>(&data_[2]);
  }
};
static_assert(sizeof(node_impl) == 18,
              "sizeof(node_impl) should be 18. Check struct alignment compilation flags.");

// eval_node() should use manual stack implementation to avoid SEGFAULT. Using plain recursion
// will overflow the stack for every formula containing repeated operation on the top level.
enum class reuse_cache : int8_t { invalidate = 0, reuse = 1 };
inline fncas_value_type eval_node(node_index_type index,
                                  const std::vector<fncas_value_type>& x,
                                  reuse_cache reuse = reuse_cache::invalidate) {
  std::vector<fncas_value_type>& V = internals_singleton().node_value_;
  std::vector<int8_t>& B = internals_singleton().node_computed_;
  if (reuse == reuse_cache::invalidate) {
    B.clear();
  }
  std::stack<node_index_type> stack;
  stack.push(index);
  while (!stack.empty()) {
    const node_index_type i = stack.top();
    stack.pop();
    const node_index_type dependent_i = ~i;
    if (i > dependent_i) {
      if (!growing_vector_access(B, i, static_cast<int8_t>(false))) {
        node_impl& f = node_vector_singleton()[i];
        if (f.type() == type_t::variable) {
          int32_t v = f.variable();
          assert(v >= 0 && v < static_cast<int32_t>(x.size()));
          growing_vector_access(V, i, 0.0) = x[v];
          growing_vector_access(B, i, static_cast<int8_t>(false)) = true;

        } else if (f.type() == type_t::value) {
          growing_vector_access(V, i, 0.0) = f.value();
          growing_vector_access(B, i, static_cast<int8_t>(false)) = true;
        } else if (f.type() == type_t::operation) {
          stack.push(~i);
          stack.push(f.lhs_index());
          stack.push(f.rhs_index());
        } else if (f.type() == type_t::function) {
          stack.push(~i);
          stack.push(f.argument_index());
        } else {
          assert(false);
          return std::numeric_limits<fncas_value_type>::quiet_NaN();
        }
      }
    } else {
      node_impl& f = node_vector_singleton()[dependent_i];
      if (f.type() == type_t::operation) {
        growing_vector_access(V, dependent_i, 0.0) =
            apply_operation<fncas_value_type>(f.operation(), V[f.lhs_index()], V[f.rhs_index()]);
        growing_vector_access(B, dependent_i, static_cast<int8_t>(false)) = true;
      } else if (f.type() == type_t::function) {
        growing_vector_access(V, dependent_i, 0.0) =
            apply_function<fncas_value_type>(f.function(), V[f.argument_index()]);
        growing_vector_access(B, dependent_i, static_cast<int8_t>(false)) = true;
      } else {
        assert(false);
        return std::numeric_limits<fncas_value_type>::quiet_NaN();
      }
    }
  }
  assert(B[index]);
  return V[index];
}

// The code that deals with nodes directly uses class node as a wrapper to node_impl.
// Since the storage for node_impl-s is global, class node just holds an index of node_impl.
// User code that defines the function to work with is effectively dealing with class node objects:
// arithmetical and mathematical operations are overloaded for class node.

struct allocate_new {};
enum class from_index : node_index_type;

struct node_index_allocator {
  node_index_type index_;  // non-const since `node` objects can be modified.
  explicit inline node_index_allocator(from_index i) : index_(static_cast<node_index_type>(i)) {}
  explicit inline node_index_allocator(allocate_new) : index_(node_vector_singleton().size()) {
    node_vector_singleton().resize(index_ + 1);
  }
  node_index_type index() const { return index_; }
  node_index_allocator() = delete;
};

// Template used as a header-only way to move implemneation to another source file.
struct node;
template <typename T>
struct node_differentiate_impl {
  //  node differentiate(const x& x_ref, int32_t variable_index) const;
};

struct node : node_index_allocator {
 private:
  node(const node_index_allocator& instance) : node_index_allocator(instance) {}

 public:
  node() : node_index_allocator(allocate_new()) {}
  node(fncas_value_type x) : node_index_allocator(allocate_new()) {
    type() = type_t::value;
    value() = x;
  }
  node(from_index i) : node_index_allocator(i) {}
  type_t& type() const { return node_vector_singleton()[index_].type(); }
  int32_t& variable() const { return node_vector_singleton()[index_].variable(); }
  fncas_value_type& value() const { return node_vector_singleton()[index_].value(); }
  operation_t& operation() const { return node_vector_singleton()[index_].operation(); }
  node_index_type& lhs_index() const { return node_vector_singleton()[index_].lhs_index(); }
  node_index_type& rhs_index() const { return node_vector_singleton()[index_].rhs_index(); }
  node lhs() const { return from_index(node_vector_singleton()[index_].lhs_index()); }
  node rhs() const { return from_index(node_vector_singleton()[index_].rhs_index()); }
  function_t& function() const { return node_vector_singleton()[index_].function(); }
  node_index_type& argument_index() const { return node_vector_singleton()[index_].argument_index(); }
  node argument() const { return from_index(node_vector_singleton()[index_].argument_index()); }
  static node variable(node_index_type index) {
    node result;
    result.type() = type_t::variable;
    result.variable() = index;
    return result;
  }
  std::string debug_as_string() const {
    if (type() == type_t::variable) {
      return "x[" + std::to_string(variable()) + "]";
    } else if (type() == type_t::value) {
      return std::to_string(value());
    } else if (type() == type_t::operation) {
      // Note: this recursive call will overflow the stack with SEGFAULT on deep functions.
      // For debugging purposes only.
      return "(" + lhs().debug_as_string() + operation_as_string(operation()) + rhs().debug_as_string() + ")";
    } else if (type() == type_t::function) {
      // Note: this recursive call will overflow the stack with SEGFAULT on deep functions.
      // For debugging purposes only.
      return std::string(function_as_string(function())) + "(" + argument().debug_as_string() + ")";
    } else {
      return "?";
    }
  }
  fncas_value_type operator()(const std::vector<fncas_value_type>& x,
                              reuse_cache reuse = reuse_cache::invalidate) const {
    return eval_node(index_, x, reuse);
  }
  // Template is used here as a form of forward declaration.
  template <typename X>
  node differentiate(const X& x_ref, int32_t variable_index) const {
    static_assert(std::is_same<X, x>::value, "node::differentiate(const x& x, int32_t variable_index);");
    // Note: This method will not build unless `fncas_differentiate.h` is included.
    return node_differentiate_impl<X>::differentiate(x_ref, index_, variable_index);
  }
};
static_assert(sizeof(node) == 8, "sizeof(node) should be 8, as sizeof(node_index_type).");

// Class "x" is the placeholder class an instance of which is to be passed to the user function
// to record the computation rather than perform it.

struct x : noncopyable {
  explicit x(int32_t dim) {
    assert(dim > 0);
    auto& meta = internals_singleton();
    assert(!meta.x_ptr_);
    assert(!meta.dim_);
    meta.x_ptr_ = this;
    meta.dim_ = dim;
  }
  node operator[](int32_t i) const {
    assert(i >= 0);
    assert(i < internals_singleton().dim_);
    return node(node::variable(i));
  }
  size_t size() const {
    auto& meta = internals_singleton();
    assert(meta.x_ptr_ == this);
    return static_cast<size_t>(meta.dim_);
  }
};

// Class "f" is the placeholder for function evaluators.
// One implementation -- f_intermediate -- is provided by default.
// Compiled implementations using the same interface are defined in fncas_jit.h.

struct f : noncopyable {
  virtual ~f() = default;
  virtual fncas_value_type operator()(const std::vector<fncas_value_type>& x) const = 0;
  virtual int32_t dim() const = 0;
};

struct f_native : f {
  std::function<fncas_value_type(const std::vector<fncas_value_type>&)> f_;
  int32_t d_;
  f_native(std::function<fncas_value_type(std::vector<fncas_value_type>)> f, int32_t d) : f_(f), d_(d) {}
  virtual fncas_value_type operator()(const std::vector<fncas_value_type>& x) const { return f_(x); }
  virtual int32_t dim() const { return d_; }
};

struct f_intermediate : f {
  const node f_;
  f_intermediate(const node& f) : f_(f) {}
  f_intermediate(f_intermediate&& rhs) : f_(rhs.f_) {}
  virtual fncas_value_type operator()(const std::vector<fncas_value_type>& x) const {
    assert(static_cast<int32_t>(x.size()) == dim());
    return f_(x);
  }
  std::string debug_as_string() const { return f_.debug_as_string(); }
  // Template is used here as a form of forward declaration.
  template <typename X>
  node differentiate(const X& x_ref, int32_t variable_index) const {
    static_assert(std::is_same<X, x>::value,
                  "f_intermediate::differentiate(const x& x, int32_t variable_index);");
    assert(&x_ref == internals_singleton().x_ptr_);
    assert(variable_index >= 0);
    assert(variable_index < dim());
    return f_.template differentiate<X>(x_ref, variable_index);
  }
  virtual int32_t dim() const { return internals_singleton().dim_; }
};

// Helper code to allow writing polymorphic functions that can be both evaluated and recorded.
// Synopsis: template<typename T> typename fncas::output<T>::type f(const T& x);

template <typename T>
struct output {};
template <>
struct output<std::vector<fncas_value_type>> {
  typedef fncas_value_type type;
};
template <>
struct output<x> {
  typedef fncas::node type;
};

}  // namespace fncas

// Arithmetic operations and mathematical functions are defined outside namespace fncas.

#define DECLARE_OP(OP, OP2, NAME)                                                    \
  inline fncas::node operator OP(const fncas::node& lhs, const fncas::node& rhs) {   \
    fncas::node result;                                                              \
    result.type() = fncas::type_t::operation;                                        \
    result.operation() = fncas::operation_t::NAME;                                   \
    result.lhs_index() = lhs.index_;                                                 \
    result.rhs_index() = rhs.index_;                                                 \
    return result;                                                                   \
  }                                                                                  \
  inline const fncas::node& operator OP2(fncas::node& lhs, const fncas::node& rhs) { \
    lhs = lhs OP rhs;                                                                \
    return lhs;                                                                      \
  }

// TODO(dkorolev): Support unary minus as well.
DECLARE_OP(+, +=, add);
DECLARE_OP(-, -=, subtract);
DECLARE_OP(*, *=, multiply);
DECLARE_OP(/, /=, divide);

#define DECLARE_FUNCTION(F)                           \
  inline fncas::node F(const fncas::node& argument) { \
    fncas::node result;                               \
    result.type() = fncas::type_t::function;          \
    result.function() = fncas::function_t::F;         \
    result.argument_index() = argument.index_;        \
    return result;                                    \
  }

DECLARE_FUNCTION(sqrt);
DECLARE_FUNCTION(exp);
DECLARE_FUNCTION(log);
DECLARE_FUNCTION(sin);
DECLARE_FUNCTION(cos);
DECLARE_FUNCTION(tan);
DECLARE_FUNCTION(asin);
DECLARE_FUNCTION(acos);
DECLARE_FUNCTION(atan);

#endif  // #ifndef FNCAS_NODE_H
