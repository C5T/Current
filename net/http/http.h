#ifndef BRICKS_NET_HTTP_HTTP_H
#define BRICKS_NET_HTTP_HTTP_H

#include "../../port.h"

#if defined(BRICKS_POSIX) || defined(BRICKS_APPLE)
#include "impl/server.h"
#else
#error "No implementation for `net/http.h` is available for your system."
#endif

#endif  // BRICKS_NET_HTTP_HTTP_H
