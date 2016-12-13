# `FnCAS`

FnCAS, originally stands for "FnCAS is not a Computer Algebra System", is a fast and easy-to-use wrapper for gradient-descent-based optimization of convex functions. The speed is achieved by JIT-compilation of the function and its gradient, and the ease of use comes from automated analytical differnetiation.

## Gentle Introduction

The snippets below require nothing but `git clone`-ing Current and `#include`-ing `FnCAS/fncas/fncas.h`.

```cpp
EXPECT_EQ(4, 2*2);
```

## Links

For a deeper dive, check out:

* An [end-to-end optimization example](https://github.com/C5T/Current/blob/master/examples/Optimize/optimize.cc).
* The [unit test](https://github.com/C5T/Current/blob/master/FnCAS/test.cc).
