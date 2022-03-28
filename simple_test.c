#include <stdio.h>

#include "ebs.h"

int main() {
  ebs_init();
  char buf[4096];
  printf("%d\n", ebs_write(buf, 0));
  printf("%d\n", ebs_read(buf, 0));
  return 0;
}
