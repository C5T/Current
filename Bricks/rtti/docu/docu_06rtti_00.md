## Run-Time Type Dispatching

Bricks can dispatch calls to the right implementation at runtime, with user code being free of virtual functions.

This comes especially handy when processing log entries from a large stream of data, where only a few types are of immediate interest.

Use the [`#include "Bricks/rtti/dispatcher.h"`](https://github.com/C5T/Current/blob/Bricks/master/rtti/dispatcher.h) header to run the code snippets below.

`TODO(dkorolev)` a wiser way for the end user to leverage the above is by means of `Sherlock` once it's checked in.
