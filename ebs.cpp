#include <grpc++/grpc++.h>
#include <iostream>

#include "client_status_defs.h"
#include "ebs.grpc.pb.h"
#include "ebs.h"

// Client can set up both channels at the beginning and simply switch which one
// it is sending to whenever primary changes.
static std::shared_ptr<grpc::Channel> channels[2];
static std::unique_ptr<ebs::Server::Stub> stubs[2];

static bool initialized = false;

static size_t primary_idx = 0;

// initialize both channels and stubs
int ebs_init(char* ip1, char* port1, char* ip2, char* port2) {

  if (initialized) {
    return 0;
  }

  //TODO: probably need some error checking here
  //TODO: read ip addresses and ports from config file
  channels[0] = grpc::CreateChannel(std::string() + ip1 + ":" + port1, grpc::InsecureChannelCredentials());
  channels[1] = grpc::CreateChannel(std::string() + ip2 + ":" + port2, grpc::InsecureChannelCredentials());

  stubs[0] = ebs::Server::NewStub(channels[0]);
  stubs[1] = ebs::Server::NewStub(channels[1]);

  initialized = 1;

  return 0;
}

int ebs_read(void *buf, off_t offset) {
  std::cout << "Read" << std::endl;

  ebs::ReadReq request;
  request.set_offset(offset);

  ebs::ReadReply reply;

  for (int i = 0; i < 2; ++i) {
    grpc::ClientContext context;
    grpc::Status status = stubs[primary_idx]->read(&context, request, &reply);

    if (status.ok()) {
      switch (reply.status()) {
        case EBS_SUCCESS:
          memcpy(buf, reply.data().data(), 4096);
          return EBS_SUCCESS;
        case EBS_NOT_PRIMARY:
          primary_idx = (primary_idx + 1) % 2;
          break;
        default:
          return EBS_UNKNOWN_ERROR;
      }
    }
    else {
      if (channels[primary_idx]->GetState(true) == GRPC_CHANNEL_TRANSIENT_FAILURE) {
        primary_idx = (primary_idx + 1) % 2;
      }
      else {
        --i; //try again on same server
      }
    }
  }

  return EBS_NO_SERVER;
}

int ebs_write(void *buf, off_t offset) {
  std::cout << "Write" << std::endl;

  std::string s_buf;
  s_buf.resize(4096);
  memcpy(s_buf.data(), buf, 4096);

  ebs::WriteReq request;
  request.set_offset(offset);
  request.set_data(s_buf);

  ebs::WriteReply reply;

  for (int i = 0; i < 2; ++i) {
    grpc::ClientContext context;
    grpc::Status status = stubs[primary_idx]->write(&context, request, &reply);

    if (status.ok()) {
      switch (reply.status()) {
        case EBS_SUCCESS:
          return EBS_SUCCESS;
        case EBS_NOT_PRIMARY:
          primary_idx = (primary_idx + 1) % 2;
          break;
        default:
          return EBS_UNKNOWN_ERROR;
      }
    }
    if (channels[primary_idx]->GetState(true) == GRPC_CHANNEL_TRANSIENT_FAILURE) {
        primary_idx = (primary_idx + 1) % 2;
      }
      else {
        --i; //try again on same server
      }
  }

  return EBS_NO_SERVER;
}
