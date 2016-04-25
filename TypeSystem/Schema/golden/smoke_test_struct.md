# Data Dictionary

### `Primitives`
| **Field** | **Type** |
| ---: | :--- |
| `a` | Integer (8-bit unsigned) |
| `b` | Integer (16-bit unsigned) |
| `c` | Integer (32-bit unsigned) |
| `d` | Integer (64-bit unsigned) |
| `e` | Integer (8-bit signed) |
| `f` | Integer (16-bit signed) |
| `g` | Integer (32-bit signed) |
| `h` | Integer (64-bit signed) |
| `i` | Character |
| `j` | String |
| `k` | Number (floating point, single precision) |
| `l` | Number (floating point, double precision) |
| `m` | `true` or `false` |
| `n` | Time (microseconds since epoch) |
| `o` | Time (milliseconds since epoch) |


### `A`
| **Field** | **Type** |
| ---: | :--- |
| `a` | Integer (32-bit signed) |


### `B`
| **Field** | **Type** |
| ---: | :--- |
| `a` | Integer (32-bit signed) |
| `b` | Integer (32-bit signed) |


### `Empty`
Intentionally contains no fields.

### `X`
| **Field** | **Type** |
| ---: | :--- |
| `x` | Integer (32-bit signed) |


### `Y`
| **Field** | **Type** |
| ---: | :--- |
| `e` | Index `E`, underlying type `Integer (16-bit unsigned)` |


### `C`
| **Field** | **Type** |
| ---: | :--- |
| `e` | `Empty` |
| `c` | Algebraic `X` / `Y` |


### `FullTest`
| **Field** | **Type** |
| ---: | :--- |
| `primitives` | `Primitives` |
| `v1` | Array of String |
| `v2` | Array of `Primitives` |
| `p` | Pair of String and `Primitives` |
| `o` | `null` or `Primitives` |
| `q` | Algebraic `A` / `B` / `C` / `Empty` |

