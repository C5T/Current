# `FnCAS`

FnCAS, originally stands for "FnCAS is not a Computer Algebra System", is a fast and easy-to-use wrapper for gradient-descent-based optimizations. The speed is achieved by JIT-compilation of the function and its gradient, and the ease of use comes from automated analytical differentiation.

## Gentle Introduction

The snippets below require nothing but `git clone`-ing Current and `#include`-ing `FnCAS/fncas/fncas.h`.

(Fine, for `JSON()` you'd also need `TypeSystem/serialization/json.h`. But, really, that's it.)

