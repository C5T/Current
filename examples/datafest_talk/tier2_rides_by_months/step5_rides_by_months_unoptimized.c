// To run: gcc -O3 step5_rides_by_months_unoptimized.c && time ./a.out | tee >(md5sum)
// 
// Count rides by month, print the counters in the lexicographically sorted order of keys.
// The "canonical C" implementation, with no extra checks, unsafe in a few ways, but still only 2x slower than `wc -l`.

#include <stdio.h>

int MM[12];

int main(int argc, char** argv) {
  char line[10000];  // Unsafe if we encounter longer lines. -- D.K.
  FILE* f = fopen(argc >= 2 ? argv[1] : "../cooked.csv", "r");
  while (fgets(line, sizeof(line), f)) {
    ++MM[(line[7] - '0') * 10 + (line[8] - '0') - 1];  // Unsafe if those two raw characters are not "01" .. "12" range.
  }
  for (int m = 0; m < 12; ++m) {
    printf("%d %02d/%d\n", MM[m], m + 1, 2016);
  }
  fclose(f);
}
