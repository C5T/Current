// TODO(dkorolev): Readme, build instructions, note that tested on Ubuntu only.

// TODO(dkorolev): Guard rnutime-compilation-specific and linux-specific code by #define-s.

// TODO(dkorolev): Header.

// Requires:
// * C++0X, use g++ --std=c++0x
// * Boost, sudo apt-get install libboost-dev

#ifndef FNCAS_H
#define FNCAS_H

#include <cassert>
#include <cstdio>
#include <limits>
#include <stack>
#include <string>
#include <vector>

// TODO(dkorolev): Guard this.
#include <dlfcn.h>

#include <boost/assert.hpp>
#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/static_assert.hpp>
#include <boost/utility.hpp>

namespace fncas {

typedef double fncas_value_type;

// Parsed expressions are stored in an array of node_impl objects.
// Instances of node_impl take 10 bytes each and are packed.
// Each node_impl refers to a value, an input variable, an operation or math function invocation.
// Singleton vector<node_impl> is the allocator, therefore the code is single-threaded.

enum type_t : uint8_t { variable, value, operation, function };
enum struct operation_t : uint8_t { add, subtract, multiply, divide, end };
enum struct function_t : uint8_t { exp, log, sin, cos, tan, atan, end };

const char* const operation_as_string(operation_t operation) {
  static const char* representation[static_cast<size_t>(operation_t::end)] = {
    "+", "-", "*", "/"
  };
  return operation < operation_t::end ? representation[static_cast<size_t>(operation)] : "?";
}

const char* const function_as_string(function_t function) {
  static const char* representation[static_cast<size_t>(function_t::end)] = {
    "exp", "log", "sin", "cos", "tan", "atan"
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
    exp, log, sin, cos, tan, atan
  };
  return
    function < function_t::end
    ? evaluator[static_cast<size_t>(function)](argument)
    : std::numeric_limits<T>::quiet_NaN();
}

BOOST_STATIC_ASSERT(sizeof(type_t) == 1);
BOOST_STATIC_ASSERT(sizeof(operation_t) == 1);

// Counting on "double", comment out if trying other data type.
BOOST_STATIC_ASSERT(sizeof(fncas_value_type) == 8);

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

struct internals {
  std::vector<node_impl> node_vector;
  std::vector<double> ram_for_evaluations;
  void reset() {
    node_vector.clear();
    ram_for_evaluations.clear();
  }
};

inline internals& internal_singleton() {
  static internals storage;
  return storage;
}

// Invalidates cached functions, resets temp notes enumberation from zero and frees cache memory.
inline void reset() {
  internal_singleton().reset();
}

inline std::vector<node_impl>& node_vector_singleton() {
  return internal_singleton().node_vector;
}

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

// Unportable and unsafe, but designed to only work on Ubuntu Linux as of now.
// TODO(dkorolev): Make it more portable one day. And make the implementaion(s) template-friendly.

struct compiled_expression : boost::noncopyable {
  typedef int (*DIM)();
  typedef double (*EVAL)(const double* x, double* a);
  void* lib_;
  DIM dim_;
  EVAL eval_;
  explicit compiled_expression(const std::string& lib_filename) {
    lib_ = dlopen(lib_filename.c_str(), RTLD_LAZY);
    BOOST_ASSERT(lib_);
    dim_ = reinterpret_cast<DIM>(dlsym(lib_, "dim"));
    eval_ = reinterpret_cast<EVAL>(dlsym(lib_, "eval"));
    BOOST_ASSERT(dim_);
    BOOST_ASSERT(eval_);
  }
  ~compiled_expression() {
    dlclose(lib_);
  }
  double eval(const double* x) const {
    std::vector<double>& tmp = internal_singleton().ram_for_evaluations;
    size_t dim = static_cast<size_t>(dim_());
    if (tmp.size() < dim) {
      tmp.resize(dim);
    }
    return eval_(x, &tmp[0]);
  }
  double eval(const std::vector<double>& x) const {
    return eval(&x[0]);
  }
  static std::string syscall(const std::string command) {
    FILE* f = popen(command.c_str(), "r");
    BOOST_ASSERT(f);
    static char buffer[1024 * 1024];
    fscanf(f, "%s", buffer);
    fclose(f);
    return buffer;
  }
};

// generate_c_code_for_node() writes C code to evaluate the expression to a file.
void generate_c_code_for_node(uint32_t index, FILE* f) {
  fprintf(f, "#include <math.h>\n");
  fprintf(f, "double eval(const double* x, double* a) {\n");
  size_t max_dim = index;
  std::vector<fncas_value_type> cache(node_vector_singleton().size());
  std::stack<uint32_t> stack;
  stack.push(index);
  while (!stack.empty()) {
    const uint32_t i = stack.top();
    stack.pop();
    const uint32_t dependent_i = ~i;
    if (i < dependent_i) {
      max_dim = std::max(max_dim, static_cast<size_t>(i));
      node_impl& node = node_vector_singleton()[i];
      if (node.type() == type_t::variable) {
        uint32_t v = node.variable();
        fprintf(f, "  a[%d] = x[%d];\n", i, v);
      } else if (node.type() == type_t::value) {
        fprintf(f, "  a[%d] = %a;\n", i, node.value());  // "%a" is hexadecial full precision.
      } else if (node.type() == type_t::operation) {
        stack.push(~i);
        stack.push(node.lhs_index());
        stack.push(node.rhs_index());
      } else if (node.type() == type_t::function) {
        stack.push(~i);
        stack.push(node.argument_index());
      } else {
        BOOST_ASSERT(false);
      }
    } else {
      node_impl& node = node_vector_singleton()[dependent_i];
      if (node.type() == type_t::operation) {
        fprintf(
          f,
          "  a[%d] = a[%d] %s a[%d];\n",
          dependent_i,
          node.lhs_index(),
          operation_as_string(node.operation()),
          node.rhs_index());
      } else if (node.type() == type_t::function) {
        fprintf(
          f,
          "  a[%d] = %s(a[%d]);\n",
          dependent_i,
          function_as_string(node.function()),
          node.argument_index());
      } else {
        BOOST_ASSERT(false);
      }
    }
  }
  fprintf(f, "  return a[%d];\n", index);
  fprintf(f, "}\n");
  fprintf(f, "int dim() { return %d; }\n", static_cast<int>(max_dim + 1));
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
      // Note: this recursive call may overflow the stack with SEGFAULT on deep functions.
      return
        "(" + lhs().debug_as_string() +
        operation_as_string(operation()) +
        rhs().debug_as_string() + ")";
    } else if (type() == type_t::function) {
      // Note: this recursive call may overflow the stack with SEGFAULT on deep functions.
      return
        std::string(function_as_string(function())) + "(" + argument().debug_as_string() + ")";
    } else {
      return "?";
    }
  }
  fncas_value_type eval(const std::vector<fncas_value_type>& x) const {
    return eval_node(index_, x);
  }
  // Generates and compiles code.
  // TODO(dkorolev): Factor out per-language and per-system code generation and compilation as template parameters.
  std::unique_ptr<compiled_expression> compile() const {
    std::string tmp_filename = compiled_expression::syscall("mktemp");
    FILE* f = fopen((tmp_filename + ".c").c_str(), "w");
    BOOST_ASSERT(f);
    generate_c_code_for_node(index_, f);
    fclose(f);
    const char* compile_cmdline = "clang -O3 -fPIC -shared -nostartfiles %1%.c -o %1%.so";
    std::string cmdline = (boost::format(compile_cmdline) % tmp_filename).str();
    compiled_expression::syscall(cmdline);
    return std::unique_ptr<compiled_expression>(new compiled_expression(tmp_filename + ".so"));
  }
};
BOOST_STATIC_ASSERT(sizeof(node) == 8);

// Class "x" is the placeholder class an instance of which is to be passed to the user function
// to record the computation rather than perform it.

struct x : boost::noncopyable {
  int dim;
  explicit x(int dim) : dim(dim) {
    BOOST_ASSERT(dim >= 0);
  }
  node operator[](int i) const {
    BOOST_ASSERT(i >= 0);
    BOOST_ASSERT(i < dim);
    return node::variable(i);
  }
};

// Helper code to allow writing polymorphic function that can be both evaluated and recorded.
// Synopsis: template<typename T> typename fncas::output<T>::type f(const T& x);

template<typename T> struct output {};
template<> struct output<std::vector<fncas_value_type> > { typedef fncas_value_type type; };
template<> struct output<x> { typedef fncas::node type; };

}  // namespace fncas

// Aritmetical operations and mathematical functions are defined outside namespace fncas.

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
DECLARE_FUNCTION(atan);

#endif
