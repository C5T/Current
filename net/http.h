// TODO(dkorolev): Add Mac and find out the right header guards once the code is ready.
#if defined(__linux)
#include "http/posix.h"
#else
#error "No HTTP implementation available for your platform."
#endif
