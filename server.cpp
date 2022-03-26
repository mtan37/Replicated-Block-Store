#include <google/protobuf/empty.pb.h>
#include <grpc++/grpc++.h>
#include <iostream>

#include "ebs.grpc.pb.h"


//This is the the client->server part so both primary and backup need to export
//this. When the backup receives a request on this interface however,
class ServerImpl final : public ebs::Server::Service {
private:
  //These are for the primary to send RPCs to the backup. They should be null on
  //the backup.
  std::shared_ptr<grpc::Channel> channel;
  std::unique_ptr<ebs::Backup::Stub> stub;
public:
  ServerImpl () {
    // if I am primary
      //init channel with backup addr and create stub
  }

  grpc::Status read (grpc::ServerContext *context,
                    const ebs::ReadReq *request,
                    const ebs::ReadReply *reply) {
    return grpc::Status::OK;
  }

  grpc::Status write (grpc::ServerContext *context,
                      const ebs::WriteReq *request,
                      const ebs::WriteReply *reply) {
    return grpc::Status::OK;
  }
};

// This is running on the backup only to receive messages from the primary
class BackupImpl final : public ebs::Backup::Service {
public:
  grpc::Status heartBeat (grpc::ServerContext *context,
                          const google::protobuf::Empty *request,
                          const google::protobuf::Empty *reply) {
    return grpc::Status::OK;
  }

  grpc::Status write (grpc::ServerContext *context,
                      const ebs::WriteReq *requestt,
                      const ebs::WriteReply *reply) {
    return grpc::Status::OK;
  }

  grpc::Status replayLog (grpc::ServerContext *context,
                          const ebs::ReplayReq *request,
                          const ebs::ReplayReply *reply) {
    return grpc::Status::OK;
  }
};


//Both servers can use the same port for the Server service, because they will
//be on different IPs. However, the Backup service must be on a different port
//than the Server service running on the same server.
void run_server () {
  std::string my_address = "ip:port";
  ServerImpl primary;

  grpc::ServerBuilder builder;
  builder.RegisterService(&primary);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  server->Wait();
}

void run_backup () {
  std::string my_address = "ip:port";
  BackupImpl backup;

  grpc::ServerBuilder builder;
  builder.RegisterService(&backup);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  server->Wait();
}

int main () {
}
