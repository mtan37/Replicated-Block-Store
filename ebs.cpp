#include <grpc++/grpc++.h>
#include <iostream>

#include "ebs.grpc.pb.h"
#include "ebs.h"

// Client can set up both channels at the beginning and simply switch which one
// it is sending to whenever primary changes.
static std::shared_ptr<grpc::Channel> channels[2];
static std::unique_ptr<ebs::Server::Stub> stubs[2];

// initialize both channels and stubs
int ebs_init() {
  //TODO: probably need some error checking here
  //TODO: read ip addresses and ports from config file
  channels[0] = grpc::CreateChannel("ip1:port2", grpc::InsecureChannelCredentials());
  channels[1] = grpc::CreateChannel("ip2:port2", grpc::InsecureChannelCredentials());

  stubs[0] = ebs::Server::NewStub(channels[0]);
  stubs[1] = ebs::Server::NewStub(channels[1]);

  return 1;
}

int ebs_read(void *buf, off_t offset) {
  std::cout << "Read" << std::endl;
  return 1;
}

int ebs_write(void *but, off_t offset) {
  std::cout << "Write" << std::endl;
  return 1;
}
