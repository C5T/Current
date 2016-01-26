/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// Defines base event type for an imaginary `MapsYou` app.

#ifndef CEREALIZE_TEST_EVENT_BASE_H
#define CEREALIZE_TEST_EVENT_BASE_H

#include <string>
#include <sstream>
#include <memory>

#include "../cerealize.h"

struct MapsYouEventBase {
  std::string uid = "";
  std::string uid_apple = "";
  std::string uid_google = "";
  std::string uid_facebook = "";
  virtual std::string Type() const { throw true; }       // LCOV_EXCL_LINE
  virtual std::string ShortType() const { throw true; }  // LCOV_EXCL_LINE
  virtual std::ostream& AppendTo(std::ostream& os) const {
    os << "Type=" << Type() << ", ShortType=\"" << ShortType() << '\"';
    os << ", UID=" << uid;
    os << ", UID_Google=" << uid_google;
    os << ", UID_Apple=" << uid_apple;
    os << ", UID_Facebook=" << uid_facebook;
    return os;
  }
  template <class A>
  void serialize(A& ar) {
    // `NVP` below stands for Name-Value-Pair.
    // The `CEREAL_NVP` syntax is only to have JSON-based serialization contain human-readable field names.
    // The binary format is identical, no matter whether `uid`, `CEREAL_NVP(uid)` or `cereal::make_nvp("foo",
    // uid)`.
    // For binary serialization format, the order and types of the fields should stay the same.
    ar(CEREAL_NVP(uid), CEREAL_NVP(uid_facebook), CEREAL_NVP(uid_google), CEREAL_NVP(uid_apple));
  }
};

// Helper macros to define new event types.
// *** NOTE: Unforunately, events should be defined at global namespace scope. ***
#define MAPSYOU_EVENT(M_EVENT_CLASS_NAME, M_IMMEDIATE_BASE_CLASS_NAME, M_SHORT_NAME) \
  struct M_EVENT_CLASS_NAME;                                                         \
  CEREAL_REGISTER_TYPE_WITH_NAME(M_EVENT_CLASS_NAME, M_SHORT_NAME);                  \
  struct M_EVENT_CLASS_NAME##Helper : MapsYouEventBase {                             \
    typedef MapsYouEventBase CEREAL_BASE_TYPE;                                       \
    typedef M_IMMEDIATE_BASE_CLASS_NAME EVENT_SUPER;                                 \
    virtual std::string Type() const override { return #M_EVENT_CLASS_NAME; }        \
    virtual std::string ShortType() const override { return M_SHORT_NAME; }          \
  };                                                                                 \
  struct M_EVENT_CLASS_NAME : M_EVENT_CLASS_NAME##Helper

#endif  // CEREALIZE_TEST_EVENT_BASE_H
