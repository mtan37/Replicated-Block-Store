#include <stdio.h>
#include <assert.h>

#include "ebs.h"

int main() {
  ebs_init("localhost", "18001", "localhost", "18002");
  char buf[4096];
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

  return 0;
}
