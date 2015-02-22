## `gtest`

Bricks constains a header-only port of Google's [`gtest`](http://www.google.com): a great C++ unit testing library.

A three-minute intro:

1. `ASSERT_*` interrupts the `TEST() { .. }` if failing,
   `EXPECT_*` considers the `TEST()` failed, but continues to execute it.

2. `ASSERT`-s and `EXPECT`-s can be used as output streams. No newline needed.
   `EXPECT_EQ(4, 2 * 2) << "Something is wrong with multiplication.";`

3. `ASSERT`-s and `EXPECT`-s can use {`EQ`,`NE`,`LT`,`GT`,`LE`,`NE`,`TRUE`} after the underscore.
   This results in more meaningful messages.

4. For `{ASSERT,EXPECT}_{EQ,NE}`, put the expected value as the first parameter.
   For cleaner error messages.

5. `ASSERT_THROW(statement, exception_type);` ensures the exception is thrown.

6. `ASSERT_DEATH(function(), "Expected regex for the last line of standard error.");`
   can be used to ensure certain call fails. The convention is to use
   the `"DeathTest"` suffix for those tests and to not mix functional tests with death tests.

7. `gtest` supports templated tests, where objects of various tests are passed to the same test method.
   Each type results in the whole new statically compiled test.

8. Prefix a test name with `"DISABLED_"` to exclude it from being run.
   Use sparingly and try to keep master clean from disabled tests.

Full documentation: [`gtest`](http://www.google.com). 

The code in Bricks is a header-only port of the code originally released by Google. It requires no linker dependencies.
