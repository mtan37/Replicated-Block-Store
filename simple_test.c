#include <stdio.h>

#include "ebs.h"

int main() {
  ebs_init("localhost", "18001", "localhost", "18002");
  char write_buf[4096];
  for (int i = 0; i < 4096; ++i) {
    write_buf[i] = i%256;
  }
  char read_buf[4096];
  printf("%d\n", ebs_write(write_buf, 0));
  printf("%d\n", ebs_read(read_buf, 0));

  for (int i = 0; i < 4096; ++i) {
    if (read_buf[i] != write_buf[i]) {
      printf("Read did not return same data as write\n");
      return 0;
    }
  }
  printf("Test passed\n");
  return 0;
}
