#include <stdio.h>

#include "ebs.h"
#include "timing.h"

void usage(char *name) {
  printf("Usage: %s <server1> <port1> <server2> <port2> [iterations]\n", name);
  printf("  server1     ip of first server\n");
  printf("  server2     ip of second server\n");
  printf("  port1       port of first server\n");
  printf("  port2       port of second server\n");
  printf("  iterations  number of iterations to perform (default: 1000)\n");
}

int main(int argc, char** argv) {
  if (argc != 5 && argc != 6) {
    usage(argv[0]);
    return 1;
  }

  if (ebs_init(argv[1], argv[2], argv[3], argv[4]) != EBS_SUCCESS) {
    return 1;
  }

  int iterations = 1000;
  if (argc == 6) {
    iterations = atoi(argv[5]);
  }

  char buf[4096];
  int offset = 0;
  int read_latency = 0;
  DO_TRIALS(
    {offset = (offset + 4096) % (1024*1024);},
    {
      if (ebs_read(buf, offset) != EBS_SUCCESS) {
        return 1;
      }
    },
    iterations,
    read_latency
  );

  int write_latency = 0;
  char write_buf[4096];
  for (int i = 0; i < 4096; ++i) {
    write_buf[i] = i%256;
  }
  DO_TRIALS(
    {offset = (offset + 4096) % (1024*1024);},
    {
      if (ebs_write(write_buf, offset) != EBS_SUCCESS) {
        return 1;
      }
    },
    iterations,
    write_latency
  );

  print_result("Read latency", read_latency);
  print_result("Write latency", write_latency);

  return 0;
}
