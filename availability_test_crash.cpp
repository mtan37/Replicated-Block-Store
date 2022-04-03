#include <stdio.h>
#include <iostream>
#include "helper.h"
#include "ebs.h"
#include <unistd.h>

const int num_tests = 3;

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
  char codeB[] = {'C', 'R', 'A', 'S', 'H', 2, 0, 0};
  char * codes[2];
  codes[0] = codeA;
  codes[1] = codeB;


  // Repeate test 3 times - normal read, crash primary, crash backup

  for (int i = 0; i < num_tests; i++){
    std::cout << "\n************\n*";
    switch(i){
      case 0:
        std::cout << "* Write / Read Test";
        break;
      case 1:
        std::cout << "* Crash Primary / Write  / Read Test";
        break;
      case 2:
        std::cout << "* Crash Backup / Write / Read Test";
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

    if (i > 0){
      std::cout << "Crashing Server\n";
      // crash server
      if (ebs_write(write_buf, *(long*) codes[i-1]) != EBS_SUCCESS) {
        printf("Crash/Write failed\n");
        return 0;
      }
    }

    
    // write data
    std::cout << "Writing Data\n";
    if (ebs_write(write_buf, 0) != EBS_SUCCESS) {
      std::cout << "Write failed\n";
      return 0;
    }

    
    
    // read data
    std::cout << "Reading Data\n";
    if (ebs_read(read_buf, 0) != EBS_SUCCESS) {
      printf("Read failed\n");
      return 0;
    }

    // verify read and write match
    std::cout << "Verifying match\n";
    for (int i = 0; i < 4096; ++i) {
      if (read_buf[i] != write_buf[i]) {
        std::cout << "Read did not return same data as write\n";
        return 0;
      }
    }

    set_time(&end); 
    double elapsed = difftimespec_ns(start, end);
    printf("Test passed - it took %f (s) \n", elapsed*1e-9);
    if(i==1) sleep(4);
  }
  
  sleep(20);
  return 0;
}
