// To run: g++ -std=c++11 -O3 step6_rides_by_months_readfullfile.cc && time ./a.out | tee >(md5sum)
// 
// Count rides by month, print the counters in the lexicographically sorted order of keys.
// The "read whole file into memory and analyze it" C implementation, still unsafe in a few ways.

#include <cstdlib>
#include <cstdio>

int MM[12];

int main(int argc, char** argv) {
  FILE* f = ::fopen(argc > 2 ? argv[1] : "../cooked.csv", "r");
  if (!f) {
    return -1;
  }

  ::fseek(f, 0, SEEK_END);
  size_t const length = ::ftell(f);
  ::rewind(f);

  char* buffer = static_cast<char*>(::malloc(length));
  if (!buffer) {
    return -1;
  }
  if (::fread(buffer, 1, length,  f) != length) {
    return -1;
  }
  ::fclose(f);

  char const* ptr = buffer;
  char const* const end = buffer + length;

  while (ptr < end) {
    ++MM[(ptr[7] - '0') * 10 + (ptr[8] - '0') - 1];  // Unsafe. Also, the year is not checked. -- D.K.
    while (ptr < end && *ptr != '\n') {
      ++ptr;
    }
    if (*ptr == '\n') {
      ++ptr;
    }
  }
  ::free(buffer);

  for (int m = 0; m < 12; ++m) {
    printf("%d %02d/2016\n", MM[m], m + 1);
  }
}
