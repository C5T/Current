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

#include "../TypeSystem/struct.h"
#include "../Bricks/time/chrono.h"

namespace current {
namespace karl {

CURRENT_STRUCT(ClaireToKarlBase) {
  // A required field. Changes to `true` as Karl has accepted Claire.
  CURRENT_FIELD(up, bool, false);

  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(codename, std::string);

  // Local port. For the outer world to be able to connect to this service.
  // By convention, `http://localhost:local_port/current` or `http://localhost:local_port/.../current`
  // should respond with the JSON representation of this structure.
  CURRENT_FIELD(local_port, uint16_t);
};

CURRENT_STRUCT(ClaireToKarl, ClaireToKarlBase) {
  // TODO(dkorolev): Dependencies.
  // TODO(dkorolev): PID.

  // Other basic fields.
  CURRENT_FIELD(us_start, std::chrono::microseconds);
  CURRENT_FIELD(us_now, std::chrono::microseconds);     // For time sync check.
  CURRENT_FIELD(us_uptime, std::chrono::microseconds);  // The actual uptime.

  // A custom-filled-in `map<string,string>` for what has to be journaled by Karl.
  CURRENT_FIELD(status, (std::map<std::string, std::string>));
};

// TODO(dkorolev): The above was the structure for runtime data. Need to inject static data too.
//                 Can do it in a clever way so that the type of static data is binary-specific,
//                 and it has a default in the default Current built; much like we injected `make_unique<>`.

}  // namespace current::karl
}  // namespace current

#endif  // KARL_SCHEMA_H
