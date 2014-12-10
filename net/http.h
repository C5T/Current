#if defined(__linux) || defined(__APPLE__)
#include "http/posix.h"
#else
#error "No HTTP implementation available for your platform."
#endif
