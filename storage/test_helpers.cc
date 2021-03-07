/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2021 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// This "source" file has a "header" guard, as it is `#include`-d from which
// of the individual test runners (of which there are several to please Travis).

#ifndef CURRENT_STORAGE_TEST_HELPERS_CC
#define CURRENT_STORAGE_TEST_HELPERS_CC

#define CURRENT_MOCK_TIME

#include <set>
#include <type_traits>

#include "storage.h"
#include "api.h"
#include "persister/stream.h"

#include "rest/plain.h"
#include "rest/simple.h"
#include "rest/hypermedia.h"

#include "../blocks/http/api.h"

#include "../bricks/file/file.h"
#include "../bricks/dflags/dflags.h"

#include "../stream/replicator.h"

#include "../3rdparty/gtest/gtest-main.h"

#ifndef CURRENT_MAKE_CHECK_MODE

#ifndef CURRENT_WINDOWS
DEFINE_string(transactional_storage_test_tmpdir, ".current", "Local path for the test to create temporary files in.");
#else
DEFINE_string(transactional_storage_test_tmpdir, "Debug", "Local path for the test to create temporary files in.");
#endif

DEFINE_int32(transactional_storage_test_port, PickPortForUnitTest(), "Local port to run [REST] API tests against.");

#endif  // !CURRENT_MAKE_CHECK_MODE

namespace transactional_storage_test {

CURRENT_STRUCT(Element) {
  CURRENT_FIELD(x, int32_t);
  CURRENT_CONSTRUCTOR(Element)(int32_t x = 0) : x(x) {}
};

CURRENT_STRUCT(Record) {
  CURRENT_FIELD(lhs, std::string);
  CURRENT_FIELD(rhs, int32_t);

  CURRENT_USE_FIELD_AS_KEY(lhs);

  CURRENT_CONSTRUCTOR(Record)(const std::string& lhs = "", int32_t rhs = 0) : lhs(lhs), rhs(rhs) {}
};

CURRENT_STRUCT(Cell) {
  CURRENT_FIELD(foo, int32_t);
  CURRENT_FIELD(bar, std::string);
  CURRENT_FIELD(phew, int32_t);

  CURRENT_USE_FIELD_AS_ROW(foo);
  CURRENT_USE_FIELD_AS_COL(bar);

  CURRENT_CONSTRUCTOR(Cell)(int32_t foo = 0, const std::string& bar = "", int32_t phew = 0)
      : foo(foo), bar(bar), phew(phew) {}
};

CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, Record, RecordDictionary);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedManyToUnorderedMany, Cell, CellUnorderedManyToUnorderedMany);
CURRENT_STORAGE_FIELD_ENTRY(OrderedManyToOrderedMany, Cell, CellOrderedManyToOrderedMany);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedManyToOrderedMany, Cell, CellUnorderedManyToOrderedMany);
CURRENT_STORAGE_FIELD_ENTRY(OrderedManyToUnorderedMany, Cell, CellOrderedManyToUnorderedMany);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedOneToUnorderedOne, Cell, CellUnorderedOneToUnorderedOne);
CURRENT_STORAGE_FIELD_ENTRY(OrderedOneToOrderedOne, Cell, CellOrderedOneToOrderedOne);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedOneToOrderedOne, Cell, CellUnorderedOneToOrderedOne);
CURRENT_STORAGE_FIELD_ENTRY(OrderedOneToUnorderedOne, Cell, CellOrderedOneToUnorderedOne);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedOneToUnorderedMany, Cell, CellUnorderedOneToUnorderedMany);
CURRENT_STORAGE_FIELD_ENTRY(OrderedOneToOrderedMany, Cell, CellOrderedOneToOrderedMany);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedOneToOrderedMany, Cell, CellUnorderedOneToOrderedMany);
CURRENT_STORAGE_FIELD_ENTRY(OrderedOneToUnorderedMany, Cell, CellOrderedOneToUnorderedMany);

CURRENT_STORAGE(TestStorage) {
  CURRENT_STORAGE_FIELD(d, RecordDictionary);
  CURRENT_STORAGE_FIELD(umany_to_umany, CellUnorderedManyToUnorderedMany);
  CURRENT_STORAGE_FIELD(omany_to_omany, CellOrderedManyToOrderedMany);
  CURRENT_STORAGE_FIELD(umany_to_omany, CellUnorderedManyToOrderedMany);
  CURRENT_STORAGE_FIELD(omany_to_umany, CellOrderedManyToUnorderedMany);
  CURRENT_STORAGE_FIELD(uone_to_uone, CellUnorderedOneToUnorderedOne);
  CURRENT_STORAGE_FIELD(oone_to_oone, CellOrderedOneToOrderedOne);
  CURRENT_STORAGE_FIELD(uone_to_oone, CellUnorderedOneToOrderedOne);
  CURRENT_STORAGE_FIELD(oone_to_uone, CellOrderedOneToUnorderedOne);
  CURRENT_STORAGE_FIELD(uone_to_umany, CellUnorderedOneToUnorderedMany);
  CURRENT_STORAGE_FIELD(oone_to_omany, CellOrderedOneToOrderedMany);
  CURRENT_STORAGE_FIELD(uone_to_omany, CellUnorderedOneToOrderedMany);
  CURRENT_STORAGE_FIELD(oone_to_umany, CellOrderedOneToUnorderedMany);
};

}  // namespace transactional_storage_test

static_assert(std::is_same<transactional_storage_test::RecordDictionary::update_event_t::storage_field_t,
                           transactional_storage_test::RecordDictionary>::value,
              "Expected the type of `RecordDictionary::update_event_t::storage_field_t` to be `RecordDictionary`.");

namespace transactional_storage_test {

CURRENT_STRUCT(SimpleUser) {
  CURRENT_FIELD(key, std::string);
  CURRENT_FIELD(name, std::string);
  CURRENT_CONSTRUCTOR(SimpleUser)(const std::string& key = "", const std::string& name = "") : key(key), name(name) {}
  void InitializeOwnKey() { key = current::ToString(std::hash<std::string>()(name)); }
};

CURRENT_STRUCT(SimpleUserValidPatch) {
  CURRENT_FIELD(name, std::string);
  CURRENT_CONSTRUCTOR(SimpleUserValidPatch)(const std::string& name = "") : name(name) {}
};

CURRENT_STRUCT(SimpleUserInvalidPatch) {
  CURRENT_FIELD(name, bool, true);  // Won't be able to cast `bool` into `string`.
};

CURRENT_STRUCT(SimplePost) {
  CURRENT_FIELD(key, std::string);
  CURRENT_FIELD(text, std::string);
  CURRENT_CONSTRUCTOR(SimplePost)(const std::string& key = "", const std::string& text = "") : key(key), text(text) {}
  void InitializeOwnKey() { key = current::ToString(std::hash<std::string>()(text)); }  // LCOV_EXCL_LINE
};

CURRENT_STRUCT(SimpleLikeBase) {
  CURRENT_FIELD(row, std::string);
  CURRENT_FIELD(col, std::string);
  CURRENT_CONSTRUCTOR(SimpleLikeBase)(const std::string& who = "", const std::string& what = "")
      : row(who), col(what) {}
};

CURRENT_STRUCT(SimpleLike, SimpleLikeBase) {
  using brief_t = SimpleLikeBase;
  CURRENT_FIELD(details, Optional<std::string>);
  CURRENT_CONSTRUCTOR(SimpleLike)(const std::string& who = "", const std::string& what = "") : SUPER(who, what) {}
  CURRENT_CONSTRUCTOR(SimpleLike)(const std::string& who, const std::string& what, const std::string& details)
      : SUPER(who, what), details(details) {}
};

CURRENT_STRUCT(SimpleLikeValidPatch) {
  CURRENT_FIELD(details, Optional<std::string>);
  CURRENT_CONSTRUCTOR(SimpleLikeValidPatch)(Optional<std::string> details) : details(details) {}
};

CURRENT_STRUCT(SimpleLikeInvalidPatch) {
  CURRENT_FIELD(row, std::string, "meh");  // Modifying row/col is not allowed.
};

// Test RESTful compilation with different `row` and `col` types.
CURRENT_STRUCT(SimpleComposite) {
  CURRENT_FIELD(row, std::string);
  CURRENT_FIELD(col, std::chrono::microseconds);
  CURRENT_CONSTRUCTOR(SimpleComposite)(const std::string& row = "",
                                       const std::chrono::microseconds col = std::chrono::microseconds(0))
      : row(row), col(col) {}

  // Without this line, POST is not allowed in RESTful access to this container.
  // TODO(dkorolev): We may want to revisit POST for matrix containers this one day.
  void InitializeOwnKey() {}
};

CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, SimpleUser, SimpleUserPersisted);  // Ordered for list view.
CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, SimplePost, SimplePostPersisted);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedManyToUnorderedMany, SimpleLike, SimpleLikePersisted);
CURRENT_STORAGE_FIELD_ENTRY(OrderedOneToOrderedOne, SimpleComposite, SimpleCompositeO2OPersisted);
CURRENT_STORAGE_FIELD_ENTRY(OrderedOneToOrderedMany, SimpleComposite, SimpleCompositeO2MPersisted);
CURRENT_STORAGE_FIELD_ENTRY(OrderedManyToOrderedMany, SimpleComposite, SimpleCompositeM2MPersisted);

CURRENT_STORAGE(SimpleStorage) {
  CURRENT_STORAGE_FIELD(user, SimpleUserPersisted);
  CURRENT_STORAGE_FIELD(post, SimplePostPersisted);
  CURRENT_STORAGE_FIELD(like, SimpleLikePersisted);
  CURRENT_STORAGE_FIELD(composite_o2o, SimpleCompositeO2OPersisted);
  CURRENT_STORAGE_FIELD(composite_o2m, SimpleCompositeO2MPersisted);
  CURRENT_STORAGE_FIELD(composite_m2m, SimpleCompositeM2MPersisted);
};

template <typename STREAM_ENTRY>
class StorageStreamTestProcessorImpl {
 public:
  using EntryResponse = current::ss::EntryResponse;
  using TerminationResponse = current::ss::TerminationResponse;

  StorageStreamTestProcessorImpl(std::string& output) : output_(output) {}

  void SetAllowTerminate() { allow_terminate_ = true; }
  void SetAllowTerminateOnOnMoreEntriesOfRightType() { allow_terminate_on_no_more_entries_of_right_type_ = true; }

  EntryResponse operator()(const STREAM_ENTRY& entry, idxts_t current, idxts_t last) const {
    output_ += JSON(current) + '\t' + JSON(entry) + '\n';
    if (current.index != last.index) {
      return EntryResponse::More;
    } else {
      allow_terminate_ = true;
      return EntryResponse::Done;
    }
  }

  EntryResponse operator()(std::chrono::microseconds) const { return EntryResponse::More; }

  TerminationResponse Terminate() const {
    if (allow_terminate_) {
      return TerminationResponse::Terminate;
    } else {
      return TerminationResponse::Wait;  // LCOV_EXCL_LINE
    }
  }

  EntryResponse EntryResponseIfNoMorePassTypeFilter() const {
    if (allow_terminate_on_no_more_entries_of_right_type_) {
      return EntryResponse::Done;  // LCOV_EXCL_LINE
    } else {
      return EntryResponse::More;
    }
  }

 private:
  mutable bool allow_terminate_ = false;
  bool allow_terminate_on_no_more_entries_of_right_type_ = false;
  std::string& output_;
};

template <typename E>
using StorageStreamTestProcessor = current::ss::StreamSubscriber<StorageStreamTestProcessorImpl<E>, E>;

struct CurrentStorageTestMagicTypesExtractor {
  std::string& s;
  CurrentStorageTestMagicTypesExtractor(std::string& s) : s(s) {}
  template <typename CONTAINER_TYPE_WRAPPER, typename ENTRY_TYPE_WRAPPER>
  int operator()(const char* name, CONTAINER_TYPE_WRAPPER, ENTRY_TYPE_WRAPPER) {
    s = std::string(name) + ", " + CONTAINER_TYPE_WRAPPER::HumanReadableName() + ", " +
        current::reflection::CurrentTypeName<typename ENTRY_TYPE_WRAPPER::entry_t>();
    return 42;  // Checked against via `::current::storage::FieldNameAndTypeByIndexAndReturn`.
  }
};

struct CQSTestException : current::Exception {
  CQSTestException() : current::Exception("CQS test exception.") {}
};

CURRENT_STRUCT(CQSQuery) {
  CURRENT_FIELD(reverse_sort, bool, false);
  CURRENT_FIELD(test_current_exception, bool, false);
  CURRENT_FIELD(test_native_exception, bool, false);

  CURRENT_CONSTRUCTOR(CQSQuery)(bool reverse_sort = false) : reverse_sort(reverse_sort) {}

  // clang-format off
  // Keep these two lines together.
  static int DoThrowCurrentExceptionLine() { return __LINE__ + 1; }
  static void DoThrowCurrentException() { CURRENT_THROW(CQSTestException()); }
  // clang-format on

  template <class IMMUTABLE_FIELDS>
  Response Query(const IMMUTABLE_FIELDS& fields, const current::storage::rest::cqs::CQSParameters& cqs_parameters)
      const {
    const auto OptionalBoolQueryParameterToString = [&cqs_parameters](const std::string& param_name) -> std::string {
      if (cqs_parameters.original_url.query.has(param_name)) {
        const auto value = cqs_parameters.original_url.query[param_name];
        return value.empty() ? "true" : value;
      } else {
        return "false";
      }
    };
    EXPECT_EQ(current::ToString(reverse_sort), OptionalBoolQueryParameterToString("reverse_sort"));
    EXPECT_EQ(current::ToString(test_current_exception), OptionalBoolQueryParameterToString("test_current_exception"));
    EXPECT_EQ(current::ToString(test_native_exception), OptionalBoolQueryParameterToString("test_native_exception"));

    std::vector<std::string> names;
    names.reserve(fields.user.Size());

    for (const auto& user : fields.user) {
      names.emplace_back(user.name);
    }

    if (!reverse_sort) {
      std::sort(names.begin(), names.end());
    } else {
      std::sort(names.rbegin(), names.rend());
    }

    if (test_native_exception) {
      static_cast<void>(std::map<int, int>().at(42));  // Throws `std::out_of_range`.
    } else if (test_current_exception) {
      DoThrowCurrentException();
    }

    return Response(cqs_parameters.restful_url_prefix + " = " + current::strings::Join(names, ','));
  }
};

CURRENT_STRUCT(CQSCommandResponse) {
  CURRENT_FIELD(url, std::string);
  CURRENT_FIELD(before, uint64_t, 0);
  CURRENT_FIELD(after, uint64_t, 0);
};

CURRENT_STRUCT(CQSCommandRollbackMessage) {
  CURRENT_FIELD(rolled_back, bool, true);
  CURRENT_FIELD(command, std::vector<std::string>);
  CURRENT_DEFAULT_CONSTRUCTOR(CQSCommandRollbackMessage) {}
  CURRENT_CONSTRUCTOR(CQSCommandRollbackMessage)(const std::vector<std::string>& command) : command(command) {}
};

CURRENT_STRUCT(CQSCommand) {
  CURRENT_FIELD(users, std::vector<std::string>);
  CURRENT_FIELD(test_current_exception, Optional<bool>, false);
  CURRENT_FIELD(test_native_exception, Optional<bool>, false);
  CURRENT_FIELD(test_simple_rollback, Optional<bool>, false);
  CURRENT_FIELD(test_rollback_with_json, Optional<bool>, false);
  CURRENT_FIELD(test_rollback_with_raw_response, Optional<bool>, false);

  CURRENT_DEFAULT_CONSTRUCTOR(CQSCommand) {}
  CURRENT_CONSTRUCTOR(CQSCommand)(const std::vector<std::string>& users) : users(users) {}

  // clang-format off
  // Keep these two lines together.
  static int DoThrowCurrentExceptionLine() { return __LINE__ + 1; }
  static void DoThrowCurrentException() { CURRENT_THROW(CQSTestException()); }

  template <typename MUTABLE_FIELDS>
  Response Command(MUTABLE_FIELDS& fields, const current::storage::rest::cqs::CQSParameters& cqs_parameters) const {
    // clang-format on
    CQSCommandResponse result;

    result.url = cqs_parameters.restful_url_prefix;
    result.before = fields.user.Size();

    for (const auto& u : users) {
      fields.user.Add(SimpleUser(u, u));
    }

    if (Exists(test_native_exception) && Value(test_native_exception)) {
      static_cast<void>(std::map<int, int>().at(42));  // Throws `std::out_of_range`.
    } else if (Exists(test_current_exception) && Value(test_current_exception)) {
      DoThrowCurrentException();
    } else if (Exists(test_simple_rollback) && Value(test_simple_rollback)) {
      CURRENT_STORAGE_THROW_ROLLBACK();
    } else if (Exists(test_rollback_with_json) && Value(test_rollback_with_json)) {
      CURRENT_STORAGE_THROW_ROLLBACK_WITH_VALUE(
          Response, CQSCommandRollbackMessage(users), HTTPResponseCode.PreconditionFailed);
    } else if (Exists(test_rollback_with_raw_response) && Value(test_rollback_with_raw_response)) {
      CURRENT_STORAGE_THROW_ROLLBACK_WITH_VALUE(Response, Response("HA!", HTTPResponseCode.OK));
    }

    result.after = fields.user.Size();
    return Response(result);
  }
};

// LCOV_EXCL_START
// Test the `CURRENT_STORAGE_FIELD_EXCLUDE_FROM_REST(field)` macro.
CURRENT_STORAGE_FIELD_ENTRY(OrderedDictionary, SimpleUser, SimpleUserPersistedExposed);
CURRENT_STORAGE_FIELD_ENTRY(UnorderedDictionary, SimplePost, SimplePostPersistedNotExposed);
CURRENT_STORAGE(PartiallyExposedStorage) {
  CURRENT_STORAGE_FIELD(user, SimpleUserPersistedExposed);
  CURRENT_STORAGE_FIELD(post, SimplePostPersistedNotExposed);
  CURRENT_STORAGE_FIELD(like, SimpleLikePersisted);
  CURRENT_STORAGE_FIELD(composite_o2o, SimpleCompositeO2OPersisted);
  CURRENT_STORAGE_FIELD(composite_o2m, SimpleCompositeO2MPersisted);
  CURRENT_STORAGE_FIELD(composite_m2m, SimpleCompositeM2MPersisted);
};
// LCOV_EXCL_STOP

}  // namespace transactional_storage_test

CURRENT_STORAGE_FIELD_EXCLUDE_FROM_REST(transactional_storage_test::SimplePostPersistedNotExposed);

#endif  // CURRENT_STORAGE_TEST_HELPERS_CC
