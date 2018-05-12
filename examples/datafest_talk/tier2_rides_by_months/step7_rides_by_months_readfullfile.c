// To run: gcc -O3 step7_rides_by_months_readfullfile.c && time ./a.out | tee >(md5sum)
// 
// Count rides by month, print the counters in the lexicographically sorted order of keys.
// The read-based C implementation, still not trying to be safe in any way.

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int MM[12];

int main(int argc, char** argv) {
  const char* filename = argc >= 2 ? argv[1] : "../cooked.csv";

  struct stat st;
  stat(filename, &st);
  const ssize_t length = st.st_size;

  char* buffer = (char*)malloc(length);
  if (!buffer) {
    fprintf(stderr, "0\n");
    return -1;
  }

  const int fd = open(filename, O_RDONLY, 0);
  if (fd == -1) {
    fprintf(stderr, "1\n");
    return -1;
  }

  // NOTE(dkorolev): `read` can only read up to (2GB - 4KB) per call.
  ssize_t position = 0u;
  ssize_t current;
  while ((position += (current = read(fd, buffer + position, length - position))) != length) {
    if (current == -1) {
      fprintf(stderr, "2 %lld %lld\n", (long long)current, (long long)length);
      return -1;
    }
  }

  const char* ptr = buffer;
  const char* end = buffer + length;

  while (ptr < end) {
    ++MM[(ptr[7] - '0') * 10 + (ptr[8] - '0') - 1];  // Unsafe for all the same reasons as the previous steps. D.K.
    while (ptr < end && *ptr != '\n') {
      ++ptr;
    }
    if (*ptr == '\n') {
      ++ptr;
    }
  }

  for (int m = 0; m < 12; ++m) {
    printf("%d %02d/2016\n", MM[m], m + 1);
  }

  free(buffer);
  close(fd);
}
