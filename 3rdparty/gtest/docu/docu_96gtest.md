`TODO(dkorolev): Include this file in some documentation -- likely a separate .md for "3rdparty".`

## Unit Testing: `gtest`

Bricks contains a header-only port of Google's [`GoogleTest`](http://code.google.com/p/googletest/): a great C++ unit testing library open-sourced by [**Google**](https://www.google.com/finance?q=GOOG).

Check out [`Bricks/file/test.cc`](https://github.com/KnowSheet/Bricks/blob/master/file/test.cc), or most of other `test.cc` files in `Bricks` for example usage.

A three-minute intro:

1. **Logic**
  
  `ASSERT_*` interrupts the `TEST() { .. }` if failing,
  
   `EXPECT_*` considers the `TEST()` failed, but continues to execute it.

2. **Debug Output**
  
   `ASSERT`-s and `EXPECT`-s can be used as output streams. No newline needed.
  
   `EXPECT_EQ(4, 2 * 2) << "Something is wrong with multiplication.";`

3. **Conditions**
  
   `ASSERT`-s and `EXPECT`-s can use {`EQ`,`NE`,`LT`,`GT`,`LE`,`NE`,`TRUE`} after the underscore.
  
   This results in more meaningful human-readable test failure messages.

4. **Parameters Order**
  
   For `{ASSERT,EXPECT}_{EQ,NE}`, put the expected value as the first parameter.
  
   For clean error messages wrt `expected` vs. `actual`.
   

5. **Exceptions**
  
   `ASSERT_THROW(statement, exception_type);` ensures the exception is thrown.

6. **Death Tests**

   The following contsruct:
  
   `ASSERT_DEATH(function(), "Expected regex for the last line of standard error.");`
  
   can be used to ensure certain call fails. The failure implies the binary terminating with a non-zero exit code. The convention is to use the `"DeathTest"` suffix for those tests and to not mix functional tests with death tests.

7. **Templated Tests**
  
   `gtest` supports templated tests, where objects of various tests are passed to the same test method.
  
   Each type results in the whole new statically compiled test.

8. **Disabled Tests**
  
   Prefix a test name with `"DISABLED_"` to exclude it from being run.
  
   Use sparingly and try to keep master clean from disabled tests.

For more details please refer to the original [`GoogleTest` documentation](http://code.google.com/p/googletest/wiki/Documentation).

The code in Bricks is a header-only port of the code originally released by Google. It requires no linker dependencies.
