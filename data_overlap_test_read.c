#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "ebs.h"

int main() {
  ebs_init("localhost", "18001", "localhost", "18002");
  
  char buf[4096];
  
/*  printf("Zeroing out volume\n\n");
  for (int i = 0; i < 4096; i++)
    buf[i] = 0;  
  ebs_write(buf, 0);
  ebs_write(buf, 4096);
  ebs_write(buf, 20000);
  ebs_write(buf, 20004);
  
  printf("ALIGNED, NORMAL OPERATION:\n");
  
  for (int i = 0; i < 4096; i++)
    buf[i] = 'a';  
  printf("Writing 4K x 'a' to offset 0\n");
  ebs_write(buf, 0);
  
  for (int i = 0; i < 4096; i++)
    buf[i] = 'b';
  printf("Writing 4K x 'b' to offset 4K\n");
  ebs_write(buf, 4096);
  
  ebs_read(buf, 2048);
  for (int i = 0; i < 2048; i++) {
    assert(buf[2048+i] = 'a');
    assert(buf[4096+i] = 'b');
  }
  printf("Assert: Read from offset 2K returned 2K x 'a' followed by 2K x 'b'\n\n");
  
  printf("MISALIGNED, NORMAL OPERATION:\n");
  
  for (int i = 0; i < 4096; i++)
    buf[i] = 'a';  
  printf("Writing 4K x 'c' to offset 42\n");
  ebs_write(buf, 42);
  
  for (int i = 0; i < 4096; i++)
    buf[i] = 'b';
  printf("Writing 4K x 'd' to offset 45\n");
  ebs_write(buf, 45);
*/  
  ebs_read(buf, 0);
  assert(buf[42] = 'c');
  assert(buf[43] = 'c');
  assert(buf[44] = 'c');
  assert(buf[45] = 'd');
  assert(buf[46] = 'd');
  assert(buf[47] = 'd');
  printf("Assert: Read from offset 0 returned 'c' at bytes 42-44, 'd' at bytes 45-47\n\n");
  
/*  for (int i = 0; i < 4096; i++)
    buf[i] = 0;  
  printf("Zeroing out volume\n\n");
  ebs_write(buf, 0);
  ebs_write(buf, 4096);
*/
  
// OLD TEST:
/*  char buf[4096];
  buf[0] = 0;
  buf[1] = 't';
  buf[2] = 'e';
  buf[3] = 's';
  buf[4] = 't';
  for (int i = 5; i < 4096; i++)
	  buf[i] = 0;
  printf("Write status: %d\n", ebs_write(buf, 0));
  printf("Read status: %d\n", ebs_read(buf, 0));

  printf("%c\n", buf[4]);
  assert(buf[4] == 't');

  printf("%c\n", buf[40]);
  printf("%c\n", buf[400]);
  assert(buf[40] == 0);
  assert(buf[400] == 0);

  ebs_read(buf, 5000);
  printf("%c\n", buf[0]);
  assert(buf[0] == 0);

  for (int i = 0; i < 4096; i++)
	  buf[i] = 'a';
  ebs_write(buf, 0);
  for (int i = 0; i < 4096; i++)
	  buf[i] = 'b';
  ebs_write(buf, 20);
  ebs_read(buf, 10);
  printf("%c\n", buf[9]);
  printf("%c\n", buf[10]);
  assert(buf[9] == 'a');
  assert(buf[10] == 'b');
*/
  return 0;
}
