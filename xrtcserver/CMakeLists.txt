cmake_minimum_required(VERSION 3.5)

project(xrtcserver)

add_definitions("-g -pipe -W -Wall -fPIC -std=gnu++14 -DWEBRTC_POSIX
    -DWEBRTC_LINUX ")

SET(CMAKE_EXE_LINKER_FLAGS " -no-pie")

include_directories(
    "./src"
    "./third_party/include"
    "../libwebrtc/src"
)

link_directories(
    "./third_party/lib"
    "../libwebrtc/out"
)

file(GLOB all_src
    "./src/*.cpp"
    "./src/base/*.cpp"
    "./src/call/*.cpp"
    "./src/server/*.cpp"
    "./src/stream/*.cpp"
    "./src/pc/*.cpp"
    "./src/ice/*.cpp"
    "./src/audio/*.cpp"
    "./src/video/*.cpp"
    "./src/modules/rtp_rtcp/*.cpp"
    "./src/modules/pacing/*.cpp"
    "./src/modules/rtp_rtcp/rtcp_packet/*.cpp"
    "./src/modules/rtp_rtcp/include/*.cpp"
    "./src/modules/video_coding/*.cpp"
)

add_executable(xrtcserver ${all_src})

target_link_libraries(xrtcserver libyaml-cpp.a
    libabsl_strings.a libabsl_throw_delegate.a libev.a libjsoncpp.a  libwebrtc.a
    libssl.a libcrypto.a libabsl_bad_optional_access.a libsrtp2.a 
    -lpthread -ldl 
)
