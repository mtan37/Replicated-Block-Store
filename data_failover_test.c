#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "ebs.h"

int main() {
  ebs_init("localhost", "18001", "localhost", "18002");
  
  char buf[4096];
  char code[] = {'C', 'R', 'A', 'S', 'H', 1, 0, 0};
  
  for (int i = 0; i < 4096; i++)
    buf[i] = 0;  
  printf("Zeroing out volume\n\n");
  ebs_write(buf, 0);
  ebs_write(buf, 4096);
  ebs_write(buf, 20000);
  ebs_write(buf, 20004);
  
    
  printf("PRIMARY CRASH BETWEEN WRITES:\n");
  for (int i = 0; i < 4096; i++)
    buf[i] = 'a';  
  printf("Writing 4K x 'a' to offset 20000\n");
  ebs_write(buf, 20000);

  
  for (int i = 0; i < 4096; i++)
    buf[i] = 'b';  
  printf("Writing 4K x 'b' to offset 20001\n");
  ebs_write(buf, 20001);


  printf("Crashing the primary server\n");  
  ebs_write(buf, *(long*) code);

  for (int i = 0; i < 4096; i++)
    buf[i] = 'c';  
  printf("Writing 4K x 'c' to offset 20002\n");
  ebs_write(buf, 20002);

  
  for (int i = 0; i < 4096; i++)
    buf[i] = 'd';  
  printf("Writing 4K x 'd' to offset 20003\n");
  ebs_write(buf, 20003);
  
  ebs_read(buf, 20000);
  assert(buf[0] = 'a');
  assert(buf[1] = 'b');
  assert(buf[2] = 'c');
  assert(buf[3] = 'd');
  printf("Assert: Read from offset 20000 returned 'abcd' at bytes 0-3\n\n");
  
  for (int i = 0; i < 4096; i++)
    buf[i] = 0;  
  printf("Zeroing out volume\n\n");
  ebs_write(buf, 20000);
  ebs_write(buf, 20003);
  
  return 0;
}