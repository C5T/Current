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

#ifndef EXAMPLES_EVENT_STORE_SCHEMA_H
#define EXAMPLES_EVENT_STORE_SCHEMA_H

#include "../../typesystem/struct.h"
#include "../../storage/storage.h"

using EID = std::string;

CURRENT_STRUCT(EventBody) { CURRENT_FIELD(some_event_data, std::string); };

CURRENT_STRUCT(Event) {
  CURRENT_FIELD(key, EID);
  CURRENT_FIELD(body, EventBody);
};

CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, Event, PersistedEvent);
CURRENT_STORAGE(EventStoreDB) { CURRENT_STORAGE_FIELD(events, PersistedEvent); };

CURRENT_STRUCT(EventOutsideStorage) {
  CURRENT_FIELD(message, std::string);
  CURRENT_DEFAULT_CONSTRUCTOR(EventOutsideStorage) {}
  CURRENT_CONSTRUCTOR(EventOutsideStorage)(const Event& e) : message("Event added: " + e.key) {}
};

#endif  // EXAMPLES_EVENT_STORE_SCHEMA_H
