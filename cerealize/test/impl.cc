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

// TOOD(dkorolev): WORK IN PROGRESS.
//
// TODO(dkorolev): TESTS TO WRITE.
//
// * Compare against golden files.
// * Polymorphic non-trivial hierarchy diagram, JSON and binary with golden files.
// * Parse polymorphic objects and count them: Via lambda, via class, via tuple<> type list spec.
// * Add test names to file names.
// * Add and parse --n=10000 entries: --vbros=FILENAME and --vyser=FILENAME.
// * Non-polymorphic types, JSON and binary, success and failure.

#include <iostream>
#include <tuple>

#include "../cerealize.h"

#include "../../file/file.h"
#include "../../dflags/dflags.h"

#include "test_event_base.h"
#include "test_event_derived.cc"

#include "../../3party/gtest/gtest.h"
#include "../../3party/gtest/gtest-main-with-dflags.h"

using namespace bricks;
using namespace cerealize;

DEFINE_string(cerealize_test_tmpdir, ".noshit", "The directory to create temporary files in.");

static std::string CurrentTestName() {
  // via https://code.google.com/p/googletest/wiki/AdvancedGuide#Getting_the_Current_Test%27s_Name
  return ::testing::UnitTest::GetInstance()->current_test_info()->name();
}

static std::string CurrentTestTempFileName() {
  return FileSystem::JoinPath(FLAGS_cerealize_test_tmpdir, CurrentTestName());
}

TEST(Cerealize, BinarySerializesAndParses) {
  FileSystem::CreateDirectory(FLAGS_cerealize_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  FileSystem::RemoveFile(CurrentTestTempFileName(), FileSystem::RemoveFileParameters::Silent);

  EventAppStart a;
  EventAppSuspend b;
  EventAppResume c;

  CerealFileAppender(CurrentTestTempFileName()) << a << b << c;

  CerealFileParser<MapsYouEventBase> f(CurrentTestTempFileName());

  std::ostringstream os;
  while (f.Next([&os](const MapsYouEventBase& e) { e.AppendTo(os) << '\n'; }))
    ;

  EXPECT_EQ(
      "Type=EventAppStart, ShortType=\"a\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, foo=foo\n"
      "Type=EventAppSuspend, ShortType=\"as\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, bar=bar\n"
      "Type=EventAppResume, ShortType=\"ar\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, baz=baz\n",
      os.str());
}

TEST(Cerealize, JSONSerializesAndParses) {
  FileSystem::CreateDirectory(FLAGS_cerealize_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  FileSystem::RemoveFile(CurrentTestTempFileName(), FileSystem::RemoveFileParameters::Silent);

  EventAppStart a;
  EventAppSuspend b;
  EventAppResume c;

  GenericCerealFileAppender<CerealFormat::JSON>(CurrentTestTempFileName()) << a << b;
  GenericCerealFileParser<MapsYouEventBase, CerealFormat::JSON> f(CurrentTestTempFileName());

  std::ostringstream os;
  while (f.Next([&os](const MapsYouEventBase& e) { e.AppendTo(os) << '\n'; }))
    ;

  EXPECT_EQ(
      "Type=EventAppStart, ShortType=\"a\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, foo=foo\n"
      "Type=EventAppSuspend, ShortType=\"as\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, bar=bar\n",
      os.str());
}

TEST(Cerealize, BinaryStreamCanBeAppendedTo) {
  FileSystem::CreateDirectory(FLAGS_cerealize_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  FileSystem::RemoveFile(CurrentTestTempFileName(), FileSystem::RemoveFileParameters::Silent);

  EventAppStart a;
  EventAppSuspend b;
  EventAppResume c;

  CerealFileAppender(CurrentTestTempFileName()) << a << b;
  CerealFileAppender(CurrentTestTempFileName()) << c;

  CerealFileParser<MapsYouEventBase> f(CurrentTestTempFileName());

  std::ostringstream os;
  while (f.Next([&os](const MapsYouEventBase& e) { e.AppendTo(os) << '\n'; }))
    ;

  EXPECT_EQ(
      "Type=EventAppStart, ShortType=\"a\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, foo=foo\n"
      "Type=EventAppSuspend, ShortType=\"as\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, bar=bar\n"
      "Type=EventAppResume, ShortType=\"ar\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, baz=baz\n",
      os.str());
}

TEST(Cerealize, JSONStreamCanNotBeJustAppendedTo) {
  FileSystem::CreateDirectory(FLAGS_cerealize_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  FileSystem::RemoveFile(CurrentTestTempFileName(), FileSystem::RemoveFileParameters::Silent);

  EventAppStart a;
  EventAppSuspend b;
  EventAppResume c;

  GenericCerealFileAppender<CerealFormat::JSON>(CurrentTestTempFileName()) << a << b;
  GenericCerealFileAppender<CerealFormat::JSON>(CurrentTestTempFileName()) << c;
  ASSERT_THROW((GenericCerealFileParser<MapsYouEventBase, CerealFormat::JSON>(CurrentTestTempFileName())),
               cereal::Exception);
}

TEST(Cerealize, ConsumerSupportsPolymorphicTypes) {
  FileSystem::CreateDirectory(FLAGS_cerealize_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  FileSystem::RemoveFile(CurrentTestTempFileName(), FileSystem::RemoveFileParameters::Silent);

  EventAppStart a;
  EventAppSuspend b;
  EventAppResume c;

  CerealFileAppender(CurrentTestTempFileName()) << a << b;
  CerealFileAppender(CurrentTestTempFileName()) << c;

  CerealFileParser<MapsYouEventBase> f(CurrentTestTempFileName());

  struct ExampleConsumer {
    // TODO(dkorolev): Chat with Alex what the best way to handle this would be.
    // I looked through Cereal, they use a map<k,v> using typeid. It's a bit off what we need here.
    typedef MapsYouEventBase BASE_TYPE;
    // Note that this tuple does not list `EventAppSuspend`.
    typedef std::tuple<EventAppStart, EventAppResume> DERIVED_TYPE_LIST;
    enum FixTypedefDefinedButNotUsedWarning { FOO = sizeof(BASE_TYPE), BAR = sizeof(DERIVED_TYPE_LIST) };

    std::ostringstream os;
    void operator()(const MapsYouEventBase& e) { e.AppendTo(os) << " *** WRONG *** " << std::endl; }
    void operator()(const EventAppStart& e) { e.AppendTo(os) << " START " << std::endl; }
    void operator()(const EventAppResume& e) { e.AppendTo(os) << " RESUME " << std::endl; }

    // This function will not be called since the `EventAppSuspend` type is not in `DERIVED_TYPE_LIST` above.
    void operator()(const EventAppSuspend& e) { e.AppendTo(os) << " *** NOT CALLED SUSPEND *** " << std::endl; }
  };

  ExampleConsumer consumer;
  while (f.NextWithDispatching<ExampleConsumer>(consumer))
    ;

  EXPECT_EQ(
      "Type=EventAppStart, ShortType=\"a\","
      " UID=, UID_Google=, UID_Apple=, UID_Facebook=, foo=foo START \n"
      "Type=EventAppSuspend, ShortType=\"as\","
      " UID=, UID_Google=, UID_Apple=, UID_Facebook=, bar=bar *** WRONG *** \n"
      "Type=EventAppResume, ShortType=\"ar\","
      " UID=, UID_Google=, UID_Apple=, UID_Facebook=, baz=baz RESUME \n",
      consumer.os.str());
}
