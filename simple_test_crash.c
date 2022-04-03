#include <stdio.h>

#include "ebs.h"

int main() {
  ebs_init("10.10.1.2", "18001", "10.10.1.3", "18002");

  char code[] = {'C', 'R', 'A', 'S', 'H', 1, 0, 0};

  char write_buf[4096];
  for (int i = 0; i < 4096; ++i) {
    write_buf[i] = i%256;
  }
  char read_buf[4096];
  if (ebs_write(write_buf, 0) != EBS_SUCCESS) {
    printf("Write failed\n");
    return 0;
  }
  if (ebs_write(write_buf, *(long*) code) != EBS_SUCCESS) {
    printf("Crash/Write failed\n");
    return 0;
  }
  if (ebs_read(read_buf, 0) != EBS_SUCCESS) {
    printf("Read failed\n");
    return 0;
  }

  for (int i = 0; i < 4096; ++i) {
    if (read_buf[i] != write_buf[i]) {
      printf("Read did not return same data as write\n");
      return 0;
    }
  }
  printf("Test passed\n");
  return 0;
}
