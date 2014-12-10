#if defined(__linux) || defined(__APPLE__)
#include "tcp/posix.h"
#else
#error "No TCP implementation available for your platform."
#endif
