// TODO(dkorolev): Add Mac and find out the right header guards once the code is ready.
#if defined(__linux)
#include "tcp/posix.h"
#else
#error "No TCP implementation available for your platform."
#endif
