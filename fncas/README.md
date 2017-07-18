# `FnCAS`

FnCAS, originally stands for "FnCAS is not a Computer Algebra System", is a fast and easy-to-use wrapper for gradient-descent-based optimizations. The speed is achieved by JIT-compilation of the function and its gradient, and the ease of use comes from automated analytical differentiation.

## Gentle Introduction

The snippets below require nothing but `git clone`-ing Current and `#include`-ing `FnCAS/fncas/fncas.h`.

(Fine, for `JSON()` you'd also need `TypeSystem/serialization/json.h`. But, really, that's it.)

```cpp

// Define a simple function of two arguments.
//
// Note that the only change required in the user code is to change `double`
// into `template T` in the good old `double f(const std::vector<double>& x)`
// declaration.
//
// The rest of the magic is handled transparently by FnCAS.
template <typename T>
T simple_function(const std::vector<T>& x) {
  ++number_of_calls;
  assert(x.size() == 2u);
  // `sqr(x)` is a convenience wrapper defined within `fncas::` for ML purposes,
  // along with `ramp(v)` and `unit_step(v)`.
  return fncas::sqr(x[0] + 1) + fncas::sqr(x[1] + 2);
  // Alternatively, `#define INJECT_FNCAS_INTO_NAMESPACE_STD` and use
  // the mathematical library from within the `std::` namespace.
}

// Shorten the snippets in this docu.
using namespace fncas;

// Make a few native C++ calls.
number_of_calls = 0;
EXPECT_EQ(5, simple_function(std::vector<double>({0, 0})));
EXPECT_EQ(4*4 + 3*3, simple_function(std::vector<double>({-5, -5})));
ASSERT_EQ(2, number_of_calls);

// Wrap a native C++ function into an FnCAS type.
// This type allows passing functions around without thinking of whether they
// are native functions, blueprints, or JIT-compiled dynamically linked `.so`-s.
// The `2` parameter is the dimensionality of the function.
function_t<JIT::NativeWrapper> native(simple_function<double>, 2);
// The superclass, `function_t<>` is a shortcut for `function_t<JIT::Super>`.
const function_t<>& reference = native;

number_of_calls = 0;
EXPECT_EQ(5, reference(std::vector<double>({0, 0})));
EXPECT_EQ(4*4 + 3*3, reference(std::vector<double>({-5, -5})));
ASSERT_EQ(2, number_of_calls);  // By-reference evaluation just calls the function.

// Create the blueprint of this function: its internal tree representation.
// The scope of `x` would be where the blueprint and its uses are valid
// from within this particular thread. It uses a thread-local singleton.
variables_vector_t x(2);
number_of_calls = 0;
function_t<JIT::Blueprint> blueprint = simple_function(x);
ASSERT_EQ(1, number_of_calls);
number_of_calls = 0;
EXPECT_EQ(5, blueprint(std::vector<double>({0, 0})));
EXPECT_EQ(4*4 + 3*3, blueprint(std::vector<double>({-5, -5})));
ASSERT_EQ(0, number_of_calls);  // Blueprint evalution doesn't call the function.

// For demo purposes only: Examine the textual representation of the blueprint.
EXPECT_EQ("(sqr((x[0]+1))+sqr((x[1]+2)))", blueprint.debug_as_string());

// Create the JIT-compiled representation of the function.
const function_t<JIT::AS> jit(blueprint);
number_of_calls = 0;
EXPECT_EQ(5, jit(std::vector<double>({0, 0})));
EXPECT_EQ(4*4 + 3*3, jit(std::vector<double>({-5, -5})));
ASSERT_EQ(0, number_of_calls);

// Confirm both the blueprint and the JIT version can be cast down to `function_t<>`,
const function_t<>& reference = blueprint;
number_of_calls = 0;
EXPECT_EQ(5, reference(std::vector<double>({0, 0})));
EXPECT_EQ(4*4 + 3*3, reference(std::vector<double>({-5, -5})));
ASSERT_EQ(0, number_of_calls);

const function_t<>& jit_reference = jit;
number_of_calls = 0;
EXPECT_EQ(5, jit_reference(std::vector<double>({0, 0})));
EXPECT_EQ(4*4 + 3*3, jit_reference(std::vector<double>({-5, -5})));
ASSERT_EQ(0, number_of_calls);

// Wrap the function into the approximate gradient computer, which simply does
// `g[i] = (f(x + unit[i] * eps) - f(x - unit[i] * eps)) / (eps * 2)` per each dimension,
// where `g[i]` is the i-th component of the gradient, `x` is the point, `unit[i]`
// is the `(0,...,0,1,0,...,0)` vector with a `1` at index `i`, and `eps` is small.
// The `2` parameter is the dimensionality of the function.
const gradient_t<JIT::NativeWrapper> g_approximate(simple_function<double>, 2);
number_of_calls = 0;
EXPECT_NEAR(2.0, g_approximate({0, 0})[0], 1e-5);
ASSERT_EQ(4, number_of_calls);  // Plus delta and minus delta, one per variable.
EXPECT_NEAR(4.0, g_approximate({0, 0})[1], 1e-5);
ASSERT_EQ(8, number_of_calls);

// Compute the blueprint of the gradient from the blueprint of the function.
const gradient_t<JIT::Blueprint> g_blueprint(blueprint);
number_of_calls = 0;
EXPECT_EQ(2.0, g_blueprint({0, 0})[0]);
EXPECT_EQ(4.0, g_blueprint({0, 0})[1]);
ASSERT_EQ(0, number_of_calls);  // No function calls, of course.

// Generate the JIT-compiled version of the gradient.
const gradient_t<JIT::AS> g_jit(blueprint, g_blueprint);
number_of_calls = 0;
EXPECT_EQ(2.0, g_jit({0, 0})[0]);
EXPECT_EQ(4.0, g_jit({0, 0})[1]);
ASSERT_EQ(0, number_of_calls);  // No function calls, of course.

// Confirm the gradients, too, can be cast down to `gradient_t<>`.
std::vector<std::reference_wrapper<const gradient_t<>>> g_references({ 
  g_approximate,
  g_blueprint,
  g_jit
});
number_of_calls = 0;
EXPECT_NEAR(2.0, g_references[0].get()({0, 0})[0], 1e-5);
EXPECT_NEAR(4.0, g_references[0].get()({0, 0})[1], 1e-5);
ASSERT_EQ(8, number_of_calls);
number_of_calls = 0;
EXPECT_EQ("[2.0,4.0]", JSON(g_references[1].get()({0, 0})));
EXPECT_EQ("[2.0,4.0]", JSON(g_references[2].get()({0, 0})));
ASSERT_EQ(0, number_of_calls);
```

## Links

For a deeper dive, check out:

* An [end-to-end optimization example](https://github.com/C5T/Current/blob/master/examples/Optimize/optimize.cc).
* The [unit test](https://github.com/C5T/Current/blob/master/fncas/test.cc).
