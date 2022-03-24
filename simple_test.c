#include "ebs.h"

int main() {
  char buf[4096];
  ebs_write(buf, 0);
  ebs_read(buf, 0);
  return 0;
}
