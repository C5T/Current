// To run: gcc -O3 step8_rides_by_months_mmapfullfile.c && time ./a.out | tee >(md5sum)
// 
// Count rides by month, print the counters in the lexicographically sorted order of keys.
// The mmap-based C implementation, still not trying to be safe in any way.

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int MM[12];

int main(int argc, char** argv) {
  const char* filename = argc >= 2 ? argv[1] : "../cooked.csv";

  struct stat st;
  stat(filename, &st);
  const size_t length = st.st_size;

  const int fd = open(filename, O_RDONLY, 0);
  if (fd == -1) {
    return -1;
  }

  char* readonly_buffer = (char*)(mmap(NULL, length, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0));
  if (readonly_buffer == MAP_FAILED) {
    return -1;
  }

  const char* ptr = readonly_buffer;
  const char* end = readonly_buffer + length;

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

  munmap((void*)readonly_buffer, length);
  close(fd);
}
