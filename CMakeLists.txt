# modified from https://github.com/grpc/grpc/raw/v1.45.0/examples/cpp/helloworld/CMakeLists.txt

cmake_minimum_required(VERSION 3.5.1)
project(CS739-EBS C CXX)

find_package(Threads REQUIRED)

find_package(Protobuf REQUIRED)
message(STATUS "Using protobuf ${Protobuf_VERSION}")
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
if(CMAKE_CROSSCOMPILING)
    find_program(_PROTOBUF_PROTOC protoc)
else()
    set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
endif()

find_package(gRPC REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")
set(_GRPC_GRPCPP gRPC::grpc++)
if(CMAKE_CROSSCOMPILING)
    find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
endif()

# Proto file
get_filename_component(hw_proto "./protos/ebs.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

# Generated sources
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/ebs.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/ebs.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/ebs.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/ebs.grpc.pb.h")

add_custom_command(
      OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${hw_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}"
      DEPENDS "${hw_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# hw_grpc_proto
add_library(hw_grpc_proto
  ${hw_grpc_srcs}
  ${hw_grpc_hdrs}
  ${hw_proto_srcs}
  ${hw_proto_hdrs})
target_link_libraries(hw_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

add_library(ebs ebs.cpp
  ${hw_grpc_srcs}
  ${hw_grpc_hdrs}
  ${hw_proto_srcs}
  ${hw_proto_hdrs})
target_link_libraries(ebs
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# c++ targets
foreach(_target server)
  add_executable(${_target} "${_target}.cpp" ReaderWriter.cpp helper.cpp)
  target_link_libraries(${_target}
    hw_grpc_proto
  )
endforeach()

# c test targets
foreach(_target simple_test simple_test_crash single_client_benchmark continuous_write_read data_overlap_test data_writecrash_test data_failover_test data_singleserver_test)
  add_executable(${_target} "${_target}.c")
  target_link_libraries(${_target}
    ebs
  )
endforeach()

add_executable(fuse_backing backing.c)
target_include_directories(fuse_backing PUBLIC "/usr/include/fuse3/")
target_link_libraries(fuse_backing ebs fuse3 pthread)
