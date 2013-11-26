// https://github.com/dkorolev/fncas

// Defines FNCAS node class for basic expressions and the means to use it.

#ifndef FNCAS_NODE_H
#define FNCAS_NODE_H

#include <limits>
#include <stack>
#include <string>
#include <vector>

#include <boost/assert.hpp>
#include <boost/function.hpp>
#include <boost/static_assert.hpp>
#include <boost/utility.hpp>

#include "fncas_base.h"

namespace fncas {

// Parsed expressions are stored in an array of node_impl objects.
// Instances of node_impl take 10 bytes each and are packed.
// Each node_impl refers to a value, an input variable, an operation or math function invocation.
// Singleton vector<node_impl> is the allocator, therefore the code is single-threaded.

enum type_t : uint8_t { variable, value, operation, function };
enum struct operation_t : uint8_t { add, subtract, multiply, divide, end };
enum struct function_t : uint8_t { exp, log, sin, cos, tan, asin, acos, atan, end };

const char* const operation_as_string(operation_t operation) {
  static const char* representation[static_cast<size_t>(operation_t::end)] = {
    "+", "-", "*", "/"
  };
  return operation < operation_t::end ? representation[static_cast<size_t>(operation)] : "?";
}

const char* const function_as_string(function_t function) {
  static const char* representation[static_cast<size_t>(function_t::end)] = {
    "exp", "log", "sin", "cos", "tan", "asin", "acos", "atan"
  };
  return function < function_t::end ? representation[static_cast<size_t>(function)] : "?";
}

template<typename T> T apply_operation(operation_t operation, T lhs, T rhs) {
  static boost::function<T(T, T)> evaluator[static_cast<size_t>(operation_t::end)] = {
    std::plus<T>(),
    std::minus<T>(),
    std::multiplies<T>(),
    std::divides<T>(),
  };
  return
    operation < operation_t::end
    ? evaluator[static_cast<size_t>(operation)](lhs, rhs)
    : std::numeric_limits<T>::quiet_NaN();
}

template<typename T> T apply_function(function_t function, T argument) {
  static boost::function<T(T)> evaluator[static_cast<size_t>(function_t::end)] = {
    exp, log, sin, cos, tan, asin, acos, atan
  };
  return
    function < function_t::end
    ? evaluator[static_cast<size_t>(function)](argument)
    : std::numeric_limits<T>::quiet_NaN();
}

struct node_impl;
struct internals_impl {
  std::vector<node_impl> node_vector_;
  std::vector<fncas_value_type> ram_for_evaluations_;
  void reset() {
    node_vector_.clear();
    ram_for_evaluations_.clear();
  }
};

inline internals_impl& internals() {
  static internals_impl storage;
  return storage;
}

// Invalidates cached functions, resets temp nodes enumeration from zero and frees cache memory.
inline void reset() {
  internals().reset();
}

inline std::vector<node_impl>& node_vector_singleton() {
  return internals().node_vector_;
}

struct node_impl {
  uint8_t data_[10];
  type_t& type() {
    return *reinterpret_cast<type_t*>(&data_[0]);
  }
  uint32_t& variable() {
    BOOST_ASSERT(type() == type_t::variable); 
    return *reinterpret_cast<uint32_t*>(&data_[2]);
  }
  fncas_value_type& value() { 
    BOOST_ASSERT(type() == type_t::value); 
    return *reinterpret_cast<fncas_value_type*>(&data_[2]);
  }
  operation_t& operation() {
    BOOST_ASSERT(type() == type_t::operation);
    return *reinterpret_cast<operation_t*>(&data_[1]);
  }
  uint32_t& lhs_index() { 
    BOOST_ASSERT(type() == type_t::operation); 
    return *reinterpret_cast<uint32_t*>(&data_[2]);
  }
  uint32_t& rhs_index() { 
    BOOST_ASSERT(type() == type_t::operation);
    return *reinterpret_cast<uint32_t*>(&data_[6]);
  }
  function_t& function() {
    BOOST_ASSERT(type() == type_t::function);
    return *reinterpret_cast<function_t*>(&data_[1]);
  }
  uint32_t& argument_index() {
    BOOST_ASSERT(type() == type_t::function); 
    return *reinterpret_cast<uint32_t*>(&data_[2]);
  }
};
BOOST_STATIC_ASSERT(sizeof(node_impl) == 10);

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
      node_impl& node = node_vector_singleton()[i];
      if (node.type() == type_t::variable) {
        uint32_t v = node.variable();
        BOOST_ASSERT(v >= 0 && v < x.size());
        cache[i] = x[v];
      } else if (node.type() == type_t::value) {
        cache[i] = node.value();
      } else if (node.type() == type_t::operation) {
        stack.push(~i);
        stack.push(node.lhs_index());
        stack.push(node.rhs_index());
      } else if (node.type() == type_t::function) {
        stack.push(~i);
        stack.push(node.argument_index());
      } else {
        BOOST_ASSERT(false);
        return std::numeric_limits<fncas_value_type>::quiet_NaN();
      }
    } else {
      node_impl& node = node_vector_singleton()[dependent_i];
      if (node.type() == type_t::operation) {
        cache[dependent_i] = apply_operation<fncas_value_type>(
          node.operation(),
          cache[node.lhs_index()],
          cache[node.rhs_index()]);
      } else if (node.type() == type_t::function) {
        cache[dependent_i] = apply_function<fncas_value_type>(
          node.function(),
          cache[node.argument_index()]);
      } else {
        BOOST_ASSERT(false);
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

struct node_constructor {
  mutable uint64_t index_;
  explicit node_constructor(uint64_t index) : index_(static_cast<uint64_t>(index)) {}
  explicit node_constructor() : index_(node_vector_singleton().size()) {
    node_vector_singleton().resize(index_ + 1);
  }
};

struct node : node_constructor {
 private:
  node(const node_constructor& instance) : node_constructor(instance) {}
 public:
  node() : node_constructor() {}
  node(fncas_value_type x) : node_constructor() { type() = type_t::value; value() = x; }
  type_t& type() const { return node_vector_singleton()[index_].type(); }
  uint32_t& variable() const { return node_vector_singleton()[index_].variable(); }
  fncas_value_type& value() const { return node_vector_singleton()[index_].value(); }
  operation_t& operation() const { return node_vector_singleton()[index_].operation(); }
  uint32_t& lhs_index() const { return node_vector_singleton()[index_].lhs_index(); }
  uint32_t& rhs_index() const { return node_vector_singleton()[index_].rhs_index(); }
  node lhs() const { return node_constructor(node_vector_singleton()[index_].lhs_index()); }
  node rhs() const { return node_constructor(node_vector_singleton()[index_].rhs_index()); }
  function_t& function() const { return node_vector_singleton()[index_].function(); }
  uint32_t& argument_index() const { return node_vector_singleton()[index_].argument_index(); }
  node argument() const { return node_constructor(node_vector_singleton()[index_].argument_index()); }
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
      return
        "(" + lhs().debug_as_string() +
        operation_as_string(operation()) +
        rhs().debug_as_string() + ")";
    } else if (type() == type_t::function) {
      // Note: this recursive call will overflow the stack with SEGFAULT on deep functions.
      // For debugging purposes only.
      return
        std::string(function_as_string(function())) + "(" + argument().debug_as_string() + ")";
    } else {
      return "?";
    }
  }
  fncas_value_type eval(const std::vector<fncas_value_type>& x) const {
    return eval_node(index_, x);
  }
};
BOOST_STATIC_ASSERT(sizeof(node) == 8);

// Class "x" is the placeholder class an instance of which is to be passed to the user function
// to record the computation rather than perform it.

struct x : boost::noncopyable {
  int dim_;
  explicit x(int dim) : dim_(dim) {
    BOOST_ASSERT(dim_ >= 0);
  }
  node operator[](int i) const {
    BOOST_ASSERT(i >= 0);
    BOOST_ASSERT(i < dim_);
    return node::variable(i);
  }
};

// Class "f" is the placeholder for function evaluators.
// One implementation -- f_intermediate -- is provided by default.
// Compiled implementations using the same interface are defined in fncas_jit.h.

struct f : boost::noncopyable {
  virtual ~f() {}
  virtual fncas_value_type invoke(const std::vector<fncas_value_type>& x) const = 0;
};

struct f_intermediate : f {
  const node node_;
  explicit f_intermediate(const node& node) : node_(node) {
  }
  virtual double invoke(const std::vector<double>& x) const {
    return node_.eval(x);
  }
};

// Helper code to allow writing polymorphic functions that can be both evaluated and recorded.
// Synopsis: template<typename T> typename fncas::output<T>::type f(const T& x);

template<typename T> struct output {};
template<> struct output<std::vector<fncas_value_type> > { typedef fncas_value_type type; };
template<> struct output<x> { typedef fncas::node type; };

}  // namespace fncas

// Arithmetic operations and mathematical functions are defined outside namespace fncas.

#define DECLARE_OP(OP,OP2,NAME) \
inline fncas::node operator OP(const fncas::node& lhs, const fncas::node& rhs) { \
  fncas::node result; \
  result.type() = fncas::type_t::operation; \
  result.operation() = fncas::operation_t::NAME; \
  result.lhs_index() = lhs.index_; \
  result.rhs_index() = rhs.index_; \
  return result; \
} \
inline const fncas::node& operator OP2(fncas::node& lhs, const fncas::node& rhs) { \
  lhs = lhs OP rhs; \
  return lhs; \
}
DECLARE_OP(+,+=,add);
DECLARE_OP(-,-=,subtract);
DECLARE_OP(*,*=,multiply);
DECLARE_OP(/,/=,divide);

#define DECLARE_FUNCTION(F) \
inline fncas::node F(const fncas::node& argument) { \
  fncas::node result; \
  result.type() = fncas::type_t::function; \
  result.function() = fncas::function_t:: F; \
  result.argument_index() = argument.index_; \
  return result; \
}
DECLARE_FUNCTION(exp);
DECLARE_FUNCTION(log);
DECLARE_FUNCTION(sin);
DECLARE_FUNCTION(cos);
DECLARE_FUNCTION(tan);
DECLARE_FUNCTION(asin);
DECLARE_FUNCTION(acos);
DECLARE_FUNCTION(atan);

#endif  // #ifndef FNCAS_NODE_H
