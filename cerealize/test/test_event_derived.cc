// NOTE: This file is a `.cc`, not `.h`, since all `.h` files
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
