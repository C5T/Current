/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#ifndef KARL_SCHEMA_H
#define KARL_SCHEMA_H

#include "../port.h"

#include "current_build.h"

#include "../TypeSystem/struct.h"
#include "../TypeSystem/optional.h"
#include "../Bricks/time/chrono.h"

namespace current {
namespace karl {

CURRENT_STRUCT(ClaireStatusBase) {
  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(codename, std::string);
  CURRENT_FIELD(local_port, uint16_t);

  CURRENT_FIELD(registered, bool, false);

  CURRENT_FIELD(us_start, std::chrono::microseconds);
  CURRENT_FIELD(us_now, std::chrono::microseconds);  // Uptime is calculated by Karl, along with time skew.

  CURRENT_FIELD(build, Optional<build::Info>);
};

CURRENT_STRUCT(ClaireStatus, ClaireStatusBase) {
  CURRENT_FIELD(status, Optional<std::string>);
  CURRENT_FIELD(details, (std::map<std::string, std::string>));
};

}  // namespace current::karl
}  // namespace current

#endif  // KARL_SCHEMA_H
