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

// The schema for Web events originating from `segment.io`.

// This schema is by no means complete and/or correct.
// TODO(dkorolev) + TODO(mzhurovich): Revisit this.

#ifndef INTEGRATIONS_SEGMENT_SCHEMA_WEB_H
#define INTEGRATIONS_SEGMENT_SCHEMA_WEB_H

#include "../../typesystem/struct.h"
#include "../../typesystem/optional.h"

namespace current {
namespace integrations {
namespace segment {
namespace web {

CURRENT_STRUCT(EventProperties) {
  CURRENT_FIELD(url, Optional<std::string>);
  CURRENT_FIELD(path, Optional<std::string>);
  CURRENT_FIELD(search, Optional<std::string>);
};

CURRENT_STRUCT(EventContextPage) {
  CURRENT_FIELD(url, Optional<std::string>);
  CURRENT_FIELD(path, Optional<std::string>);
  CURRENT_FIELD(referrer, Optional<std::string>);
  CURRENT_FIELD(title, Optional<std::string>);
  CURRENT_FIELD(search, Optional<std::string>);
};

CURRENT_STRUCT(EventContextLibrary) {
  CURRENT_FIELD(version, Optional<std::string>);
  CURRENT_FIELD(name, Optional<std::string>);
};

CURRENT_STRUCT(EventContext) {
  CURRENT_FIELD(page, Optional<EventContextPage>);
  CURRENT_FIELD(ip, Optional<std::string>);
  CURRENT_FIELD(library, Optional<EventContextLibrary>);
  CURRENT_FIELD(userAgent, Optional<std::string>);
};

CURRENT_STRUCT(Event) {
  CURRENT_FIELD(type, Optional<std::string>);
  CURRENT_FIELD(event, Optional<std::string>);
  CURRENT_FIELD(channel, Optional<std::string>);
  CURRENT_FIELD(userId, Optional<std::string>);
  CURRENT_FIELD(anonymousId, Optional<std::string>);
  CURRENT_FIELD(messageId, Optional<std::string>);
  CURRENT_FIELD(projectId, Optional<std::string>);
  CURRENT_FIELD(properties, Optional<EventProperties>);
  CURRENT_FIELD(context, Optional<EventContext>);
  CURRENT_FIELD(timestamp, Optional<std::string>);
  CURRENT_FIELD(sentAt, Optional<std::string>);
  CURRENT_FIELD(receivedAt, Optional<std::string>);
  CURRENT_FIELD(originalTimestamp, Optional<std::string>);
  CURRENT_FIELD(integrations, (Optional<std::map<std::string, bool>>));
  CURRENT_FIELD(version, Optional<double>);  // What if they decide to use `2.1`. -- D.K.
};

}  // namespace web
}  // namespace segment
}  // namespace integrations
}  // namespace current

#endif  // INTEGRATIONS_SEGMENT_SCHEMA_WEB_H
