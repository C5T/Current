# Bricks

![](https://raw.githubusercontent.com/KnowSheet/Bricks/master/holy_bricks.jpg)

Our own core pieces to reuse.

# Documentation
## Cerealize
```c++
// A C++ structure is "cerealize"-able if it can be serialized.
struct SimpleType {
  int number;
  std::string string;
  std::vector<int> vector_int;
  std::map<int, std::string> map_int_string;
  // Add a templated `serialize()` method to make it serializable to and from JSON and binary.
  template <typename A> void serialize(A& ar) {
    // Use the `CEREAL_NVP(member)` syntax to keep member names in JSON format.
    ar(CEREAL_NVP(number), CEREAL_NVP(string), CEREAL_NVP(vector_int), CEREAL_NVP(map_int_string));
  }
};
```
```c++
// Cerealize-able types can be serialized and de-serialized into JSON and binary formats.
SimpleType x;
x.number = 42;
x.string = "test passed";
x.vector_int.push_back(1);
x.vector_int.push_back(2);
x.vector_int.push_back(3);
x.map_int_string[1] = "one";
x.map_int_string[42] = "the question";
// Use `JSON(object)` to convert a cerealize-able object into a JSON string.
const std::string json = JSON(x);
// Use `JSONParse<T>(json)` to create an instance of a cerializable type T from its JSON representation.
const SimpleType y = JSONParse<SimpleType>(json);
// `JSONParse(json, T&)` is an alternate two-parameters form that enables omitting the template type.
SimpleType z;
JSONParse(json, z);
```
