#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "ebs.h"

int main() {
  ebs_init("localhost", "18001", "localhost", "18002");
  
  char buf[4096];
  
  printf("Zeroing out volume\n\n");
  for (int i = 0; i < 4096; i++)
    buf[i] = 'a';  
  
  for (int i = 0; i < 2048; i++)  
    ebs_write(buf, i*4096);
  
  return 0;
  
}