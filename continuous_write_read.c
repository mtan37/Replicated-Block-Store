#include <stdio.h>

#include "ebs.h"
#include "timing.h"

void usage(char *name) {
  printf("Usage: %s <server1> <port1> <server2> <port2> [iterations]\n", name);
  printf("  server1     ip of first server\n");
  printf("  server2     ip of second server\n");
  printf("  port1       port of first server\n");
  printf("  port2       port of second server\n");
}

int main(int argc, char** argv) {
  if (argc != 5) {
    usage(argv[0]);
    return 1;
  }

  if (ebs_init(argv[1], argv[2], argv[3], argv[4]) != EBS_SUCCESS) {
    return 1;
  }

  const long 
  const long MAX_OFFSET = 256L * 1024L * 1024L * 1024L - 4096 + 1;
  char buf[4096];
  char write_buf[4096];
  for (int i = 0; i < 4096; ++i) {
    write_buf[i] = i%256;
  }
  int offset = 0;

  while(1==1) {
    if (offset > MAX_OFFSET) offset = 0;
    if (ebs_read(buf, offset) != EBS_SUCCESS) return 1;
    if (ebs_write(write_buf, offset) != EBS_SUCCESS) return 1;
    offset++;
  }

} 
