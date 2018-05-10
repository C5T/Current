// When `JITCompiledCPP` is provided the second parameter, the path to `current`, the
// symbolic link called `current` is created in the directory in which the user code is compiled.
//
// This immediately allows the user code to access Current headers by means of `#include "current/..."`.
//
// Much like without the second parameter provided to `JITCompiledCPP`, the user code
// is compiled from within a directory where the `current.h` header file is placed.
//
// In the absence of Current symlinked in that directory, the dummy, empty, `current.h` header created there
// serves the purpose of allowing the user to have their own, different `current.h` file in the direcory
// from which they would be "curl POST"-ing this file to the server, to define the symbols the "server-side" dlopen()
// would contain in the bolierplate, thus making sure the user code is self-sufficient from the IDE purposes,
// such that features such as completion and errors highlighting would work just fine.
//
// In the presence of Current, the `current.h` header is no longer dummy, but serves as a boilerplate to include
// various Current-specific wrappers so that the user code has immediate access to them.
//
// This contents of this header file are then "tuned" to demonstrate the power of C++ with Current,
// without slowing the compilation much.

#include "current/bricks/strings/printf.h"
#include "current/bricks/strings/util.h"
#include "current/bricks/strings/split.h"
#include "current/bricks/strings/join.h"
#include "current/bricks/strings/chunk.h"

using current::strings::Printf;
using current::strings::FromString;
using current::strings::ToString;
using current::strings::ToLower;
using current::strings::ToUpper;
using current::strings::Trim;
using current::strings::Split;
using current::strings::Join;
using current::strings::Chunk;

#include "current/bricks/util/singleton.h"

using current::Singleton;

#include "current/bricks/util/random.h"
using namespace current::random;

// #include "current/typesystem/serialization/json.h" -- A bit too slow, but perhaps add a symlink to `json.h` as well.
