set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.15)
project(terminal VERSION 1.0 LANGUAGES C CXX)

# Fix MSVC __cplusplus macro and explicitly set C++20 standard
if(MSVC)
    add_compile_options(/Zc:__cplusplus /std:c++20)
endif()



find_package(protobuf CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)




add_library(libbid STATIC IMPORTED GLOBAL)
set_target_properties(libbid PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/third_party/tws_api/source/CppClient/client/lib/libbid.lib"
)

add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)

add_subdirectory(third_party/tws_api)
add_subdirectory(TwsAdapter)
add_subdirectory(cpp45k)
