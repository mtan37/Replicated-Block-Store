#include <stdio.h>
#include <iostream>
#include "helper.h"
#include "ebs.h"


/*  
  Show availability and impacts
  a. normal write\read
  b. write\primary failure\read
  c. write\backup failure\read
*/
int main() {
  char ip[] = "10.10.1.2";
  char port[] = "18001";
  char alt_ip[] = "10.10.1.3";
  char alt_port[] = "18002";
  
  ebs_init(ip, port, alt_ip, alt_port);

  char code[] = {'C', 'R', 'A', 'S', 'H', 1, 0, 0};

  timespec start, end;
  set_time(&start); 
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

  set_time(&end); 
  double elapsed = difftimespec_ns(start, end);
  printf("Test passed - it took %f (s) \n", elapsed*1e-9);
  
  return 0;
}
