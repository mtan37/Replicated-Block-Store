#include <condition_variable>
#include <google/protobuf/empty.pb.h>
#include <grpc++/grpc++.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <fstream>

#include "client_status_defs.h"
#include "ebs.grpc.pb.h"
#include "ReaderWriter.h"
#include "helper.h"

/*################
# Constants
################*/
// Ports to listen on 
const std::string DEF_SERVER_PORT = "18001";// default port to listen on server service
const std::string DEF_SERVER_PORT_ALT = "18002";// TEST

const std::string DEF_BACKUP_PORT = "18003";// default port to listen on backup service
const std::string DEF_BACKUP_PORT_ALT = "18003";// TEST
const int HB_FAIL_TIMEOUT = 8;
const int HB_SEND_TIMEOUT = 1;

#define NUM_BLOCKS 256 // 1MB volume
#define BLOCK_SIZE 4096

/*################
# Globals
################*/
std::string pb_ip = "0.0.0.0"; // IP addr I listen on
std::string alt_ip = "0.0.0.0"; // IP addr of secondary server
int is_alt = false; // Variable used for easy local testing...
timespec last_heartbeat; // time last heartbeat received by backup

// Initial state: BACKUP_NORMAL
// BACKUP_NORMAL -> INITIALIZING when backup doesn't get heartbeat from primary
// INITIALIZING -> SINGLE_SERVER when a backup is ready to act as a primary
// PRIMARY_NORMAL -> SINGLE_SERVER when the primary can't reach the backup
// SINGLE_SERVER -> RECOVERING when the primary detects the backup comes up
// RECOVERING -> PRIMARY_NORMAL when recovery is done
enum {
  PRIMARY_NORMAL = 1,
  BACKUP_NORMAL = 2,
  SINGLE_SERVER = 3,
  INITIALIZING = 4,
  RECOVERING = 5
} state;

ReaderWriter recovery_lock;

std::mutex state_mutex;
std::condition_variable state_cv;

ReaderWriter* block_locks;

std::vector<int> offset_log;

std::shared_ptr<grpc::Channel> channel;
std::unique_ptr<ebs::Backup::Stub> stub;

int initialize_volume() {
  std::ifstream volume_exists("volume");
  if (volume_exists.good()) {
    volume_exists.close();
    return 1;
  } else {
    std::ofstream volume_create("volume");
    if (!volume_create.good())
      return 0;
    
    char zero = 0;
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    for (int i = 0; i < NUM_BLOCKS; i++)
      volume_create.write(buf, BLOCK_SIZE);
  
    volume_create.close();
    return 1;
  }
}

char* volume_read(int offset) {
  std::ifstream volume("volume");
  if (!volume.good())
    return 0;
  char* buf = (char*) malloc(BLOCK_SIZE);
  volume.seekg(offset, std::ios::beg);
  volume.read(buf, BLOCK_SIZE);
  return buf;
}

//Not sure what type is being sent for data; string, char[], etc?
int volume_write(const char* data, int offset) {
  std::fstream volume("volume", std::ios::in | std::ios::out);
  if (!volume.good())
    return 0;
  volume.seekp(offset, std::ios::beg);
  volume.write(data, BLOCK_SIZE);
  volume.flush();
  volume.close();
  return 1;
}

void start_primary_heartbeat() {
  // initialize the variable needed for grpc heartbeat call
  std::string address = alt_ip + ":" + DEF_BACKUP_PORT_ALT;
  if (is_alt) {
    address = alt_ip + ":" + DEF_BACKUP_PORT;
  }

  std::cout << "Start operating as - send heartbeat to " << address << std::endl;
  channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
  
  stub = ebs::Backup::NewStub(channel);
  google::protobuf::Empty request;
  google::protobuf::Empty reply;

  while (true){

    std::cout << "TEST: start of new primary heartbeat iteration. My state is " << state <<"\n";
    grpc::ClientContext context;  
    grpc::Status status = stub->heartBeat(&context, request, &reply);
    std::cout << "grpc call status is " << status.error_code() << "\n";

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
      for (int i : offset_log) {
        ebs::WriteReq* log_item = log_request.add_item();
        log_item->set_offset(i);
        char* buf = volume_read(i);
        if (buf == 0) {
          std::cout << "Error reading data for log replay" << std::endl;
        }
        std::string s_buf;
        s_buf.resize(BLOCK_SIZE);
        memcpy(s_buf.data(), buf, BLOCK_SIZE);
        free(buf);
        log_item->set_data(s_buf);
      }
      ebs::ReplayReply log_reply;
      grpc::Status log_status = 
        stub->replayLog(&log_context, log_request, &log_reply);
      std::cout << "send log grpc call status is " << log_status.error_code() << "\n";
      if (log_status.ok()){
        state = PRIMARY_NORMAL;
        offset_log.clear();
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

  delete[] block_locks;
  std::cout << "Primary heartbeat call terminate\n";
}

void start_backup_heartbeat(
  grpc::Server *backup_service,
  std::thread *backup_service_thread) {

    // start monitoring heartbeat
    double elapsed = 0;
    timespec now;
    set_time(&last_heartbeat); 

    while (true){
      std::cout << "TEST: start of new backup heartbeat iteration. My state is " << state <<"\n";
      
      set_time(&now);
      elapsed = difftimespec_s(last_heartbeat, now);

      std::cout << "Checking Timeout: " << elapsed <<"\n";
      if (elapsed < HB_FAIL_TIMEOUT){
        if (elapsed < 0) elapsed = 0;
        // continue - still good, sleep until HB_LISTEN_TIMEOUT period and check again
        sleep(HB_SEND_TIMEOUT); 
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

    block_locks = new ReaderWriter[NUM_BLOCKS];

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
                      const ebs::WriteReq *request,
                      ebs::WriteReply *reply) {
    set_time(&last_heartbeat);
    std::cout << "GOT WRITE RELAY" << std::endl;
    long offset = request->offset();
    
    if (volume_write(request->data().data(), offset) == 0) {
      reply->set_status(EBS_VOLUME_ERR);
    }
    else {
      reply->set_status(EBS_SUCCESS);
    }
    
    return grpc::Status::OK;
  }

  grpc::Status replayLog (grpc::ServerContext *context,
                          const ebs::ReplayReq *request,
                          ebs::ReplayReply *reply) {
    // update heartbeat
    set_time(&last_heartbeat);
    
    //while more writes:
    for (int i = 0; i < request->item_size(); i++) {
      ebs::WriteReq log_item = request->item(i);
      //do write
      std::cout << "GOT WRITE RELAY" << std::endl;
      long offset = log_item.offset();
      
      if (volume_write(log_item.data().data(), offset) == 0) {
        reply->set_status(EBS_VOLUME_ERR);
        std::cout << "replayLog write error" << std::endl;
      }
    }
    
    reply->set_status(EBS_SUCCESS);
    
    //return success
    std::cout << "get replayLog call \n";
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
                    ebs::ReadReply *reply) {
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

    if (state == BACKUP_NORMAL) {
      reply->set_status(EBS_NOT_PRIMARY);
      reply->set_primary(alt_ip + ":" + DEF_BACKUP_PORT_ALT);
      return grpc::Status::OK;
    }

    std::unique_lock<std::mutex> state_lock(state_mutex);
    while (state == INITIALIZING) {
      state_cv.wait(state_lock);
    }
    state_lock.unlock();

    long remainder = request->offset()%BLOCK_SIZE;
    long offset = request->offset();

    block_locks[offset/BLOCK_SIZE].acquire_read();
    if (remainder) {
      block_locks[offset/BLOCK_SIZE + 1].acquire_read();
    }

    char* buf = volume_read(offset);
    if (buf == 0) {
      reply->set_status(EBS_VOLUME_ERR);
      return grpc::Status::OK;
    }
    else {
      std::string s_buf;
      s_buf.resize(BLOCK_SIZE);
      memcpy((void*) s_buf.data(), buf, BLOCK_SIZE);
      free(buf);
      reply->set_status(EBS_SUCCESS);
      reply->set_data(s_buf);
    }


    if (remainder) {
      block_locks[offset/BLOCK_SIZE + 1].release_read();
    }
    block_locks[offset/BLOCK_SIZE].release_read();

    return grpc::Status::OK;
  }

  grpc::Status write (grpc::ServerContext *context,
                      const ebs::WriteReq *request,
                      ebs::WriteReply *reply) {
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

    if (state == BACKUP_NORMAL) {
      reply->set_status(EBS_NOT_PRIMARY);
      reply->set_primary(alt_ip + ":" + DEF_BACKUP_PORT_ALT);
      return grpc::Status::OK;
    }

    recovery_lock.acquire_read();

    std::unique_lock<std::mutex> state_lock(state_mutex);
    while (state == INITIALIZING) {
      state_cv.wait(state_lock);
    }
    state_lock.unlock();

    long remainder = request->offset()%BLOCK_SIZE;
    long offset = request->offset();

    block_locks[offset/BLOCK_SIZE].acquire_write();
    if (remainder) {
      block_locks[offset/BLOCK_SIZE + 1].acquire_write();
    }

    //log or send to backup here
    
    //Send to backup
    if (state == PRIMARY_NORMAL) {
      ebs::WriteReply relay_reply;
      grpc::ClientContext relay_context;
      grpc::Status status = stub->write(&relay_context, *request, &relay_reply);
      std::cout << "WRITE RELAY " << status.ok() << " " << relay_reply.status() << std::endl;
      
      if (!status.ok())
        state = SINGLE_SERVER;
      else
        if (relay_reply.status() == EBS_VOLUME_ERR) {
          reply->set_status(EBS_VOLUME_ERR);
          goto free_locks;
        } 
    }
    
    //Send to log
    if (state == SINGLE_SERVER) {
      if (std::find(offset_log.begin(), offset_log.end(), offset) == offset_log.end())
        offset_log.push_back(offset);
    }
    
    //Make local write
    if (volume_write(request->data().data(), offset) == 0) {
      reply->set_status(EBS_VOLUME_ERR);
    }
    else {
      reply->set_status(EBS_SUCCESS);
    }

    free_locks:
    if (remainder) {
      block_locks[offset/BLOCK_SIZE + 1].release_write();
    }
    block_locks[offset/BLOCK_SIZE].release_write();
    recovery_lock.release_read();

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
std::unique_ptr<grpc::Server> export_server (std::string ip, ServerImpl *ebs_server) {
  std::string my_address = ip + ":" + DEF_SERVER_PORT;
  if (is_alt) my_address = ip + ":" + DEF_SERVER_PORT_ALT;
  std::cout << "server service listening on "  << my_address << "\n";

  grpc::ServerBuilder builder;
    builder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
  builder.RegisterService(ebs_server);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  return server;
}

/**
 * Export backup grpc interface
 */
std::unique_ptr<grpc::Server> export_backup (std::string ip, BackupImpl *backup) {
  initialize_volume();
  std::string my_address = ip + ":" + DEF_BACKUP_PORT;
  if (is_alt) my_address = ip + ":" + DEF_BACKUP_PORT_ALT;
  std::cout << "backup service listening on "  << my_address << "\n";

  grpc::ServerBuilder builder;
  builder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
  builder.RegisterService(backup);
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
      std::cout << "Usage: prog <pb srvr ip> <alt srvr ip> -alt (default = 0.0.0.0)\n"; 
      return -1;
    }

    // TODO need to do error checking on the argument
    pb_ip = std::string(argv[1]);
    alt_ip = std::string(argv[2]);

    if (argc >= 4 && std::string(argv[3]).compare("-alt") == 0) {
      is_alt = true; // used for testing!!
    }
    
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
  BackupImpl backup;
  std::unique_ptr<grpc::Server> backup_service = export_backup(pb_ip, &backup);
  std::string name = "backup";
  std::thread backup_service_thread(run_service, backup_service.get(), name);

  // export server interface to listen for clients
  ServerImpl ebs_server;
  std::unique_ptr<grpc::Server> server_service = export_server(pb_ip, &ebs_server);
  name = "server";
  std::thread server_service_thread(run_service, server_service.get(), name);

  // start heartbeat thread(as backup)
  // This thread monitors for timeout and update state for transition
  std::thread heartbeat(start_backup_heartbeat, backup_service.get(), &backup_service_thread);

  // once hearbeat thread stop, stop service services
  heartbeat.join();
  
  // Just to be safe(?). Theoretically backup service will be shutdown by the backup thread already. 
  backup_service->Shutdown();
  backup_service_thread.join();
  
  server_service->Shutdown();
  server_service_thread.join();
  
  std::cout << "TEST: server terminated successfully\n";
  return 0;
}
