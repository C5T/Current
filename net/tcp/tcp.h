#ifndef BRICKS_NET_TCP_TCP_H
#define BRICKS_NET_TCP_TCP_H

#include "../../port.h"

#if defined(BRICKS_POSIX) || defined(BRICKS_APPLE) || defined(BRICKS_JAVA)
#include "impl/posix.h"
#elif defined(BRICKS_ANDROID)
#error "bricks/port.h should not be included in ANDROID builds."
#else
#error "No implementation for `net/tcp.h` is available for your system."
#endif

#endif  // BRICKS_NET_TCP_TCP_H
