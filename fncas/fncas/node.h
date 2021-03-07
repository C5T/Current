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

#ifndef FNCAS_FNCAS_NODE_H
#define FNCAS_FNCAS_NODE_H

#include <cmath>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <stack>
#include <string>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "base.h"
#include "exceptions.h"

#include "../../bricks/exception.h"
#include "../../bricks/util/singleton.h"

namespace fncas {
// Non-standard functions useful in data science.
namespace function_impl {
using namespace std;
inline double_t sqr(double_t x) { return x * x; }
inline double_t unit_step(double_t x) { return x >= 0 ? 1 : 0; }
inline double_t ramp(double_t x) { return x > 0 ? x : 0; }
// TODO(dkorolev): Sigmoid, its derivative, normal distribution, ERF, etc.
}  // namespace function_impl
using function_impl::sqr;
using function_impl::unit_step;
using function_impl::ramp;

#define FNCAS_FUNCTION(f) inline double current_fncas_##f(double_t x) { return f(x); }
#include "fncas_functions.dsl.h"
#undef FNCAS_FUNCTION

template <typename T>
T apply_function(::fncas::impl::MathFunction function, T argument) {
  static std::function<T(T)> evaluator[static_cast<size_t>(::fncas::impl::MathFunction::end)] = {
#define FNCAS_FUNCTION(f) current_fncas_##f,
#include "fncas_functions.dsl.h"
#undef FNCAS_FUNCTION
  };
  return function < ::fncas::impl::MathFunction::end ? evaluator[static_cast<size_t>(function)](argument)
                                                     : std::numeric_limits<T>::quiet_NaN();
}
}  // namespace fncas

namespace fncas {
namespace impl {

// Parsed expressions are stored in an array of node_impl objects.
// Instances of `node_impl` take 10 bytes each and are packed.
// Each node_impl refers to a value, an input variable, an operation or math function invocation.
// The `ThreadLocalSingleton` containing `vector<node_impl>` is the allocator, thus
// at most one expression per thread (at most one scope of `fncas::impl::X`) can be "recorded" at a time.

inline const char* operation_as_string(MathOperation operation) {
  static const char* representation[static_cast<size_t>(MathOperation::end)] = {"+", "-", "*", "/"};
  return operation < MathOperation::end ? representation[static_cast<size_t>(operation)] : "?";
}

inline const char* function_as_string(MathFunction function) {
  static const char* representation[static_cast<size_t>(MathFunction::end)] = {
      "sqr", "sqrt", "exp", "log", "sin", "cos", "tan", "asin", "acos", "atan", "unit_step", "ramp"};
  return function < MathFunction::end ? representation[static_cast<size_t>(function)] : "?";
}

template <typename T>
T apply_operation(MathOperation operation, T lhs, T rhs) {
  static std::function<T(T, T)> evaluator[static_cast<size_t>(MathOperation::end)] = {
      std::plus<T>(), std::minus<T>(), std::multiplies<T>(), std::divides<T>(),
  };
  return operation < MathOperation::end ? evaluator[static_cast<size_t>(operation)](lhs, rhs)
                                        : std::numeric_limits<T>::quiet_NaN();
}

struct node_impl;
struct X;
struct internals_impl {
  // The dimensionality of the function that is currently being worked with.
  size_t dim_;

  // The pointer to a thread-local vector of "variables" used for expression parsing.
  X* x_ptr_;

  // All expression nodes created so far, with fixed indexes.
  std::vector<node_impl> node_vector_;

  // Values per node computed so far.
  std::vector<double_t> node_value_;
  std::vector<int8_t> node_computed_;

  // df_[var_index][node_index] => node index for d (node[node_index]) / d (x[variable_index]), -1 if unknown.
  std::vector<std::vector<node_index_t>> df_;

  // A block of RAM to be used as the buffer for externally compiled functions.
  std::vector<double_t> heap_for_compiled_evaluations_;

  // A hashmap of per-immediate-value-created nodes, to not create constants such as zeroes and ones way too often.
  std::unordered_map<double_t, node_index_t> allocated_values_map_;

  void reset() {
    dim_ = 0;
    x_ptr_ = nullptr;
    node_vector_.clear();
    df_.clear();
    heap_for_compiled_evaluations_.clear();
    allocated_values_map_.clear();
  }
};

inline internals_impl& internals_singleton() { return current::ThreadLocalSingleton<internals_impl>(); }

inline std::vector<node_impl>& node_vector_singleton() { return internals_singleton().node_vector_; }

struct node_impl {
  uint8_t data_[18];
  NodeType& type() { return *reinterpret_cast<NodeType*>(&data_[0]); }
  int32_t& variable() {
    CURRENT_ASSERT(type() == NodeType::variable);
    return *reinterpret_cast<int32_t*>(&data_[2]);
  }
  double_t& value() {
    CURRENT_ASSERT(type() == NodeType::value);
    return *reinterpret_cast<double_t*>(&data_[2]);
  }
  MathOperation& operation() {
    CURRENT_ASSERT(type() == NodeType::operation);
    return *reinterpret_cast<MathOperation*>(&data_[1]);
  }
  node_index_t& lhs_index() {
    CURRENT_ASSERT(type() == NodeType::operation);
    return *reinterpret_cast<node_index_t*>(&data_[2]);
  }
  node_index_t& rhs_index() {
    CURRENT_ASSERT(type() == NodeType::operation);
    return *reinterpret_cast<node_index_t*>(&data_[10]);
  }
  MathFunction& function() {
    CURRENT_ASSERT(type() == NodeType::function);
    return *reinterpret_cast<MathFunction*>(&data_[1]);
  }
  node_index_t& argument_index() {
    CURRENT_ASSERT(type() == NodeType::function);
    return *reinterpret_cast<node_index_t*>(&data_[2]);
  }
};
static_assert(sizeof(node_impl) == 18, "sizeof(node_impl) should be 18. Check struct alignment compilation flags.");

// eval_node() should use manual stack implementation to avoid SEGFAULT. Using plain recursion
// will overflow the stack for every formula containing repeated operation on the top level.
enum class reuse_cache : int8_t { invalidate = 0, reuse = 1 };
inline double_t eval_node(node_index_t index,
                          const std::vector<double_t>& x,
                          reuse_cache reuse = reuse_cache::invalidate) {
  std::vector<double_t>& node_value = internals_singleton().node_value_;
  std::vector<int8_t>& B = internals_singleton().node_computed_;
  if (reuse == reuse_cache::invalidate) {
    B.clear();
  }
  std::stack<node_index_t> stack;
  stack.push(index);
  while (!stack.empty()) {
    const node_index_t i = stack.top();
    stack.pop();
    const node_index_t dependent_i = ~i;
    if (i > dependent_i) {
      if (!growing_vector_access(B, i, static_cast<int8_t>(false))) {
        node_impl& f = node_vector_singleton()[static_cast<size_t>(i)];
        if (f.type() == NodeType::variable) {
          const int32_t v = f.variable();
          CURRENT_ASSERT(v >= 0 && v < static_cast<int32_t>(x.size()));
          growing_vector_access(node_value, i, 0.0) = x[v];
          growing_vector_access(B, i, static_cast<int8_t>(false)) = true;

        } else if (f.type() == NodeType::value) {
          growing_vector_access(node_value, i, 0.0) = f.value();
          growing_vector_access(B, i, static_cast<int8_t>(false)) = true;
        } else if (f.type() == NodeType::operation) {
          stack.push(~i);
          stack.push(f.lhs_index());
          stack.push(f.rhs_index());
        } else if (f.type() == NodeType::function) {
          stack.push(~i);
          stack.push(f.argument_index());
        } else {
          CURRENT_ASSERT(false);
          return std::numeric_limits<double_t>::quiet_NaN();
        }
      }
    } else {
      node_impl& f = node_vector_singleton()[static_cast<size_t>(dependent_i)];
      if (f.type() == NodeType::operation) {
        growing_vector_access(node_value, dependent_i, 0.0) =
            apply_operation<double_t>(f.operation(), node_value[static_cast<size_t>(f.lhs_index())], node_value[static_cast<size_t>(f.rhs_index())]);
        growing_vector_access(B, dependent_i, static_cast<int8_t>(false)) = true;
      } else if (f.type() == NodeType::function) {
        growing_vector_access(node_value, dependent_i, 0.0) =
            ::fncas::apply_function<double_t>(f.function(), node_value[static_cast<size_t>(f.argument_index())]);
        growing_vector_access(B, dependent_i, static_cast<int8_t>(false)) = true;
      } else {
        CURRENT_ASSERT(false);
        return std::numeric_limits<double_t>::quiet_NaN();
      }
    }
  }
  CURRENT_ASSERT(B[static_cast<size_t>(index)]);
  return node_value[static_cast<size_t>(index)];
}

// The code that deals with nodes directly uses class V as a wrapper to node_impl.
// Since the storage for node_impl-s is global, class V just holds an index of node_impl.
// User code that defines the function to work with is effectively dealing with class V objects:
// arithmetical and mathematical operations are overloaded for class V.

struct allocate_new {};
struct allocate_for_double {};
enum class from_index : node_index_t;

struct node_index_allocator {
  node_index_t index_;  // non-const since `V` objects can be modified.
  explicit node_index_allocator(from_index i) : index_(static_cast<node_index_t>(i)) {}
  explicit node_index_allocator(allocate_new) : index_(node_vector_singleton().size()) {
    node_vector_singleton().resize(static_cast<size_t>(index_) + 1u);
  }
  node_index_allocator(allocate_for_double, double_t value) {
    std::unordered_map<double_t, node_index_t>& map = internals_singleton().allocated_values_map_;
    auto const cit = map.find(value);
    if (cit != map.end()) {
      index_ = cit->second;
    } else {
      index_ = node_vector_singleton().size();
      map[value] = index_;
      node_vector_singleton().resize(static_cast<size_t>(index_) + 1u);
    }
  }
  node_index_t index() const { return index_; }
  node_index_allocator() = delete;
};

// Template, used as a header-only way to move implementation to another source or header file.
template <typename T>
struct node_differentiate_impl {};

// Need `IS_BOXED`, as `std::vector<V>` can be either a special helper type, or a real `std::vector<V>`, hence two `V`s.
template <bool IS_BOXED>
struct GenericV : node_index_allocator {
 private:
  friend class ::std::vector<GenericV<IS_BOXED>>;
  GenericV(const node_index_allocator& instance) : node_index_allocator(instance) {}

 public:
  GenericV() : node_index_allocator(allocate_new()) {}
  GenericV(double_t x) : node_index_allocator(allocate_for_double(), x) {
    type() = NodeType::value;
    value() = x;
  }
  GenericV(from_index i) : node_index_allocator(i) {}
  NodeType& type() const { return node_vector_singleton()[static_cast<size_t>(index_)].type(); }
  int32_t& variable() const { return node_vector_singleton()[static_cast<size_t>(index_)].variable(); }
  double_t& value() const { return node_vector_singleton()[static_cast<size_t>(index_)].value(); }
  bool is_value() const { return type() == NodeType::value; }
  bool equals_to(double_t expected_value) const { return is_value() && value() == expected_value; }
  MathOperation& operation() const { return node_vector_singleton()[static_cast<size_t>(index_)].operation(); }
  node_index_t& lhs_index() const { return node_vector_singleton()[static_cast<size_t>(index_)].lhs_index(); }
  node_index_t& rhs_index() const { return node_vector_singleton()[static_cast<size_t>(index_)].rhs_index(); }
  GenericV lhs() const { return from_index(node_vector_singleton()[static_cast<size_t>(index_)].lhs_index()); }
  GenericV rhs() const { return from_index(node_vector_singleton()[static_cast<size_t>(index_)].rhs_index()); }
  MathFunction& function() const { return node_vector_singleton()[static_cast<size_t>(index_)].function(); }
  node_index_t& argument_index() const { return node_vector_singleton()[static_cast<size_t>(index_)].argument_index(); }
  GenericV argument() const { return from_index(node_vector_singleton()[static_cast<size_t>(index_)].argument_index()); }
  static GenericV create_variable_node(node_index_t index) {
    GenericV result;
    result.type() = NodeType::variable;
    result.variable() = static_cast<int32_t>(index);
    return result;
  }
  std::string debug_as_string() const {
    if (type() == NodeType::variable) {
      return "x[" + std::to_string(variable()) + "]";
    } else if (type() == NodeType::value) {
      std::ostringstream os;
      os << value();
      return os.str();  // std::to_string(value()); <- this doesn't print integers as integers -- D.K.
    } else if (type() == NodeType::operation) {
      // Note: this recursive call will overflow the stack with SEGFAULT on deep functions.
      // For debugging purposes only.
      return "(" + lhs().debug_as_string() + operation_as_string(operation()) + rhs().debug_as_string() + ")";
    } else if (type() == NodeType::function) {
      // Note: this recursive call will overflow the stack with SEGFAULT on deep functions.
      // For debugging purposes only.
      return std::string(function_as_string(function())) + "(" + argument().debug_as_string() + ")";
    } else {
      return "?";
    }
  }
  double_t operator()(const std::vector<double_t>& x, reuse_cache reuse = reuse_cache::invalidate) const {
    return eval_node(index_, x, reuse);
  }
  // Template is used here as a form of forward declaration.
  template <typename TX>
  GenericV differentiate(const TX& x_ref, size_t variable_index) const {
    static_assert(std::is_same_v<TX, X>, "V::differentiate(const x& x, size_t variable_index);");
    // Note: This method will not build unless `fncas_differentiate.h` is included.
    return node_differentiate_impl<TX>::differentiate(x_ref, index_, variable_index);
  }
};
using V = GenericV<false>;
static_assert(sizeof(V) == 8, "sizeof(V) should be 8, as sizeof(node_index_t).");

// Class "x" is the placeholder class an instance of which is to be passed to the user function
// to record the computation rather than perform it.

}  // namespace fncas::impl
}  // namespace fncas

// A thin wrapper replacing `std::vector<V>`, with the sole purpose of being able to inherit from it
// when defining the special "parameters" class, the lifetime of which is the lifetime of the "formula"
// being dealt with.
namespace std {

// NOTE(dkorolev): This is by no means a complete `vector<>` implementation.
template <>
class vector<::fncas::impl::V> {
 public:
  ~vector() = default;  // Not a final class.

  using value_type = ::fncas::impl::V;
  using contained_value_type = ::fncas::impl::GenericV<true>;

  std::vector<contained_value_type> v;

  struct iterator {
    value_type* v;
    size_t i;
    iterator(value_type* v, size_t i) : v(v), i(i) {}
    value_type& operator*() const { return v[i]; }
    bool operator==(const iterator& rhs) const { return i == rhs.i; }
    bool operator!=(const iterator& rhs) const { return i != rhs.i; }
    void operator++() { ++i; }
  };

  struct const_iterator {
    const value_type* v;
    size_t i;
    const_iterator(const value_type* v, size_t i) : v(v), i(i) {}
    value_type operator*() const { return v[i]; }
    bool operator==(const const_iterator& rhs) const { return i == rhs.i; }
    bool operator!=(const const_iterator& rhs) const { return i != rhs.i; }
    void operator++() { ++i; }
  };

  vector(size_t n = 0u) : v(n) {}
  vector(size_t n, value_type default_value) : v(n) {
    for (auto& value : v) {
      value = reinterpret_cast<contained_value_type&>(default_value);
    }
  }

  bool empty() const { return v.empty(); }
  size_t size() const { return v.size(); }
  void resize(size_t n) { v.resize(n); }
  value_type& operator[](size_t i) { return reinterpret_cast<value_type&>(v[i]); }
  value_type operator[](size_t i) const { return reinterpret_cast<const value_type&>(v[i]); }

  value_type* data() { return v.empty() ? nullptr : reinterpret_cast<value_type*>(&v[0]); }
  const value_type* data() const { return v.empty() ? nullptr : reinterpret_cast<const value_type*>(&v[0]); }

  iterator begin() { return iterator(data(), 0u); }
  iterator end() { return iterator(data(), v.size()); }
  const_iterator begin() const { return const_iterator(data(), 0u); }
  const_iterator end() const { return const_iterator(data(), v.size()); }
};

}  // namespace std

namespace fncas {
namespace impl {

struct X final : std::vector<V>, noncopyable {
  using super_t = std::vector<V>;

  explicit X(size_t dim) {
    CURRENT_ASSERT(dim > 0);
    auto& meta = internals_singleton();
    if (meta.x_ptr_) {
      CURRENT_THROW(exceptions::FnCASConcurrentEvaluationAttemptException());
    }
    CURRENT_ASSERT(!meta.dim_);
    // Invalidates cached functions, resets temp nodes enumeration from zero and frees cache memory.
    meta.reset();
    meta.x_ptr_ = this;
    meta.dim_ = dim;

    // Initialize the actual `vector<V>`.
    super_t::resize(internals_singleton().dim_);
    for (size_t i = 0; i < super_t::size(); ++i) {
      super_t::operator[](static_cast<size_t>(i)) = V::create_variable_node(i);
    }
  }

  virtual ~X() {
    auto& meta = internals_singleton();
    if (meta.x_ptr_ == this) {
      // The condition is required to correctly handle the case when the constructor did `throw`.
      meta.x_ptr_ = nullptr;
      meta.dim_ = 0;
    }
  }
};

// Class "f_super" is the placeholder for function evaluators.
// Two implementations -- f_impl<JIT::NativeWrapper> and f_impl<JIT::Blueprint> -- are provided in this header.
// Compiled implementations using the same interface are defined in fncas_jit.h.

template <JIT>
struct f_impl;

struct f_super : noncopyable {
  virtual ~f_super() = default;
  // The evaluator of the function.
  virtual double_t operator()(const std::vector<double_t>& x) const = 0;
  // The dimensionality of the parameters vector for the function.
  virtual size_t dim() const = 0;
  // The number of external `double_t` "registers" required to compute it, for compiled versions.
  virtual size_t heap_size() const { return 0; }
};

template <>
struct f_impl<JIT::NativeWrapper> final : f_super {
  std::function<double_t(const std::vector<double_t>&)> f_;
  size_t dim_;
  f_impl(std::function<double_t(std::vector<double_t>)> f, size_t d) : f_(f), dim_(d) {}
  double_t operator()(const std::vector<double_t>& x) const override { return f_(x); }
  size_t dim() const override { return dim_; }
};

template <>
struct f_impl<JIT::Blueprint> final : f_super {
  const V f_;
  f_impl(const V& f) : f_(f) {}
  f_impl(f_impl&& rhs) : f_(rhs.f_) {}
  double_t operator()(const std::vector<double_t>& x) const override {
    CURRENT_ASSERT(x.size() == dim());
    return f_(x);
  }
  std::string debug_as_string() const { return f_.debug_as_string(); }
  // Template is used here as a form of forward declaration.
  template <typename TX>
  V differentiate(const TX& x_ref, size_t variable_index) const {
    static_assert(std::is_same_v<TX, X>,
                  "f_impl<JIT::Blueprint>::differentiate(const x& x, size_t variable_index);");
    CURRENT_ASSERT(&x_ref == internals_singleton().x_ptr_);
    CURRENT_ASSERT(variable_index >= 0);
    CURRENT_ASSERT(variable_index < dim());
    return f_.template differentiate<X>(x_ref, variable_index);
  }
  size_t dim() const override { return internals_singleton().dim_; }
};

// Helper code to enable JIT-based `f_impl`-s to be exposed as `fncas::function_t`.
template <JIT JIT_IMPLEMENTATION>
struct f_impl_selector {
  using type = f_impl<JIT_IMPLEMENTATION>;
};

template <>
struct f_impl_selector<JIT::Super> {
  using type = f_super;
};

}  // namespace fncas::impl
}  // namespace fncas

// Arithmetic operations and mathematical functions are defined outside namespace fncas::impl.

#define DECLARE_OP(OP, OP2, NAME)                                                                   \
  inline ::fncas::impl::V operator OP(const ::fncas::impl::V& lhs, const ::fncas::impl::V& rhs) {   \
    ::fncas::impl::V result;                                                                        \
    result.type() = ::fncas::impl::NodeType::operation;                                             \
    result.operation() = ::fncas::impl::MathOperation::NAME;                                        \
    result.lhs_index() = lhs.index_;                                                                \
    result.rhs_index() = rhs.index_;                                                                \
    return result;                                                                                  \
  }                                                                                                 \
  inline const ::fncas::impl::V& operator OP2(::fncas::impl::V& lhs, const ::fncas::impl::V& rhs) { \
    lhs = lhs OP rhs;                                                                               \
    return lhs;                                                                                     \
  }

DECLARE_OP(+, +=, add);
DECLARE_OP(-, -=, subtract);
DECLARE_OP(*, *=, multiply);
DECLARE_OP(/, /=, divide);

// NOTE(dkorolev): This `using namespace std` declaration within `fncas` should be here, not above.
namespace fncas {
using namespace std;
}  // namespace fncas

#ifndef INJECT_FNCAS_INTO_NAMESPACE_STD
// Put the "compile-as-you-execute" implementations of math functions into `fncas::impl::functions::`.
#define DECLARE_FUNCTION(F)                                     \
  namespace fncas {                                             \
  using function_impl::F;                                       \
  inline ::fncas::impl::V F(const ::fncas::impl::V& argument) { \
    ::fncas::impl::V result;                                    \
    result.type() = ::fncas::impl::NodeType::function;          \
    result.function() = ::fncas::impl::MathFunction::F;         \
    result.argument_index() = argument.index_;                  \
    return result;                                              \
  }                                                             \
  }                                                             \
  namespace fncas {                                             \
  namespace impl {                                              \
  using ::fncas::F;                                             \
  }                                                             \
  }
#else
// Expose math functions into `std::` as well.
// NOTE: This is in violation of `C++11: 17.6.4.2.1/1`, and hence guarded. CC @dkorolev, @mzhurovich.
#define DECLARE_FUNCTION(F)                                     \
  namespace fncas {                                             \
  inline ::fncas::impl::V F(const ::fncas::impl::V& argument) { \
    ::fncas::impl::V result;                                    \
    result.type() = ::fncas::impl::NodeType::function;          \
    result.function() = ::fncas::impl::MathFunction::F;         \
    result.argument_index() = argument.index_;                  \
    return result;                                              \
  }                                                             \
  }                                                             \
  namespace std {                                               \
  using ::fncas::F;                                             \
  }
#endif

DECLARE_FUNCTION(sqr);
DECLARE_FUNCTION(sqrt);
DECLARE_FUNCTION(exp);
DECLARE_FUNCTION(log);
DECLARE_FUNCTION(sin);
DECLARE_FUNCTION(cos);
DECLARE_FUNCTION(tan);
DECLARE_FUNCTION(asin);
DECLARE_FUNCTION(acos);
DECLARE_FUNCTION(atan);
DECLARE_FUNCTION(unit_step);
DECLARE_FUNCTION(ramp);

// Unary plus and unary minus.
inline ::fncas::impl::V operator+(const ::fncas::impl::V& x) { return x; }
inline ::fncas::impl::V operator-(const ::fncas::impl::V& x) { return 0.0 - x; }

namespace fncas {

// A smart `double`: behaves like one, but allows for function recording, differentiation, and JIT.
using term_t = impl::V;

// A smart `vector<double>`: behaves like one, but allows for function recording, differentiation, and JIT.
using term_vector_t = std::vector<term_t>;

// Extends `term_vector_t` to be used as the "default" parameter to user functions.
// Passing a `variables_vector_t x(10)` as the parameter stands for "record the expression of this function
// assuming it takes a 10-dimensional vector as the parameter".
// The instance of the `variables_vector_t` is what maintains the state of the thread-local singleton corresponding
// to the function being recorded, and thus at most one `variables_vector_t` per thread can exist at any given
// point in time.
using variables_vector_t = impl::X;

template <JIT JIT_IMPLEMENTATION = JIT::Super>
using function_t = typename impl::f_impl_selector<JIT_IMPLEMENTATION>::type;

}  // namespace fncas

#endif  // #ifndef FNCAS_FNCAS_NODE_H
