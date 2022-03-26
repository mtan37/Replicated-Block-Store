#include <google/protobuf/empty.pb.h>
#include <grpc++/grpc++.h>
#include <iostream>
#include <thread>
#include <fstream>

#include "ebs.grpc.pb.h"

bool replica_mode = false;
bool primary = false;

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
    
    //Read from disk          
    
    return grpc::Status::OK;
  }

  grpc::Status write (grpc::ServerContext *context,
                      const ebs::WriteReq *request,
                      ebs::WriteReply *reply) {
             
    std::cout << "Server write" << std::endl;
    
    if (primary) {
      if (replica_mode) {
        //Send write to backup
        auto channel = CreateChannel("localhost:12346", grpc::InsecureChannelCredentials());
        auto stub = ebs::Server::NewStub(channel);
        grpc::ClientContext context;
        ebs::WriteReq request;
        ebs::WriteReply reply;
        grpc::Status status = stub->write(&context, request, &reply);
        if (status.ok()) {
          // check reply.message()
          std::cout << "Write to backup RPC success" << std::endl;
        } else {
          // rpc failed.
          std::cout << "Write to backup RPC failed " << status.error_code() << std::endl;
        }
      } else {
        //Write to log
      }
    }
    
    //Commit write to disk
    
    return grpc::Status::OK;
  }
};

// This is running on the backup only to receive messages from the primary
class BackupImpl final : public ebs::Backup::Service {
public:
  grpc::Status heartBeat (grpc::ServerContext *context,
                          const google::protobuf::Empty *request,
                          google::protobuf::Empty *reply) {
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
void run_server (std::string port) {
  std::string my_address = "localhost:" + port;
  ServerImpl primary;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&primary);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  server->Wait();
}

void run_backup () {
  std::string my_address = "localhost:12347";
  BackupImpl backup;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&backup);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  server->Wait();
}

int main (int argc, char** argv) {
  if (strcmp(argv[1], "-p") == 0) {
    std::cout << "I'm the primary" << std::endl;
    std::ofstream MyFile("volume");
    primary = true; 
    
    std::thread server_thread(run_server, "12345");
    server_thread.detach();
    
    while (true) {
      auto channel = CreateChannel("localhost:12347", grpc::InsecureChannelCredentials());
      auto stub = ebs::Backup::NewStub(channel);
      grpc::ClientContext context;
      google::protobuf::Empty request;
      google::protobuf::Empty reply;
      grpc::Status status = stub->heartBeat(&context, request, &reply);
      if (status.ok()) {
        // check reply.message()
        std::cout << "Heartbeat RPC success" << std::endl;
      } else {
        // rpc failed.
        std::cout << "Heartbeat RPC failed " << status.error_code() << std::endl;
      }
      sleep(5);
    }

  }
  if (strcmp(argv[1], "-b") == 0) {
    std::cout << "I'm the backup" << std::endl;
    std::ofstream MyFile("volume");
    
    std::thread server_thread(run_server, "12346");
    server_thread.detach();
    
    std::thread backup_thread(run_backup);
    backup_thread.detach();
    
    while(true);
    
  }
}
