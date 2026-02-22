cmake_minimum_required(VERSION 3.15)
project(terminal VERSION 1.0 LANGUAGES C CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(protobuf CONFIG REQUIRED)

file(GLOB_RECURSE PROTO_SOURCES "${CMAKE_SOURCE_DIR}/third_party/ibkr_api/client/protobuf/*.pb.cc")
file(GLOB_RECURSE PROTO_HEADERS "${CMAKE_SOURCE_DIR}/third_party/ibkr_api/client/protobuf/*.pb.h")
add_library(ibkr_proto STATIC ${PROTO_SOURCES} ${PROTO_HEADERS})
target_include_directories(ibkr_proto PUBLIC ${CMAKE_SOURCE_DIR}/third_party/ibkr_api/client/protobuf)
target_link_libraries(ibkr_proto PUBLIC protobuf::libprotobuf)

add_subdirectory(third_party/ibkr_api/client)
target_include_directories(twsapi PUBLIC ${CMAKE_SOURCE_DIR}/third_party/ibkr_api/client/protobuf)
target_link_libraries(twsapi PUBLIC protobuf::libprotobuf ibkr_proto)

add_library(libbid STATIC IMPORTED)
set_target_properties(libbid PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/third_party/ibkr_api/client/lib/libbid.lib"
)
target_link_libraries(twsapi PUBLIC libbid)

add_subdirectory(chart_glfw)
