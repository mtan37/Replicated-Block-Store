#include <stdio.h>
#include <iostream>
#include "helper.h"
#include "ebs.h"

const int num_tests = 2;

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
  
  char codeA[] = {'C', 'R', 'A', 'S', 'H', 1, 0, 0};
  char codeB[] = {'C', 'R', 'A', 'S', 'H', 1, 0, 0};
  char * codes[2];
  codes[0] = codeA;
  codes[1] = codeB;


  // Repeate test 3 times - normal read, crash primary, crash backup

  for (int i = 0; i < num_tests; i++){
    std::cout << "\n************\n*";
    switch(i){
      case 0:
        std::cout << "* Write / Read";
        break;
      case 1:
        std::cout << "* Write / Crash Primary / Read";
        break;
      case 2:
        std::cout << "* Write / Crash Backup / Read";
        break;
      default:
        break;
    }
    std::cout << "\n*************" << std::endl;
    // Setup
    timespec start, end;
    set_time(&start); 
    char write_buf[4096];
    for (int i = 0; i < 4096; ++i) {
      write_buf[i] = i%256;
    }
    char read_buf[4096];
    // write data
    if (ebs_write(write_buf, 0) != EBS_SUCCESS) {
      printf("Write failed\n");
      return 0;
    }
    if (i > 0){
      // crash server
      if (ebs_write(write_buf, *(long*) codes[i-1]) != EBS_SUCCESS) {
        printf("Crash/Write failed\n");
        return 0;
      }
    }
    
    // read data
    if (ebs_read(read_buf, 0) != EBS_SUCCESS) {
      printf("Read failed\n");
      return 0;
    }

    // verify read and write match
    for (int i = 0; i < 4096; ++i) {
      if (read_buf[i] != write_buf[i]) {
        printf("Read did not return same data as write\n");
        return 0;
      }
    }

    set_time(&end); 
    double elapsed = difftimespec_ns(start, end);
    printf("Test passed - it took %f (s) \n", elapsed*1e-9);
  }
  
  
  return 0;
}
