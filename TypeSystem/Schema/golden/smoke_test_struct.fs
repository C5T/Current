// Usage: `fsharpi -r Newtonsoft.Json.dll schema.fs`
(*
open Newtonsoft.Json
let inline JSON o = JsonConvert.SerializeObject(o)
let inline ParseJSON (s : string) : 'T = JsonConvert.DeserializeObject<'T>(s)
*)

type Primitives = {
  // It's the "order" of fields that matters.
  a : byte

  // Field descriptions can be set in any order.
  b : uint16
  c : uint32
  d : uint64
  e : sbyte
  f : int16
  g : int32
  h : int64
  i : char
  j : string
  k : float
  l : double

  // Multiline
  // descriptions
  // can be used.
  m : bool
  n : int64  // microseconds.
  o : int64  // milliseconds.
}

type A = {
  a : int32
}

type B = {
  a : int32
  b : int32
}

type B2 = {
  a : int32
}

type X = {
  x : int32
}

type E = uint16

type Y = {
  e : E
}

type MyFreakingVariant =
| A of A
| X of X
| Y of Y

type C = {
  e : Empty
  c : MyFreakingVariant
}

type Variant_B_A_B_B2_C_Empty_E =
| A of A
| B of B
| B2 of B2
| C of C
| Empty

type Templated_T9209980946934124423 = {
  foo : int32
  bar : X
}

type Templated_T9227782344077896555 = {
  foo : int32
  bar : MyFreakingVariant
}

type TemplatedInheriting_T9200000002835747520 = {
  a : int32
  baz : string
  meh : Empty
}

type Templated_T9209626390174323094 = {
  foo : int32
  bar : TemplatedInheriting_T9200000002835747520
}

type TemplatedInheriting_T9209980946934124423 = {
  a : int32
  baz : string
  meh : X
}

type TemplatedInheriting_T9227782344077896555 = {
  a : int32
  baz : string
  meh : MyFreakingVariant
}

type Templated_T9200000002835747520 = {
  foo : int32
  bar : Empty
}

type TemplatedInheriting_T9201673071807149456 = {
  a : int32
  baz : string
  meh : Templated_T9200000002835747520
}

type FullTest = {
  // A structure with a lot of primitive types.
  primitives : Primitives
  v1 : string array
  v2 : Primitives array
  p : string * Primitives
  o : Primitives option

  // Field | descriptions | FTW !
  q : Variant_B_A_B_B2_C_Empty_E
  w1 : Templated_T9209980946934124423
  w2 : Templated_T9227782344077896555
  w3 : Templated_T9209626390174323094
  w4 : TemplatedInheriting_T9209980946934124423
  w5 : TemplatedInheriting_T9227782344077896555
  w6 : TemplatedInheriting_T9201673071807149456
}
