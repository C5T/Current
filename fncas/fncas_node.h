// https://github.com/dkorolev/fncas

// Defines FNCAS node class for basic expressions and the means to use it.

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

enum type_t : uint8_t { variable, value, operation, function };
enum struct operation_t : uint8_t { add, subtract, multiply, divide, end };
enum struct function_t : uint8_t { sqrt, exp, log, sin, cos, tan, asin, acos, atan, end };

const char* const operation_as_string(operation_t operation) {
  static const char* representation[static_cast<size_t>(operation_t::end)] = {"+", "-", "*", "/"};
  return operation < operation_t::end ? representation[static_cast<size_t>(operation)] : "?";
}

const char* const function_as_string(function_t function) {
  static const char* representation[static_cast<size_t>(function_t::end)] = {
      "sqrt", "exp", "log", "sin", "cos", "tan", "asin", "acos", "atan"};
  return function < function_t::end ? representation[static_cast<size_t>(function)] : "?";
}

template <typename T> T apply_operation(operation_t operation, T lhs, T rhs) {
  static std::function<T(T, T)> evaluator[static_cast<size_t>(operation_t::end)] = {
      std::plus<T>(), std::minus<T>(), std::multiplies<T>(), std::divides<T>(),
  };
  return operation < operation_t::end ? evaluator[static_cast<size_t>(operation)](lhs, rhs)
                                      : std::numeric_limits<T>::quiet_NaN();
}

template <typename T> T apply_function(function_t function, T argument) {
  static std::function<T(T)> evaluator[static_cast<size_t>(function_t::end)] = {
      sqrt, exp, log, sin, cos, tan, asin, acos, atan};
  return function < function_t::end ? evaluator[static_cast<size_t>(function)](argument)
                                    : std::numeric_limits<T>::quiet_NaN();
}

struct node_impl;
struct internals_impl {
  std::vector<node_impl> node_vector_;
  std::vector<fncas_value_type> ram_for_evaluations_;
  // df_by_var_[var_index][node_index] => node index for d (node[node_index]) / d (x[variable_index]).
  std::map<uint32_t, std::map<uint32_t, uint32_t>> df_by_var_;
  void reset() {
    node_vector_.clear();
    ram_for_evaluations_.clear();
    df_by_var_.clear();
  }
};

inline internals_impl& internals_singleton() {
  static internals_impl storage;
  return storage;
}

// Invalidates cached functions, resets temp nodes enumeration from zero and frees cache memory.
inline void reset_internals_singleton() {
  internals_singleton().reset();
}

inline std::vector<node_impl>& node_vector_singleton() {
  return internals_singleton().node_vector_;
}

struct node_impl {
  uint8_t data_[10];
  type_t& type() {
    return *reinterpret_cast<type_t*>(&data_[0]);
  }
  uint32_t& variable() {
    assert(type() == type_t::variable);
    return *reinterpret_cast<uint32_t*>(&data_[2]);
  }
  fncas_value_type& value() {
    assert(type() == type_t::value);
    return *reinterpret_cast<fncas_value_type*>(&data_[2]);
  }
  operation_t& operation() {
    assert(type() == type_t::operation);
    return *reinterpret_cast<operation_t*>(&data_[1]);
  }
  uint32_t& lhs_index() {
    assert(type() == type_t::operation);
    return *reinterpret_cast<uint32_t*>(&data_[2]);
  }
  uint32_t& rhs_index() {
    assert(type() == type_t::operation);
    return *reinterpret_cast<uint32_t*>(&data_[6]);
  }
  function_t& function() {
    assert(type() == type_t::function);
    return *reinterpret_cast<function_t*>(&data_[1]);
  }
  uint32_t& argument_index() {
    assert(type() == type_t::function);
    return *reinterpret_cast<uint32_t*>(&data_[2]);
  }
};
static_assert(sizeof(node_impl) == 10, "sizeof(node_impl) should be 10. Check struct alignment compilation flags.");

// eval_node() should use manual stack implementation to avoid SEGFAULT. Using plain recursion
// will overflow the stack for every formula containing repeated operation on the top level.
fncas_value_type eval_node(uint32_t index, const std::vector<fncas_value_type>& x) {
  std::vector<fncas_value_type> cache(node_vector_singleton().size());
  std::stack<uint32_t> stack;
  stack.push(index);
  while (!stack.empty()) {
    const uint32_t i = stack.top();
    stack.pop();
    const uint32_t dependent_i = ~i;
    if (i < dependent_i) {
      node_impl& f = node_vector_singleton()[i];
      if (f.type() == type_t::variable) {
        uint32_t v = f.variable();
        assert(v >= 0 && v < x.size());
        cache[i] = x[v];
      } else if (f.type() == type_t::value) {
        cache[i] = f.value();
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
    } else {
      node_impl& f = node_vector_singleton()[dependent_i];
      if (f.type() == type_t::operation) {
        cache[dependent_i] =
            apply_operation<fncas_value_type>(f.operation(), cache[f.lhs_index()], cache[f.rhs_index()]);
      } else if (f.type() == type_t::function) {
        cache[dependent_i] = apply_function<fncas_value_type>(f.function(), cache[f.argument_index()]);
      } else {
        assert(false);
        return std::numeric_limits<fncas_value_type>::quiet_NaN();
      }
    }
  }
  return cache[index];
}

// The code that deals with nodes directly uses class node as a wrapper to node_impl.
// Since the storage for node_impl-s is global, class node just holds an index of node_impl.
// User code that defines the function to work with is effectively dealing with class node objects:
// arithmetical and mathematical operations are overloaded for class node.

struct allocate_new {};
enum class from_index : uint32_t;

struct node_index_allocator {
  uint64_t index_;  // non-const since `node` objects can be modified.
  explicit inline node_index_allocator(from_index i) : index_(static_cast<uint64_t>(i)) {
  }
  explicit inline node_index_allocator(allocate_new) : index_(node_vector_singleton().size()) {
    node_vector_singleton().resize(index_ + 1);
  }
  uint64_t index() const {
    return index_;
  }
  node_index_allocator() = delete;
};

struct node : node_index_allocator {
 private:
  node(const node_index_allocator& instance) : node_index_allocator(instance) {
  }

 public:
  node() : node_index_allocator(allocate_new()) {
  }
  node(fncas_value_type x) : node_index_allocator(allocate_new()) {
    type() = type_t::value;
    value() = x;
  }
  node(from_index i) : node_index_allocator(i) {
  }
  type_t& type() const {
    return node_vector_singleton()[index_].type();
  }
  uint32_t& variable() const {
    return node_vector_singleton()[index_].variable();
  }
  fncas_value_type& value() const {
    return node_vector_singleton()[index_].value();
  }
  operation_t& operation() const {
    return node_vector_singleton()[index_].operation();
  }
  uint32_t& lhs_index() const {
    return node_vector_singleton()[index_].lhs_index();
  }
  uint32_t& rhs_index() const {
    return node_vector_singleton()[index_].rhs_index();
  }
  node lhs() const {
    return from_index(node_vector_singleton()[index_].lhs_index());
  }
  node rhs() const {
    return from_index(node_vector_singleton()[index_].rhs_index());
  }
  function_t& function() const {
    return node_vector_singleton()[index_].function();
  }
  uint32_t& argument_index() const {
    return node_vector_singleton()[index_].argument_index();
  }
  node argument() const {
    return from_index(node_vector_singleton()[index_].argument_index());
  }
  static node variable(uint32_t index) {
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
  fncas_value_type operator()(const std::vector<fncas_value_type>& x) const {
    return eval_node(index_, x);
  }
  node differentiate(size_t variable_index) const {
    // The implemenation of the differentiation operator is in `fncas_differentiate.h`.
    uint32_t differentiate_node(uint32_t index, uint32_t var_index);
    return from_index(differentiate_node(index_, variable_index));
  }
};
static_assert(sizeof(node) == 8, "sizeof(node) should be 8. Check struct alignment compilation flags.");

struct node_with_dim {
  node f;
  size_t d;
  node_with_dim() = default;
  node_with_dim(node f, size_t d) : f(f), d(d) {
  }
  node_with_dim(fncas_value_type x) : f(x), d(0) {
  }
};

// Class "x" is the placeholder class an instance of which is to be passed to the user function
// to record the computation rather than perform it.

struct x : noncopyable {
  size_t dim_;
  explicit x(size_t dim) : dim_(dim) {
    assert(dim_ < static_cast<size_t>(1e9));  // Defent against negative.
  }
  node_with_dim operator[](int i) const {
    assert(i >= 0);
    assert(i < dim_);
    return node_with_dim(node::variable(i), dim_);
  }
};

// Class "f" is the placeholder for function evaluators.
// One implementation -- f_intermediate -- is provided by default.
// Compiled implementations using the same interface are defined in fncas_jit.h.

struct f : noncopyable {
  virtual ~f() {
  }
  virtual fncas_value_type operator()(const std::vector<fncas_value_type>& x) const = 0;
  virtual size_t dim() const = 0;
};

struct f_intermediate : f {
  const node_with_dim fd_;
  f_intermediate(const node_with_dim& fd) : fd_(fd) {
    assert(fd_.d > 0);
    assert(fd_.d < static_cast<size_t>(1e9));  // To defend against negatives.
  }
  f_intermediate(f_intermediate&& rhs) : fd_(rhs.fd_) {
  }
  virtual fncas_value_type operator()(const std::vector<fncas_value_type>& x) const {
    assert(x.size() == fd_.d);
    return fd_.f(x);
  }
  std::string debug_as_string() const {
    return fd_.f.debug_as_string();
  }
  node differentiate(size_t variable_index) const {
    assert(variable_index < fd_.d);
    return fd_.f.differentiate(variable_index);
  }
  virtual size_t dim() const {
    return fd_.d;
  }
};

// Helper code to allow writing polymorphic functions that can be both evaluated and recorded.
// Synopsis: template<typename T> typename fncas::output<T>::type f(const T& x);

template <typename T> struct output {};
template <> struct output<std::vector<fncas_value_type>> { typedef fncas_value_type type; };
template <> struct output<x> { typedef fncas::node_with_dim type; };

}  // namespace fncas

// Arithmetic operations and mathematical functions are defined outside namespace fncas.

#define DECLARE_OP(OP, OP2, NAME)                                                                               \
  inline fncas::node operator OP(const fncas::node& lhs, const fncas::node& rhs) {                              \
    fncas::node result;                                                                                         \
    result.type() = fncas::type_t::operation;                                                                   \
    result.operation() = fncas::operation_t::NAME;                                                              \
    result.lhs_index() = lhs.index_;                                                                            \
    result.rhs_index() = rhs.index_;                                                                            \
    return result;                                                                                              \
  }                                                                                                             \
  inline const fncas::node& operator OP2(fncas::node& lhs, const fncas::node& rhs) {                            \
    lhs = lhs OP rhs;                                                                                           \
    return lhs;                                                                                                 \
  }                                                                                                             \
  inline fncas::node_with_dim operator OP(const fncas::node_with_dim& lhs, const fncas::node_with_dim& rhs) {   \
    assert(!lhs.d || !rhs.d || lhs.d == rhs.d);                                                                 \
    return fncas::node_with_dim({lhs.f OP rhs.f, std::max(lhs.d, rhs.d)});                                      \
  }                                                                                                             \
  inline const fncas::node_with_dim& operator OP2(fncas::node_with_dim& lhs, const fncas::node_with_dim& rhs) { \
    lhs = lhs OP rhs;                                                                                           \
    return lhs;                                                                                                 \
  }
DECLARE_OP(+, +=, add);
DECLARE_OP(-, -=, subtract);
DECLARE_OP(*, *=, multiply);
DECLARE_OP(/, /=, divide);

#define DECLARE_FUNCTION(F)                                             \
  inline fncas::node F(const fncas::node& argument) {                   \
    fncas::node result;                                                 \
    result.type() = fncas::type_t::function;                            \
    result.function() = fncas::function_t::F;                           \
    result.argument_index() = argument.index_;                          \
    return result;                                                      \
  }                                                                     \
  inline fncas::node_with_dim F(const fncas::node_with_dim& argument) { \
    return fncas::node_with_dim({F(argument.f), argument.d});           \
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
