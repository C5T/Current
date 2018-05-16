// To run: gcc -O3 step2_sanitycheck_rides_by_months.c && time ./a.out | tee >(md5sum)
// 
// Sanity check #1: Number of rides per month, same as in Tier 2.

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "schema.h"

int MM[12];

void Run(const struct Ride* data, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    ++MM[data[i].pickup.month - 1];
  }
  for (int m = 0; m < 12; ++m) {
    printf("%d %02d/2016\n", MM[m], m + 1);
  }
}

int main(int argc, char** argv) {
  const char* filename = argc >= 2 ? argv[1] : "../cooked.bin";

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

  if (!((length % sizeof(struct Ride)) == 0)) {
    fprintf(stderr, "Wrong file size.\n");
    return -1;
  }

  Run((const struct Ride*)readonly_buffer, length / (sizeof(struct Ride)));

  munmap((void*)readonly_buffer, length);
  close(fd);
}
