#include <condition_variable>
#include <google/protobuf/empty.pb.h>
#include <grpc++/grpc++.h>
#include <iostream>
#include <mutex>
#include <thread>

#include "ebs.grpc.pb.h"
#include "ReaderWriter.h"


/*################
# Constants
################*/
// Ports to listen on 
const std::string DEF_CS_PORT = "5000";
const std::string DEF_PB_PORT = "5001";
const int HB_LISTEN_TIMEOUT = 8;
const int HB_SEND_TIMEOUT = 2;
/*################
# Globals
################*/
std::string pb_server = "0.0.0.0:" + DEF_PB_PORT; // IP addr I listen on
std::string alt_sever = "0.0.0.0:" + DEF_PB_PORT; // IP addr of secondary server
timespec last_heartbeat; // time last heartbeat received by backup

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

/*################
# Helper Functions
################*/

void set_time(struct timespec* ts)
{
    clock_gettime(CLOCK_MONOTONIC, ts);
}

double difftimespec_s(const struct timespec before, const struct timespec after)
{
    return ((double)after.tv_sec - (double)before.tv_sec);
}

/*################
# Run as Primary (client to backup) 
################*/


void primary_heartbeat_thread() {
  
  std::cout << "Starting as primary - initiating heartbeat to " << alt_sever << std::endl;
  //These are for the primary to send RPCs to the backup. They should be null on
  //the backup.
  std::shared_ptr<grpc::Channel> channel = 
    grpc::CreateChannel(alt_sever, grpc::InsecureChannelCredentials());
  std::unique_ptr<ebs::Backup::Stub> stub(ebs::Backup::NewStub(channel));

  
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
    } else {
      if (status.error_code() != 14){
        std::cout << "...oops\n";
        std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;        
      }
        
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
    sleep(HB_SEND_TIMEOUT);
  }    
  std::cout << "Leaving against my will\n";
}

/*################
# Primary-Backup Communication
################*/

// This is running on the backup only to receive messages from the primary
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
# Run as Backup (server to primary - default mode)
################*/

// Run server on seperate thread
void run_backup_server_thread(grpc::Server *server) {
  std::cout << "Starting to listen as backup\n";
  server->Wait();
}

/**
 Run in backup mode 
 Spawns second thread
  a. listen on grpc server
  b. heartbeat
  listen is shut down if heartbeat fails. When heartbeat fails it spawns  thread
  that runs primary heartbeat.
*/
void run_as_backup(){
  // Prepare for heartbeat    
  set_time(&last_heartbeat);     
  
  // Setup server, get ready to listen
  std::cout << "Initializing as Backup Server - listening on " << pb_server << std::endl;
  BackupImpl backup;
  grpc::ServerBuilder builder;
  builder.AddListeningPort(pb_server, grpc::InsecureServerCredentials());
  builder.RegisterService(&backup);
  // grpc::Server *server = new grpc::Server(builder.BuildAndStart());
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  // Start listening on seperate thread  
  // std::thread backup_server(run_backup_server_thread, server.get());
  std::thread backup_server(run_backup_server_thread, server.get());

  // start monitoring heartbeat - breaks when heartbeat stops
  // backup_heartbeat_thread();
  double elapsed;
  timespec now;

  // Monitor heartbeat 
  //while thread running 
  while (true){     
    //start = now        
    set_time(&now);
    //sleep(start - last_heartbeat + HB_LISTEN_TIMEOUT)
    elapsed = difftimespec_s(last_heartbeat, now);
    if (elapsed<0) elapsed = 0;
    //if start older than last_heartbeat sleep and check again
    if (elapsed < HB_LISTEN_TIMEOUT){
      std::cout << "Checking Timeout\n";
      // continue - still good, sleep until HB_LISTEN_TIMEOUT period and check again
      sleep(HB_LISTEN_TIMEOUT - elapsed); 
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
  std::thread primary_server(primary_heartbeat_thread);
  
  // stop backup service
  server->Shutdown();
  backup_server.join();
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



/*################
# Client-Server communication
################*/


//This is the the client->server part so both primary and backup need to export
//this. When the backup receives a request on this interface however,
class ServerImpl final : public ebs::Server::Service {
private:
  
public:
  ServerImpl () {
    
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


/*################
# Main
################*/

/**
 Parse out arguments sent into program
 -alt = secondary server ip addy & port
 -listn = what port we want to listen on
 */
int parse_args(int argc, char** argv){    
    int arg = 1;
    std::string argx;
    while (arg < argc){
        if (argc < arg) return 0;
        argx = std::string(argv[arg]);
        if (argx == "-alt") {
            alt_sever = std::string(argv[++arg]);            
        } if (argx == "-backup") {
            pb_server = "0.0.0.0:" + std::string(argv[++arg]);
        } else {
            std::cout << "Usage: prog -alt <alt srvr ip> (default = 0.0.0.0:5001)\n" 
                      << "            -backup <backup listens on port> (default = 5001)\n";

                return -1;        
        }                
        arg++;
    }
    return 0;
}

int main (int argc, char** argv) {
  // Parse any arguments
  if (parse_args(argc, argv) <0) return -1;
  
  //mode = backup
  state = BACKUP_NORMAL;

  // Run server
  // std::thread client_server(run_server);

  //Run backup    
  run_as_backup();

  // client_server.join();
  return 0;

}
