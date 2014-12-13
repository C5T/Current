#ifndef BRICKS_NET_TCP_TCP_H
#define BRICKS_NET_TCP_TCP_H

#include "../../port.h"

#if defined(BRICKS_POSIX) || defined(BRICKS_APPLE)
#include "impl/posix.h"
#else
#error "No implementation for `net/tcp.h` is available for your system."
#endif

#endif  // BRICKS_NET_TCP_TCP_H
