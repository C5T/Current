// HTTP server for the unit test in `net/api/test.cc`.
//
// This implementation uses the API that Bricks provides.
// Compare this code to the one in `net/http/test_server.cc.h`.
//
// This is a `.cc.h` file, since it violates the one-definition rule for gtest, and thus
// should not be an independent header. It is nonetheless #include-d by net/api/test.cc.

#ifndef BRICKS_NET_API_TEST_SERVER_CC_H
#define BRICKS_NET_API_TEST_SERVER_CC_H

#include "../../dflags/dflags.h"

#include "../http/test_server.cc.h"

namespace bricks {
namespace net {
namespace api {
namespace test {

struct TestHTTPServer_APIImpl : bricks::net::http::test::TestHTTPServer_HTTPImpl {
  // TODO(dkorolev): Real API implementation goes here.
};

}  // namespace test
}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_TEST_SERVER_CC_H
