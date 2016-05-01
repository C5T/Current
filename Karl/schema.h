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
#include "../TypeSystem/optional.h"
#include "../TypeSystem/variant.h"

#include "../Bricks/time/chrono.h"

#include "../Bricks/net/http/impl/server.h"  // current::net::constants

namespace current {
namespace karl {

// clang-format off
CURRENT_ENUM(KeepaliveAttemptStatus, uint8_t) {
  Unknown = 0u,
  Success = 1u,
  CouldNotConnect = 2u,
  ErrorCodeReturned = 3u
};
// clang-format on

CURRENT_STRUCT(KeepaliveAttemptResult) {
  CURRENT_FIELD(timestamp, std::chrono::microseconds);
  CURRENT_FIELD(status, KeepaliveAttemptStatus, KeepaliveAttemptStatus::Unknown);
  CURRENT_FIELD(http_code, uint16_t, static_cast<uint16_t>(net::HTTPResponseCodeValue::InvalidCode));
};

// The generic status.
// Persisted by Karl, except for the `build` part, which is only persisted on the first call, or if changed.
CURRENT_STRUCT(ClaireStatus) {
  CURRENT_FIELD(service, std::string);
  CURRENT_FIELD(codename, std::string);
  CURRENT_FIELD(local_port, uint16_t);

  CURRENT_FIELD(now, std::chrono::microseconds);  // To calculated time skew as well.
  CURRENT_FIELD(uptime_epoch_microseconds, std::chrono::microseconds);
  CURRENT_FIELD(uptime, std::string);

  CURRENT_FIELD(last_keepalive_sent, std::string);
  CURRENT_FIELD(last_keepalive_status, std::string);
  CURRENT_FIELD(last_successful_keepalive, Optional<std::string>);
  CURRENT_FIELD(last_successful_keepalive_ping, Optional<std::string>);

  CURRENT_FIELD(build, Optional<build::Info>);
};

namespace default_user_status {
// The default user-defined status. Wrapped into a namespace for cleaner JSON output.
CURRENT_STRUCT(status) {
  CURRENT_FIELD(message, std::string, "OK");
  CURRENT_FIELD(details, (std::map<std::string, std::string>));
};
}  // namespace current::karl::default_user_status

using DefaultClaireServiceStatus = default_user_status::status;

// For now, Karl parses the passed in JSON twice: once as `ClaireStatus` for generic response,
// and once as `ClaireServiceStatus<T>`, where `T` is a `Variant` containing the client-, service-side blob.
// clang-format off
CURRENT_STRUCT_T_DERIVED(ClaireServiceStatus, ClaireStatus) {
  CURRENT_FIELD(runtime, Optional<T>);  // `T` is a `Variant<>` which should be `Optional<>`.
};
// clang-format on

}  // namespace current::karl
}  // namespace current

#endif  // KARL_SCHEMA_H
