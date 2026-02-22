
file(GLOB_RECURSE HEADERS "*.h")
file(GLOB_RECURSE SOURCES "*.cpp")

add_executable(chart_glfw 
    main.cpp
    ${SOURCES}
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.json ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)

add_library(glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glad/include)

file(GLOB_RECURSE IMGUI_SOURCES "../third_party/imgui/*.cpp")
file(GLOB_RECURSE IMGUI_HEADERS "../third_party/imgui/*.h")
add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_HEADERS})

target_include_directories(chart_glfw PUBLIC ${HEADERS})
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/imgui)
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glfw)
target_link_libraries(imgui PUBLIC glad)

target_include_directories(chart_glfw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glfw)

add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glfw/lib/Debug/glfw3.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glfw"
)
find_package(glm CONFIG REQUIRED)

file(GLOB_RECURSE IBKR_SOURCES "data_api/ikbr/*.cpp")
file(GLOB_RECURSE IBKR_HEADERS "data_api/ikbr/*.h")

add_library(ibkr STATIC ${IBKR_SOURCES} ${IBKR_HEADERS})
target_include_directories(ibkr PUBLIC ${CMAKE_SOURCE_DIR}/third_party/ibkr_api/client)
target_include_directories(ibkr PUBLIC ${CMAKE_SOURCE_DIR}/third_party/ibkr_api)
target_link_libraries(ibkr PUBLIC ibkr_proto)

target_include_directories(chart_glfw PRIVATE ${CMAKE_SOURCE_DIR}/third_party/ibkr_api/client)
target_link_libraries(chart_glfw PRIVATE imgui glfw glm::glm ibkr twsapi glad)
