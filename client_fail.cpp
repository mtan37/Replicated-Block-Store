#include <stdio.h>
#include <cstring>
#include <string>
#include "ebs.h"

const int NUM_RUNS = 1;

int main() {
  char host[] = "localhost";
  char port1[] = "18001";
  char port2[] = "18002";
  
  ebs_init(host, port1, host, port2);
  char buf[4096];
  printf("%d\n", ebs_write(buf, 0));

  // Read X times
  for(int i = 0; i<NUM_RUNS; i++){
    printf("%d\n", ebs_read(buf, 0));
  }  
  return 0;
}
