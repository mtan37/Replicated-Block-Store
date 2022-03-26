#include <condition_variable>
#include <google/protobuf/empty.pb.h>
#include <grpc++/grpc++.h>
#include <iostream>
#include <mutex>

#include "ebs.grpc.pb.h"
#include "ReaderWriter.h"

// Initial state: BACKUP_NORMAL
// BACKUP_NORMAL -> INITIALIZING when backup doesn't get heartbeat from primary
// INITIALIZING -> SINGLE_SERVER when a backup is
// PRIMARY_NORMAL -> SINGLE_SERVER when the primary can't reach the backup
// SINGLE_SERVER -> RECOVERING when the primary detects the backup comes up
// RECOVERING -> PRIMARY_NORMAL when recovery is done
enum {
  PRIMARY_NORMAL,
  BACKUP_NORMAL,
  SINGLE_SERVER,
  INITIALIZING,
  RECOVERING
} state;

ReaderWriter recovery_lock;

std::mutex state_mutex;
std::condition_variable state_cv;

void primary_heartbeat_thread() {
  //while thread running
    //send heartBeat rpc to backup
    //if state != INITIALIZING
      //if failed:
        //state = SINGLE_SERVER
      //else if state == SINGLE_SERVER
        //recovery_lock.acquire_write() // exclusive
        //state = RECOVERING
        //send log
        //if success:
          //state = PRIMARY_NORMAL
        //else:
          //state = SINGLE_SERVER
        //recovery_lock.release_write()
    //sleep
}

void backup_heartbeat_thread() {
  //while thread running
    //start = now
    //sleep(start - last_heartbeat + timeout)
    //if start older than last_heartbeat
      //continue
    //no need to lock state_mutex here because it would be impossible to get past the wait for initialized loop in write before state is set to initialized
    //state = INITIALIZING
    //start primary_heartbeat_thread
    //stop backup service
    //stop backup_heartbeat_thread
    //initialize block reader/writer locks
    //state = SINGLE_SERVER
    //state_cv.notify_all()
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
    //if state == BACKUP_NORMAL
      //return primary primary_address
    //else:
      //lock state_mutex
      //while (state == INITIALIZING)
        // state_cv.wait(state_mutex);
      //unlock state_mutex
      //acquire read lock
      //read data
      //release read lock
      //return data
    return grpc::Status::OK;
  }

  grpc::Status write (grpc::ServerContext *context,
                      const ebs::WriteReq *request,
                      const ebs::WriteReply *reply) {
    //if state == BACKUP_NORMAL
      //return primary primary_address
    //else:
      //recovery_lock.acquire_read() // shared
      //lock state_mutex
      //while (state == INITIALIZING)
        // state_cv.wait(state_mutex);
      //unlock state_mutex
      //acquire write lock
      //if state == PRIMARY_NORMAL
        //send write to backup
        //if failed:
          //state = SINGLE_SERVER
      //if state = SINGLE_SERVER
        //add write to log
      //write locally
      //release write lock
      //recovery_lock.release_read()
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
