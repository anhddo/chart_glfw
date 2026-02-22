
file(GLOB_RECURSE SOURCES "*.cpp")

add_executable(chart_glfw 
    main.cpp
    ${SOURCES}
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.json ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)

file(GLOB_RECURSE IMGUI_SOURCES "../third_party/imgui/*.cpp")
file(GLOB_RECURSE IMGUI_HEADERS "../third_party/imgui/*.h")
add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_HEADERS})

target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/imgui)
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glfw)
target_compile_definitions(imgui PUBLIC GLFW_INCLUDE_NONE)

target_link_libraries(imgui PUBLIC glad)

target_include_directories(chart_glfw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(chart_glfw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/glfw)
find_package(glm CONFIG REQUIRED)

target_link_libraries(chart_glfw PRIVATE imgui glfw glm::glm twsadapter glad)

add_custom_command(TARGET chart_glfw POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:twsadapter>
        $<TARGET_FILE_DIR:chart_glfw>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "$<IF:$<CONFIG:Debug>,${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin,${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin>"
        $<TARGET_FILE_DIR:chart_glfw>
)
