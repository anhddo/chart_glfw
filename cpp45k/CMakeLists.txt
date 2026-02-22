find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(cpp45k
    main.cpp
    App.cpp
    App.h
    Config.h
    DataManager.h
    renderer.cpp
    renderer.h
    data_api/polygon_io.cpp
    data_api/polygon_io.h
)



target_include_directories(cpp45k
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/data_api
)

target_link_libraries(cpp45k
    PRIVATE twsadapter
    PRIVATE glad
    PRIVATE glfw
    PRIVATE glm::glm
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE imgui::imgui
    PRIVATE opengl32
)

# Copy required DLLs to output directory
add_custom_command(TARGET cpp45k POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:twsadapter>
        $<TARGET_FILE_DIR:cpp45k>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:twsapi>
        $<TARGET_FILE_DIR:cpp45k>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:protobuf::libprotobuf>
        $<TARGET_FILE_DIR:cpp45k>
    COMMENT "Copying DLL dependencies to output directory"
)

# Copy config files to output directory
add_custom_command(TARGET cpp45k POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.json.template
        $<TARGET_FILE_DIR:cpp45k>/config.json.template
    COMMENT "Copying config template"
)

# Copy config.json if it exists, otherwise copy from template
add_custom_command(TARGET cpp45k POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.json
        $<TARGET_FILE_DIR:cpp45k>/config.json
    COMMAND ${CMAKE_COMMAND} -E echo "Copied config.json"
    COMMENT "Copying config.json if it exists"
)

# If config.json doesn't exist in source, copy template as config.json
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.json")
    add_custom_command(TARGET cpp45k POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/config.json.template
            $<TARGET_FILE_DIR:cpp45k>/config.json
        COMMENT "Creating config.json from template"
    )
endif()

# Copy be_data.json if it exists
add_custom_command(TARGET cpp45k POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/be_data.json
        $<TARGET_FILE_DIR:cpp45k>/be_data.json
    COMMAND_EXPAND_LISTS
)
