# Data Dictionary

### `Primitives`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `a` | Integer (8-bit unsigned) | It's the "order" of fields that matters. |
| `b` | Integer (16-bit unsigned) | Field descriptions can be set in any order. |
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
| `m` | `true` or `false` | Multiline<br>descriptions<br>can be used. |
| `n` | Time (microseconds since epoch) |
| `o` | Time (milliseconds since epoch) |


### `A`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `a` | Integer (32-bit signed) |


### `B`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `a` | Integer (32-bit signed) |
| `b` | Integer (32-bit signed) |


### `B2`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `a` | Integer (32-bit signed) |


### `X`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `x` | Integer (32-bit signed) |


### `Y`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `e` | Index `E`, underlying type `Integer (16-bit unsigned)` |


### `MyFreakingVariant`
Algebraic type, `A` or `X` or `Y`


### `C`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `e` | `Empty` |
| `c` | Algebraic `A` / `X` / `Y` (a.k.a. `MyFreakingVariant`) |


### `Variant_B_A_B_B2_C_Empty_E`
Algebraic type, `A` or `B` or `B2` or `C` or `Empty`


### `Templated_T9209980946934124423`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `foo` | Integer (32-bit signed) |
| `bar` | `X` |


### `Templated_T9227782344077896555`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `foo` | Integer (32-bit signed) |
| `bar` | Algebraic `A` / `X` / `Y` (a.k.a. `MyFreakingVariant`) |


### `TemplatedInheriting_T9200000002835747520`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `a` | Integer (32-bit signed) |
| `baz` | String |
| `meh` | `Empty` |


### `Templated_T9202973911416238761`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `foo` | Integer (32-bit signed) |
| `bar` | `TemplatedInheriting_T9200000002835747520` |


### `TemplatedInheriting_T9209980946934124423`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `a` | Integer (32-bit signed) |
| `baz` | String |
| `meh` | `X` |


### `TemplatedInheriting_T9227782344077896555`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `a` | Integer (32-bit signed) |
| `baz` | String |
| `meh` | Algebraic `A` / `X` / `Y` (a.k.a. `MyFreakingVariant`) |


### `Templated_T9200000002835747520`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `foo` | Integer (32-bit signed) |
| `bar` | `Empty` |


### `TemplatedInheriting_T9201673071807149456`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `a` | Integer (32-bit signed) |
| `baz` | String |
| `meh` | `Templated_T9200000002835747520` |


### `FullTest`
| **Field** | **Type** | **Description** |
| ---: | :--- | :--- |
| `primitives` | `Primitives` | A structure with a lot of primitive types. |
| `v1` | Array of String |
| `v2` | Array of `Primitives` |
| `p` | Pair of String and `Primitives` |
| `o` | `null` or `Primitives` |
| `q` | Algebraic `A` / `B` / `B2` / `C` / `Empty` (a.k.a. `Variant_B_A_B_B2_C_Empty_E`) | Field &#124; descriptions &#124; FTW ! |
| `w1` | `Templated_T9209980946934124423` |
| `w2` | `Templated_T9227782344077896555` |
| `w3` | `Templated_T9202973911416238761` |
| `w4` | `TemplatedInheriting_T9209980946934124423` |
| `w5` | `TemplatedInheriting_T9227782344077896555` |
| `w6` | `TemplatedInheriting_T9201673071807149456` |

