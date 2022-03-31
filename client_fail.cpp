#include <stdio.h>
#include <cstring>
#include <string>
#include "ebs.h"
#include <bits/stdc++.h>

const int NUM_RUNS = 1;

void run_server(){
  int ret = system("./server 0.0.0.0 0.0.0.0");
}

int main() {
  char host[] = "localhost";
  char port1[] = "18001";
  char port2[] = "18002";
  
  ebs_init(host, port1, host, port2);
  char buf[4096];
  //printf("%d\n", ebs_write(buf, 0));

  // Read X times
  std::thread t1(run_server);
  std::thread t2(run_server);
  for(int i = 0; i<NUM_RUNS; i++){
    printf("%d\n", ebs_read(buf, 0, true));
  }  

  printf("%d\n", ebs_read(buf, 0, true));
  printf("%d\n", ebs_read(buf, 0, true));
  t1.join();
  t2.join();
  printf("Done with read\n");
  return 0;
}
