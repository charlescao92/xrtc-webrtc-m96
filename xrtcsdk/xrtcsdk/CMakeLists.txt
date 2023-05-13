cmake_minimum_required(VERSION 3.8)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CMAKE_C_COMPILER "/usr/bin/clang")
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
endif()

project(krtcsdk)

if (CMAKE_CL_64) 
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(BUILD_TYPE "x64-Debug")
    else()
        set(BUILD_TYPE "x64-Release")
    endif()
else()
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(BUILD_TYPE "x86-Debug")
    else()
        set(BUILD_TYPE "x86-Release")
    endif()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/bin)

set(KRTC_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(KRTC_THIRD_PARTY_DIR ${KRTC_DIR}/../xrtc_third_party/out/install/${BUILD_TYPE}/${CMAKE_SYSTEM_NAME})
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(KRTC_THIRD_PARTY_DIR ${KRTC_DIR}/third_party)
endif()

set(WEBRTC_DIR ${KRTC_DIR}/../../webrtc/webrtc-checkout/src)
set(WEBRTC_INCLUDE_DIR ${WEBRTC_DIR})
set(WEBRTC_LIB_DIR ${WEBRTC_DIR}/out/x64_debug_m96/obj)

add_definitions(-DUNICODE -D_UNICODE)

add_subdirectory("./xrtc")
add_subdirectory("./examples")
