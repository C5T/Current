/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// TODO(dkorolev): Refactor this example to use `CURRENT_STORAGE`, or retire it.

#ifndef SCHEMA_H
#define SCHEMA_H

#include "../../current.h"

using namespace current;

// Storage schema, for persistence, publish, and subscribe.

CURRENT_STRUCT(BaseEvent) {
  CURRENT_FIELD(timestamp, std::chrono::microseconds);
  CURRENT_USE_FIELD_AS_TIMESTAMP(timestamp);
};

CURRENT_STRUCT(UserAdded, BaseEvent) {
  CURRENT_FIELD(user_id, std::string);
  CURRENT_FIELD(nickname, std::string);
};

CURRENT_STRUCT(PostAdded, BaseEvent) {
  CURRENT_FIELD(post_id, std::string);
  CURRENT_FIELD(content, std::string);
  CURRENT_FIELD(author_user_id, std::string);
};

CURRENT_STRUCT(UserLike, BaseEvent) {
  CURRENT_FIELD(user_id, std::string);
  CURRENT_FIELD(post_id, std::string);
};

// `Event` is the top-level message to persist. This structure is not default-constructible, not copyable,
// and not assignable, since it contains a `Variant<>`. It's serializable and movable though.
CURRENT_STRUCT(Event) {
  CURRENT_FIELD(event, (Variant<UserAdded, PostAdded, UserLike>));

  CURRENT_USE_FIELD_AS_TIMESTAMP(event);

  CURRENT_DEFAULT_CONSTRUCTOR(Event) {}
  CURRENT_CONSTRUCTOR(Event)(Variant<UserAdded, PostAdded, UserLike> && event) : event(std::move(event)) {}
};

// JSON responses schema.

CURRENT_STRUCT(UserNickname) {
  CURRENT_FIELD(user_id, std::string);
  CURRENT_FIELD(nickname, std::string);

  CURRENT_CONSTRUCTOR(UserNickname)
  (const std::string& user_id, const std::string& nickname) : user_id(user_id), nickname(nickname) {}
};

CURRENT_STRUCT(UserNicknameNotFound) {
  CURRENT_FIELD(user_id, std::string);
  CURRENT_FIELD(error, std::string, "User not found.");

  CURRENT_CONSTRUCTOR(UserNicknameNotFound)(const std::string& user_id) : user_id(user_id) {}
};

CURRENT_STRUCT(Error) {
  CURRENT_FIELD(error, std::string);
  CURRENT_CONSTRUCTOR(Error)(const std::string& error = "Internal error.") : error(error) {}
};

#endif  // SCHEMA_H
