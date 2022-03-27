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

void start_primary_heartbeat() {
  
  std::cout << "Start operating as - send heartbeat to " << alt_ip << std::endl;

  // initialize the variable needed for grpc heartbeat call
  std::shared_ptr<grpc::Channel> channel = 
    grpc::CreateChannel(alt_ip + ":" + DEF_BACKUP_PORT, grpc::InsecureChannelCredentials());
    std::unique_ptr<ebs::Backup::Stub> stub(ebs::Backup::NewStub(channel));
  google::protobuf::Empty request;
  google::protobuf::Empty reply;

  while (true){

    std::cout << "TEST: start of new primary heartbeat iteration. My state is " << state <<"\n";
    grpc::ClientContext context;  
    grpc::Status status = stub->heartBeat(&context, request, &reply);

    if (state == INITIALIZING) {
      // don't handle the responses if the servrer is still initializing
    } else if (status.ok() && state == PRIMARY_NORMAL) {
      std::cout << "(p) BoopBoop\n"; 
    } else if (status.ok() && state == SINGLE_SERVER) {
      // send log to backup
      recovery_lock.acquire_write(); // exclusive
      state = RECOVERING;

      grpc::ClientContext log_context;
      ebs::ReplayReq log_request;
      ebs::ReplayReply log_reply;
      grpc::Status log_status = 
        stub->replayLog(&log_context, log_request, &log_reply);
      if (log_status.ok()){
        state = PRIMARY_NORMAL;
      } else {
        state = SINGLE_SERVER;
      }
      recovery_lock.release_write();

    } else if (status.error_code() == grpc::UNAVAILABLE) {       
      state = SINGLE_SERVER;
    } else {
      std::cout << "Something unexpected happend. Shuting down the server\n";
      std::cout << status.error_code() << ": " << status.error_message()
              << std::endl;  
      break;
    }

    sleep(HB_SEND_TIMEOUT);
  }

  std::cout << "Primary heartbeat call terminate\n";
}

void start_backup_heartbeat(
  grpc::Server *backup_service,
  std::thread *backup_service_thread) {

    // start monitoring heartbeat
    double elapsed;
    timespec now;

    // Initialize time for last recieved heartbeat    
    set_time(&last_heartbeat);  
    while (true){
      std::cout << "TEST: start of new backup heartbeat iteration. My state is " << state <<"\n";
      
      set_time(&now);
      elapsed = difftimespec_s(last_heartbeat, now);

      std::cout << "Checking Timeout\n";
      if (elapsed < HB_FAIL_TIMEOUT){
        if (elapsed<0) elapsed = 0;
        // continue - still good, sleep until HB_LISTEN_TIMEOUT period and check again
        sleep(HB_FAIL_TIMEOUT - elapsed); 
      } else {
        // Primary has timed out
        std::cout << "Primary is non-responsive, transitioning to primary" << std::endl;      
        break;            
      }    
    }

    // Transition into primary state
    state = INITIALIZING;
    std::thread primary_server_heartbeat_thread(start_primary_heartbeat);
    
    // stop backup service
    backup_service->Shutdown();
    backup_service_thread->join();
    // initialize block reader/writer locks? i don't think this is needed in C++
    state = SINGLE_SERVER;
    state_cv.notify_all();

    primary_server_heartbeat_thread.join();
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
    std::cout << "(b) BoopBoop" << std::endl;
    set_time(&last_heartbeat);
    return grpc::Status::OK;
  }

  grpc::Status write (grpc::ServerContext *context,
                      const ebs::WriteReq *requestt,
                      const ebs::WriteReply *reply) {
    set_time(&last_heartbeat);
    // TODO do write
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

// Run grpc service in a loop
void run_service(grpc::Server *server, std::string serviceName) {
  std::cout << "Starting to run " << serviceName << "\n";
  server->Wait();
}

/**
 * Export server grpc interface
 */
std::unique_ptr<grpc::Server> export_server (std::string ip) {
  std::string my_address = ip + ":" + DEF_SERVER_PORT;
  ServerImpl ebs_server;

  grpc::ServerBuilder builder;
  builder.RegisterService(&ebs_server);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  return server;
}

/**
 * Export backup grpc interface
 */
std::unique_ptr<grpc::Server> export_backup (std::string ip) {
  std::string my_address = ip + ":" + DEF_SERVER_PORT;
  BackupImpl backup;

  grpc::ServerBuilder builder;
  builder.RegisterService(&backup);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  return server;
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
  grpc::Server *backup_service = export_backup(pb_ip).get();
  std::string name = "backup";
  std::thread backup_service_thread(run_service, backup_service, name);

  // export server interface to listen for clients
  grpc::Server *server_service = export_server(pb_ip).get();
  name = "server";
  std::thread server_service_thread(run_service, server_service, name);

  // start heartbeat thread(as backup)
  // This thread monitors for timeout and update state for transition
  std::thread heartbeat(start_backup_heartbeat, backup_service, &backup_service_thread);

  // heartbeat.join(); 
  // once hearbeat thread stop, stop service services
  server_service->Shutdown();
  server_service_thread.join();
  
  return 0;
}
