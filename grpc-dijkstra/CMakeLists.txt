# Copyright 2018 gRPC authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# cmake build file for C++ helloworld example.
# Assumes protobuf and gRPC have been installed using cmake.
# See cmake_externalproject/CMakeLists.txt for all-in-one cmake build
# that automatically builds all the dependencies before building dijkstra.

cmake_minimum_required(VERSION 3.5.1)

project(Dijkstra C CXX)

include(common.cmake)

# Proto file
get_filename_component(dijkstra_proto "dijkstra.proto" ABSOLUTE)
get_filename_component(dijkstra_proto_path "${dijkstra_proto}" PATH)

# Generated sources
set(dijkstra_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/dijkstra.pb.cc")
set(dijkstra_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/dijkstra.pb.h")
set(dijkstra_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/dijkstra.grpc.pb.cc")
set(dijkstra_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/dijkstra.grpc.pb.h")
add_custom_command(
      OUTPUT "${dijkstra_proto_srcs}" "${dijkstra_proto_hdrs}" "${dijkstra_grpc_srcs}" "${dijkstra_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${dijkstra_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${dijkstra_proto}"
      DEPENDS "${dijkstra_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# dijkstra_grpc_proto
add_library(dijkstra_grpc_proto
  ${dijkstra_grpc_srcs}
  ${dijkstra_grpc_hdrs}
  ${dijkstra_proto_srcs}
  ${dijkstra_proto_hdrs})
target_link_libraries(dijkstra_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# Targets greeter_[async_](client|server)
foreach(_target
  dijkstra_client dijkstra_server)
  add_executable(${_target} "${_target}.cc")
  target_link_libraries(${_target}
    dijkstra_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()
