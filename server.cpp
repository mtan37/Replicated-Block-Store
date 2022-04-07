//#define GRPC_ARG_MAX_RECONNECT_BACKOFF_MS 1000

#include <condition_variable>
#include <google/protobuf/empty.pb.h>
#include <grpc++/grpc++.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <fstream>

#include <fcntl.h>
#include <unistd.h>

#include "client_status_defs.h"
#include "ebs.grpc.pb.h"
#include "ReaderWriter.h"
#include "helper.h"
#include <sys/stat.h>

/*################
# Constants
################*/
// Ports to listen on 
const std::string DEF_SERVER_PORT = "18001";// default port to listen on server service
const std::string DEF_SERVER_PORT_ALT = "18002";// TEST

const std::string DEF_BACKUP_PORT = "18003";// default port to listen on backup service
const std::string DEF_BACKUP_PORT_ALT = "18004";// TEST
const std::string DEF_SHARED_PORT = "18005";// TEST
const int HB_INIT_TIMEOUT = 15;
const int HB_FAIL_TIMEOUT = 2;
const int HB_SEND_TIMEOUT = 1;

#define NUM_BLOCKS 256 // 1MB volume
#define BLOCK_SIZE 4096

/*################
# Globals
################*/
std::string pb_ip = "0.0.0.0"; // IP addr I listen on
std::string alt_ip = "0.0.0.0"; // IP addr of secondary server
std::string shared_addy = "0.0.0.0";
int is_alt = false; // Variable used for easy local testing...
int is_shared = false; // Variable used to share client server IP (10.10.1.4)
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

std::vector<int> offset_log = {};

std::shared_ptr<grpc::Channel> channel;
std::unique_ptr<ebs::Backup::Stub> stub;

// Run grpc service in a loop
void run_service(grpc::Server *server, std::string serviceName) {
  std::cout << "Starting to run " << serviceName << "\n";
  server->Wait();
}

inline void check_offset(char* offset, int16_t code) {
  if (strncmp(offset, "CRASH", 5) == 0) {
    if ((char)state == offset[5] && code == (int16_t)(offset[6])) {
      assert(0);
      std::cout << "JUST ASSERTED SOME BULLSHIT" << std::endl;
    }
    else {
      memset(offset, 0, 8);
    }
  }
}

/**
 makes copy of copy_path and saves it as tmp_path
*/
int copy_file(std::string copy_path, std::string temp_path){
    int fd;
    // open cache file
    fd = ::open(copy_path.c_str(), O_RDONLY, 0777);
    if (fd < 0){return -1;}
    // get copy file deets and write to string
    int length = 4096;
    if (length<0){return -1;}
    if (lseek(fd, 0, SEEK_SET) < 0) {return -1;}
    char *pChars = new char[length];
    int read_length = read(fd, pChars, length);
    // now have cache file as string, close cache file
    // and save data to temp location
    if (::close(fd) < 0){return -1;}
    // write char string to new file
    //Open file for write
    fd = ::open(temp_path.c_str(), O_CREAT | O_WRONLY | O_SYNC, 0777);
    if (fd < 0){return -1;}
    // Write to temp file and close
    if (write(fd, pChars, length) < 0){
        ::close(fd);
        return -1;
    }
    // Force flush
    if (fsync(fd) < 0){
        ::close(fd);
        return -1;
    }
    ::close(fd);
    delete pChars;
    return 0;
}
std::string get_time_str(){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long tmstr;
    tmstr = (long)ts.tv_nsec;
    return std::to_string(tmstr);
}
/**
 get_temp_path()
 Generates temporary file name by adding date time to file name
*/
std::string get_temp_path(std::string userpath){
    // add current timestamp to name
    userpath = userpath + "_tmp_" + get_time_str();
    // std::cout << "Temp path : " << userpath << std::endl;
    return userpath;
}

int initialize_volume(int block) {
  std::ifstream volume_exists("volume/" + std::to_string(block));
  if (volume_exists.good()) {
    volume_exists.close();
    return 1;
  } else {
    std::ofstream volume_create("volume/" + std::to_string(block));
    if (!volume_create.good())
      return 0;
    
    char zero = 0;
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    volume_create.write(buf, BLOCK_SIZE);
  
    volume_create.close();
    return 1;
  }
}

char* volume_read(int offset) {
  //Reading 1 block
  int block = offset / 4096;
  initialize_volume(block);
  std::ifstream volume("volume/" + std::to_string(block));
  if (!volume.good())
    return 0;
  char* buf = (char*) malloc(BLOCK_SIZE);
  volume.seekg(offset % 4096, std::ios::beg);
  volume.read(buf, BLOCK_SIZE - (offset % 4096));
  
  //Reading 2nd block (misaligned)
  if (offset % 4096 != 0) {
    int block_extra = (offset / 4096) + 1;
    initialize_volume(block_extra);
    std::ifstream volume_extra("volume/" + std::to_string(block_extra));
    if (!volume_extra.good())
      return 0;
    volume_extra.seekg(0, std::ios::beg);
    volume_extra.read(buf + BLOCK_SIZE - (offset % 4096), offset % 4096);
  }
  
  return buf;
}

//Not sure what type is being sent for data; string, char[], etc?
int volume_write(const char* data, int offset) {
  int block = offset / 4096;
  initialize_volume(block);
 
  std::string true_file = "volume/" + std::to_string(block);
	std::string block_temp = get_temp_path(true_file);
	copy_file(true_file, block_temp);
	
  std::fstream volume(block_temp, std::ios::in | std::ios::out);
  if (!volume.good())
    return 0;
  volume.seekp(offset % 4096, std::ios::beg);
  volume.write(data, BLOCK_SIZE - (offset % 4096));
  volume.flush();
  volume.close();
  std::cout << "write1 " << block_temp << " " << true_file << std::endl;
  rename(block_temp.c_str(), true_file.c_str());
  
  if (offset % 4096 != 0) {
    int block_extra = (offset / 4096) + 1;
    initialize_volume(block_extra);
    
    std::string true_file_extra = "volume/" + std::to_string(block_extra);
	  std::string block_temp_extra = get_temp_path(true_file_extra);
	  copy_file(true_file_extra, block_temp_extra);
    
    std::fstream volume_extra(block_temp_extra, std::ios::in | std::ios::out);
    if (!volume_extra.good())
      return 0;
    volume_extra.seekp(0, std::ios::beg);
    volume_extra.write(data + BLOCK_SIZE - (offset % 4096), offset % 4096);
    volume_extra.flush();
    volume_extra.close();
      std::cout << "write2 " << block_temp_extra << " " << true_file_extra << std::endl;
    rename(block_temp_extra.c_str(), true_file_extra.c_str());
  }
  
  return 1;
}




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

    long offset = request->offset();
    check_offset((char*)&offset, 0);

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
    long block_num = (offset/BLOCK_SIZE) % NUM_BLOCKS;
    long next_block = block_num + 1;
    if (next_block == NUM_BLOCKS) {
      next_block = block_num;
      block_num = 0; // acquire the lower number block lock first
    }

    block_locks[block_num].acquire_read();
    if (remainder) {
      block_locks[next_block].acquire_read();
    }

    char* buf = volume_read(offset);
    if (buf == 0) {
      reply->set_status(EBS_VOLUME_ERR);
      return grpc::Status::OK;
    }
    else {
      std::string s_buf;
      s_buf.resize(BLOCK_SIZE);
      memcpy(const_cast<char*>(s_buf.data()), buf, BLOCK_SIZE);
      free(buf);
      reply->set_status(EBS_SUCCESS);
      reply->set_data(s_buf);
    }


    if (remainder) {
      block_locks[next_block].release_read();
    }
    block_locks[block_num].release_read();

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

    long offset = request->offset();
    check_offset((char*)&offset, 0);

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
    long block_num = (offset/BLOCK_SIZE) % NUM_BLOCKS;
    long next_block = block_num + 1;
    if (next_block == NUM_BLOCKS) {
      next_block = block_num;
      block_num = 0; // acquire the lower number block lock first
    }

    block_locks[block_num].acquire_write();
    if (remainder) {
      block_locks[next_block].acquire_write();
    }

    //log or send to backup here
    
    //Send to backup
    if (state == PRIMARY_NORMAL) {
      ebs::WriteReply relay_reply;
      grpc::ClientContext relay_context;
      grpc::Status status = stub->write(&relay_context, *request, &relay_reply);
      std::cout << "Primary sent write relay" << status.ok() << " " << relay_reply.status() << std::endl;
      
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
      long log_off = request->offset();
      if (std::find(offset_log.begin(), offset_log.end(), log_off) == offset_log.end())
        offset_log.push_back(log_off);
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
      block_locks[next_block].release_write();
    }
    block_locks[block_num].release_write();
    recovery_lock.release_read();

    return grpc::Status::OK;
  }
};



/**
 * Helper function to initialize grpc channel
 */
void initialize_grpc_channel() {
  std::string address = alt_ip + ":" + DEF_BACKUP_PORT_ALT;
  if (is_alt) {
    address = alt_ip + ":" + DEF_BACKUP_PORT;
  }
  std::cout << "Primary starting to send heatbeat to " << address << std::endl;

  grpc::ChannelArguments args;
  args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 1000);
  channel = grpc::CreateCustomChannel(address, grpc::InsecureChannelCredentials(), args);
  stub = ebs::Backup::NewStub(channel);
}

/**
 * Export server grpc interface
 */
std::unique_ptr<grpc::Server> export_server (std::string ip, ServerImpl *ebs_server) {
  std::string my_address = ip + ":" + DEF_SERVER_PORT;
  if (is_alt) my_address = ip + ":" + DEF_SERVER_PORT_ALT;
  std::string new_addy = "10.10.1.4:";
  if (is_shared) shared_addy = new_addy + DEF_SHARED_PORT;
  std::cout << "server service listening on "  << my_address << "\n";
  if (is_shared) std::cout << "server service listening on "  << shared_addy << "\n";


  grpc::ServerBuilder builder;
  builder.AddListeningPort(my_address, grpc::InsecureServerCredentials());
  if (is_shared) builder.AddListeningPort(shared_addy, grpc::InsecureServerCredentials());
  builder.RegisterService(ebs_server);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  return server;
}

void start_primary_heartbeat() {
  // initialize the variable needed for grpc heartbeat call

   

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

      ebs::ReplayReq log_request;
      for (int i : offset_log) {
        ebs::WriteReq* log_item = log_request.add_item();
        log_item->set_offset(i);
        check_offset((char*)&i, -1);
        char* buf = volume_read(i);
        if (buf == 0) {
          std::cout << "Error reading data for log replay" << std::endl;
        }
        std::string s_buf;
        s_buf.resize(BLOCK_SIZE);
        memcpy(const_cast<char*>(s_buf.data()), buf, BLOCK_SIZE);
        free(buf);
        log_item->set_data(s_buf);
      }

      // send the log request to backup
      bool backup_write_success = false;
      while (!backup_write_success) {
        grpc::ClientContext log_context;
        ebs::ReplayReply log_reply;

        grpc::Status log_status = 
          stub->replayLog(&log_context, log_request, &log_reply);
        std::cout << "send log grpc call status is " << log_status.error_code() << std::endl;

        if (log_status.ok()){

          // check the status returned by the backup
          if (log_reply.status() == EBS_SUCCESS) {
            backup_write_success = true;
            offset_log.clear();
          } else {
            // Backup disk error. This is probably a case that require manual intervention 
            // TODO not going to handle this for now, just break
            break;
          }

        } else {
          break;
        }

      }

      if (backup_write_success) {
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

    // Want larger timeout on init then rest of time
    int timeout = HB_INIT_TIMEOUT;

    while (true){
      std::cout << "TEST: start of new backup heartbeat iteration. My state is " << state <<"\n";
      
      set_time(&now);
      elapsed = difftimespec_s(last_heartbeat, now);

      std::cout << "Checking Timeout: " << elapsed <<"\n";
      if (elapsed < timeout){
        if (elapsed < 0) elapsed = 0;
        // continue - still good, sleep until HB_LISTEN_TIMEOUT period and check again
        sleep(timeout); 
        timeout = HB_FAIL_TIMEOUT;
      } else {
        // Primary has timed out
        std::cout << "Primary is non-responsive, transitioning to primary" << std::endl;      
        break;            
      }    
    }

    // Transition into primary state
    state = INITIALIZING;
    initialize_grpc_channel();
    std::thread primary_server_heartbeat_thread(start_primary_heartbeat);
    
    // stop backup service
    backup_service->Shutdown();
    backup_service_thread->join();

    block_locks = new ReaderWriter[NUM_BLOCKS];

    // if (is_shared){
    //   // Test failover with swapped IP
    //   std::cout << "NOW LISTENING ON 10.10.1.4" << std::endl;
    //   int ret = system("sudo ip addr add 10.10.1.4/24 dev ens1f0");
    // }


    // // channels should already be initialized in the backup heartbeat before heartbeat thread start
    // // export server interface to listen for clients
    // ServerImpl ebs_server;
    // std::unique_ptr<grpc::Server> server_service = export_server(pb_ip, &ebs_server);
    // std::string name = "server";
    // std::thread server_service_thread(run_service, server_service.get(), name);

    // std::cout << "Start sending out primary heartbeat" << std::endl;
    state = SINGLE_SERVER;
    state_cv.notify_all();

    primary_server_heartbeat_thread.join();
    // server_service->Shutdown();
    // server_service_thread.join();
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
    std::cout << "Backup got write relay" << std::endl;
    long offset = request->offset();
    check_offset((char*)&offset, 0);
    
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
    std::cout << "Backup got replayLog call \n";

    // update heartbeat
    set_time(&last_heartbeat);
    
    //while more writes:
    for (int i = 0; i < request->item_size(); i++) {
      ebs::WriteReq log_item = request->item(i);
      //do write
      long offset = log_item.offset();
      std::cout << "Replaying log, offset " << offset << std::endl;
      // update heartbeat again in case the log is really long
      set_time(&last_heartbeat);
            
      if (volume_write(log_item.data().data(), offset) == 0) {
        reply->set_status(EBS_VOLUME_ERR);
        std::cout << "replayLog write error" << std::endl;
        return grpc::Status::OK;
      }
    }
    
    reply->set_status(EBS_SUCCESS);
    
    //return success
    return grpc::Status::OK;
  }
};




/**
 * Export backup grpc interface
 */
std::unique_ptr<grpc::Server> export_backup (std::string ip, BackupImpl *backup) {
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

    // if (argc >= 4 && std::string(argv[3]).compare("-alt") == 0) {
    //   is_alt = true; // used for testing!!
    // }
    
    for (int i = 3; i < argc; i++){
      if (std::string(argv[i]).compare("-alt") == 0) {
        is_alt = true; // used for testing!!
      } else if (std::string(argv[i]).compare("-shared") == 0) {
        std::cout << "Setting is_shared to true" << std::endl;
        is_shared = true;
      }
    }

    return 0;
}

int main (int argc, char** argv) {
  // Parse any arguments to get ip address of the other server
  if (parse_args(argc, argv) <0) return -1;
  std::cout << "TEST: local computer ip " << pb_ip << "\n";
  std::cout << "TEST: alternate computer ip " << alt_ip << "\n";
  if (is_shared) int ret = system("sudo ip addr del 10.10.1.4/24 dev ens1f0");
  
  mkdir("volume", 0700);
  
  // server start up as a backup
  state = BACKUP_NORMAL;

  // export backup grpc service in a seperate thread
  BackupImpl backup;
  std::unique_ptr<grpc::Server> backup_service = export_backup(pb_ip, &backup);
  std::string name = "backup";
  std::thread backup_service_thread(run_service, backup_service.get(), name);

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
