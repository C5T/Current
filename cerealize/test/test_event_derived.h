#ifndef CEREALIZE_TEST_EVENT_DERIVED_H
#define CEREALIZE_TEST_EVENT_DERIVED_H

MAPSYOU_EVENT(EventAppStart, MapsYouEventBase, "a") {
  std::string foo = "foo";
  virtual std::ostream& AppendTo(std::ostream & os) const {
    return SUPER::AppendTo(os) << ", foo=" << foo;
  }
  template <class A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
    ar(CEREAL_NVP(foo));
  }
};

MAPSYOU_EVENT(EventAppSuspend, MapsYouEventBase, "as") {
  std::string bar = "bar";
  virtual std::ostream& AppendTo(std::ostream & os) const {
    return SUPER::AppendTo(os) << ", bar=" << bar;
  }
  template <class A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
    ar(CEREAL_NVP(bar));
  }
};

MAPSYOU_EVENT(EventAppResume, MapsYouEventBase, "ar") {
  std::string baz = "baz";
  virtual std::ostream& AppendTo(std::ostream & os) const {
    return SUPER::AppendTo(os) << ", baz=" << baz;
  }
  template <class A>
  void serialize(A & ar) {
    SUPER::serialize(ar);
    ar(CEREAL_NVP(baz));
  }
};

// TODO(dkorolev): Add more topology here, like base `EventSearch`
// and derived `EventSearchBegin`, `EventSearchBrowsingResults`, `EventSearchResultClicked`, etc.

#endif  // CEREALIZE_TEST_EVENT_DERIVED_H
