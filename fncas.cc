// TODO(dkorolev): Test evaluation.
// TODO(dkorolev): Use BFS instead of DFS for evaluating and printing.

// TODO(dkorolev): Move to the header.
// TODO(dkorolev): Readme, build instructions and a reference from this file.
//
// Requires:
// * C++0X, use g++ --std=c++0x
// * Boost, sudo apt-get install libboost-dev

#include <cassert>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <boost/assert.hpp>
#include <boost/function.hpp>
#include <boost/static_assert.hpp>
#include <boost/utility.hpp>

namespace fncas {

// Parsed expression are stored in an array of node_impl objects.
// Instances of node_impl take 10 bytes each and are packed.
// Each node_impl refers to a value, an input variable, an operation or math function invocation.
// Singleton vector<node_impl> is the allocator, therefore the code is single-threaded.

enum type_t : uint8_t { var, value, op };
enum op_t : uint8_t { add = 0, subtract = 1, multiply = 2, end = 3 };

std::string op_as_string(op_t op) {
  static std::string represenatation[op_t::end] = { "+", "-", "*" };
  return (op >= 0 && op < static_cast<int>(op_t::end)) ? represenatation[op] : "?";
}

double apply_op(op_t op, double lhs, double rhs) {
  static boost::function<double(double, double)> evaluator[op_t::end] = {
    std::plus<double>(),
    std::minus<double>(),
    std::multiplies<double>(),
  };
  return (op >= 0 && op < static_cast<int>(op_t::end)) ? evaluator[op](lhs, rhs) : std::numeric_limits<double>::quiet_NaN();
}

BOOST_STATIC_ASSERT(sizeof(type_t) == 1);
BOOST_STATIC_ASSERT(sizeof(op_t) == 1);
BOOST_STATIC_ASSERT(sizeof(double) == 8);

struct node_impl {
  uint8_t data_[10];
  type_t& type() { return *reinterpret_cast<type_t*>(&data_[0]); }
  uint32_t& var_index() { BOOST_ASSERT(type() == type_t::var); return *reinterpret_cast<uint32_t*>(&data_[2]); }
  op_t& op() { BOOST_ASSERT(type() == type_t::op); return *reinterpret_cast<op_t*>(&data_[1]); }
  uint32_t& lhs_index() { BOOST_ASSERT(type() == type_t::op); return *reinterpret_cast<uint32_t*>(&data_[2]); }
  uint32_t& rhs_index() { BOOST_ASSERT(type() == type_t::op); return *reinterpret_cast<uint32_t*>(&data_[6]); }
  double& value() { BOOST_ASSERT(type() == type_t::value); return *reinterpret_cast<double*>(&data_[2]); }
};
BOOST_STATIC_ASSERT(sizeof(node_impl) == 10);

inline std::vector<node_impl>& node_vector_singleton() {
  static std::vector<node_impl> storage;
  return storage;
}

// The code that deals with nodes directly uses class node as a wrapper to node_impl.
// Since the storage for node_impl-s is global, class node just holds an index of node_impl.
// User code that defines the function to work with is effectively dealing with class node objects:
// arithmetical and mathematical operations are overloaded for class node.

struct node_constructor {
  mutable uint64_t index_;
  explicit node_constructor(uint64_t index) : index_(static_cast<uint64_t>(index)) {}
  explicit node_constructor() : index_(node_vector_singleton().size()) { node_vector_singleton().resize(index_ + 1); }
};

struct node : node_constructor {
 private:
  node() : node_constructor() {}
  node(const node_constructor& instance) : node_constructor(instance) {}
 public:
  node(double x) : node_constructor() { type() = type_t::value; value() = x; }
  type_t& type() const { return node_vector_singleton()[index_].type(); }
  uint32_t& var_index() const { return node_vector_singleton()[index_].var_index(); }
  op_t& op() const { return node_vector_singleton()[index_].op(); }
  uint32_t& lhs_index() const { return node_vector_singleton()[index_].lhs_index(); }
  uint32_t& rhs_index() const { return node_vector_singleton()[index_].rhs_index(); }
  node lhs() const { return node_constructor(node_vector_singleton()[index_].lhs_index()); }
  node rhs() const { return node_constructor(node_vector_singleton()[index_].rhs_index()); }
  double& value() const { return node_vector_singleton()[index_].value(); }
  static node alloc() { return node(); }
  static node var(uint32_t index) {
    node result;
    result.type() = type_t::var;
    result.var_index() = index;
    return result;
  }
  std::string debug_as_string() const {
    if (type() == type_t::var) {
      return "x[" + std::to_string(var_index()) + "]";
    } else if (type() == type_t::value) {
      return std::to_string(value());
    } else if (type() == type_t::op) {
      return "(" + lhs().debug_as_string() + op_as_string(op()) + rhs().debug_as_string() + ")";
    } else {
      return "?";
    }
  }
  double eval(const std::vector<double>& x) const {
    if (type() == type_t::var) {
      uint32_t i = var_index();
      BOOST_ASSERT(i >= 0 && i < x.size());
      return x[i];
    } else if (type() == type_t::value) {
      return value();
    } else if (type() == type_t::op) {
      return apply_op(op(), lhs().eval(x), rhs().eval(x));
    } else {
      return std::numeric_limits<double>::quiet_NaN();
    }
  }
};
BOOST_STATIC_ASSERT(sizeof(node) == 8);

// Class "x" is the placeholder class an instance of which is to be passed to the user function
// to record the computation rather than perform it.

struct x : boost::noncopyable {
  int dim;
  explicit x(int dim) : dim(dim) {
    BOOST_ASSERT(dim > 0);
  }
  node operator[](int i) const {
    BOOST_ASSERT(i >= 0);
    BOOST_ASSERT(i < dim);
    return node::var(i);
  }
};

// Helper code to allow writing polymorphic function that can be both evaluated and recorded.
// Synopsis: template<typename T> typename fncas::output<T>::type f(const T& x);

template<typename T> struct output {};
template<> struct output<std::vector<double> > { typedef double type; };
template<> struct output<x> { typedef fncas::node type; };

}  // namespace fncas

#define DECLARE_OP(OP,NAME) \
inline fncas::node operator OP(const fncas::node& lhs, const fncas::node& rhs) { \
  fncas::node result = fncas::node::alloc(); \
  result.type() = fncas::type_t::op; \
  result.op() = fncas::op_t::NAME; \
  result.lhs_index() = lhs.index_; \
  result.rhs_index() = rhs.index_; \
  return result; \
}
DECLARE_OP(+,add);
DECLARE_OP(-,subtract);
DECLARE_OP(*,multiply);

template<typename T> typename fncas::output<T>::type f(const T& x) {
  return 1 + x[0] * x[1] + x[2] - x[3] + 1;
}

void test_smoke() {
  std::vector<double> x({5,8,9,7});
  printf("%.3lf\n", f(x));
  auto e = f(fncas::x(4));
  printf("%s\n", e.debug_as_string().c_str());
  printf("%.3lf\n", e.eval(x));
}

int main() {
  test_smoke();
}
