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
  
    
  printf("SINGLE-SERVER WRITES THEN READ AFTER FAILOVER:\n");
  for (int i = 0; i < 4096; i++)
    buf[i] = 'a';  
  printf("Writing 4K x 'a' to offset 20000 in single-server mode\n");
  ebs_write(buf, 20000);

  for (int i = 0; i < 4096; i++)
    buf[i] = 'b';  
  printf("Writing 4K x 'b' to offset 20001 in single-server mode\n");
  ebs_write(buf, 20001);
  
  printf("PLEASE RESTART SERVER NOW\n");
  sleep(10);

  printf("Crashing the primary server\n"); 
  ebs_write(buf, *(long*) code);
 
  ebs_read(buf, 19999);
  assert(buf[0] == 0);
  assert(buf[1] == 'a');
  assert(buf[2] == 'b');
  printf("Assert: Read from offset 19999 returned '0ab' at bytes 0-2\n");
  
  return 0;
}