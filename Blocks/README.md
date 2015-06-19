## REST API Toolkit

The [`#include "Blocks/HTTP/api.h"`](https://github.com/C5T/Current/blob/master/Blocks/HTTP/api.h) header enables to run the code snippets below.

### HTTP Client

```cpp
// Simple GET.
EXPECT_EQ("OK", HTTP(GET("http://test.tailproduce.org/ok")).body);

// More fields.
const auto response = HTTP(GET("http://test.tailproduce.org/ok"));
EXPECT_EQ("OK", response.body);
EXPECT_TRUE(response.code == HTTPResponseCode.OK);
```
```cpp
// POST is supported as well.
EXPECT_EQ("OK", HTTP(POST("http://test.tailproduce.org/ok"), "BODY", "text/plain").body);

// Beyond plain strings, cerealizable objects can be passed in.
// JSON will be sent, as "application/json" content type.
EXPECT_EQ("OK", HTTP(POST("http://test.tailproduce.org/ok"), SimpleType()).body);

```
HTTP client supports headers, POST-ing data to and from files, and many other features as well. Check the unit test in [`Blocks/HTTP/test.cc`](https://github.com/C5T/Current/blob/master/Blocks/HTTP/test.cc) for more details.
### HTTP Server
```cpp
// Simple "OK" endpoint.
HTTP(port).Register("/ok", [](Request r) {
  r("OK");
});
```
```cpp
// Accessing input fields.
HTTP(port).Register("/demo", [](Request r) {
  r(r.url.query["q"] + ' ' + r.method + ' ' + r.body);
});
```
```cpp
// Constructing a more complex response.
HTTP(port).Register("/found", [](Request r) {
  r("Yes.",
    HTTPResponseCode.Accepted,
    "text/html",
    HTTPHeaders().Set("custom", "header").Set("another", "one"));
});
```
```cpp
// An input record that would be passed in as a JSON.
struct PennyInput {
  std::string op;
  std::vector<int> x;
  std::string error;  // Not serialized.
  template <typename A> void serialize(A& ar) {
    ar(CEREAL_NVP(op), CEREAL_NVP(x));
  }
  void FromInvalidJSON(const std::string& input_json) {
    error = "JSON parse error: " + input_json;
  }
};

// An output record that would be sent back as a JSON.
struct PennyOutput {
  std::string error;
  int result;
  template <typename A> void serialize(A& ar) {
    ar(CEREAL_NVP(error), CEREAL_NVP(result));
  }
}; 

// Doing Penny-level arithmetics for fun and performance testing.
HTTP(port).Register("/penny", [](Request r) {
  const auto input = ParseJSON<PennyInput>(r.body);
  if (!input.error.empty()) {
    r(PennyOutput{input.error, 0});
  } else {
    if (input.op == "add") {
      if (!input.x.empty()) {
        int result = 0;
        for (const auto v : input.x) {
          result += v;
        }
        r(PennyOutput{"", result});
      } else {
        r(PennyOutput{"Not enough arguments for 'add'.", 0});
      }
    } else if (input.op == "mul") {
      if (!input.x.empty()) {
        int result = 1;
        for (const auto v : input.x) {
          result *= v;
        }
        r(PennyOutput{"", result});
      } else {
        r(PennyOutput{"Not enough arguments for 'mul'.", 0});
      }
    } else {
      r(PennyOutput{"Unknown operation: " + input.op, 0});
    }
  }
});
```
```cpp
// Returning a potentially unlimited response chunk by chunk.
HTTP(port).Register("/chunked", [](Request r) {
  const size_t n = atoi(r.url.query["n"].c_str());
  const size_t delay_ms = atoi(r.url.query["delay_ms"].c_str());
    
  const auto sleep = [&delay_ms]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
  };
    
  auto response = r.SendChunkedResponse();

  sleep();
  for (size_t i = 0; n && i < n; ++i) {
    response(".");  // Use double quotes for a string, not single quotes for a char.
    sleep();
  }
  response("\n");
});

EXPECT_EQ(".....\n", HTTP(GET("http://test.tailproduce.org/chunked?n=5&delay_ms=2")).body);

// NOTE: For most legitimate practical usecases of returning unlimited
// amounts of data, consider Sherlock's stream data replication mechanisms.
```
HTTP server also has support for several other features, check out the [`Blocks/HTTP/test.cc`](https://github.com/C5T/Current/blob/master/Blocks/HTTP/test.cc) unit test.
