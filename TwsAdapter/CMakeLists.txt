set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fix MSVC __cplusplus macro to report correct C++ standard
if(MSVC)
    add_compile_options(/Zc:__cplusplus /std:c++20)
endif()

set(TWS_SAMPLES_DIR ${CMAKE_SOURCE_DIR}/third_party/tws_api/samples/Cpp/TestCppClient)

add_library(twsadapter SHARED
    ibkr.cpp
    
    ${TWS_SAMPLES_DIR}/TestCppClient.cpp
    ${TWS_SAMPLES_DIR}/AccountSummaryTags.cpp
    ${TWS_SAMPLES_DIR}/AvailableAlgoParams.cpp
    ${TWS_SAMPLES_DIR}/ContractSamples.cpp
    ${TWS_SAMPLES_DIR}/OrderSamples.cpp
    ${TWS_SAMPLES_DIR}/ScannerSubscriptionSamples.cpp
    ${TWS_SAMPLES_DIR}/Utils.cpp
    ${TWS_SAMPLES_DIR}/StdAfx.cpp
    ibkr.h
    
    command.h
    event.h
)
set_target_properties(twsadapter PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_include_directories(twsadapter
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC  ${CMAKE_SOURCE_DIR}/third_party/tws_api/source/cppclient/client
    PUBLIC  ${TWS_SAMPLES_DIR}
    PUBLIC  ${CMAKE_SOURCE_DIR}/third_party/tws_api/source/cppclient
)

target_link_libraries(twsadapter PUBLIC twsapi)
