#include <google/protobuf/empty.pb.h>
#include <grpc++/grpc++.h>
#include <iostream>

#include "ebs.grpc.pb.h"
#include "ReaderWriter.h>

void primary_heartbeat_thread() {
  //while thread running
    //send heartBeat rpc to backup
    //if failed:
      //backup_status = dead
      //mode = single_server
    //else if mode == single_server
      //backup_status = recovering
      //send log
      //if success:
        //backup_status = ready
        //mode = primary
      //else:
        //backup_status = dead
    //sleep
}

void backup_heartbeat_thread() {
  //while thread running
    //start = now
    //sleep(start - last_heartbeat + timeout)
    //if start older than last_heartbeat
      //continue
    //backup_status = dead
    //mode = primary
    //start primary_heartbeat_thread
    //stop backup service
    //stop backup_heartbeat_thread
}

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
    //mode = backup
    //run_backup()
    //start backup_heartbeat_thread
  }

  grpc::Status read (grpc::ServerContext *context,
                    const ebs::ReadReq *request,
                    const ebs::ReadReply *reply) {
    //if mode == backup
      //return primary primary_address
    //else:
      //acquire read lock
      //read data
      //release read lock
      //return data
    return grpc::Status::OK;
  }

  grpc::Status write (grpc::ServerContext *context,
                      const ebs::WriteReq *request,
                      const ebs::WriteReply *reply) {
    //if mode == backup
      //return primary primary_address
    //else:
      //acquire write lock
      //while backup_status == recovering {}
      //if backup_status == ready:
        //send write to backup
        //if failed:
          //backup_status = dead
          //mode = single_server
      //if mode == single_server
        //add write to log
      //write locally
      //release write lock
      //return success
    return grpc::Status::OK;
  }
};

// This is running on the backup only to receive messages from the primary
class BackupImpl final : public ebs::Backup::Service {
public:
  grpc::Status heartBeat (grpc::ServerContext *context,
                          const google::protobuf::Empty *request,
                          const google::protobuf::Empty *reply) {
    //last_heartbeat = now
    //return success
    return grpc::Status::OK;
  }

  grpc::Status write (grpc::ServerContext *context,
                      const ebs::WriteReq *requestt,
                      const ebs::WriteReply *reply) {
    //do write
    //return success
    return grpc::Status::OK;
  }

  grpc::Status replayLog (grpc::ServerContext *context,
                          const ebs::ReplayReq *request,
                          const ebs::ReplayReply *reply) {
    //while more writes:
      //do write
    //return success
    return grpc::Status::OK;
  }
};


//Both servers can use the same port for the Server service, because they will
//be on different IPs. However, the Backup service must be on a different port
//than the Server service running on the same server.
void run_server () {
  std::string my_address = "ip:port";
  ServerImpl ebs_server;

  grpc::ServerBuilder builder;
  builder.RegisterService(&ebs_server);
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
  //run_server()
}
