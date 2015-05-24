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

// NOTE: This file is a `.cc.h`, not `.h`, since all `.h` files
// are subject to an automated test that verifies they do not violate ODR,
// while this file does violate it by using Cereal registration macroses via MAPSYOU_EVENT().
//
// In practice, this means that the actual uses of Cereal-ization are not header-only,
// since registering Cereal types is a non-inline global name definition -- D.K.

#ifndef CEREALIZE_TEST_EVENT_DERIVED_H
#define CEREALIZE_TEST_EVENT_DERIVED_H

#include "test_event_base.h"

MAPSYOU_EVENT(EventAppStart, MapsYouEventBase, "a") {
  std::string foo = "foo";
  virtual std::ostream& AppendTo(std::ostream & os) const { return SUPER::AppendTo(os) << ", foo=" << foo; }
  template <class A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
    ar(CEREAL_NVP(foo));
  }
};

MAPSYOU_EVENT(EventAppSuspend, MapsYouEventBase, "as") {
  std::string bar = "bar";
  virtual std::ostream& AppendTo(std::ostream & os) const { return SUPER::AppendTo(os) << ", bar=" << bar; }
  template <class A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
    ar(CEREAL_NVP(bar));
  }
};

MAPSYOU_EVENT(EventAppResume, MapsYouEventBase, "ar") {
  std::string baz = "baz";
  virtual std::ostream& AppendTo(std::ostream & os) const { return SUPER::AppendTo(os) << ", baz=" << baz; }
  template <class A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
    ar(CEREAL_NVP(baz));
  }
};

// TODO(dkorolev): Add more topology here, like base `EventSearch`
// and derived `EventSearchBegin`, `EventSearchBrowsingResults`, `EventSearchResultClicked`, etc.

#endif  // CEREALIZE_TEST_EVENT_DERIVED_H
