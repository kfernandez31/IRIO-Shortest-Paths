cmake_minimum_required(VERSION 3.5.1)

project(shortestpaths C CXX)

include(common.cmake)

# Proto file
get_filename_component(shortestpaths_proto "shortestpaths.proto" ABSOLUTE)
get_filename_component(shortestpaths_proto_path "${shortestpaths_proto}" PATH)

# Generated sources
set(shortestpaths_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/shortestpaths.pb.cc")
set(shortestpaths_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/shortestpaths.pb.h")
set(shortestpaths_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/shortestpaths.grpc.pb.cc")
set(shortestpaths_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/shortestpaths.grpc.pb.h")
add_custom_command(
      OUTPUT "${shortestpaths_proto_srcs}" "${shortestpaths_proto_hdrs}" "${shortestpaths_grpc_srcs}" "${shortestpaths_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${shortestpaths_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${shortestpaths_proto}"
      DEPENDS "${shortestpaths_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# shortestpaths_grpc_proto
add_library(shortestpaths_grpc_proto
  ${shortestpaths_grpc_srcs}
  ${shortestpaths_grpc_hdrs}
  ${shortestpaths_proto_srcs}
  ${shortestpaths_proto_hdrs})
target_link_libraries(shortestpaths_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# client
add_executable(client client.cc common.hh)
target_link_libraries(client
shortestpaths_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF}
  ${REDIS_PLUS_PLUS_LIB})


# server
add_executable(server server.cc common.hh)
target_link_libraries(server
  shortestpaths_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})