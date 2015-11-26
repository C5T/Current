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

#ifndef DATA_DICTIONARY_H
#define DATA_DICTIONARY_H

#include "../../TypeSystem/Reflection/schema.h"
#include "../../Bricks/time/chrono.h"

using namespace current;

CURRENT_STRUCT(Nop) {};

CURRENT_STRUCT(UserAdded) {
  CURRENT_FIELD(user_id, std::string);
  CURRENT_FIELD(nickname, std::string);
};

CURRENT_STRUCT(PostAdded) {
  CURRENT_FIELD(post_id, std::string);
  CURRENT_FIELD(content, std::string);
  CURRENT_FIELD(author_user_id, std::string);
};

CURRENT_STRUCT(UserLike) {
  CURRENT_FIELD(user_id, std::string);
  CURRENT_FIELD(post_id, std::string);
};

CURRENT_STRUCT(Event) {
  CURRENT_FIELD(timestamp, uint64_t);
  CURRENT_FIELD(event, (Polymorphic<Nop, UserAdded, PostAdded, UserLike>));

  CURRENT_DEFAULT_CONSTRUCTOR(Event) : event(Nop()) {}

  EPOCH_MILLISECONDS ExtractTimestamp() const { return static_cast<EPOCH_MILLISECONDS>(timestamp); }
};

CURRENT_STRUCT(Error) {
  CURRENT_FIELD(error, std::string);
  CURRENT_CONSTRUCTOR(Error)(const std::string& error = "Internal error.") : error(error) {}
};

#endif  // DATA_DICTIONARY_H
