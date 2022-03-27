#include <condition_variable>
#include <google/protobuf/empty.pb.h>
#include <grpc++/grpc++.h>
#include <iostream>
#include <thread>

#include "ebs.grpc.pb.h"
#include "ReaderWriter.h"
#include "helper.h"

/*################
# Constants
################*/
// Ports to listen on 
const std::string DEF_SERVER_PORT = "5000";// default port to listen on server service
const std::string DEF_BACKUP_PORT = "5001";// default port to listen on backup service
const int HB_FAIL_TIMEOUT = 8;
const int HB_SEND_TIMEOUT = 2;
/*################
# Globals
################*/
std::string pb_ip = "0.0.0.0"; // IP addr I listen on
std::string alt_ip = "0.0.0.0"; // IP addr of secondary server
timespec last_heartbeat; // time last heartbeat received by backup

// Initial state: BACKUP_NORMAL
// BACKUP_NORMAL -> INITIALIZING when backup doesn't get heartbeat from primary
// INITIALIZING -> SINGLE_SERVER when a backup is ready to act as a primary
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

void primary_heartbeat_thread(ebs::Backup::Stub *stub) {
  
  std::cout << "Start operating as - send heartbeat to " << alt_ip << std::endl;

  std::shared_ptr<grpc::Channel> channel = 
    grpc::CreateChannel(alt_ip + ":" + DEF_BACKUP_PORT, grpc::InsecureChannelCredentials());
  // std::unique_ptr<ebs::Backup::Stub> stub(ebs::Backup::NewStub(channel));

  google::protobuf::Empty request;
  google::protobuf::Empty reply;
  //while thread running
  while (true){
    //send heartBeat rpc to backup.
    grpc::ClientContext context;  
    // std::cout << "Boopboop\n";
    grpc::Status status = stub->heartBeat(&context, request, &reply);
    if (status.ok()) {
      std::cout << "(p) BoopBoop\n"; 
      sleep(HB_SEND_TIMEOUT);     
    } else {
      if (status.error_code() != 14){
        std::cout << "...ope\n";
        std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;        
      } 
      sleep(HB_FAIL_TIMEOUT);  
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
                
    }
    
    
    //sleep
    
  }    
  std::cout << "Leaving against my will\n";
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

/*################
# Primary-Backup Communication
# This is running on the backup only to receive messages from the primary
################*/
class BackupImpl final : public ebs::Backup::Service {
public:
  grpc::Status heartBeat (grpc::ServerContext *context,
                          const google::protobuf::Empty *request,
                          google::protobuf::Empty *reply) {
    //last_heartbeat = now
    std::cout << "(b) BoopBoop" << std::endl;
    set_time(&last_heartbeat);
    //return success
    return grpc::Status::OK;
  }

  grpc::Status write (grpc::ServerContext *context,
                      const ebs::WriteReq *requestt,
                      const ebs::WriteReply *reply) {
    // update heartbeat
    set_time(&last_heartbeat);
    //do write
    //return success
    return grpc::Status::OK;
  }

  grpc::Status replayLog (grpc::ServerContext *context,
                          const ebs::ReplayReq *request,
                          const ebs::ReplayReply *reply) {
    
    //while more writes:
      // update heartbeat - here or above?
      set_time(&last_heartbeat);
      //do write
    //return success
    return grpc::Status::OK;
  }
};

/*################
# Client-Server communication
# The interface for servers(both primary and backup) to listen for clients
################*/
class ServerImpl final : public ebs::Server::Service {
private:
  
public:
  ServerImpl () {}

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

/**
 Run in backup mode 
 Spawns second thread
  a. listen on grpc server
  b. heartbeat
  listen is shut down if heartbeat fails. When heartbeat fails it spawns  thread
  that runs primary heartbeat.
*/
void run_as_backup(ebs::Backup::Stub *stub){
  // Prepare for heartbeat    
  set_time(&last_heartbeat);     

  // start monitoring heartbeat - breaks when heartbeat stops
  // backup_heartbeat_thread();
  double elapsed;
  timespec now;
  int timeout = HB_FAIL_TIMEOUT;
  // Monitor heartbeat 
  //while thread running 
  while (true){     
    //start = now        
    set_time(&now);
    //sleep(start - last_heartbeat + HB_LISTEN_TIMEOUT)
    elapsed = difftimespec_s(last_heartbeat, now);
    if (elapsed<0) elapsed = 0;
    //if start older than last_heartbeat sleep and check again

    if (elapsed < timeout){
      std::cout << "Checking Timeout\n";
      // continue - still good, sleep until HB_LISTEN_TIMEOUT period and check again
      sleep(timeout - elapsed); 
      timeout = HB_FAIL_TIMEOUT;
    } else {
      // Primary has timed out
      std::cout << "Primary is non-responsive, transitioning to primary" << std::endl;      
      break;            
    }    
  }

  // no need to lock state_mutex here because it would be impossible to get past the wait for initialized loop in write before state is set to initialized
  // state = INITIALIZING
  state = INITIALIZING;
  // start primary_heartbeat_thread
  std::thread primary_server(primary_heartbeat_thread, stub);
  
  // stop backup service
  // server->Shutdown();
  // backup_server.join();
  // initialize block reader/writer locks
  // ?? What do I need to do to satisfy above?
  // state = SINGLE_SERVER
  state = SINGLE_SERVER;
  // state_cv.notify_all()  
  // ADD BACK IN state_cv.notify_all();  
  // Stopped monitoring HB, Shutdown server and stop backup service  
  // std::thread primary_server(primary_heartbeat_thread);
  primary_server.join();
}

/**
 * Export server grpc interface
 */
void export_server (std::string ip) {
  std::string my_address = ip + ":" + DEF_SERVER_PORT;
  ServerImpl ebs_server;

  grpc::ServerBuilder builder;
  builder.RegisterService(&ebs_server);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  server->Wait();
}

/**
 * Export backup grpc interface
 */
void export_backup (std::string ip) {
  std::string my_address = ip + ":" + DEF_SERVER_PORT;
  BackupImpl backup;

  grpc::ServerBuilder builder;
  builder.RegisterService(&backup);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  server->Wait();
}

/*################
# Main
################*/

/**
 Parse out arguments sent into program
 -alt = secondary server ip addy & port
 -listn = what port we want to listen on
 */
int parse_args(int argc, char** argv){    
    if (argc < 3) {
      std::cout << "Usage: prog <pb srvr ip> <alt srvr ip> (default = 0.0.0.0)\n"; 
      return -1;
    }

    // TODO need to do error checking on the argument
    pb_ip = std::string(argv[1]);
    alt_ip = std::string(argv[2]);
    return 0;
}

int main (int argc, char** argv) {
  // Parse any arguments to get ip address of the other server
  if (parse_args(argc, argv) <0) return -1;
  std::cout << "TEST: local computer ip " << pb_ip << "\n";
  std::cout << "TEST: alternate computer ip " << alt_ip << "\n";

  // server start up as a backup
  state = BACKUP_NORMAL;

  // export backup grpc service in a seperate thread
  std::thread backup_service(export_backup, pb_ip);

  // export server interface to listen for clients
  std::thread server_service(export_server, pb_ip);

  // start heartbeat thread(as backup)
  // This thread monitors for timeout and update state for transition
  std::thread heartbeat(backup_heartbeat_thread);

  backup_service.join();
  server_service.join();
  return 0;
}
