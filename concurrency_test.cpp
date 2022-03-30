#include <grpc++/grpc++.h>
#include <iostream>
#include <string>

#include "client_status_defs.h"
#include "ebs.grpc.pb.h"
#include "ReaderWriter.h"
#include "helper.h"

const std::string DEF_SERVER_PORT = "18001";

int main (int argc, char** argv) { 
    if (argc < 2) {
        std::cout << "Usage: concurrency <condition>" << std::endl; 
        return -1;
    }

    int condition = std::stoi(argv[1]);

    // call the test function for concurrency
    std::string address = "localhost:" + DEF_SERVER_PORT;
    std::shared_ptr<grpc::Channel> channel = 
        grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    std::unique_ptr<ebs::Server::Stub> stub = ebs::Server::NewStub(channel);

    ebs::TestConReq request;
    request.set_condition(condition);
    ebs::TestConReply reply;
    grpc::ClientContext context; 

    stub->concurrency_write_test(&context, request, &reply);
}

