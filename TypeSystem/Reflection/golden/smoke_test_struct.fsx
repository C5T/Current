// fsharpi -r Newtonsoft.Json.dll current.fsx

open Newtonsoft.Json
let inline JSON o = JsonConvert.SerializeObject(o)
let inline ParseJSON (s : string) : 'T = JsonConvert.DeserializeObject<'T>(s)

type Primitives = {
  a : byte
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
  m : bool
  n : int64  // microseconds.
  o : int64  // milliseconds.
}
type FullTest = {
  primitives : Primitives
  v1 : string array
  v2 : Primitives array
  p : string * Primitives
  o : Primitives option
}
