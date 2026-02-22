set(TWS_SAMPLES_DIR ${CMAKE_SOURCE_DIR}/third_party/tws_api/samples/Cpp/TestCppClient)

add_library(twsadapter SHARED
    ibkr.cpp
    ibkr_c_api.cpp
    ${TWS_SAMPLES_DIR}/TestCppClient.cpp
    ${TWS_SAMPLES_DIR}/AccountSummaryTags.cpp
    ${TWS_SAMPLES_DIR}/AvailableAlgoParams.cpp
    ${TWS_SAMPLES_DIR}/ContractSamples.cpp
    ${TWS_SAMPLES_DIR}/OrderSamples.cpp
    ${TWS_SAMPLES_DIR}/ScannerSubscriptionSamples.cpp
    ${TWS_SAMPLES_DIR}/Utils.cpp
    ${TWS_SAMPLES_DIR}/StdAfx.cpp
    ibkr.h
    ibkr_c_api.h
    command.h
)
set_target_properties(twsadapter PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_include_directories(twsadapter
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC  ${CMAKE_SOURCE_DIR}/third_party/ibkr_api/client
    PRIVATE ${TWS_SAMPLES_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/third_party/ibkr_api
    PRIVATE ${CMAKE_SOURCE_DIR}/chart_glfw
)

target_link_libraries(twsadapter PUBLIC twsapi ibkr_proto)
