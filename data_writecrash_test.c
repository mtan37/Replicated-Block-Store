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
  
  printf("CRASH IN THE MIDDLE OF A WRITE:\n");
  for (int i = 0; i < 4096; i++)
    buf[i] = 'a';
  printf("Writing 4K x 'a' to offset 0\n");
  ebs_write(buf, *(long*) code);

  
  ebs_read(buf, 0);
  for (int i = 0; i < 4096; i++)
    assert(buf[i] = 'a');
  printf("Assert: Read from offset 0 on new primary returned 'a' for all bytes\n");
  
  printf("PLEASE RESTART SERVER NOW\n");
  sleep(10);
  printf("PLEASE MANUALLY TERMINATE PRIMARY NOW\n");
  sleep(10);
  
  ebs_read(buf, 0);
  for (int i = 0; i < 4096; i++)
    assert(buf[i] = 'a');
  printf("Assert: Read from offset 0 on original primary returned 'a' for all bytes\n");

  for (int i = 0; i < 4096; i++)
    buf[i] = 0;  
  printf("Zeroing out volume\n\n");
  ebs_write(buf, 0);
  
  printf("PLEASE RESTART SERVER NOW\n\n");
  
  return 0;
}